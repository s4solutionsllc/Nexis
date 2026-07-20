#include "wipe_free_space_dialog.h"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <Utils/format_util.h>

namespace {

// SSO-15382: conservative assumed sustained write throughput used only to
// give the user a rough "how long will this take" preview number on the
// non-TRIM (fill-and-delete) path. Deliberately pessimistic — better to
// under-promise than to leave the user staring at a stalled-looking bar.
constexpr double kAssumedWriteBytesPerSecond = 100.0 * 1000.0 * 1000.0; // 100 MB/s

QString formatDuration(double seconds)
{
    if (seconds < 60)
        return QObject::tr("~%1 sec").arg(qMax(1, qRound(seconds)));
    const int minutes = qRound(seconds / 60.0);
    return QObject::tr("~%1 min").arg(qMax(1, minutes));
}

} // namespace

WipeFreeSpaceDialog::WipeFreeSpaceDialog(QWidget *parent)
    : QDialog(parent)
    , mService(WipeFreeSpaceService::ins())
{
    setObjectName(QStringLiteral("WipeFreeSpaceDialog"));
    setWindowTitle(tr("Wipe Free Space"));
    setMinimumWidth(520);

    buildUi();
    populateVolumes();

    connect(mService, &WipeFreeSpaceService::progressUpdated,
            this, &WipeFreeSpaceDialog::onProgress);
    connect(mService, &WipeFreeSpaceService::finished,
            this, &WipeFreeSpaceDialog::onFinished);
    connect(mService, &WipeFreeSpaceService::cancelled,
            this, &WipeFreeSpaceDialog::onCancelled);
}

WipeFreeSpaceDialog::~WipeFreeSpaceDialog()
{
    disconnect(mService, nullptr, this, nullptr);
    if (mRunning)
        mService->cancel();
}

void WipeFreeSpaceDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setSpacing(12);
    root->setContentsMargins(20, 20, 20, 20);

    mStack = new QStackedWidget(this);
    root->addWidget(mStack);

    // ---- Step 0: pick + preview ----
    auto *pickPage = new QWidget(mStack);
    auto *pickLayout = new QVBoxLayout(pickPage);
    pickLayout->setContentsMargins(0, 0, 0, 0);
    pickLayout->setSpacing(10);

    auto *intro = new QLabel(
        tr("Select a volume to wipe. Nexis fills its free space and deletes the "
           "fill file; on solid-state volumes with TRIM support it reclaims free "
           "space by signalling blocks as reusable instead."), pickPage);
    intro->setWordWrap(true);
    pickLayout->addWidget(intro);

    mVolumeList = new QListWidget(pickPage);
    mVolumeList->setObjectName(QStringLiteral("wipeVolumeList"));
    connect(mVolumeList, &QListWidget::currentRowChanged,
            this, &WipeFreeSpaceDialog::onVolumeSelectionChanged);
    pickLayout->addWidget(mVolumeList);

    mLblPreview = new QLabel(pickPage);
    mLblPreview->setObjectName(QStringLiteral("lblWipePreview"));
    mLblPreview->setWordWrap(true);
    pickLayout->addWidget(mLblPreview);

    auto *disclosure = new QLabel(
        tr("Reclaiming via TRIM signals previously-used blocks as reusable to the "
           "drive — it is not equivalent to a cryptographic-erase guarantee."), pickPage);
    disclosure->setWordWrap(true);
    pickLayout->addWidget(disclosure);

    auto *pickBtnRow = new QHBoxLayout();
    pickBtnRow->addStretch();
    auto *cancelBtn = new QPushButton(tr("Cancel"), pickPage);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    pickBtnRow->addWidget(cancelBtn);

    mBtnWipe = new QPushButton(tr("Wipe Free Space…"), pickPage);
    mBtnWipe->setObjectName(QStringLiteral("btnWipeFreeSpaceConfirm"));
    mBtnWipe->setAccessibleName(QStringLiteral("danger"));
    mBtnWipe->setEnabled(false);
    connect(mBtnWipe, &QPushButton::clicked, this, &WipeFreeSpaceDialog::onWipeClicked);
    pickBtnRow->addWidget(mBtnWipe);
    pickLayout->addLayout(pickBtnRow);

    mStack->addWidget(pickPage);

    // ---- Step 1: progress ----
    auto *progressPage = new QWidget(mStack);
    auto *progressLayout = new QVBoxLayout(progressPage);
    progressLayout->setContentsMargins(0, 0, 0, 0);
    progressLayout->setSpacing(10);

    mLblProgress = new QLabel(progressPage);
    mLblProgress->setObjectName(QStringLiteral("lblWipeProgress"));
    mLblProgress->setWordWrap(true);
    progressLayout->addWidget(mLblProgress);

    mProgressBar = new QProgressBar(progressPage);
    mProgressBar->setObjectName(QStringLiteral("wipeProgressBar"));
    mProgressBar->setRange(0, 0);
    progressLayout->addWidget(mProgressBar);
    progressLayout->addStretch();

    auto *progressBtnRow = new QHBoxLayout();
    progressBtnRow->addStretch();

    mBtnStop = new QPushButton(tr("Stop"), progressPage);
    connect(mBtnStop, &QPushButton::clicked, this, &WipeFreeSpaceDialog::onStopClicked);
    progressBtnRow->addWidget(mBtnStop);

    mBtnClose = new QPushButton(tr("Close"), progressPage);
    mBtnClose->hide();
    connect(mBtnClose, &QPushButton::clicked, this, &QDialog::accept);
    progressBtnRow->addWidget(mBtnClose);

    progressLayout->addLayout(progressBtnRow);

    mStack->addWidget(progressPage);
    mStack->setCurrentIndex(0);
}

