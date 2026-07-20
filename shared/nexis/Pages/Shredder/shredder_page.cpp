#include "shredder_page.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QSizePolicy>
#include <QListWidget>
#include <QPushButton>
#include <QToolButton>
#include <QProgressBar>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>

#include "shredder_drop_zone.h"
#include "shred_confirm_dialog.h"
#include "utilities.h"
#include <Utils/format_util.h>

ShredderPage::ShredderPage(QWidget *parent, FileShredderService *shredderService)
    : QWidget(parent)
    , mShredderService(shredderService ? shredderService : FileShredderService::ins())
{
    buildUi();

    connect(mShredderService, &FileShredderService::previewReady, this, &ShredderPage::onPreviewReady);
    connect(mShredderService, &FileShredderService::shredProgress, this, &ShredderPage::onShredProgress);
    connect(mShredderService, &FileShredderService::itemFailed, this, &ShredderPage::onItemFailed);
    connect(mShredderService, &FileShredderService::shredFinished, this, &ShredderPage::onShredFinished);
}

ShredderPage::~ShredderPage()
{
}

QWidget *ShredderPage::makeElevatedContainer(QWidget *parent)
{
    auto *container = new QWidget(parent);
    container->setAttribute(Qt::WA_StyledBackground, true);
    container->setProperty("cardRole", "elevated");
    Utilities::addDropShadow(container, 90, 26);
    return container;
}

