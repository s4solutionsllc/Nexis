#include "disk_treemap_dialog.h"

#include "treemap_view.h"

#include "Managers/app_manager.h"
#include "Managers/dir_size_scanner.h"
#include "signal_mapper.h"
#include "Services/file_search_service.h"
#include "dpi.h"

#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QString formatBytes(qint64 b)
{
    if (b < 1024)
        return QString::number(b) + " B";
    static const char *suffixes[] = {"KiB", "MiB", "GiB", "TiB", "PiB"};
    double v = static_cast<double>(b);
    int i = -1;
    do { v /= 1024.0; ++i; } while (v >= 1024.0 && i < 4);
    return QString::number(v, 'f', v < 10 ? 2 : 1) + " " + suffixes[i];
}

// Cross-platform "reveal in file manager". On Linux we lean on xdg-open of
// the parent directory which behaves like Files' "Show in folder" in
// practice; on macOS `open -R` selects the entry inside Finder.
void revealInFileManager(const QString &path)
{
    if (path.isEmpty())
        return;
#ifdef Q_OS_MACOS
    QProcess::startDetached("open", {"-R", path});
#else
    const QFileInfo fi(path);
    const QString dir = fi.isDir() ? fi.absoluteFilePath()
                                   : fi.absolutePath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
#endif
}

} // namespace

DiskTreemapDialog::DiskTreemapDialog(QWidget *parent,
                                     AppManager *appManager,
                                     SignalMapper *signalMapper)
    : QDialog(parent),
      mAppManager(appManager ? appManager : AppManager::ins()),
      mSignalMapper(signalMapper ? signalMapper : SignalMapper::ins())
{
    setWindowTitle(tr("Disk Space Visualizer"));
    setMinimumSize(Dpi::scale(720), Dpi::scale(520));

    mScanner = new DirSizeScanner(this);

    mFolderCombo = new QComboBox(this);
    mFolderCombo->setEditable(true);
    mFolderCombo->setMinimumWidth(Dpi::scale(280));

    mChooseButton  = new QPushButton(tr("Choose..."), this);
    mScanButton    = new QPushButton(tr("Scan"), this);
    mCancelButton  = new QPushButton(tr("Cancel"), this);
    mDrillUpButton = new QPushButton(tr("Up"), this);
    mCancelButton->setEnabled(false);
    mDrillUpButton->setEnabled(false);

    mBreadcrumb  = new QLabel(this);
    mBreadcrumb->setTextInteractionFlags(Qt::TextSelectableByMouse);
    mStatusLabel = new QLabel(this);
    mProgress    = new QProgressBar(this);
    mProgress->setRange(0, 0);   // indeterminate during scan
    mProgress->setVisible(false);

    mView = new TreemapView(this);
    connect(mView, &TreemapView::tileHovered, this, &DiskTreemapDialog::onTileHovered);
    connect(mView, &TreemapView::drillRequested, this, &DiskTreemapDialog::onDrillRequested);
    connect(mView, &TreemapView::revealRequested, this, &DiskTreemapDialog::onRevealRequested);
    connect(mView, &TreemapView::trashRequested, this, &DiskTreemapDialog::onTrashRequested);

    auto *topBar = new QHBoxLayout;
    topBar->addWidget(new QLabel(tr("Folder:"), this));
    topBar->addWidget(mFolderCombo, 1);
    topBar->addWidget(mChooseButton);
    topBar->addWidget(mScanButton);
    topBar->addWidget(mCancelButton);
    topBar->addWidget(mDrillUpButton);

    auto *crumbBar = new QHBoxLayout;
    crumbBar->addWidget(mBreadcrumb, 1);
    crumbBar->addWidget(mProgress);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(Dpi::scale(8), Dpi::scale(8),
                              Dpi::scale(8), Dpi::scale(8));
    outer->addLayout(topBar);
    outer->addLayout(crumbBar);
    outer->addWidget(mView, 1);
    outer->addWidget(mStatusLabel);

    connect(mChooseButton,  &QPushButton::clicked, this, &DiskTreemapDialog::onChooseFolder);
    connect(mScanButton,    &QPushButton::clicked, this, &DiskTreemapDialog::onScanClicked);
    connect(mCancelButton,  &QPushButton::clicked, this, &DiskTreemapDialog::onCancelClicked);
    connect(mDrillUpButton, &QPushButton::clicked, this, &DiskTreemapDialog::onDrillUpClicked);

    connect(mScanner, &DirSizeScanner::finished,
            this, &DiskTreemapDialog::onScanFinished);
    connect(mScanner, &DirSizeScanner::cancelled,
            this, &DiskTreemapDialog::onScanCancelled);
    connect(mScanner, &DirSizeScanner::progress,
            this, &DiskTreemapDialog::onScanProgress);

    if (mSignalMapper) {
        connect(mSignalMapper, &SignalMapper::sigChangedAppTheme,
                this, &DiskTreemapDialog::applyThemeColors);
    }
    applyThemeColors();

    // Sensible default: user's home volume.
    mFolderCombo->addItem(QDir::homePath());
}

void DiskTreemapDialog::prefillVolumes(const QStringList &volumeRoots)
{
    const QString current = mFolderCombo->currentText();
    mFolderCombo->clear();
    QStringList seen;
    for (const QString &v : volumeRoots) {
        if (v.isEmpty() || seen.contains(v))
            continue;
        seen.append(v);
        mFolderCombo->addItem(v);
    }
    if (!seen.contains(QDir::homePath())) {
        mFolderCombo->addItem(QDir::homePath());
    }
    if (!current.isEmpty())
        mFolderCombo->setCurrentText(current);
}

