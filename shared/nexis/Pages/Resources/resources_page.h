#ifndef RESOURCESPAGE_H
#define RESOURCESPAGE_H

#include "nexis_page.h"
#include "history_chart.h"
#include "rolling_series.h"
#include "resource_history_panel.h"
#include "disk_usage_launcher_widget.h"
#include "Managers/info_manager.h"
#include <QChart>
#include <QSpacerItem>
#include <memory>
#ifdef Q_OS_LINUX
#include <Info/psi_info.h>
#include <Info/oomd_snapshot.h>
#include <Info/rapl_power_snapshot.h>
#endif

class DataRefreshService;
class OomKillsWidget;
class PowerDrawWidget;
class CpuCoreDetailWidget;

namespace Ui {
    class ResourcesPage;
}

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
    // FR-96 fix-up: disk temp chart is created lazily on first
    // diskHealthUpdated signal when drives weren't discovered at
    // construction time.
    void ensureDiskHealthChart(const QList<DriveHealth> &drives);

private:
    Ui::ResourcesPage *ui;

    InfoManager *im;
    DataRefreshService *mRefresh;

    HistoryChart *mChartCpu;
    HistoryChart *mChartCpuLoadAvg;
    HistoryChart *mChartDiskReadWrite;
    HistoryChart *mChartMemory;
    HistoryChart *mChartNetwork;

    // SSO-26718: one RollingSeries per chart, built against that chart's own
    // getSeriesList() once it exists. Charts built lazily (GPU, disk health,
    // PSI) get theirs constructed at the same point the chart itself is
    // created. reset() on every page re-activate (B12) so a hidden gap never
    // renders as a frozen, stale chart.
    std::unique_ptr<RollingSeries> mRollingCpu;
    std::unique_ptr<RollingSeries> mRollingCpuLoadAvg;
    std::unique_ptr<RollingSeries> mRollingDiskIO;
    std::unique_ptr<RollingSeries> mRollingMemory;
    std::unique_ptr<RollingSeries> mRollingNetwork;
    std::unique_ptr<RollingSeries> mRollingGpu;
    std::unique_ptr<RollingSeries> mRollingDiskHealth;
    void resetRollingSeries();
    // SSO-10930 / GH#220: persistent history view, separate from the live
    // 0-60s charts above — see resource_history_panel.h for why it isn't a
    // HistoryChart.
    ResourceHistoryPanel *mResourceHistory = nullptr;
    HistoryChart *mChartGpu;
    HistoryChart *mChartDiskHealth;
    // Disk health is polled every 30 s; the chart advances once a second
    // holding the last reading, so its 60 s axis is truthful and a line is
    // visible as soon as the first sample arrives.
    QList<double> mLastDiskTemps;
    QStringList mLastDiskNames;
    void advanceDiskTempChart();

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
#ifdef Q_OS_LINUX
    HistoryChart *mChartPsiCpu = nullptr;
    std::unique_ptr<RollingSeries> mRollingPsi;
    OomKillsWidget *mOomKills = nullptr;
    PowerDrawWidget *mPowerDraw = nullptr;
#endif
    // Per-core utilization/frequency detail — cross-platform (CpuInfo::getClocks()
    // has a macOS implementation too), unlike the RAPL power card above.
    CpuCoreDetailWidget *mCoreDetail = nullptr;

    DiskUsageLauncherWidget *mDiskLauncher;

    bool mActive;
};

#endif // RESOURCESPAGE_H