void ShredderPage::buildUi()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(60, 10, 60, 20);
    outer->setSpacing(8);

    // ---- Header (DS §3 shared recipe) ----
    auto *headerRow = new QWidget(this);
    headerRow->setObjectName("sectionHeaderRow");
    auto *headerLayout = new QHBoxLayout(headerRow);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);

    auto *accentBar = new QFrame(headerRow);
    accentBar->setObjectName("sectionHeaderAccent");
    accentBar->setProperty("accentToken", "accent");
    accentBar->setFrameShape(QFrame::NoFrame);
    accentBar->setFixedWidth(3);
    accentBar->setMinimumHeight(26);
    accentBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    headerLayout->addWidget(accentBar);

    auto *headerTextCol = new QVBoxLayout;
    headerTextCol->setContentsMargins(0, 0, 0, 0);
    headerTextCol->setSpacing(0);

    auto *lblTitle = new QLabel(tr("File Shredder"), headerRow);
    lblTitle->setObjectName("sectionHeaderTitle");
    headerTextCol->addWidget(lblTitle);

    auto *lblSource = new QLabel(tr("Permanently overwrite and delete files and folders"), headerRow);
    lblSource->setObjectName("sectionHeaderSource");
    headerTextCol->addWidget(lblSource);

    headerLayout->addLayout(headerTextCol, 1);
    outer->addWidget(headerRow);
    outer->addSpacing(8);

    // ---- Drop zone card (DS §2 elevated card) ----
    auto *dropContainer = makeElevatedContainer(this);
    auto *dropContainerLayout = new QVBoxLayout(dropContainer);
    dropContainerLayout->setContentsMargins(0, 0, 0, 0);
    dropContainerLayout->setSpacing(0);

    mDropZone = new ShredderDropZone(dropContainer);
    mDropZone->setMinimumHeight(140);
    auto *dropZoneLayout = new QVBoxLayout(mDropZone);
    dropZoneLayout->setSpacing(10);
    dropZoneLayout->addStretch();

    auto *dropIcon = new QLabel(QString::fromUtf8("\xF0\x9F\x97\x91"), mDropZone); // wastebasket glyph
    dropIcon->setObjectName("emptyStateIcon");
    dropIcon->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    dropZoneLayout->addWidget(dropIcon);

    auto *dropHeading = new QLabel(tr("Drag files or folders here to shred"), mDropZone);
    dropHeading->setObjectName("lblShredderDropHeading");
    dropHeading->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    dropZoneLayout->addWidget(dropHeading);

    auto *dropButtonRow = new QHBoxLayout;
    dropButtonRow->addStretch();

    mBtnAddFiles = new QPushButton(tr("Choose Files…"), mDropZone);
    mBtnAddFiles->setObjectName("btnShredderAddFiles");
    mBtnAddFiles->setCursor(Qt::PointingHandCursor);
    dropButtonRow->addWidget(mBtnAddFiles);

    mBtnAddFolder = new QPushButton(tr("Choose Folder…"), mDropZone);
    mBtnAddFolder->setObjectName("btnShredderAddFolder");
    mBtnAddFolder->setCursor(Qt::PointingHandCursor);
    dropButtonRow->addWidget(mBtnAddFolder);

    dropButtonRow->addStretch();
    dropZoneLayout->addLayout(dropButtonRow);
    dropZoneLayout->addStretch();

    dropContainerLayout->addWidget(mDropZone);
    outer->addWidget(dropContainer);

    // Disclosure — SSD wear-leveling / copy-on-write caveat lives here (an
    // always-visible inline note), not in the confirm dialog, per the
    // Design Anchor's one-sentence-dialog-body rule.
    mLblDisclosure = new QLabel(
        tr("Overwrites file contents once, then deletes. On SSDs (wear leveling) and "
           "copy-on-write filesystems (APFS, Btrfs, ZFS), the overwritten blocks may not "
           "be the ones holding the original data — this does not guarantee unrecoverable erasure."),
        this);
    mLblDisclosure->setObjectName("lblShredderDisclosure");
    mLblDisclosure->setProperty("accessibleName", "dimmed-small");
    mLblDisclosure->setWordWrap(true);
    outer->addWidget(mLblDisclosure);
    outer->addSpacing(4);

    // ---- Staged items (DS §2/§7: one elevated container, flat rows) ----
    auto *listContainer = makeElevatedContainer(this);
    auto *listContainerLayout = new QVBoxLayout(listContainer);
    listContainerLayout->setContentsMargins(0, 0, 0, 0);
    listContainerLayout->setSpacing(0);

    mListStaged = new QListWidget(listContainer);
    mListStaged->setObjectName("listShredderStaged");
    mListStaged->setFrameShape(QFrame::NoFrame);
    mListStaged->setSelectionMode(QAbstractItemView::NoSelection);
    mListStaged->setFocusPolicy(Qt::NoFocus);
    listContainerLayout->addWidget(mListStaged);

    outer->addWidget(listContainer, 1);

    // ---- Progress (DS §6: thin bar, real partial progress as it runs) ----
    mProgressBar = new QProgressBar(this);
    mProgressBar->setObjectName("progressBarShredder");
    mProgressBar->setTextVisible(false);
    mProgressBar->setFixedHeight(6);
    mProgressBar->hide();
    outer->addWidget(mProgressBar);

    mLblProgressStatus = new QLabel(this);
    mLblProgressStatus->setObjectName("lblShredderStatus");
    mLblProgressStatus->setWordWrap(true);
    outer->addWidget(mLblProgressStatus);

    // ---- Footer status bar: total selected size + item count, persistent,
    // updates live (Design Anchor component convention) ----
    auto *footerRow = new QHBoxLayout;
    mLblFooterTotal = new QLabel(this);
    mLblFooterTotal->setObjectName("lblShredderFooterTotal");
    footerRow->addWidget(mLblFooterTotal);
    footerRow->addStretch();

    mBtnShredSelected = new QPushButton(tr("Shred Selected"), this);
    mBtnShredSelected->setObjectName("btnShredSelected");
    mBtnShredSelected->setProperty("accessibleName", "danger");
    mBtnShredSelected->setCursor(Qt::PointingHandCursor);
    // Design Anchor / GH-173 / GH-226 disabled-state regression class: never
    // a silent no-op — stays disabled until at least one item is staged and
    // its preview has resolved.
    mBtnShredSelected->setEnabled(false);
    footerRow->addWidget(mBtnShredSelected);

    outer->addLayout(footerRow);

    connect(mDropZone, &ShredderDropZone::pathsDropped, this, &ShredderPage::addPaths);
    connect(mBtnAddFiles, &QPushButton::clicked, this, &ShredderPage::onAddFilesClicked);
    connect(mBtnAddFolder, &QPushButton::clicked, this, &ShredderPage::onAddFolderClicked);
    connect(mBtnShredSelected, &QPushButton::clicked, this, &ShredderPage::onShredClicked);

    refreshStagedList();
}

