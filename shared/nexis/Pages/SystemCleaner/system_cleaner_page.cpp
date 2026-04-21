#include "system_cleaner_page.h"
#include "ui_system_cleaner_page.h"
#include "byte_tree_widget.h"
#include "nexis_roles.h"
#include "dpi.h"
#include <Managers/schedule_manager.h>
#include <Managers/tool_manager.h>
#include "signal_mapper.h"
#include <Utils/format_util.h>
#include "exclusion_manager_dialog.h"
#include <QLabel>
#include <QFrame>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QGridLayout>
#include <QToolButton>
#include <QMenu>
#include <QHeaderView>

SystemCleanerPage::~SystemCleanerPage()
{
    mWorkerFuture.waitForFinished();
    delete ui;
}

SystemCleanerPage::SystemCleanerPage(QWidget *parent, AppManager *appManager,
                                     SignalMapper *signalMapper, CleanerService *cleanerService,
                                     ScheduleManager *scheduleManager) :
    QWidget(parent),
    ui(new Ui::SystemCleanerPage),
    mAppManager(appManager ? appManager : AppManager::ins()),
    mSignalMapper(signalMapper ? signalMapper : SignalMapper::ins()),
    mCleanerService(cleanerService ? cleanerService : CleanerService::ins()),
    mScheduleManager(scheduleManager ? scheduleManager : ScheduleManager::ins()),
    mDefaultIcon(QIcon(":/static/themes/common/img/package.png")),
    mLoadingMovie(nullptr),
    mLoadingMovie_2(nullptr)
{
    ui->setupUi(this);

    QString themeName = mAppManager->resolveThemeName();
    mLoadingMovie = new QMovie(
        QString(":/static/themes/%1/img/scanLoading.gif").arg(themeName), {}, this);
    // BUG-112: the light-theme scanLoading.gif is 512×512 (vs 100×100 on
    // the dark/default theme), so without setScaledSize the radar fills
    // the page on light theme. Lock render size to the label's 100×100
    // minimumSize from the .ui.
    mLoadingMovie->setScaledSize(Dpi::scale(100, 100));
    ui->lblLoadingScanner->setMovie(mLoadingMovie);

    mLoadingMovie_2 = new QMovie(
        QString(":/static/themes/%1/img/loading.gif").arg(themeName), {}, this);
    mLoadingMovie_2->setScaledSize(Dpi::scale(160, 20));
    ui->lblLoadingCleaner->setMovie(mLoadingMovie_2);

    init();

    ui->stackedWidget->setCurrentIndex(0);
}