void WipeFreeSpaceDialog::populateVolumes()
{
    mVolumeList->clear();
    mTargets = mService->listWipeableVolumes();

    for (int i = 0; i < mTargets.size(); ++i) {
        const WipeTarget &t = mTargets.at(i);
        const QString badge = t.trimEligible
            ? tr("SSD · TRIM reclaim")
            : tr("fill-and-delete");
        auto *item = new QListWidgetItem(
            tr("%1 — %2 free (%3)").arg(t.displayName, FormatUtil::formatBytes(t.freeBytes), badge));
        mVolumeList->addItem(item);
    }

    if (mVolumeList->count() > 0)
        mVolumeList->setCurrentRow(0);
    else
        mLblPreview->setText(tr("No mounted, writable volumes were found."));
}

void WipeFreeSpaceDialog::onVolumeSelectionChanged()
{
    updatePreview();
}

void WipeFreeSpaceDialog::updatePreview()
{
    const int row = mVolumeList->currentRow();
    if (row < 0 || row >= mTargets.size()) {
        mBtnWipe->setEnabled(false);
        mLblPreview->clear();
        return;
    }

    const WipeTarget &t = mTargets.at(row);
    const quint64 headroom = WipeFreeSpaceService::headroomForVolume(t.totalBytes);

    QString detail = tr("Free space: %1 (%2 kept as a safety margin).")
                          .arg(FormatUtil::formatBytes(t.freeBytes), FormatUtil::formatBytes(headroom));

    if (t.trimEligible) {
        detail += QLatin1Char(' ') + tr("Solid-state with TRIM support — free space will be "
                                        "reclaimed via TRIM, not by writing filler data.");
    } else {
        const quint64 writable = t.freeBytes > headroom ? (t.freeBytes - headroom) : 0;
        const double estSeconds = static_cast<double>(writable) / kAssumedWriteBytesPerSecond;
        detail += QLatin1Char(' ') + tr("Estimated: writing and deleting ~%1 (%2).")
                                          .arg(FormatUtil::formatBytes(writable), formatDuration(estSeconds));
        if (!t.trimUnavailableReason.isEmpty())
            detail += QLatin1Char(' ') + t.trimUnavailableReason;
    }

    mLblPreview->setText(detail);
    mBtnWipe->setEnabled(true);
}

void WipeFreeSpaceDialog::onWipeClicked()
{
    const int row = mVolumeList->currentRow();
    if (row < 0 || row >= mTargets.size())
        return;

    const WipeTarget &t = mTargets.at(row);

    const QMessageBox::StandardButton reply = QMessageBox::warning(
        this,
        tr("Wipe Free Space"),
        tr("Wipe free space on %1 (%2 free)?").arg(t.displayName, FormatUtil::formatBytes(t.freeBytes)),
        QMessageBox::Ok | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if (reply != QMessageBox::Ok)
        return;

    mRunning = true;
    mBtnStop->setEnabled(true);
    mBtnStop->show();
    mBtnClose->hide();
    mProgressBar->setRange(0, 0);
    mLblProgress->setText(t.trimEligible
        ? tr("Reclaiming free space via TRIM on %1…").arg(t.displayName)
        : tr("Wiping free space on %1…").arg(t.displayName));
    mStack->setCurrentIndex(1);

    mService->startWipe(t.rootPath);
}

void WipeFreeSpaceDialog::onStopClicked()
{
    mBtnStop->setEnabled(false);
    mLblProgress->setText(tr("Stopping — removing the temporary fill file…"));
    mService->cancel();
}

void WipeFreeSpaceDialog::onProgress(qint64 bytesWritten, qint64 estimatedTotalBytes, const QString &message)
{
    mLblProgress->setText(message);
    if (estimatedTotalBytes > 0) {
        mProgressBar->setRange(0, 100);
        mProgressBar->setValue(static_cast<int>(
            qBound<qint64>(qint64{0}, (bytesWritten * 100) / estimatedTotalBytes, qint64{100})));
    }
}

void WipeFreeSpaceDialog::onFinished(bool success, const QString &message)
{
    mRunning = false;
    mProgressBar->setRange(0, 1);
    mProgressBar->setValue(success ? 1 : 0);
    mLblProgress->setText(message);
    mBtnStop->hide();
    mBtnClose->show();
}

void WipeFreeSpaceDialog::onCancelled()
{
    mRunning = false;
    mProgressBar->setRange(0, 1);
    mProgressBar->setValue(0);
    mLblProgress->setText(tr("Cancelled — the temporary fill file was removed."));
    mBtnStop->hide();
    mBtnClose->show();
}

void WipeFreeSpaceDialog::closeEvent(QCloseEvent *event)
{
    if (mRunning) {
        onStopClicked();
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}
