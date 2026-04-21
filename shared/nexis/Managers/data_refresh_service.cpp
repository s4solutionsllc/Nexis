#include "data_refresh_service.h"
#include "Managers/info_manager.h"
#include "Managers/setting_manager.h"
#include "Managers/tool_manager.h"
#include "signal_mapper.h"

#include <QtConcurrent>

#ifdef Q_OS_MACOS
#include <Tools/repo_health_checker_macos.h>
#endif

DataRefreshService *DataRefreshService::instance = nullptr;

DataRefreshService::DataRefreshService(InfoManager *infoManager,
                                       SettingManager *settingManager,
                                       QObject *parent)
    : QObject(parent),
      im(infoManager ? infoManager : InfoManager::ins()),
      sm(settingManager ? settingManager : SettingManager::ins()),
      mFastTimer(new QTimer(this)),
      mMediumTimer(new QTimer(this)),
      mSlowTimer(new QTimer(this)),
      mProcessTimer(new QTimer(this)),
      mUpdateTimer(new QTimer(this)),
      mPaused(false),
      mProcessPaused(true),
      mUpdateCheckRunning(false)
{
    connect(mFastTimer, &QTimer::timeout, this, &DataRefreshService::onFastTick);
    connect(mMediumTimer, &QTimer::timeout, this, &DataRefreshService::onMediumTick);
    connect(mSlowTimer, &QTimer::timeout, this, &DataRefreshService::onSlowTick);
    connect(mProcessTimer, &QTimer::timeout, this, &DataRefreshService::onProcessTick);
    connect(mUpdateTimer, &QTimer::timeout, this, &DataRefreshService::onUpdateTick);

    connect(SignalMapper::ins(), &SignalMapper::sigAppVisibilityChanged,
            this, [this](bool visible) {
        if (visible)
            resume();
        else
            pause();
    });

    // FR-105: downshift cadence when the main window loses focus. Full rate
    // resumes on focus restore. Battery transitions feed off the existing
    // batteryUpdated signal — no extra polling.
    connect(SignalMapper::ins(), &SignalMapper::sigAppFocusChanged,
            this, [this](bool focused) {
        mFocused = focused;
        recomputePowerMode();
    });

    connect(this, &DataRefreshService::batteryUpdated,
            this, [this](const BatteryData &data) {
        const bool onBattery = !data.isPluggedIn;
        if (onBattery != mOnBattery) {
            mOnBattery = onBattery;
            recomputePowerMode();
        }
    });
}

DataRefreshService *DataRefreshService::ins()
{
    if (!instance)
        instance = new DataRefreshService;
    return instance;
}

void DataRefreshService::start()
{
    // Fire immediate ticks so pages have data before first paint
    onFastTick();
    onMediumTick();
    // Always kick off the slow tick — onSlowTick() handles the
    // not-yet-discovered case by running discoverDrives() on a worker
    // thread (FR-96). This is async; the main thread is not blocked.
    onSlowTick();

    mFastTimer->start(1000);
    mMediumTimer->start(5000);
    mSlowTimer->start(30000);

    if (im->hasUpdateSources()) {
        onUpdateTick();
        mUpdateTimer->start(3600000);
    }
}

void DataRefreshService::stop()
{
    mFastTimer->stop();
    mMediumTimer->stop();
    mSlowTimer->stop();
    mProcessTimer->stop();
    mUpdateTimer->stop();
    mPaused = true;
}

void DataRefreshService::setPowerMode(PowerMode mode)
{
    if (mode == mPowerMode)
        return;

    mPowerMode = mode;

    // Cadence intervals, in ms. Values are first-pass guesses intended to be
    // tuned empirically. Kiosk and minimized paths are intentionally excluded
    // — kiosk overrides pause(), minimized fully pauses via pause().
    int fastMs = 1000;
    int mediumMs = 5000;
    int slowMs = 30000;

    switch (mode) {
    case PowerMode::Normal:
        fastMs = 1000;  mediumMs = 5000;   slowMs = 30000;  break;
    case PowerMode::Battery:
        fastMs = 2000;  mediumMs = 10000;  slowMs = 60000;  break;
    case PowerMode::Unfocused:
        fastMs = 5000;  mediumMs = 30000;  slowMs = 60000;  break;
    }

    // Only restart timers that were already running; leave the process and
    // update timers alone (they have their own cadence rules).
    if (mFastTimer->isActive())    mFastTimer->start(fastMs);
    if (mMediumTimer->isActive())  mMediumTimer->start(mediumMs);
    if (mSlowTimer->isActive())    mSlowTimer->start(slowMs);
}