void SystemCleanerPage::init()
{
    // Use bundled SVGs for consistent appearance across all platforms.
    // Render at 48×48 (matching disk-tool launcher icons) and enable
    // scaledContents so the label always shows the full image.
    auto setPixmap = [](QLabel *lbl, const QString &svgPath) {
        QPixmap pm = QIcon(svgPath).pixmap(Dpi::scale(48, 48));
        lbl->setFixedSize(Dpi::scale(48, 48));
        lbl->setScaledContents(true);
        lbl->setPixmap(pm);
    };
    setPixmap(ui->lblPackageCacheImg, ":/static/themes/common/img/c_package.svg");
    setPixmap(ui->lblCrashReportsImg, ":/static/themes/common/img/c_crash.svg");
    setPixmap(ui->lblLogImage,        ":/static/themes/common/img/c_logs.svg");
    setPixmap(ui->lblAppCacheImg,     ":/static/themes/common/img/c_cache.svg");
    setPixmap(ui->lblTrashImg,        ":/static/themes/common/img/c_trash.svg");
    setPixmap(ui->lblDevToolCacheImg, ":/static/themes/common/img/c_devtools.svg");
    setPixmap(ui->lblBrokenSymlinksImg, ":/static/themes/common/img/c_symlink.svg");
    setPixmap(ui->lblBrowserPrivacyImg, ":/static/themes/common/img/c_privacy.svg");

#ifndef Q_OS_MACOS
    // Snap/Flatpak category — added programmatically (Linux only)
    {
        QGridLayout *grid = qobject_cast<QGridLayout *>(ui->cleanerCategories->layout());
        if (grid) {
            mLblSnapFlatpakImg = new QLabel;
            setPixmap(mLblSnapFlatpakImg, ":/static/themes/common/img/c_snapflatpak.svg");
            grid->addWidget(mLblSnapFlatpakImg, 2, 10, Qt::AlignHCenter);

            mLblSnapFlatpakLabel = new QLabel(tr("Snap/Flatpak\nRevisions"));
            mLblSnapFlatpakLabel->setAlignment(Qt::AlignCenter);
            mLblSnapFlatpakLabel->setWordWrap(true);
            grid->addWidget(mLblSnapFlatpakLabel, 3, 10, Qt::AlignHCenter);

            mCheckSnapFlatpak = new QCheckBox;
            mCheckSnapFlatpak->setCursor(Qt::PointingHandCursor);
            mCheckSnapFlatpak->setFocusPolicy(Qt::NoFocus);
            mCheckSnapFlatpak->setAccessibleName("circle");
            grid->addWidget(mCheckSnapFlatpak, 4, 10, Qt::AlignHCenter | Qt::AlignTop);
        }
    }
#endif

    // Exclusion rules gear button — floating overlay in top-right corner
    // (BUG-52: QToolButton for macOS SVG compat)
    {
        mBtnExclusions = new QToolButton(this);
        mBtnExclusions->setAutoRaise(true);
        mBtnExclusions->setIcon(QIcon(
            QString(":/static/themes/%1/img/sidebar-icons/settings.svg")
                .arg(mAppManager->resolveThemeName())));
        mBtnExclusions->setIconSize(Dpi::scale(18, 18));
        mBtnExclusions->setFixedSize(Dpi::scale(32, 32));
        mBtnExclusions->setToolTip(tr("Manage exclusion rules"));
        mBtnExclusions->setCursor(Qt::PointingHandCursor);
        mBtnExclusions->setFocusPolicy(Qt::NoFocus);
        mBtnExclusions->raise();
        connect(mBtnExclusions, &QToolButton::clicked,
                this, &SystemCleanerPage::onManageExclusions);
    }

    // treview settings
    ui->treeWidgetScanResult->setColumnCount(2);
    ui->treeWidgetScanResult->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->treeWidgetScanResult->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    ui->treeWidgetScanResult->setColumnWidth(1, Dpi::scale(100));

    ui->treeWidgetScanResult->header()->setFixedHeight(Dpi::scale(30));
    ui->treeWidgetScanResult->setHeaderLabels({ tr("File Name"), tr("Size") });

    // loaders — update GIF source on theme change (reuse existing QMovie objects)
    connect(mSignalMapper, &SignalMapper::sigChangedAppTheme, this, [this] {
        QString themeName = mAppManager->resolveThemeName();

        mLoadingMovie->stop();
        mLoadingMovie->setFileName(
            QString(":/static/themes/%1/img/scanLoading.gif").arg(themeName));
        mLoadingMovie->setScaledSize(Dpi::scale(100, 100));   // BUG-112

        mLoadingMovie_2->stop();
        mLoadingMovie_2->setFileName(
            QString(":/static/themes/%1/img/loading.gif").arg(themeName));
        mLoadingMovie_2->setScaledSize(Dpi::scale(160, 20));

        if (mBtnExclusions) {
            mBtnExclusions->setIcon(QIcon(
                QString(":/static/themes/%1/img/sidebar-icons/settings.svg")
                    .arg(themeName)));
        }
    });

    // needed to suppress qt warnings (signal/slot <> threads)
    qRegisterMetaType<QList<QPersistentModelIndex>>();
    qRegisterMetaType<QAbstractItemModel::LayoutChangeHint>();
    qRegisterMetaType<Qt::SortOrder>();

    connect(this, &SystemCleanerPage::scanFinishedS, this, &SystemCleanerPage::onScanFinished);
    connect(this, &SystemCleanerPage::cleanFinishedS, this, &SystemCleanerPage::onCleanFinished);

    // Right-click context menu for scan results
    ui->treeWidgetScanResult->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->treeWidgetScanResult, &QTreeWidget::customContextMenuRequested,
            this, &SystemCleanerPage::onTreeContextMenu);

    initScheduleIndicator();
}

