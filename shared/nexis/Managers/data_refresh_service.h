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
#ifdef Q_OS_LINUX
#include <Info/psi_info.h>
#endif

class InfoManager;
class SettingManager;

class DataRefreshService : public QObject
{
    Q_OBJECT

public:
    // FR-103: pages subscribe to the signals they render so DataRefreshService
    // can skip the expensive sample (notably nvidia-smi, QStorageInfo walks)
    // when no page is interested. Counters are keyed by enum index.
    enum class Signal : int {
        Cpu = 0,
        Memory,
        Network,
        DiskIO,
        DiskUsage,
        Gpu,
        Temp,
        Fan,
        Battery,
        Psi,
        _Count
    };

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

    // Subscriber counting — pages call these from onPageActivated /
    // onPageDeactivated. Idempotent across many pages.
    void subscribe(Signal s);
    void unsubscribe(Signal s);
    bool hasSubscribers(Signal s) const;

    // FR-105: cadence tiers. The service picks one based on focus state
    // (sigAppFocusChanged) and whether the laptop is on battery. Minimized
    // to tray keeps the existing pause() behaviour separate.
    enum class PowerMode { Normal, Battery, Unfocused };
    void setPowerMode(PowerMode mode);
    PowerMode powerMode() const { return mPowerMode; }

    // BUG-110: cached copy of the last result emitted by systemUpdatesChecked /
    // repoHealthChecked. Pages constructed lazily (FR-97) connect too late to
    // catch the startup emission; they query these on activation to backfill
    // the initial state. hasLastUpdateCheckResult() distinguishes "never ran"
    // from "ran and found zero updates".
    bool hasLastUpdateCheckResult() const { return mHasLastUpdateCheckResult; }
    const UpdateCheckResult &lastUpdateCheckResult() const { return mLastUpdateCheckResult; }
    bool hasLastRepoHealthCache() const { return mHasLastRepoHealthCache; }
    const RepoHealthCache &lastRepoHealthCache() const { return mLastRepoHealthCache; }

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
#ifdef Q_OS_LINUX
    void psiUpdated(const PsiSnapshot &cpu);
#endif
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
    bool mDiskUsageRunning = false;
    bool mRepoHealthRunning = false;

    // BUG-110: cache of last emissions so late subscribers can backfill.
    UpdateCheckResult mLastUpdateCheckResult;
    bool mHasLastUpdateCheckResult = false;
    RepoHealthCache mLastRepoHealthCache;
    bool mHasLastRepoHealthCache = false;

    // Subscriber counters, one per Signal enum value.
    int mSubscriberCounts[static_cast<int>(Signal::_Count)] = {0};

    PowerMode mPowerMode = PowerMode::Normal;
    bool mFocused = true;
    bool mOnBattery = false;

    void recomputePowerMode();
};

#endif // DATA_REFRESH_SERVICE_H
