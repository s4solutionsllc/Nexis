#include "system_cleaner_page.h"
#include "ui_system_cleaner_page.h"
#include "byte_tree_widget.h"
#include <QLabel>

SystemCleanerPage::~SystemCleanerPage()
{
    delete ui;
}

SystemCleanerPage::SystemCleanerPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SystemCleanerPage),
    im(InfoManager::ins()),
    tmr(ToolManager::ins()),
    mDefaultIcon(QIcon::fromTheme("application-x-executable", QIcon(":/static/themes/common/img/package.png"))),
    mLoadingMovie(nullptr),
    mLoadingMovie_2(nullptr)
{
    ui->setupUi(this);

    init();

    ui->stackedWidget->setCurrentIndex(0);
}

void SystemCleanerPage::init()
{
    // Set category icons from system theme with bundled fallbacks
    auto setThemePixmap = [](QLabel *lbl, const QString &themeName, const QString &fallback) {
        QIcon icon = QIcon::fromTheme(themeName, QIcon(fallback));
        lbl->setPixmap(icon.pixmap(64, 64));
    };
    setThemePixmap(ui->lblPackageCacheImg, "package-x-generic",  ":/static/themes/default/img/c_package.png");
    setThemePixmap(ui->lblCrashReportsImg, "dialog-warning",     ":/static/themes/default/img/c_crash.png");
    setThemePixmap(ui->lblLogImage,        "text-x-generic",     ":/static/themes/default/img/c_logs.png");
    setThemePixmap(ui->lblAppCacheImg,     "folder",             ":/static/themes/default/img/c_cache.png");
    setThemePixmap(ui->lblTrashImg,        "user-trash",         ":/static/themes/default/img/c_trash.png");

    // treview settings
    ui->treeWidgetScanResult->setColumnCount(2);
    ui->treeWidgetScanResult->setColumnWidth(0, 600);

    ui->treeWidgetScanResult->header()->setFixedHeight(30);
    ui->treeWidgetScanResult->setHeaderLabels({ tr("File Name"), tr("Size") });

    // loaders
    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme, [=] {
        QString themeName = AppManager::ins()->resolveThemeName();

        mLoadingMovie = new QMovie(QString(":/static/themes/%1/img/scanLoading.gif").arg(themeName),{},this);
        ui->lblLoadingScanner->setMovie(mLoadingMovie);
        mLoadingMovie->start();
        ui->lblLoadingScanner->hide();

        mLoadingMovie_2 = new QMovie(QString(":/static/themes/%1/img/loading.gif").arg(themeName),{},this);
        ui->lblLoadingCleaner->setMovie(mLoadingMovie_2);
        mLoadingMovie_2->start();
        ui->lblLoadingCleaner->hide();
    });

    // needed to suppress qt warnings (signal/slot <> threads)
    qRegisterMetaType<QList<QPersistentModelIndex>>();
    qRegisterMetaType<QAbstractItemModel::LayoutChangeHint>();
    qRegisterMetaType<Qt::SortOrder>();

    connect(this, &SystemCleanerPage::scanFinishedS, this, &SystemCleanerPage::onScanFinished);
    connect(this, &SystemCleanerPage::cleanFinishedS, this, &SystemCleanerPage::onCleanFinished);
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
    item->setIcon(0, QIcon::fromTheme(text, mDefaultIcon));
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
    // Worker thread: only I/O, no UI access
    if (mScanPackageCache) {
        mPackageCaches = tmr->getPackageCaches();
    }
    if (mScanCrashReports) {
        mCrashReports = im->getCrashReports();
    }
    if (mScanAppLog) {
        mAppLogs = im->getAppLogs();
    }
    if (mScanAppCache) {
        mAppCaches = im->getAppCaches();
    }

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
    if (mScanTrash) {
        totalSize += addTreeRoot(TRASH, mLblTrashText,
                    { QFileInfo(QDir::homePath() + "/.local/share/Trash/") }, true);
    }

    ui->lblTotalBytes->setText(tr("Total size: %1").arg(FormatUtil::formatBytes(totalSize)));

    ui->treeWidgetScanResult->setSortingEnabled(true);
    on_cbSortBy_currentIndexChanged(ui->cbSortBy->currentIndex());

    // scan results page
    ui->stackedWidget->setCurrentIndex(1);

    ui->checkPackageCache->setChecked(false);
    ui->checkCrashReports->setChecked(false);
    ui->checkAppLog->setChecked(false);
    ui->checkAppCache->setChecked(false);
    ui->checkTrash->setChecked(false);
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
    // Worker thread: only I/O, no UI access
    mTotalCleanedSize = 0;

    // Handle trash deletion
    if (mCleanTrash) {
        QDir(mTrashPath + "/files").removeRecursively();
        QDir(mTrashPath + "/info").removeRecursively();
    }

    // Get sizes before deletion
    for (const QString &file : mFilesToDelete) {
        mTotalCleanedSize += FileUtil::getFileSize(file);
    }

    // Remove selected files
    if (!mFilesToDelete.isEmpty()) {
        CommandUtil::sudoExec("rm", QStringList() << "-rf" << mFilesToDelete);
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
        QTreeWidgetItem *parent = tree->topLevelItem(parentIdx);
        if (parent) {
            delete parent->takeChild(childIdx);
        }
    }

    // Update titles
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *it = tree->topLevelItem(i);
        it->setText(0, QString("%1 (%2)")
                    .arg(it->data(2, 1).toString())
                    .arg(it->childCount()));
        it->setText(1, QString("%1")
                    .arg(FormatUtil::formatBytes(FileUtil::getFileSize(it->data(3, 0).toString()))));
    }

    ui->lblRemovedTotalSize->setText(tr("%1 size files cleaned.")
                                     .arg(FormatUtil::formatBytes(mTotalCleanedSize)));

    ui->btnClean->show();
    ui->lblLoadingCleaner->hide();
    ui->treeWidgetScanResult->setEnabled(true);
}