quint64 SystemCleanerPage::addTreeRoot(const CleanCategories &cat, const QString &title, const QFileInfoList &infos, bool noChild)
{
    QTreeWidgetItem *root = new QTreeWidgetItem(ui->treeWidgetScanResult);
    root->setData(2, 0, cat);
    root->setData(2, 1, title);
    if (! infos.isEmpty())
        root->setData(3, 0, infos.at(0).absoluteDir().path());
    root->setCheckState(0, Qt::Unchecked);

    // add children
    quint64 totalSize = 0;

    if(! noChild) {
        for (const QFileInfo &i : infos) {
            QString path = i.absoluteFilePath();
            quint64 size = FileUtil::getFileSize(path);

            addTreeChild(path, i.fileName(), size, root);

            totalSize += size;
        }

        root->setText(0, QString("%1 (%2)")
                      .arg(title)
                      .arg(infos.count()));

    } else {
        if (! infos.isEmpty())
            totalSize += FileUtil::getFileSize(infos.first().absoluteFilePath());

        root->setText(0, QString("%1")
                      .arg(title));
    }

    root->setText(1, QString("%1").arg(FormatUtil::formatBytes(totalSize)));

    return totalSize;
}

void SystemCleanerPage::addTreeChild(const QString &data, const QString &text, const quint64 &size, QTreeWidgetItem *parent)
{
    ByteTreeWidget *item = new ByteTreeWidget(parent);
    item->setValues(text, size, data);
    item->setIcon(0, mDefaultIcon);
}

void SystemCleanerPage::addTreeChild(const CleanCategories &cat, const QString &text, const quint64 &size)
{
    ByteTreeWidget *item = new ByteTreeWidget(ui->treeWidgetScanResult);
    item->setValues(text, size, cat);
}

void SystemCleanerPage::on_treeWidgetScanResult_itemClicked(QTreeWidgetItem *item, const int &column)
{
    if(column == 0) {
      // new check state
      Qt::CheckState cs = (item->checkState(column) == Qt::Checked ? Qt::Checked : Qt::Unchecked);

      // change check state if has children
      for (int i = 0; i < item->childCount(); ++i)
        item->child(i)->setCheckState(column, cs);
    }
}

void SystemCleanerPage::systemScan()
{
    // Worker thread: delegate I/O to CleanerService
    QList<CleanerService::CleanCategory> categories;
    if (mScanPackageCache)  categories << CleanerService::PACKAGE_CACHE;
    if (mScanCrashReports)  categories << CleanerService::CRASH_REPORTS;
    if (mScanAppLog)        categories << CleanerService::APPLICATION_LOGS;
    if (mScanAppCache)      categories << CleanerService::APPLICATION_CACHES;
    if (mScanDevToolCache)  categories << CleanerService::DEV_TOOL_CACHES;
    if (mScanBrokenSymlinks) categories << CleanerService::BROKEN_SYMLINKS;
    if (mScanBrowserPrivacy) categories << CleanerService::BROWSER_PRIVACY;
    if (mScanSnapFlatpak)   categories << CleanerService::SNAP_FLATPAK_REVISIONS;

    CleanerService::ScanResult result = mCleanerService->scan(categories);

    // Distribute results back to member variables for onScanFinished()
    mPackageCaches = result.categoryFiles.value(CleanerService::PACKAGE_CACHE);
    mCrashReports  = result.categoryFiles.value(CleanerService::CRASH_REPORTS);
    mAppLogs       = result.categoryFiles.value(CleanerService::APPLICATION_LOGS);
    mAppCaches     = result.categoryFiles.value(CleanerService::APPLICATION_CACHES);
    mDevToolCaches = result.categoryFiles.value(CleanerService::DEV_TOOL_CACHES);
    mBrokenSymlinks = result.categoryFiles.value(CleanerService::BROKEN_SYMLINKS);
    mBrowserPrivacy = result.categoryFiles.value(CleanerService::BROWSER_PRIVACY);
    mSnapFlatpakRevisions = result.categoryFiles.value(CleanerService::SNAP_FLATPAK_REVISIONS);

    emit scanFinishedS();
}