void ShredderPage::onAddFilesClicked()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Select Files to Shred"), QDir::homePath());
    if (!files.isEmpty())
        addPaths(files);
}

void ShredderPage::onAddFolderClicked()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select Folder to Shred"), QDir::homePath());
    if (!dir.isEmpty())
        addPaths({dir});
}

void ShredderPage::addPaths(const QStringList &paths)
{
    bool changed = false;
    for (const QString &p : paths) {
        if (!mStagedPaths.contains(p)) {
            mStagedPaths << p;
            changed = true;
        }
    }
    if (!changed)
        return;

    refreshStagedList();
    requestPreview();
}

void ShredderPage::removePath(const QString &path)
{
    if (mStagedPaths.removeAll(path) == 0)
        return;

    if (mStagedPaths.isEmpty())
        mLastPlan = ShredPlan();

    refreshStagedList();
    if (!mStagedPaths.isEmpty())
        requestPreview();
}

void ShredderPage::requestPreview()
{
    if (mStagedPaths.isEmpty())
        return;

    // Staging is disabled in the UI while a shred is running (setBusy()),
    // but guard here too rather than race FileShredderService's mCancelled
    // reset against an in-flight shred.
    if (mPreviewInFlight || mShredderService->isBusy()) {
        mPreviewPending = true;
        return;
    }

    mPreviewInFlight = true;
    mShredderService->computePreview(mStagedPaths);
}

QString ShredderPage::metaTextFor(const QString &path) const
{
    for (const ShredItem &item : mLastPlan.items) {
        if (item.path != path)
            continue;
        return item.isDir
            ? tr("%n file(s) · %1", "", item.fileCount).arg(FormatUtil::formatBytes(item.bytes))
            : FormatUtil::formatBytes(item.bytes);
    }
    return tr("Calculating…");
}

void ShredderPage::refreshStagedList()
{
    mListStaged->clear();

    for (const QString &path : std::as_const(mStagedPaths)) {
        auto *item = new QListWidgetItem(mListStaged);
        auto *row = new QWidget(mListStaged);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(12, 8, 12, 8);
        rowLayout->setSpacing(8);

        auto *textCol = new QVBoxLayout;
        textCol->setSpacing(0);

        auto *lblName = new QLabel(QFileInfo(path).fileName(), row);
        lblName->setToolTip(path);
        textCol->addWidget(lblName);

        auto *lblMeta = new QLabel(metaTextFor(path), row);
        lblMeta->setProperty("accessibleName", "dimmed-small");
        textCol->addWidget(lblMeta);

        rowLayout->addLayout(textCol, 1);

        auto *btnRemove = new QToolButton(row);
        btnRemove->setObjectName("btnShredderRemoveItem");
        btnRemove->setText(QString::fromUtf8("\xC3\x97")); // ×
        btnRemove->setAutoRaise(true);
        btnRemove->setCursor(Qt::PointingHandCursor);
        btnRemove->setToolTip(tr("Remove from selection"));
        connect(btnRemove, &QToolButton::clicked, this, [this, path]() { removePath(path); });
        rowLayout->addWidget(btnRemove);

        item->setSizeHint(row->sizeHint());
        mListStaged->setItemWidget(item, row);
    }

    const bool hasItems = !mStagedPaths.isEmpty();
    // The plan is only trustworthy for the *current* selection once its
    // item count matches — a mismatch means an edit landed after the last
    // resolved (or is still in-flight for a) preview; refreshStagedList()
    // runs again once that settles.
    const bool planCurrent = hasItems && mLastPlan.items.size() == mStagedPaths.size();

    mBtnShredSelected->setEnabled(planCurrent && mLastPlan.totalFileCount > 0);

    if (!hasItems)
        mLblFooterTotal->clear();
    else if (planCurrent)
        mLblFooterTotal->setText(tr("%n item(s) selected — %1 total", "", mLastPlan.totalFileCount)
                                 .arg(FormatUtil::formatBytes(mLastPlan.totalBytes)));
    else
        mLblFooterTotal->setText(tr("Calculating…"));
}

