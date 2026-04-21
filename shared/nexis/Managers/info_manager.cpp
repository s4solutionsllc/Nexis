#include "info_manager.h"
#include <QStandardPaths>

#ifdef Q_OS_MACOS
#include <Info/cpu_info_macos.h>
#include <Info/disk_info_macos.h>
#include <Info/memory_info_macos.h>
#include <Info/network_info_macos.h>
#include <Info/system_info_macos.h>
#include <Info/process_info_macos.h>
#include <Info/thermal_info_macos.h>
#include <Info/fan_info_macos.h>
#include <Info/gpu_info_macos.h>
#include <Info/battery_info_macos.h>
#include <Info/disk_health_info_macos.h>
#include <Info/update_info_macos.h>
#include <Info/power_profile_info_macos.h>
#else
#include <Info/cpu_info_linux.h>
#include <Info/disk_info_linux.h>
#include <Info/memory_info_linux.h>
#include <Info/network_info_linux.h>
#include <Info/system_info_linux.h>
#include <Info/process_info_linux.h>
#include <Info/thermal_info_linux.h>
#include <Info/fan_info_linux.h>
#include <Info/gpu_info_linux.h>
#include <Info/battery_info_linux.h>
#include <Info/disk_health_info_linux.h>
#include <Info/update_info_linux.h>
#include <Info/power_profile_info_linux.h>
#endif

InfoManager *InfoManager::instance = nullptr;

InfoManager::InfoManager()
{
#ifdef Q_OS_MACOS
    ci  = std::make_unique<CpuInfoMacOS>();
    di  = std::make_unique<DiskInfoMacOS>();
    mi  = std::make_unique<MemoryInfoMacOS>();
    ni  = std::make_unique<NetworkInfoMacOS>();
    si  = std::make_unique<SystemInfoMacOS>();
    pi  = std::make_unique<ProcessInfoMacOS>();
    ti  = std::make_unique<ThermalInfoMacOS>();
    fi  = std::make_unique<FanInfoMacOS>();
    gi  = std::make_unique<GpuInfoMacOS>();
    bi  = std::make_unique<BatteryInfoMacOS>();
    dhi = std::make_unique<DiskHealthInfoMacOS>();
    upd = std::make_unique<UpdateInfoMacOS>();
    ppi = std::make_unique<PowerProfileInfoMacOS>();
#else
    ci  = std::make_unique<CpuInfoLinux>();
    di  = std::make_unique<DiskInfoLinux>();
    mi  = std::make_unique<MemoryInfoLinux>();
    ni  = std::make_unique<NetworkInfoLinux>();
    si  = std::make_unique<SystemInfoLinux>();
    pi  = std::make_unique<ProcessInfoLinux>();
    ti  = std::make_unique<ThermalInfoLinux>();
    fi  = std::make_unique<FanInfoLinux>();
    gi  = std::make_unique<GpuInfoLinux>();
    bi  = std::make_unique<BatteryInfoLinux>();
    dhi = std::make_unique<DiskHealthInfoLinux>();
    upd = std::make_unique<UpdateInfoLinux>();
    ppi = std::make_unique<PowerProfileInfoLinux>();
#endif
}

InfoManager *InfoManager::ins()
{
    if(! instance){
        instance = new InfoManager;
    }

    return instance;
}

QString InfoManager::getUserName() const
{
    return si->getUsername();
}

QStringList InfoManager::getUserList() const
{
    return si->getUserList();
}

QStringList InfoManager::getGroupList() const
{
    return si->getGroupList();
}

/*
 * CPU Provider
 */
int InfoManager::getCpuCoreCount() const
{
    return ci->getCpuCoreCount();
}

QList<int> InfoManager::getCpuPercents() const
{
    return ci->getCpuPercents();
}

QList<double> InfoManager::getCpuLoadAvgs() const
{
    return ci->getLoadAvgs();
}

double InfoManager::getCpuClock() const
{
    return ci->getAvgClock();
}

/*
 * Memory Provider
 */
void InfoManager::updateMemoryInfo()
{
    mi->updateMemoryInfo();
}

quint64 InfoManager::getSwapUsed() const
{
    return mi->getSwapUsed();
}

quint64 InfoManager::getSwapTotal() const
{
    return mi->getSwapTotal();
}

quint64 InfoManager::getMemUsed() const
{
    return mi->getMemUsed();
}

quint64 InfoManager::getMemTotal() const
{
    return mi->getMemTotal();
}

quint64 InfoManager::getMemWired() const
{
    return mi->getMemWired();
}

quint64 InfoManager::getMemActive() const
{
    return mi->getMemActive();
}

quint64 InfoManager::getMemInactive() const
{
    return mi->getMemInactive();
}

quint64 InfoManager::getMemCompressed() const
{
    return mi->getMemCompressed();
}

quint64 InfoManager::getMemAvailable() const
{
    return mi->getMemAvailable();
}

int InfoManager::getMemPressureLevel() const
{
    return mi->getPressureLevel();
}

/*
 * Disk Provider
 */
QList<Disk> InfoManager::getDisks() const
{
    return di->getDisks();
}

void InfoManager::updateDiskInfo()
{
    di->updateDiskInfo();
}