void SystemCleanerPage::onScanFinished()
{
    // Main thread: all UI updates
    ui->treeWidgetScanResult->setSortingEnabled(false);
    ui->treeWidgetScanResult->clear();

    quint64 totalSize = 0;

    if (mScanPackageCache) {
        totalSize += addTreeRoot(PACKAGE_CACHE, mLblPackageCacheText, mPackageCaches);
    }
    if (mScanCrashReports) {
        totalSize += addTreeRoot(CRASH_REPORTS, mLblCrashReportsText, mCrashReports);
    }
    if (mScanAppLog) {
        totalSize += addTreeRoot(APPLICATION_LOGS, mLblAppLogText, mAppLogs);
    }
    if (mScanAppCache) {
        totalSize += addTreeRoot(APPLICATION_CACHES, mLblAppCacheText, mAppCaches);
    }
    if (mScanDevToolCache) {
        totalSize += addTreeRoot(DEV_TOOL_CACHES, mLblDevToolCacheText, mDevToolCaches);

        // Post-process: rename ambiguous "Cache"/"GPUCache" entries to "appName/Cache"
        // so users can distinguish which Electron app each cache belongs to.
        for (int i = 0; i < ui->treeWidgetScanResult->topLevelItemCount(); ++i) {
            QTreeWidgetItem *root = ui->treeWidgetScanResult->topLevelItem(i);
            if (root->data(2, 0).toInt() == DEV_TOOL_CACHES) {
                for (int j = 0; j < root->childCount(); ++j) {
                    QTreeWidgetItem *child = root->child(j);
                    QString name = child->text(0);
                    if (name == "Cache" || name == "GPUCache") {
                        // Extract parent dir name from the absolute path
                        QString absPath = child->data(2, 0).toString();
                        QDir dir(absPath);
                        dir.cdUp();
                        child->setText(0, dir.dirName() + "/" + name);
                    }
                }
                break;
            }
        }
    }
    if (mScanBrokenSymlinks) {
        totalSize += addTreeRoot(BROKEN_SYMLINKS, mLblBrokenSymlinksText, mBrokenSymlinks);
    }
    if (mScanBrowserPrivacy) {
        totalSize += addTreeRoot(BROWSER_PRIVACY, mLblBrowserPrivacyText, mBrowserPrivacy);
    }
    if (mScanSnapFlatpak) {
        totalSize += addTreeRoot(SNAP_FLATPAK_REVISIONS, mLblSnapFlatpakText, mSnapFlatpakRevisions);
    }
    if (mScanTrash) {
#ifdef Q_OS_MACOS
        totalSize += addTreeRoot(TRASH, mLblTrashText,
                    { QFileInfo(QDir::homePath() + "/.Trash/") }, true);
#else
        totalSize += addTreeRoot(TRASH, mLblTrashText,
                    { QFileInfo(QDir::homePath() + "/.local/share/Trash/") }, true);
#endif
    }

    ui->lblTotalBytes->setText(tr("Total size: %1").arg(FormatUtil::formatBytes(totalSize)));

    ui->treeWidgetScanResult->setSortingEnabled(true);
    on_cbSortBy_currentIndexChanged(ui->cbSortBy->currentIndex());

    // scan results page
    mLoadingMovie->stop();
    ui->stackedWidget->setCurrentIndex(1);

    ui->checkPackageCache->setChecked(false);
    ui->checkCrashReports->setChecked(false);
    ui->checkAppLog->setChecked(false);
    ui->checkAppCache->setChecked(false);
    ui->checkTrash->setChecked(false);
    ui->checkDevToolCache->setChecked(false);
    ui->checkBrokenSymlinks->setChecked(false);
    ui->checkBrowserPrivacy->setChecked(false);
    if (mCheckSnapFlatpak) mCheckSnapFlatpak->setChecked(false);

    // Release scan result lists — data is now in the tree widget (BUG-10)
    mPackageCaches.clear();
    mCrashReports.clear();
    mAppLogs.clear();
    mAppCaches.clear();
    mDevToolCaches.clear();
    mBrokenSymlinks.clear();
    mBrowserPrivacy.clear();
    mSnapFlatpakRevisions.clear();

    mScanInProgress = false;
}

bool SystemCleanerPage::cleanValid()
{
    for (int i = 0; i < ui->treeWidgetScanResult->topLevelItemCount(); ++i) {

        QTreeWidgetItem *it = ui->treeWidgetScanResult->topLevelItem(i);

        if (it->checkState(0) == Qt::Checked)
            return true;

        for (int j = 0; j < it->childCount(); ++j)
            if (it->child(j)->checkState(0) == Qt::Checked)
                return true;
    }

    return false;
}

