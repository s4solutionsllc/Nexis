#include "disk_map_page.h"

#include "bubble_map_view.h"
#include "sunburst_view.h"
#include "treemap_view.h"

#include "Managers/app_manager.h"
#include "Managers/dir_size_scanner.h"
#include "signal_mapper.h"
#include "Services/file_search_service.h"
#include "dpi.h"
#include "utilities.h"

#include <QApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
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

DiskMapPage::DiskMapPage(QWidget *parent,
                         AppManager *appManager,
                         SignalMapper *signalMapper)
    : QWidget(parent),
      mAppManager(appManager ? appManager : AppManager::ins()),
      mSignalMapper(signalMapper ? signalMapper : SignalMapper::ins())
{
    mScanner = new DirSizeScanner(this);

    mFolderCombo = new QComboBox(this);
    mFolderCombo->setEditable(true);
    mFolderCombo->setMinimumWidth(Dpi::scale(280));

    mChooseButton  = new QPushButton(tr("Choose..."), this);
    mScanButton    = new QPushButton(tr("Scan"), this);
    mScanButton->setProperty("variant", "primary");
    mCancelButton  = new QPushButton(tr("Cancel"), this);
    mDrillUpButton = new QPushButton(tr("Up"), this);
    mCancelButton->setEnabled(false);
    mDrillUpButton->setEnabled(false);

    mVisPicker = new QComboBox(this);
    mVisPicker->addItem(tr("Treemap"));
    mVisPicker->addItem(tr("Bubble Map"));
    mVisPicker->addItem(tr("Sunburst"));

    mBreadcrumb  = new QLabel(this);
    mBreadcrumb->setTextInteractionFlags(Qt::TextSelectableByMouse);
    mStatusLabel = new QLabel(this);
    mProgress    = new QProgressBar(this);
    mProgress->setRange(0, 0);   // indeterminate during scan
    mProgress->setVisible(false);

    mStack = new QStackedWidget(this);
    mViews = { new TreemapView(this), new BubbleMapView(this), new SunburstView(this) };
    for (DiskMapView *v : mViews) {
        mStack->addWidget(v);
        connect(v, &DiskMapView::tileHovered, this, &DiskMapPage::onTileHovered);
        connect(v, &DiskMapView::drillRequested, this, &DiskMapPage::onDrillRequested);
        connect(v, &DiskMapView::revealRequested, this, &DiskMapPage::onRevealRequested);
        connect(v, &DiskMapView::trashRequested, this, &DiskMapPage::onTrashRequested);
    }
    mView = mViews.first();
    mStack->setCurrentWidget(mView);

    auto *topBar = new QHBoxLayout;
    topBar->addWidget(new QLabel(tr("Folder:"), this));
    topBar->addWidget(mFolderCombo, 1);
    topBar->addWidget(mChooseButton);
    topBar->addWidget(mScanButton);
    topBar->addWidget(mCancelButton);
    topBar->addWidget(mDrillUpButton);
    topBar->addWidget(new QLabel(tr("View:"), this));
    topBar->addWidget(mVisPicker);

    auto *crumbBar = new QHBoxLayout;
    crumbBar->addWidget(mBreadcrumb, 1);
    crumbBar->addWidget(mProgress);

    mTitleLabel = new QLabel(tr("Disk Map"), this);
    mTitleLabel->setObjectName("sectionHeaderTitle");

    // DS §2 elevated container (NEX F1): fill/border/radius/shadow come from
    // the shared [cardRole="elevated"] QSS recipe — see
    // DiskToolsPage::makeElevatedContainer() for the same recipe.
    mCard = new QFrame(this);
    mCard->setAttribute(Qt::WA_StyledBackground, true);
    mCard->setProperty("cardRole", "elevated");
    Utilities::addDropShadow(mCard, 90, 26);
    auto *cardLayout = new QVBoxLayout(mCard);
    cardLayout->setContentsMargins(Dpi::scale(10), Dpi::scale(10), Dpi::scale(10), Dpi::scale(10));
    cardLayout->addWidget(mStack);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(Dpi::scale(16), Dpi::scale(16), Dpi::scale(16), Dpi::scale(12));
    outer->setSpacing(Dpi::scale(8));
    outer->addWidget(mTitleLabel);
    outer->addLayout(topBar);
    outer->addLayout(crumbBar);
    outer->addWidget(mCard, 1);
    outer->addWidget(mStatusLabel);

    connect(mChooseButton,  &QPushButton::clicked, this, &DiskMapPage::onChooseFolder);
    connect(mScanButton,    &QPushButton::clicked, this, &DiskMapPage::onScanClicked);
    connect(mCancelButton,  &QPushButton::clicked, this, &DiskMapPage::onCancelClicked);
    connect(mDrillUpButton, &QPushButton::clicked, this, &DiskMapPage::onDrillUpClicked);
    connect(mVisPicker, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DiskMapPage::onVisualizationChanged);

    connect(mScanner, &DirSizeScanner::finished,
            this, &DiskMapPage::onScanFinished);
    connect(mScanner, &DirSizeScanner::cancelled,
            this, &DiskMapPage::onScanCancelled);
    connect(mScanner, &DirSizeScanner::progress,
            this, &DiskMapPage::onScanProgress);

    connect(FileSearchService::ins(), &FileSearchService::fileOperationFinished,
            this, &DiskMapPage::onFileOperationFinished);

    if (mSignalMapper) {
        connect(mSignalMapper, &SignalMapper::sigChangedAppTheme,
                this, &DiskMapPage::applyThemeColors);
    }
    applyThemeColors();

    // Sensible default: user's home volume.
    mFolderCombo->addItem(QDir::homePath());
}

