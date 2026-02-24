#ifndef RESOURCESPAGE_H
#define RESOURCESPAGE_H

#include <QWidget>

#include "history_chart.h"
#include "disk_usage_launcher_widget.h"
#include "Managers/info_manager.h"
#include <QChart>
#include <QSpacerItem>

class DataRefreshService;

namespace Ui {
    class ResourcesPage;
}

class ResourcesPage : public QWidget
{
    Q_OBJECT

public:
    explicit ResourcesPage(QWidget *parent = nullptr,
                           InfoManager *infoManager = nullptr,
                           DataRefreshService *refreshService = nullptr);
    ~ResourcesPage();

private slots:
    void onCpuUpdated(const QList<int> &percents, double clockGHz, const QList<double> &loadAvgs);
    void onMemoryUpdated(const MemorySnapshot &snap);
    void onNetworkUpdated(quint64 rxBytes, quint64 txBytes);
    void onDiskIOUpdated(const QList<quint64> &io);
    void onGpuUpdated(const QList<GpuDevice> &gpus);
    void onDiskHealthUpdated(const QList<DriveHealth> &drives);

private:
    void init();

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

    DiskUsageLauncherWidget *mDiskLauncher;
};

#endif // RESOURCESPAGE_H