void SystemCleanerPage::systemClean()
{
    // Worker thread: delegate I/O to CleanerService
    mTotalCleanedSize = 0;

    // FR-112: take a snapshot before deleting, if the user opted in.
    // systemClean bypasses CleanerService::clean (historical divergence), so
    // the hook here is what covers interactive page-initiated cleans. The
    // category list is best-effort — the page doesn't track per-category
    // checkboxes at clean time, only the aggregated mFilesToDelete list.
    QList<CleanerService::CleanCategory> cats;
    if (mCleanTrash)       cats << CleanerService::TRASH;
    if (mCleanSnapFlatpak) cats << CleanerService::SNAP_FLATPAK_REVISIONS;
    if (!mFilesToDelete.isEmpty()) {
        // Stand-in: represent the file-cleaning bucket as "application
        // caches" for the snapshot annotation. The snapshot captures the
        // whole filesystem regardless.
        cats << CleanerService::APPLICATION_CACHES;
    }
    if (!cats.isEmpty())
        mCleanerService->maybeTakeSnapshot(cats);

    if (mCleanTrash) {
        mTotalCleanedSize += mCleanerService->cleanTrash();
    }

    if (mCleanSnapFlatpak) {
        ToolManager *tmr = ToolManager::ins();
        QList<StaleSnapRevision> snapRevs = tmr->getStaleSnapRevisions();
        for (const StaleSnapRevision &rev : snapRevs)
            mTotalCleanedSize += rev.size;
        tmr->removeStaleSnapRevisions(snapRevs);
        tmr->removeUnusedFlatpakRuntimes();
    }

    if (!mFilesToDelete.isEmpty()) {
        mTotalCleanedSize += mCleanerService->cleanFiles(mFilesToDelete);
    }

    emit cleanFinishedS();
}

void SystemCleanerPage::onCleanFinished()
{
    // Main thread: all UI updates
    QTreeWidget *tree = ui->treeWidgetScanResult;

    // Remove children in reverse order to preserve indices
    for (int k = mChildrenToRemove.size() - 1; k >= 0; --k) {
        int parentIdx = mChildrenToRemove.at(k).first;
        int childIdx = mChildrenToRemove.at(k).second;
        if (childIdx == -1) {
            // Remove entire top-level item (e.g. Snap/Flatpak all-or-nothing clean)
            delete tree->takeTopLevelItem(parentIdx);
        } else {
            QTreeWidgetItem *parent = tree->topLevelItem(parentIdx);
            if (parent) {
                delete parent->takeChild(childIdx);
            }
        }
    }

    // Update titles — sum remaining children's stored sizes instead of
    // re-traversing the filesystem (BUG-10)
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *it = tree->topLevelItem(i);
        quint64 remainingSize = 0;
        for (int j = 0; j < it->childCount(); ++j)
            remainingSize += it->child(j)->data(1, SortRole).toULongLong();
        it->setText(0, QString("%1 (%2)")
                    .arg(it->data(2, 1).toString())
                    .arg(it->childCount()));
        it->setText(1, FormatUtil::formatBytes(remainingSize));
    }

    ui->lblRemovedTotalSize->setText(tr("%1 size files cleaned.")
                                     .arg(FormatUtil::formatBytes(mTotalCleanedSize)));

    ui->btnClean->show();
    mLoadingMovie_2->stop();
    ui->lblLoadingCleaner->hide();
    ui->treeWidgetScanResult->setEnabled(true);

    mCleanInProgress = false;
}

