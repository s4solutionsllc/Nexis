#include "data_refresh_service.h"
#include "Managers/info_manager.h"
#include "Managers/setting_manager.h"
#include "signal_mapper.h"

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
      mPaused(false)
{
    connect(mFastTimer, &QTimer::timeout, this, &DataRefreshService::onFastTick);
    connect(mMediumTimer, &QTimer::timeout, this, &DataRefreshService::onMediumTick);
    connect(mSlowTimer, &QTimer::timeout, this, &DataRefreshService::onSlowTick);
    connect(mProcessTimer, &QTimer::timeout, this, &DataRefreshService::onProcessTick);

    connect(SignalMapper::ins(), &SignalMapper::sigAppVisibilityChanged,
            this, [this](bool visible) {
        if (visible)
            resume();
        else
            pause();
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
    if (im->hasDiskHealth())
        onSlowTick();
    onProcessTick();

    mFastTimer->start(1000);
    mMediumTimer->start(5000);
    mSlowTimer->start(30000);
    mProcessTimer->start(1000);
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

    mFastTimer->start();
    mMediumTimer->start();
    mSlowTimer->start();
    mProcessTimer->start();
}

bool DataRefreshService::isPaused() const
{
    return mPaused;
}

void DataRefreshService::setProcessRefreshInterval(int ms)
{
    mProcessTimer->setInterval(ms);
}

void DataRefreshService::onFastTick()
{
    // CPU
    QList<int> percents = im->getCpuPercents();
    double clockGHz = im->getCpuClock() / 1000.0;
    QList<double> loadAvgs = im->getCpuLoadAvgs();
    emit cpuUpdated(percents, clockGHz, loadAvgs);

    // Memory
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

    // Network
    emit networkUpdated(im->getRXbytes(), im->getTXbytes());

    // Disk I/O
    emit diskIOUpdated(im->getDiskIO());

    // GPU (conditional)
    if (im->hasGpu()) {
        im->updateGpuInfo();
        emit gpuUpdated(im->getGpuDevices());
    }

    // Temperature (conditional)
    if (im->hasThermalSensors())
        emit tempUpdated();

    // Fan (conditional)
    if (im->hasFanSensors())
        emit fanUpdated();

    // Battery (conditional)
    if (im->hasBattery()) {
        im->updateBatteryInfo();
        emit batteryUpdated(im->getBatteryData());
    }
}

void DataRefreshService::onMediumTick()
{
    im->updateDiskInfo();
    emit diskUsageUpdated(im->getDisks());
}

void DataRefreshService::onSlowTick()
{
    if (!im->hasDiskHealth())
        return;

    im->refreshDiskHealth();
    emit diskHealthUpdated(im->getDriveHealth());
}

void DataRefreshService::onProcessTick()
{
    im->updateProcesses();
    emit processesUpdated(im->getProcesses(), im->getUserName());
}
