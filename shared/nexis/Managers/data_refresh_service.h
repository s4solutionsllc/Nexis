#ifndef DATA_REFRESH_SERVICE_H
#define DATA_REFRESH_SERVICE_H

#include <QObject>
#include <QTimer>
#include <QList>

#include <Info/disk_info.h>
#include <Info/gpu_info.h>
#include <Info/memory_info.h>
#include <Info/battery_info.h>
#include <Info/disk_health_info.h>
#include <Info/process.h>

class InfoManager;
class SettingManager;

class DataRefreshService : public QObject
{
    Q_OBJECT

public:
    static DataRefreshService *ins();

    void start();
    void pause();
    void resume();
    bool isPaused() const;
    void setProcessRefreshInterval(int ms);

signals:
    void cpuUpdated(QList<int> percents, double clockGHz, QList<double> loadAvgs);
    void memoryUpdated(const MemorySnapshot &snap);
    void networkUpdated(quint64 rxBytes, quint64 txBytes);
    void diskIOUpdated(QList<quint64> io);
    void gpuUpdated(QList<GpuDevice> devices);
    void tempUpdated();
    void fanUpdated();
    void batteryUpdated(BatteryData data);
    void diskUsageUpdated(QList<Disk> disks);
    void diskHealthUpdated(QList<DriveHealth> drives);
    void processesUpdated(QList<Process> processes, QString userName);

private slots:
    void onFastTick();
    void onMediumTick();
    void onSlowTick();
    void onProcessTick();

private:
    explicit DataRefreshService(InfoManager *infoManager = nullptr,
                                SettingManager *settingManager = nullptr,
                                QObject *parent = nullptr);

    static DataRefreshService *instance;

    InfoManager *im;
    SettingManager *sm;

    QTimer *mFastTimer;
    QTimer *mMediumTimer;
    QTimer *mSlowTimer;
    QTimer *mProcessTimer;

    bool mPaused;
};

#endif // DATA_REFRESH_SERVICE_H