void SystemCleanerPage::on_btnScan_clicked()
{
    if (mScanInProgress || mCleanInProgress)
        return;

    // Read checkbox states on main thread
    mScanPackageCache = ui->checkPackageCache->isChecked();
    mScanCrashReports = ui->checkCrashReports->isChecked();
    mScanAppLog       = ui->checkAppLog->isChecked();
    mScanAppCache     = ui->checkAppCache->isChecked();
    mScanTrash        = ui->checkTrash->isChecked();
    mScanDevToolCache = ui->checkDevToolCache->isChecked();
    mScanBrokenSymlinks = ui->checkBrokenSymlinks->isChecked();
    mScanBrowserPrivacy = ui->checkBrowserPrivacy->isChecked();
    mScanSnapFlatpak = mCheckSnapFlatpak ? mCheckSnapFlatpak->isChecked() : false;

    if (!(mScanPackageCache || mScanCrashReports || mScanAppLog || mScanAppCache || mScanTrash || mScanDevToolCache || mScanBrokenSymlinks || mScanBrowserPrivacy || mScanSnapFlatpak)) {
        return;
    }

    // Read label texts on main thread (for tree root titles)
    mLblPackageCacheText = ui->lblPackageCache->text();
    mLblCrashReportsText = ui->lblCrashReports->text();
    mLblAppLogText       = ui->lblAppLog->text();
    mLblAppCacheText     = ui->lblAppCache->text();
    mLblTrashText        = ui->lblTrash->text();
    mLblDevToolCacheText = ui->lblDevToolCache->text();
    mLblBrokenSymlinksText = ui->lblBrokenSymlinks->text();
    mLblBrowserPrivacyText = ui->lblBrowserPrivacy->text();
    mLblSnapFlatpakText = mLblSnapFlatpakLabel ? mLblSnapFlatpakLabel->text() : tr("Snap/Flatpak Revisions");

    // Pre-scan UI updates (main thread)
    ui->btnScan->hide();
    mLoadingMovie->start();
    ui->lblLoadingScanner->show();
    ui->checkPackageCache->setEnabled(false);
    ui->checkCrashReports->setEnabled(false);
    ui->checkAppLog->setEnabled(false);
    ui->checkAppCache->setEnabled(false);
    ui->checkTrash->setEnabled(false);
    ui->checkDevToolCache->setEnabled(false);
    ui->checkBrokenSymlinks->setEnabled(false);
    ui->checkBrowserPrivacy->setEnabled(false);
    if (mCheckSnapFlatpak) mCheckSnapFlatpak->setEnabled(false);
    ui->checkSelectAllSystemScan->setEnabled(false);

    // Clear cached results
    mPackageCaches.clear();
    mCrashReports.clear();
    mAppLogs.clear();
    mAppCaches.clear();
    mDevToolCaches.clear();
    mBrokenSymlinks.clear();
    mBrowserPrivacy.clear();
    mSnapFlatpakRevisions.clear();

    mScanInProgress = true;

    // Launch worker thread (I/O only)
    mWorkerFuture = QtConcurrent::run([this]() { systemScan(); });
}

void SystemCleanerPage::on_btnClean_clicked()
{
    if (mScanInProgress || mCleanInProgress)
        return;

    if (!cleanValid()) {
        return;
    }

    // Pre-clean UI updates (main thread)
    ui->btnClean->hide();
    mLoadingMovie_2->start();
    ui->lblLoadingCleaner->show();
    ui->treeWidgetScanResult->setEnabled(false);

    // Read tree widget state on main thread to build work lists
    QTreeWidget *tree = ui->treeWidgetScanResult;
    mFilesToDelete.clear();
    mChildrenToRemove.clear();
    mCleanTrash = false;
    mCleanSnapFlatpak = false;

    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *it = tree->topLevelItem(i);
        CleanCategories cat = (CleanCategories) it->data(2, 0).toInt();

        if (cat == CleanCategories::TRASH) {
            if (it->checkState(0) == Qt::Checked) {
                mCleanTrash = true;
            }
        } else if (cat == CleanCategories::SNAP_FLATPAK_REVISIONS) {
            if (it->checkState(0) == Qt::Checked) {
                mCleanSnapFlatpak = true;
                mChildrenToRemove.append(QPair<int,int>(i, -1));
            }
        } else {
            // Normal file-based category — collect checked children
            for (int j = 0; j < it->childCount(); ++j) {
                if (it->child(j)->checkState(0) == Qt::Checked) {
                    mFilesToDelete << it->child(j)->data(2, 0).toString();
                    mChildrenToRemove.append(QPair<int,int>(i, j));
                }
            }
        }
    }

    mCleanInProgress = true;

    // Launch worker thread (I/O only)
    mWorkerFuture = QtConcurrent::run([this]() { systemClean(); });
}