void DataRefreshService::recomputePowerMode()
{
    PowerMode target = PowerMode::Normal;
    if (!mFocused)
        target = PowerMode::Unfocused;   // unfocused dominates — most aggressive
    else if (mOnBattery)
        target = PowerMode::Battery;
    setPowerMode(target);
}

void DataRefreshService::subscribe(Signal s)
{
    const int idx = static_cast<int>(s);
    if (idx >= 0 && idx < static_cast<int>(Signal::_Count))
        ++mSubscriberCounts[idx];
}

void DataRefreshService::unsubscribe(Signal s)
{
    const int idx = static_cast<int>(s);
    if (idx >= 0 && idx < static_cast<int>(Signal::_Count) && mSubscriberCounts[idx] > 0)
        --mSubscriberCounts[idx];
}

bool DataRefreshService::hasSubscribers(Signal s) const
{
    const int idx = static_cast<int>(s);
    if (idx < 0 || idx >= static_cast<int>(Signal::_Count))
        return false;
    return mSubscriberCounts[idx] > 0;
}

void DataRefreshService::pause()
{
    if (mPaused)
        return;

    // Don't pause in kiosk mode — monitoring display must stay live
    if (sm->getKioskMode())
        return;

    mPaused = true;
    mFastTimer->stop();
    mMediumTimer->stop();
    mSlowTimer->stop();
    mProcessTimer->stop();
    mUpdateTimer->stop();
}

void DataRefreshService::resume()
{
    if (!mPaused)
        return;

    mPaused = false;

    // Immediate refresh to bring UI up to date
    onFastTick();
    onMediumTick();
    // Skip onSlowTick — smartctl is expensive; let the 30s timer handle it
    // Skip onUpdateTick — too slow; let the hourly timer handle it

    mFastTimer->start();
    mMediumTimer->start();
    mSlowTimer->start();
    if (!mProcessPaused)
        mProcessTimer->start();
    if (im->hasUpdateSources())
        mUpdateTimer->start();
}

bool DataRefreshService::isPaused() const
{
    return mPaused;
}

void DataRefreshService::setProcessRefreshInterval(int ms)
{
    mProcessTimer->setInterval(ms);
}

void DataRefreshService::pauseProcessTimer()
{
    if (mProcessPaused)
        return;
    mProcessPaused = true;
    mProcessTimer->stop();
}

void DataRefreshService::resumeProcessTimer()
{
    if (!mProcessPaused)
        return;
    mProcessPaused = false;
    if (!mPaused) {
        onProcessTick();
        mProcessTimer->start(mProcessTimer->interval() > 0 ? mProcessTimer->interval() : 1000);
    }
}

void DataRefreshService::triggerUpdateCheck()
{
    onUpdateTick();
}

void DataRefreshService::triggerRepoHealthCheck()
{
    if (mRepoHealthRunning)
        return;

    mRepoHealthRunning = true;
    QtConcurrent::run([this]() {
        RepoHealthChecker *checker = ToolManager::ins()->repoHealthChecker();
        RepoHealthCache cache;
#ifdef Q_OS_MACOS
        auto *macChecker = dynamic_cast<RepoHealthCheckerMac *>(checker);
        if (macChecker)
            cache = macChecker->checkBrewPackages(ToolManager::ins()->packageTool()->getPackages());
#else
        cache = checker->checkAll(ToolManager::ins()->getSourceList());
#endif
        QMetaObject::invokeMethod(this, [this, cache]() {
            mRepoHealthRunning = false;
            mLastRepoHealthCache = cache;
            mHasLastRepoHealthCache = true;
            emit repoHealthChecked(cache);
        }, Qt::QueuedConnection);
    });
}