QList<Disk> InfoManager::collectDiskInfo() const
{
    return di->collectDiskInfo();
}

void InfoManager::setDisks(QList<Disk> disks)
{
    di->setDisks(std::move(disks));
}

QList<quint64> InfoManager::getDiskIO()
{
    return di->getDiskIO();
}

QList<QString> InfoManager::getFileSystemTypes()
{
    return di->fileSystemTypes();
}

QList<QString> InfoManager::getDevices()
{
    return di->devices();
}

/********************
 * Network Provider
 *******************/
void InfoManager::updateNetworkBytes()
{
    ni->updateNetworkBytes();
}

quint64 InfoManager::getRXbytes() const
{
    return ni->getRXbytes();
}

quint64 InfoManager::getTXbytes() const
{
    return ni->getTXbytes();
}

QString InfoManager::getDefaultNetworkInterface() const
{
    return ni->getDefaultNetworkInterface();
}

/********************
 * System Provider
 *******************/
QFileInfoList InfoManager::getCrashReports() const
{
    return si->getCrashReports();
}

QFileInfoList InfoManager::getAppLogs() const
{
    return si->getAppLogs();
}

QFileInfoList InfoManager::getAppCaches() const
{
    return si->getAppCaches();
}

QFileInfoList InfoManager::getDevToolCaches() const
{
    return si->getDevToolCaches();
}

QFileInfoList InfoManager::getBrokenSymlinks() const
{
    return si->getBrokenSymlinks();
}

QFileInfoList InfoManager::getBrowserPrivacyArtifacts() const
{
    return si->getBrowserPrivacyArtifacts();
}

/********************
 * Process Provider
 *******************/
void InfoManager::updateProcesses()
{
    pi->updateProcesses();
}

QList<Process> InfoManager::getProcesses() const
{
    return pi->getProcessList();
}

void InfoManager::setCollectProcessDiskIO(bool enabled)
{
    pi->setCollectDiskIO(enabled);
}

void InfoManager::setCollectProcessNetIO(bool enabled)
{
    pi->setCollectNetIO(enabled);
}

/********************
 * Thermal Provider
 *******************/
QList<ThermalSensor> InfoManager::getThermalSensors() const
{
    return ti->getSensors();
}

double InfoManager::getThermalTemperature(int index) const
{
    return ti->getTemperature(index);
}

bool InfoManager::hasThermalSensors() const
{
    return ti->hasSensors();
}

/********************
 * Fan Provider
 *******************/
QList<FanSensor> InfoManager::getFanSensors() const
{
    return fi->getSensors();
}

int InfoManager::getFanSpeed(int index) const
{
    return fi->getFanSpeed(index);
}

bool InfoManager::hasFanSensors() const
{
    return fi->hasSensors();
}

/********************
 * GPU Provider
 *******************/
QList<GpuDevice> InfoManager::getGpuDevices() const
{
    return gi->getGpuDevices();
}

void InfoManager::updateGpuInfo()
{
    gi->updateGpuInfo();
}

bool InfoManager::hasGpu() const
{
    return gi->hasGpu();
}

QString InfoManager::getGpuDiagnosticReport() const
{
    return gi->getDiagnosticReport();
}

/********************
 * Battery Provider
 *******************/
BatteryData InfoManager::getBatteryData() const
{
    return bi->getBatteryData();
}

void InfoManager::updateBatteryInfo()
{
    bi->updateBatteryInfo();
}

bool InfoManager::hasBattery() const
{
    return bi->hasBattery();
}

/********************
 * Disk Health Provider
 *******************/
QList<DriveHealth> InfoManager::getDriveHealth() const
{
    return dhi->getDrives();
}

void InfoManager::discoverDiskHealth()
{
    dhi->discoverDrives();
}

void InfoManager::refreshDiskHealth()
{
    dhi->refreshHealth();
}

void InfoManager::refreshDiskHealthElevated(const QString &device)
{
    dhi->refreshHealthElevated(device);
}

void InfoManager::refreshDiskHealthElevatedBatch(const QStringList &devices, bool applySetcap)
{
    QString smartctlPath;
    if (applySetcap)
        smartctlPath = QStandardPaths::findExecutable("smartctl");
    dhi->refreshHealthElevatedBatch(devices, applySetcap, smartctlPath);
}

bool InfoManager::hasDiskHealth() const
{
    return dhi->hasDrives();
}

bool InfoManager::hasSmartctl() const
{
    return dhi->hasSmartctl();
}

/********************
 * Update Provider
 *******************/
UpdateCheckResult InfoManager::checkForSystemUpdates()
{
    return upd->checkForUpdates();
}

QStringList InfoManager::updateSources() const
{
    return upd->availableSources();
}

bool InfoManager::hasUpdateSources() const
{
    return !upd->availableSources().isEmpty();
}

/********************
 * Power Profile Provider
 *******************/
PowerProfileData InfoManager::getPowerProfileData() const
{
    return ppi->getData();
}

bool InfoManager::setPowerProfile(const QString &profile)
{
    return ppi->setProfile(profile);
}

bool InfoManager::hasPowerProfiles() const
{
    return ppi->hasProfiles();
}

void InfoManager::refreshPowerProfile()
{
    ppi->refresh();
}
