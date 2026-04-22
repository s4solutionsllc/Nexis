#ifndef SYSTEMCLEANERPAGE_H
#define SYSTEMCLEANERPAGE_H

#include <QWidget>
#include <QResizeEvent>
#include <QTreeWidgetItem>
#include <QHash>
#include <QMap>
#include <QMovie>
#include <QDebug>
#include <QDir>
#include <QtConcurrent>
#include <QThread>
#include "Managers/app_manager.h"

#include <Managers/cleaner_service.h>

class QCheckBox;
class QLabel;
class QFrame;
class QToolButton;
class QMenu;
class SignalMapper;
class ScheduleManager;

namespace Ui {
    class SystemCleanerPage;
}

class SystemCleanerPage : public QWidget
{
    Q_OBJECT

public:
    enum CleanCategories {
        PACKAGE_CACHE,
        CRASH_REPORTS,
        APPLICATION_LOGS,
        APPLICATION_CACHES,
        TRASH,
        DEV_TOOL_CACHES,
        BROKEN_SYMLINKS,
        BROWSER_PRIVACY,
        SNAP_FLATPAK_REVISIONS
    };

public:
    explicit SystemCleanerPage(QWidget *parent = nullptr,
                               AppManager *appManager = nullptr,
                               SignalMapper *signalMapper = nullptr,
                               CleanerService *cleanerService = nullptr,
                               ScheduleManager *scheduleManager = nullptr);
    ~SystemCleanerPage();

    void quickScan();

signals:
    void scanFinishedS();
    void cleanFinishedS();

private slots:
    quint64 addTreeRoot(const CleanCategories &cat, const QString &title, const QFileInfoList &infos, bool noChild = false);
    void addTreeChild(const CleanCategories &cat, const QString &text, const quint64 &size);
    void addTreeChild(const QString &data, const QString &text, const quint64 &size, QTreeWidgetItem *parent);

    void on_treeWidgetScanResult_itemClicked(QTreeWidgetItem *item, const int &column);
    void on_btnClean_clicked();
    void on_btnScan_clicked();
    void on_btnBackToCategories_clicked();

    void systemScan();
    void systemClean();
    void onScanFinished();
    void onCleanFinished();
    bool cleanValid();

    void on_checkSelectAllSystemScan_clicked(bool checked);
    void on_checkSelectAll_clicked(bool check);
    void on_cbSortBy_currentIndexChanged(int idx);
    void updateScheduleIndicator();
    void onManageExclusions();
    void onTreeContextMenu(const QPoint &pos);

    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void init();
    void initScheduleIndicator();
    void repositionScheduleIndicator();
    void repositionExclusionsButton();

private:
    Ui::SystemCleanerPage *ui;
    AppManager *mAppManager;
    SignalMapper *mSignalMapper;
    CleanerService *mCleanerService;
    ScheduleManager *mScheduleManager;

    QIcon mDefaultIcon;
    QMovie *mLoadingMovie;
    QMovie *mLoadingMovie_2;

    // Thread-safe scan state (set on main thread before worker, read on worker)
    bool mScanPackageCache;
    bool mScanCrashReports;
    bool mScanAppLog;
    bool mScanAppCache;
    bool mScanTrash;
    QString mLblPackageCacheText;
    QString mLblCrashReportsText;
    QString mLblAppLogText;
    QString mLblAppCacheText;
    QString mLblTrashText;
    bool mScanDevToolCache;
    QString mLblDevToolCacheText;
    bool mScanBrokenSymlinks;
    QString mLblBrokenSymlinksText;
    bool mScanBrowserPrivacy;
    QString mLblBrowserPrivacyText;
    bool mScanSnapFlatpak;
    QString mLblSnapFlatpakText;
    // Scan results (written on worker, read on main thread in onScanFinished)
    QFileInfoList mPackageCaches;
    QFileInfoList mCrashReports;
    QFileInfoList mAppLogs;
    QFileInfoList mAppCaches;
    QFileInfoList mDevToolCaches;
    QFileInfoList mBrokenSymlinks;
    QFileInfoList mBrowserPrivacy;
    QFileInfoList mSnapFlatpakRevisions;

    // Thread-safe clean state (set on main thread before worker, read on worker)
    QStringList mFilesToDelete;
    bool mCleanTrash;
    bool mCleanSnapFlatpak;
    // Clean results (written on worker, read on main thread in onCleanFinished)
    quint64 mTotalCleanedSize;
    // Children to remove from tree (indices captured on main thread before worker)
    QList<QPair<int,int>> mChildrenToRemove;

    // Prevent overlapping scan/clean workers (BUG-10)
    bool mScanInProgress = false;
    bool mCleanInProgress = false;

    // Track background tasks so they can be awaited on shutdown (BUG-05)
    QFuture<void> mWorkerFuture;

    // Snap/Flatpak category (added programmatically, Linux only)
    QLabel *mLblSnapFlatpakImg = nullptr;
    QLabel *mLblSnapFlatpakLabel = nullptr;
    QCheckBox *mCheckSnapFlatpak = nullptr;

    // FR-114: per-category scan-size trend row (programmatically placed at
    // grid row 5 for each category column). Refreshed after every scan.
    struct TrendCell {
        QLabel *sizeLabel = nullptr;
    };
    QHash<int, TrendCell> mTrendCells;
    void buildTrendRow();
    void refreshTrendCells();

    // Exclusion rules gear button
    QToolButton *mBtnExclusions = nullptr;

    // Schedule indicator panel
    QFrame *mScheduleIndicator = nullptr;
    QLabel *mLblNextSchedule = nullptr;
    QLabel *mLblLastSchedule = nullptr;
};

#endif // SYSTEMCLEANERPAGE_H