void DataRefreshService::onFastTick()
{
    // FR-103: gate each block on at least one subscribed page. When both
    // Dashboard and Resources are deactivated (or haven't been constructed
    // yet under lazy-page construction / FR-97), these samples do nothing.
    //
    // Accepted tradeoff: CPU/network rate deltas skip a beat on first
    // reactivation because their baseline state is stale. UI pages tolerate
    // the one-tick blip without visible artifacts.

    if (hasSubscribers(Signal::Cpu)) {
        QList<int> percents = im->getCpuPercents();
        double clockGHz = im->getCpuClock() / 1000.0;
        QList<double> loadAvgs = im->getCpuLoadAvgs();
        emit cpuUpdated(percents, clockGHz, loadAvgs);
    }

    if (hasSubscribers(Signal::Memory)) {
        im->updateMemoryInfo();
        MemorySnapshot snap;
        snap.used = im->getMemUsed();
        snap.total = im->getMemTotal();
        snap.swapUsed = im->getSwapUsed();
        snap.swapTotal = im->getSwapTotal();
        snap.wired = im->getMemWired();
        snap.active = im->getMemActive();
        snap.inactive = im->getMemInactive();
        snap.compressed = im->getMemCompressed();
        snap.available = im->getMemAvailable();
        snap.pressureLevel = im->getMemPressureLevel();
        emit memoryUpdated(snap);
    }

    if (hasSubscribers(Signal::Network)) {
        im->updateNetworkBytes();
        emit networkUpdated(im->getRXbytes(), im->getTXbytes());
    }

    if (hasSubscribers(Signal::DiskIO))
        emit diskIOUpdated(im->getDiskIO());

    if (hasSubscribers(Signal::Gpu) && im->hasGpu()) {
        im->updateGpuInfo();
        emit gpuUpdated(im->getGpuDevices());
    }

    if (hasSubscribers(Signal::Battery) && im->hasBattery()) {
        im->updateBatteryInfo();
        emit batteryUpdated(im->getBatteryData());
    }

    // Temperature and fan have been moved to onMediumTick (FR-104).
}

void DataRefreshService::onMediumTick()
{
    // FR-101 + FR-103: QStorageInfo::mountedVolumes() goes off-thread, and
    // we only run it when a page is subscribed to diskUsage (Dashboard).
    if (hasSubscribers(Signal::DiskUsage) && !mDiskUsageRunning) {
        mDiskUsageRunning = true;
        QtConcurrent::run([this] {
            QList<Disk> disks = im->collectDiskInfo();
            QMetaObject::invokeMethod(this, [this, disks] {
                im->setDisks(disks);
                mDiskUsageRunning = false;
                emit diskUsageUpdated(disks);
            }, Qt::QueuedConnection);
        });
    }

    // FR-104: temp/fan sampling piggybacks on the 5 s medium tick.
    if (hasSubscribers(Signal::Temp) && im->hasThermalSensors())
        emit tempUpdated();

    if (hasSubscribers(Signal::Fan) && im->hasFanSensors())
        emit fanUpdated();
}

void DataRefreshService::onSlowTick()
{
    if (mDiskHealthRunning)
        return;

    mDiskHealthRunning = true;
    QtConcurrent::run([this]() {
        // FR-96: on the very first tick, drives may not have been
        // discovered yet (initial discovery is deferred out of the
        // InfoManager/DiskHealthInfo constructors). Run discovery then;
        // subsequent ticks go through the cheaper refresh path.
        if (!im->hasDiskHealth())
            im->discoverDiskHealth();
        else
            im->refreshDiskHealth();
        QList<DriveHealth> drives = im->getDriveHealth();
        QMetaObject::invokeMethod(this, [this, drives]() {
            mDiskHealthRunning = false;
            emit diskHealthUpdated(drives);
        }, Qt::QueuedConnection);
    });
}

void DataRefreshService::onProcessTick()
{
    im->updateProcesses();
    emit processesUpdated(im->getProcesses(), im->getUserName());
}

void DataRefreshService::onUpdateTick()
{
    if (mUpdateCheckRunning || !im->hasUpdateSources())
        return;

    mUpdateCheckRunning = true;
    QtConcurrent::run([this]() {
        UpdateCheckResult result = im->checkForSystemUpdates();
        QMetaObject::invokeMethod(this, [this, result]() {
            mUpdateCheckRunning = false;
            mLastUpdateCheckResult = result;
            mHasLastUpdateCheckResult = true;
            emit systemUpdatesChecked(result);
        }, Qt::QueuedConnection);
    });
}