void ShredderPage::onPreviewReady(const ShredPlan &plan)
{
    mPreviewInFlight = false;

    if (mPreviewPending) {
        // A newer edit landed while this preview was computing — it's
        // already stale. Discard it and recompute against the current
        // selection rather than clobbering the user's later edits.
        mPreviewPending = false;
        requestPreview();
        return;
    }

    mLastPlan = plan;

    // dedupeContainedPaths() inside the service may have dropped a path
    // nested inside another staged path (or an exact repeat) — keep the
    // visible list in sync with what will actually be shredded.
    QStringList survivors;
    for (const ShredItem &item : plan.items)
        survivors << item.path;
    mStagedPaths = survivors;

    refreshStagedList();
}

void ShredderPage::onShredClicked()
{
    if (mLastPlan.totalFileCount <= 0 || mStagedPaths.isEmpty())
        return;

    ShredConfirmDialog dlg(mLastPlan.totalFileCount, mLastPlan.totalBytes, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    mFailureLog.clear();
    setBusy(true);
    mProgressBar->setRange(0, qMax(1, mLastPlan.totalFileCount));
    mProgressBar->setValue(0);
    mLblProgressStatus->setText(tr("Shredding…"));

    mShredderService->shred(mStagedPaths);
}

void ShredderPage::onShredProgress(int current, int total, quint64 bytesDone,
                                   quint64 bytesTotal, const QString &currentPath)
{
    Q_UNUSED(bytesTotal);

    mProgressBar->setRange(0, qMax(1, total));
    mProgressBar->setValue(current);
    mLblProgressStatus->setText(
        tr("Shredding %1 (%2/%3) — %4 freed so far…")
            .arg(QFileInfo(currentPath).fileName())
            .arg(current)
            .arg(total)
            .arg(FormatUtil::formatBytes(bytesDone)));
}

void ShredderPage::onItemFailed(const QString &path, const QString &reason)
{
    mFailureLog << tr("%1 — %2").arg(path, reason);
}

void ShredderPage::onShredFinished(int itemsShredded, int itemsFailed, quint64 bytesFreed)
{
    setBusy(false);
    // Any preview request that got queued while shredding was running is
    // moot now — the staged list is about to be reset to empty below.
    mPreviewPending = false;

    mProgressBar->setValue(mProgressBar->maximum());

    QString status = tr("Shredded %n item(s) — %1 freed.", "", itemsShredded)
                     .arg(FormatUtil::formatBytes(bytesFreed));
    if (itemsFailed > 0) {
        status += " " + tr("%n item(s) could not be shredded.", "", itemsFailed);
        mLblProgressStatus->setToolTip(mFailureLog.join("\n"));
    } else {
        mLblProgressStatus->setToolTip(QString());
    }
    mLblProgressStatus->setText(status);
    mFailureLog.clear();

    mStagedPaths.clear();
    mLastPlan = ShredPlan();
    refreshStagedList();
}

void ShredderPage::setBusy(bool busy)
{
    mDropZone->setEnabled(!busy);
    mBtnAddFiles->setEnabled(!busy);
    mBtnAddFolder->setEnabled(!busy);
    mListStaged->setEnabled(!busy);
    mBtnShredSelected->setEnabled(!busy && mLastPlan.totalFileCount > 0);

    mProgressBar->setVisible(busy);
    if (!busy)
        return;
    mLblProgressStatus->show();
}