void SystemCleanerPage::on_btnScan_clicked()
{
    // Read checkbox states on main thread
    mScanPackageCache = ui->checkPackageCache->isChecked();
    mScanCrashReports = ui->checkCrashReports->isChecked();
    mScanAppLog       = ui->checkAppLog->isChecked();
    mScanAppCache     = ui->checkAppCache->isChecked();
    mScanTrash        = ui->checkTrash->isChecked();

    if (!(mScanPackageCache || mScanCrashReports || mScanAppLog || mScanAppCache || mScanTrash)) {
        return;
    }

    // Read label texts on main thread (for tree root titles)
    mLblPackageCacheText = ui->lblPackageCache->text();
    mLblCrashReportsText = ui->lblCrashReports->text();
    mLblAppLogText       = ui->lblAppLog->text();
    mLblAppCacheText     = ui->lblAppCache->text();
    mLblTrashText        = ui->lblTrash->text();

    // Pre-scan UI updates (main thread)
    ui->btnScan->hide();
    ui->lblLoadingScanner->show();
    ui->checkPackageCache->setEnabled(false);
    ui->checkCrashReports->setEnabled(false);
    ui->checkAppLog->setEnabled(false);
    ui->checkAppCache->setEnabled(false);
    ui->checkTrash->setEnabled(false);
    ui->checkSelectAllSystemScan->setEnabled(false);

    // Clear cached results
    mPackageCaches.clear();
    mCrashReports.clear();
    mAppLogs.clear();
    mAppCaches.clear();

    // Launch worker thread (I/O only)
    (void)QtConcurrent::run([this]() { systemScan(); });
}

void SystemCleanerPage::on_btnClean_clicked()
{
    if (!cleanValid()) {
        return;
    }

    // Pre-clean UI updates (main thread)
    ui->btnClean->hide();
    ui->lblLoadingCleaner->show();
    ui->treeWidgetScanResult->setEnabled(false);

    // Read tree widget state on main thread to build work lists
    QTreeWidget *tree = ui->treeWidgetScanResult;
    mFilesToDelete.clear();
    mChildrenToRemove.clear();
    mCleanTrash = false;

    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *it = tree->topLevelItem(i);
        CleanCategories cat = (CleanCategories) it->data(2, 0).toInt();

        if (cat != CleanCategories::TRASH) {
            for (int j = 0; j < it->childCount(); ++j) {
                if (it->child(j)->checkState(0) == Qt::Checked) {
                    QString filePath = it->child(j)->data(2, 0).toString();
                    mFilesToDelete << filePath;
                    mChildrenToRemove.append(QPair<int,int>(i, j));
                }
            }
        } else if (cat == CleanCategories::TRASH) {
            if (it->checkState(0) == Qt::Checked) {
                mCleanTrash = true;
                mTrashPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation).append("/.local/share/Trash");
            }
        }
    }

    // Launch worker thread (I/O only)
    (void)QtConcurrent::run([this]() { systemClean(); });
}

void SystemCleanerPage::on_btnBackToCategories_clicked()
{
    ui->btnScan->show();
    ui->lblRemovedTotalSize->clear();
    ui->lblLoadingScanner->hide();
    ui->checkPackageCache->setEnabled(true);
    ui->checkCrashReports->setEnabled(true);
    ui->checkAppLog->setEnabled(true);
    ui->checkAppCache->setEnabled(true);
    ui->checkTrash->setEnabled(true);
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
