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
#include <Info/update_info.h>
#include <Tools/repo_health_types.h>

class InfoManager;
class SettingManager;

class DataRefreshService : public QObject
{
    Q_OBJECT

public:
    static DataRefreshService *ins();

    void start();
    void stop();
    void pause();
    void resume();
    bool isPaused() const;
    void setProcessRefreshInterval(int ms);
    void pauseProcessTimer();
    void resumeProcessTimer();
    void triggerUpdateCheck();
    void triggerRepoHealthCheck();

signals:
    void cpuUpdated(const QList<int> &percents, double clockGHz, const QList<double> &loadAvgs);
    void memoryUpdated(const MemorySnapshot &snap);
    void networkUpdated(quint64 rxBytes, quint64 txBytes);
    void diskIOUpdated(const QList<quint64> &io);
    void gpuUpdated(const QList<GpuDevice> &devices);
    void tempUpdated();
    void fanUpdated();
    void batteryUpdated(const BatteryData &data);
    void diskUsageUpdated(const QList<Disk> &disks);
    void diskHealthUpdated(const QList<DriveHealth> &drives);
    void processesUpdated(const QList<Process> &processes, const QString &userName);
    void systemUpdatesChecked(const UpdateCheckResult &result);
    void repoHealthChecked(const RepoHealthCache &cache);

private slots:
    void onFastTick();
    void onMediumTick();
    void onSlowTick();
    void onProcessTick();
    void onUpdateTick();

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
    QTimer *mUpdateTimer;

    bool mPaused;
    bool mProcessPaused;
    bool mUpdateCheckRunning;
    bool mDiskHealthRunning = false;
    bool mRepoHealthRunning = false;
};

#endif // DATA_REFRESH_SERVICE_H
