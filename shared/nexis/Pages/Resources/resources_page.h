#ifndef RESOURCESPAGE_H
#define RESOURCESPAGE_H

#include "nexis_page.h"
#include "history_chart.h"
#include "rolling_series.h"
#include "resource_history_panel.h"
#include "disk_usage_launcher_widget.h"
#include "Managers/info_manager.h"
#include <QChart>
#include <QGridLayout>
#include <QToolButton>
#include <QLabel>
#include <memory>
#ifdef Q_OS_LINUX
#include <Info/psi_info.h>
#include <Info/oomd_snapshot.h>
#include <Info/rapl_power_snapshot.h>
#endif

class DataRefreshService;
class OomKillsWidget;
class CpuCoreDetailWidget;

namespace Ui {
    class ResourcesPage;
}

// SSO-26719: the Resources page's 13 flat, full-width cards restructured
// into six fixed-order subsystem cards (CPU, Load & Pressure, Memory, GPU,
// Disk I/O, Network) in a responsive 2-column grid, each leading with a
// Dashboard-tile-style current-value header instead of a verbose legend.
// Per-Core Detail, Package Power (RAPL), CPU Pressure Stall (PSI) and OOM
// kills are folded into one of the six rather than kept as standalone cards
// (see SSO-26716's audit for the rationale); Disk Temperature folds into the
// Disk I/O header and its own chart is removed. Resource History (persisted)
// moves behind a Live/History switch; the Disk Usage Launcher stays as a
// full-width row below the grid, unchanged.
class ResourcesPage : public NexisPage
{
    Q_OBJECT

public:
    explicit ResourcesPage(QWidget *parent = nullptr,
                           InfoManager *infoManager = nullptr,
                           DataRefreshService *refreshService = nullptr);
    ~ResourcesPage();

    void onPageActivated() override;
    void onPageDeactivated() override;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onCpuUpdated(const QList<int> &percents, double clockGHz, const QList<double> &loadAvgs);
    void onMemoryUpdated(const MemorySnapshot &snap);
    void onNetworkUpdated(quint64 rxBytes, quint64 txBytes);
    void onDiskIOUpdated(const QList<quint64> &io);
    void onGpuUpdated(const QList<GpuDevice> &gpus);
    void onDiskHealthUpdated(const QList<DriveHealth> &drives);
#ifdef Q_OS_LINUX
    void onPsiUpdated(const PsiSnapshot &snap);
    void onOomdUpdated(const OomdSnapshot &snap);
    void onPowerUpdated(const RaplPowerSnapshot &snap);
#endif

private:
    void init();
    void buildLiveHistorySwitch();
    void buildCardGrid();
    // GH#475-style reflow (SystemCleanerPage precedent): 2 columns above
    // ~1100px, collapsing to 1 below so no card forces the page wider than
    // the window.
    void reflowCardGrid(int cols);
    void setLiveModeActive(bool live);
    // Disk I/O and CPU headers each fold in a second, independently-cadenced
    // signal (30s disk-health poll; Linux-only RAPL poll) alongside the
    // fast-tick rate/percent — these re-render from the last known value of
    // both rather than letting whichever ticked least recently go blank.
    void refreshDiskIOHeader();
    void refreshCpuHeader();

private:
    Ui::ResourcesPage *ui;

    InfoManager *im;
    DataRefreshService *mRefresh;

    HistoryChart *mChartCpu;
    HistoryChart *mChartLoadPressure;
    HistoryChart *mChartDiskReadWrite;
    HistoryChart *mChartMemory;
    HistoryChart *mChartNetwork;
    HistoryChart *mChartGpu = nullptr;

    // SSO-26718: one RollingSeries per chart. CPU is now a single "Total"
    // line — per-core detail moved to the CPU card's "Per-core" toggle view,
    // which renders current values directly rather than a trend.
    std::unique_ptr<RollingSeries> mRollingCpu;
    std::unique_ptr<RollingSeries> mRollingLoadPressure;
    std::unique_ptr<RollingSeries> mRollingDiskIO;
    std::unique_ptr<RollingSeries> mRollingMemory;
    std::unique_ptr<RollingSeries> mRollingNetwork;
    std::unique_ptr<RollingSeries> mRollingGpu;
    void resetRollingSeries();

    // CPU card's "Per-core" toggle (HistoryChart::addBodyPage(), page 1) —
    // cross-platform, unlike the RAPL wattage folded into the same header.
    CpuCoreDetailWidget *mCoreDetail = nullptr;
    QToolButton *mPerCoreToggle = nullptr;

#ifdef Q_OS_LINUX
    // Folded into the Memory card's extrasRow() rather than its own card.
    OomKillsWidget *mOomKills = nullptr;
    // Folded into the Load & Pressure card's extrasRow() as a status line;
    // null (and never subscribed) on a host without /proc/pressure/cpu.
    QLabel *mPsiStatusLabel = nullptr;
#endif

    // SSO-10930 / GH#220: persistent history view, behind the Live/History
    // switch instead of stacked below the live charts.
    ResourceHistoryPanel *mResourceHistory = nullptr;
    QToolButton *mBtnLive = nullptr;
    QToolButton *mBtnHistory = nullptr;

    // SSO-26719: six cards, fixed order, defined in exactly one place
    // (mCardOrder, built once in init()).
    QWidget *mCardGrid = nullptr;
    QGridLayout *mCardGridLayout = nullptr;
    QList<QWidget*> mCardOrder;
    int mCardCols = 0;
    int mCardStretchRow = -1;

    DiskUsageLauncherWidget *mDiskLauncher;

    // SSO-26717 (B5): per-instance I/O counter baselines — deliberately NOT
    // function-local statics, since those can't be invalidated from outside
    // the slot. Invalidated on page deactivation and on app-visibility loss
    // (DataRefreshService::pause()) so the first sample after a hidden/paused
    // gap re-baselines instead of plotting a delta spanning the whole gap.
    quint64 mLastReadBytes = 0;
    quint64 mLastWriteBytes = 0;
    bool mDiskIOBaselineValid = false;
    quint64 mLastRxBytes = 0;
    quint64 mLastTxBytes = 0;
    bool mNetworkBaselineValid = false;
    void invalidateIOBaselines();

    // Disk I/O header's folded-in current read/write rate and (B6, by
    // construction) disk temperature.
    QString mLastDiskReadText;
    QString mLastDiskWriteText;
    QString mLastDiskTempText;

    // CPU header's folded-in current total CPU% and (Linux) RAPL wattage.
    QString mLastCpuPercentText;
    QString mLastPowerText;

    bool mActive;
};

#endif // RESOURCESPAGE_H