void DiskTreemapDialog::onChooseFolder()
{
    const QString picked = QFileDialog::getExistingDirectory(
        this, tr("Choose folder to scan"),
        mFolderCombo->currentText().isEmpty() ? QDir::homePath()
                                              : mFolderCombo->currentText());
    if (picked.isEmpty())
        return;
    mFolderCombo->setCurrentText(picked);
}

void DiskTreemapDialog::onScanClicked()
{
    const QString path = mFolderCombo->currentText().trimmed();
    if (path.isEmpty())
        return;
    QFileInfo fi(path);
    if (!fi.exists() || !fi.isDir()) {
        QMessageBox::warning(this, tr("Disk Space Visualizer"),
                             tr("That folder doesn't exist."));
        return;
    }
    startScan(path);
}

void DiskTreemapDialog::onCancelClicked()
{
    if (mScanner)
        mScanner->cancel();
}

void DiskTreemapDialog::onDrillUpClicked()
{
    if (mView && mView->drillUp())
        updateBreadcrumb();
}

void DiskTreemapDialog::startScan(const QString &path)
{
    setBusy(true);
    mStatusLabel->setText(tr("Scanning %1...").arg(path));
    mBreadcrumb->setText(path);
    mScanner->start(path);
}

void DiskTreemapDialog::onScanFinished(DirSizeNodePtr root)
{
    setBusy(false);
    if (!root) {
        mStatusLabel->setText(tr("Scan finished but no data was returned."));
        return;
    }
    mView->setRoot(root);
    mStatusLabel->setText(tr("Scanned %1 — %2 across %3 files")
                              .arg(root->path)
                              .arg(formatBytes(root->size))
                              .arg(root->fileCount));
    updateBreadcrumb();
}

void DiskTreemapDialog::onScanCancelled()
{
    setBusy(false);
    mStatusLabel->setText(tr("Scan cancelled."));
}

void DiskTreemapDialog::onScanProgress(qint64 bytes, int files)
{
    mStatusLabel->setText(tr("Scanning... %1 files / %2")
                              .arg(files)
                              .arg(formatBytes(bytes)));
}

void DiskTreemapDialog::onTileHovered(DirSizeNode *node)
{
    if (!node) {
        if (mView && mView->focus()) {
            updateBreadcrumb();
        }
        return;
    }
    mStatusLabel->setText(QString("%1 — %2")
                              .arg(node->path)
                              .arg(formatBytes(node->size)));
}

void DiskTreemapDialog::onDrillRequested(DirSizeNode *node)
{
    if (!mView || !node)
        return;
    mView->drillInto(node);
    updateBreadcrumb();
}

void DiskTreemapDialog::onRevealRequested(DirSizeNode *node)
{
    if (!node)
        return;
    revealInFileManager(node->path);
}

void DiskTreemapDialog::onTrashRequested(DirSizeNode *node)
{
    if (!node)
        return;
    if (QMessageBox::question(
            this, tr("Move to trash?"),
            tr("Move \"%1\" (%2) to the trash?")
                .arg(node->path).arg(formatBytes(node->size)))
        != QMessageBox::Yes) {
        return;
    }
    // Reuse the cleaner trash path via FileSearchService.
    FileSearchService *svc = FileSearchService::ins();
    const QString currentUser = QFileInfo(QDir::homePath()).owner();
    svc->moveToTrash(node->path, QFileInfo(node->path).fileName(), currentUser);
    mStatusLabel->setText(tr("Moving %1 to trash...").arg(node->path));
}

void DiskTreemapDialog::setBusy(bool busy)
{
    mScanButton->setEnabled(!busy);
    mChooseButton->setEnabled(!busy);
    mFolderCombo->setEnabled(!busy);
    mCancelButton->setEnabled(busy);
    mProgress->setVisible(busy);
    // updateBreadcrumb() owns the drill-up enabled state; while busy we
    // force it off so the user can't drill mid-scan.
    if (busy)
        mDrillUpButton->setEnabled(false);
}

void DiskTreemapDialog::updateBreadcrumb()
{
    if (!mView || !mView->focus()) {
        mBreadcrumb->clear();
        mDrillUpButton->setEnabled(false);
        return;
    }
    mBreadcrumb->setText(mView->focus()->path);
    mDrillUpButton->setEnabled(mView->canDrillUp());
}

void DiskTreemapDialog::closeEvent(QCloseEvent *event)
{
    if (mScanner && mScanner->isRunning())
        mScanner->cancel();
    QDialog::closeEvent(event);
}

void DiskTreemapDialog::applyThemeColors()
{
    if (!mAppManager || !mView)
        return;
    QSettings *sv = mAppManager->getStyleValues();
    if (!sv)
        return;
    const QColor text   = QColor(sv->value("@color12").toString());
    const QColor border = QColor(sv->value("@chartBorderColor",
                                            sv->value("@color3")).toString());
    const QColor bg     = QColor(sv->value("@chartBackgroundColor",
                                            sv->value("@color1")).toString());
    mView->applyTheme(text, border, bg);
    if (text.isValid())
        mStatusLabel->setStyleSheet(QString("color: %1;").arg(text.name()));
    if (text.isValid())
        mBreadcrumb->setStyleSheet(QString("color: %1; font-weight: bold;")
                                       .arg(text.name()));
}