void SystemCleanerPage::on_btnBackToCategories_clicked()
{
    if (mScanInProgress || mCleanInProgress)
        return;

    ui->btnScan->show();
    ui->lblRemovedTotalSize->clear();
    mLoadingMovie->stop();
    ui->lblLoadingScanner->hide();
    ui->checkPackageCache->setEnabled(true);
    ui->checkCrashReports->setEnabled(true);
    ui->checkAppLog->setEnabled(true);
    ui->checkAppCache->setEnabled(true);
    ui->checkTrash->setEnabled(true);
    ui->checkDevToolCache->setEnabled(true);
    ui->checkBrokenSymlinks->setEnabled(true);
    ui->checkBrowserPrivacy->setEnabled(true);
    if (mCheckSnapFlatpak) mCheckSnapFlatpak->setEnabled(true);
    ui->treeWidgetScanResult->clear();
    ui->stackedWidget->setCurrentIndex(0);
    ui->checkSelectAllSystemScan->setEnabled(true);
    ui->checkSelectAllSystemScan->setChecked(false);
}

void SystemCleanerPage::on_checkSelectAllSystemScan_clicked(bool checked)
{
    ui->checkAppCache->setChecked(checked);
    ui->checkAppLog->setChecked(checked);
    ui->checkCrashReports->setChecked(checked);
    ui->checkPackageCache->setChecked(checked);
    ui->checkTrash->setChecked(checked);
    ui->checkDevToolCache->setChecked(checked);
    ui->checkBrokenSymlinks->setChecked(checked);
    ui->checkBrowserPrivacy->setChecked(checked);
    if (mCheckSnapFlatpak) mCheckSnapFlatpak->setChecked(checked);
}

void SystemCleanerPage::on_checkSelectAll_clicked(bool checked)
{
    for (int i = 0; i < ui->treeWidgetScanResult->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *it = ui->treeWidgetScanResult->topLevelItem(i);
        it->setCheckState(0, (checked ? Qt::Checked : Qt::Unchecked));

        for (int j = 0; j < it->childCount(); ++j)
            it->child(j)->setCheckState(0, (checked ? Qt::Checked : Qt::Unchecked));
    }
}

void SystemCleanerPage::on_cbSortBy_currentIndexChanged(int idx)
{
    switch (idx) {
        case 0: ui->treeWidgetScanResult->sortItems(0, Qt::AscendingOrder); break;
        case 1: ui->treeWidgetScanResult->sortItems(0, Qt::DescendingOrder); break;
        case 2: ui->treeWidgetScanResult->sortItems(1, Qt::AscendingOrder); break;
        case 3: ui->treeWidgetScanResult->sortItems(1, Qt::DescendingOrder); break;
    }
}

void SystemCleanerPage::initScheduleIndicator()
{
    // Floating overlay parented to SystemCleanerPage — not in any layout.
    // Visibility managed manually: shown only when on page 0 with enabled schedules.
    mScheduleIndicator = new QFrame(this);
    mScheduleIndicator->setObjectName("scheduleIndicator");

    QHBoxLayout *indicatorLayout = new QHBoxLayout(mScheduleIndicator);
    indicatorLayout->setContentsMargins(12, 6, 12, 6);

    mLblNextSchedule = new QLabel;
    mLblNextSchedule->setObjectName("lblNextSchedule");
    mLblLastSchedule = new QLabel;
    mLblLastSchedule->setObjectName("lblLastSchedule");

    QVBoxLayout *textLayout = new QVBoxLayout;
    textLayout->setSpacing(2);
    textLayout->addWidget(mLblNextSchedule);
    textLayout->addWidget(mLblLastSchedule);
    indicatorLayout->addLayout(textLayout, 1);

    mScheduleIndicator->raise();

    // Hide indicator when navigating away from the categories page (page 0)
    connect(ui->stackedWidget, &QStackedWidget::currentChanged, this, [this](int index) {
        if (index != 0)
            mScheduleIndicator->hide();
        else
            updateScheduleIndicator();  // re-evaluate and show if schedules exist
    });

    connect(mScheduleManager, &ScheduleManager::schedulesChanged,
            this, &SystemCleanerPage::updateScheduleIndicator);

    updateScheduleIndicator();
}

