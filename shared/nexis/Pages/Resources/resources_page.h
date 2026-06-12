#ifndef RESOURCESPAGE_H
#define RESOURCESPAGE_H

#include "nexis_page.h"
#include "history_chart.h"
#include "disk_usage_launcher_widget.h"
#include "Managers/info_manager.h"
#include <QChart>
#include <QSpacerItem>
#ifdef Q_OS_LINUX
#include <Info/psi_info.h>
#include <Info/oomd_snapshot.h>
#endif

class DataRefreshService;
class OomKillsWidget;

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
    HistoryChart *mChartGpu;
    HistoryChart *mChartDiskHealth;
#ifdef Q_OS_LINUX
    HistoryChart *mChartPsiCpu = nullptr;
    OomKillsWidget *mOomKills = nullptr;
#endif

    DiskUsageLauncherWidget *mDiskLauncher;

    bool mActive;
};

#endif // RESOURCESPAGE_H
