#ifndef SYSTEMCLEANERPAGE_H
#define SYSTEMCLEANERPAGE_H

#include <QWidget>
#include <QResizeEvent>
#include <QTreeWidgetItem>
#include <QHash>
#include <QMap>
#include <QDebug>
#include <QDir>
#include <QtConcurrent>
#include <QThread>
#include "Managers/app_manager.h"

#include <Managers/cleaner_service.h>
#include "system_cleaner_provider.h"
#include <Common/trust_safety_preview_dialog.h>

class QCheckBox;
class QLabel;
class QFrame;
class QProgressBar;
class QToolButton;
class QMenu;
class QPushButton;
class QScrollArea;
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

    struct CategoryCard {
        QFrame    *frame    = nullptr;
        QCheckBox *check    = nullptr;
        QLabel    *lblSize  = nullptr;
        quint64    lastSize = 0;
    };

public:
    explicit SystemCleanerPage(QWidget *parent = nullptr,
                               AppManager *appManager = nullptr,
                               SignalMapper *signalMapper = nullptr,
                               CleanerService *cleanerService = nullptr,
                               ScheduleManager *scheduleManager = nullptr);
    ~SystemCleanerPage();

    void quickScan();

    // SSO-15956: showEvent() kicks off an async background disk-size scan on
    // first display (startBackgroundSizeScan), which races the initial
    // paint — screenshot/UI tests need a way to wait for it to settle
    // instead of capturing whichever of the two states happens to land.
    bool isScanInProgress() const { return mScanInProgress; }

signals:
    void scanFinishedS();
    void checkedCategoryCountChanged(int count);

private slots:
    quint64 addTreeRoot(const CleanCategories &cat, const QString &title, const QFileInfoList &infos, bool noChild = false);
    void addTreeChild(const CleanCategories &cat, const QString &text, const quint64 &size);
    void addTreeChild(const QString &data, const QString &text, const quint64 &size, QTreeWidgetItem *parent);

    void on_treeWidgetScanResult_itemClicked(QTreeWidgetItem *item, const int &column);
    // SSO-3399: explicit slot name avoids Qt's "on_<objectName>_<signal>"
    // auto-connect lookup matching `btnScan` (no such widget) and emitting a
    // test-log warning. The actual button is `btnScanSystem`, wired
    // explicitly via connect().
    void onBtnScanSystemClicked();

    void systemScan();
    void onScanFinished();

    void on_cbSortBy_currentIndexChanged(int idx);
    void updateScheduleIndicator();
    void onManageExclusions();
    void onTreeContextMenu(const QPoint &pos);
    void onSnapshotTaken(const QString &toolName);
    void onSelectAllClicked();
    // SSO-3732 / FW-05: surface a persistent banner when CleanerService
    // detects macOS 27 TCC denial on cross-team app-container access. The
    // banner explains the situation and exposes a clickable link that
    // QDesktopServices opens against the Privacy & Security pane.
    void onAccessNeededDetected(const QString &message, const QString &deepLink);

    void showEvent(QShowEvent *event) override;

private:
    void init();
    void buildCategoryHeader();
    void buildCategoryCards();
    void buildCleanerFooter();
    void updateFooterTotal();
    void updateCleanerCheckBadge();
    void quickCleanByCategory();
    void startBackgroundSizeScan();
    void buildInlineResults();
    void refreshInlineTree();
    void initScheduleIndicator();
    void repositionScheduleIndicator();

private:
    Ui::SystemCleanerPage *ui;
    AppManager *mAppManager;
    SignalMapper *mSignalMapper;
    CleanerService *mCleanerService;
    ScheduleManager *mScheduleManager;

    QIcon mDefaultIcon;
    QProgressBar *mScanProgress = nullptr;

    // New card-based category widgets
    QVector<CategoryCard> mCards;        // indexed by CleanCategories enum value
    QLabel      *mLblCleanerTitle    = nullptr;
    QPushButton *mBtnScanSystem      = nullptr;
    QPushButton *mBtnSchedule        = nullptr;
    QPushButton *mBtnSelectAll       = nullptr;
    QPushButton *mBtnCleanSelected   = nullptr;
    QLabel      *mLblEstimated       = nullptr;
    QFrame      *mCleanerFooter      = nullptr;
    QLabel      *mLblLoadingScanner  = nullptr;
    bool         mHasScanned         = false;

    // Snap/Flatpak card (Linux only, added programmatically)
    QCheckBox *mCheckSnapFlatpak = nullptr;

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

    // Retained scan results for "Clean selected" on page 0
    QFileInfoList mRetainedPackageCaches;
    QFileInfoList mRetainedCrashReports;
    QFileInfoList mRetainedAppLogs;
    QFileInfoList mRetainedAppCaches;
    QFileInfoList mRetainedDevToolCaches;
    QFileInfoList mRetainedBrokenSymlinks;
    QFileInfoList mRetainedBrowserPrivacy;
    QFileInfoList mRetainedSnapFlatpak;

    // Prevent overlapping scan/clean workers (BUG-10)
    bool mScanInProgress = false;
    bool mCleanInProgress = false;   // true while TrustSafetyPreviewDialog is open
    bool mInitialScan = false;       // true during the auto-scan fired on first show
    // GH-226: suppress per-checkbox tree rebuilds during onSelectAllClicked()
    bool mBulkCategoryUpdate = false;

    // Track background tasks so they can be awaited on shutdown (BUG-05)
    QFuture<void> mWorkerFuture;

    QLabel      *mLblSnapshotToast = nullptr;
    // SSO-3732 / FW-05: macOS Privacy & Security "access needed" banner.
    // Persistent (no auto-hide) because the resolution is user action in
    // System Settings, not a transient event. Embedded link uses
    // QDesktopServices to open the Privacy_AllFiles pane.
    QLabel      *mLblAccessNeeded = nullptr;

    // Exclusion rules gear button (in header row)
    QToolButton *mBtnExclusions = nullptr;

    // Schedule indicator panel (floating overlay, bottom of page 0)
    QFrame *mScheduleIndicator = nullptr;
    QLabel *mLblNextSchedule = nullptr;
    QLabel *mLblLastSchedule = nullptr;
};

#endif // SYSTEMCLEANERPAGE_H