DiskMapPage::~DiskMapPage()
{
    if (mScanner && mScanner->isRunning())
        mScanner->cancel();
}

void DiskMapPage::prefillVolumes(const QStringList &volumeRoots)
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

void DiskMapPage::onChooseFolder()
{
    const QString picked = QFileDialog::getExistingDirectory(
        this, tr("Choose folder to scan"),
        mFolderCombo->currentText().isEmpty() ? QDir::homePath()
                                              : mFolderCombo->currentText());
    if (picked.isEmpty())
        return;
    mFolderCombo->setCurrentText(picked);
}

void DiskMapPage::onScanClicked()
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

void DiskMapPage::onCancelClicked()
{
    if (mScanner)
        mScanner->cancel();
}

void DiskMapPage::onDrillUpClicked()
{
    // Drill every view up in lockstep, not just the visible one, so
    // switching modes afterward doesn't need to re-derive anything.
    bool moved = false;
    for (DiskMapView *v : mViews) {
        if (v->drillUp())
            moved = true;
    }
    if (moved)
        updateBreadcrumb();
}

void DiskMapPage::onVisualizationChanged(int index)
{
    if (index < 0 || index >= mViews.size())
        return;
    mView = mViews[index];
    mStack->setCurrentWidget(mView);
    updateBreadcrumb();
}

void DiskMapPage::startScan(const QString &path)
{
    mLastScannedPath = path;
    setBusy(true);
    mStatusLabel->setText(tr("Scanning %1...").arg(path));
    mBreadcrumb->setText(path);
    mScanner->start(path);
}

void DiskMapPage::onScanFinished(DirSizeNodePtr root)
{
    setBusy(false);
    if (!root) {
        mStatusLabel->setText(tr("Scan finished but no data was returned."));
        return;
    }
    // Every view gets the same scan result — this is the only place any of
    // them touch DirSizeScanner's output, so no mode ever re-scans.
    for (DiskMapView *v : mViews)
        v->setRoot(root);
    mStatusLabel->setText(tr("Scanned %1 — %2 across %3 files")
                              .arg(root->path)
                              .arg(formatBytes(root->size))
                              .arg(root->fileCount));
    updateBreadcrumb();
}

void DiskMapPage::onScanCancelled()
{
    setBusy(false);
    mStatusLabel->setText(tr("Scan cancelled."));
}

void DiskMapPage::onScanProgress(qint64 bytes, int files)
{
    mStatusLabel->setText(tr("Scanning... %1 files / %2")
                              .arg(files)
                              .arg(formatBytes(bytes)));
}

void DiskMapPage::onTileHovered(DirSizeNode *node)
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

void DiskMapPage::onDrillRequested(DirSizeNode *node)
{
    if (!node)
        return;
    // Only the visible view can emit this (hidden ones get no mouse
    // events), but drill every view in lockstep so they all stay on the
    // same focus node.
    for (DiskMapView *v : mViews)
        v->drillInto(node);
    updateBreadcrumb();
}

void DiskMapPage::onRevealRequested(DirSizeNode *node)
{
    if (!node)
        return;
    revealInFileManager(node->path);
}

void DiskMapPage::onTrashRequested(DirSizeNode *node)
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

void DiskMapPage::onFileOperationFinished(FileSearchService::FileOperation op,
                                          QString filePath, bool hadError,
                                          QString errorMessage)
{
    if (op != FileSearchService::FileOperation::MoveToTrash)
        return;

    if (hadError) {
        mStatusLabel->setText(errorMessage.isEmpty()
            ? tr("Failed to move %1 to trash.").arg(filePath)
            : tr("Failed to move %1 to trash: %2").arg(filePath, errorMessage));
        return;
    }

    mStatusLabel->setText(tr("Moved %1 to trash.").arg(filePath));

    // Re-scan so the map reflects the removed entry. Skip if a scan is
    // already in flight (e.g. the user started a fresh scan before this
    // trash op settled) rather than racing DirSizeScanner::start().
    if (!mLastScannedPath.isEmpty() && mScanner && !mScanner->isRunning())
        startScan(mLastScannedPath);
}

void DiskMapPage::setBusy(bool busy)
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

void DiskMapPage::updateBreadcrumb()
{
    if (!mView || !mView->focus()) {
        mBreadcrumb->clear();
        mDrillUpButton->setEnabled(false);
        return;
    }
    mBreadcrumb->setText(mView->focus()->path);
    mDrillUpButton->setEnabled(mView->canDrillUp());
}

void DiskMapPage::applyThemeColors()
{
    if (!mAppManager || mViews.isEmpty())
        return;
    QSettings *sv = mAppManager->getStyleValues();
    if (!sv)
        return;
    const QColor text   = QColor(sv->value("@color12").toString());
    const QColor border = QColor(sv->value("@chartBorderColor",
                                            sv->value("@color03")).toString());
    const QColor bg     = QColor(sv->value("@chartBackgroundColor",
                                            sv->value("@color01")).toString());
    for (DiskMapView *v : mViews)
        v->applyTheme(text, border, bg);
    if (text.isValid())
        mStatusLabel->setStyleSheet(QString("color: %1;").arg(text.name()));
    if (text.isValid())
        mBreadcrumb->setStyleSheet(QString("color: %1; font-weight: bold;")
                                       .arg(text.name()));
}