void SystemCleanerPage::repositionScheduleIndicator()
{
    if (!mScheduleIndicator || !mScheduleIndicator->isVisible())
        return;

    // Position at the bottom of the page, inset by the outer layout margins (15px L/R, 15px bottom)
    int outerMarginLR = 15;
    int outerMarginBottom = 15;
    int indicatorH = mScheduleIndicator->sizeHint().height();
    int w = width() - outerMarginLR * 2;
    int x = outerMarginLR;
    int y = height() - indicatorH - outerMarginBottom;

    mScheduleIndicator->setGeometry(x, y, w, indicatorH);
    mScheduleIndicator->raise();
}

void SystemCleanerPage::repositionExclusionsButton()
{
    if (!mBtnExclusions)
        return;

    int outerMarginR = 15;
    int outerMarginT = 8;
    QSize btnSize = mBtnExclusions->sizeHint();
    int x = width() - btnSize.width() - outerMarginR;
    int y = outerMarginT;
    mBtnExclusions->move(x, y);
    mBtnExclusions->raise();
}

void SystemCleanerPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    repositionScheduleIndicator();
    repositionExclusionsButton();
}

void SystemCleanerPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    repositionScheduleIndicator();
    repositionExclusionsButton();
}

void SystemCleanerPage::updateScheduleIndicator()
{
    QList<ScheduleManager::CleaningSchedule> schedules = mScheduleManager->getAllSchedules();

    bool hasEnabled = false;
    QDateTime earliest;
    QString nextName;
    QDateTime lastRun;
    quint64 lastBytes = 0;

    for (const auto &s : schedules) {
        if (!s.enabled) continue;
        hasEnabled = true;

        QDateTime next = mScheduleManager->getNextRunTime(s);
        if (!earliest.isValid() || next < earliest) {
            earliest = next;
            nextName = s.name;
        }

        if (s.lastRun.isValid() && (!lastRun.isValid() || s.lastRun > lastRun)) {
            lastRun = s.lastRun;
            lastBytes = s.lastBytesFreed;
        }
    }

    if (!hasEnabled) {
        mScheduleIndicator->hide();
        return;
    }

    mScheduleIndicator->show();

    if (earliest.isValid()) {
        mLblNextSchedule->setText(
            tr("Next: %1 \xe2\x80\x94 %2").arg(nextName, earliest.toString("ddd, MMM d h:mm AP")));
    }

    if (lastRun.isValid()) {
        mLblLastSchedule->setText(
            tr("Last: %1 \xe2\x80\x94 cleaned %2")
                .arg(lastRun.toString("MMM d"))
                .arg(FormatUtil::formatBytes(lastBytes)));
    } else {
        mLblLastSchedule->setText(tr("No previous scheduled cleans"));
    }

    repositionScheduleIndicator();
}

void SystemCleanerPage::onManageExclusions()
{
    ExclusionManagerDialog dlg(this, mAppManager);
    dlg.exec();
}

void SystemCleanerPage::onTreeContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = ui->treeWidgetScanResult->itemAt(pos);
    if (!item)
        return;

    // Only show context menu for child items (individual files/folders), not category roots
    if (!item->parent())
        return;

    QString path = item->data(2, 0).toString();
    if (path.isEmpty())
        return;

    QMenu menu(this);
    QAction *actExclude = menu.addAction(tr("Always exclude this"));

    QAction *chosen = menu.exec(ui->treeWidgetScanResult->viewport()->mapToGlobal(pos));
    if (chosen == actExclude) {
        QFileInfo fi(path);
        CleanerService::ExclusionEntry::Type type = fi.isDir()
            ? CleanerService::ExclusionEntry::Folder
            : CleanerService::ExclusionEntry::File;
        mCleanerService->addExclusion(type, path);

        QTreeWidgetItem *parent = item->parent();
        delete parent->takeChild(parent->indexOfChild(item));

        // Update parent title and size
        quint64 remainingSize = 0;
        for (int j = 0; j < parent->childCount(); ++j)
            remainingSize += parent->child(j)->data(1, SortRole).toULongLong();
        parent->setText(0, QString("%1 (%2)")
                        .arg(parent->data(2, 1).toString())
                        .arg(parent->childCount()));
        parent->setText(1, FormatUtil::formatBytes(remainingSize));
    }
}
