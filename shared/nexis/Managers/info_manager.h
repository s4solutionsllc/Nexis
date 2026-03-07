#ifndef INFO_MANAGER_H
#define INFO_MANAGER_H

#include <QObject>
#include <memory>

#include <Info/cpu_info.h>
#include <Info/disk_info.h>
#include <Info/memory_info.h>
#include <Info/network_info.h>
#include <Info/system_info.h>
#include <Info/process_info.h>
#include <Info/thermal_info.h>
#include <Info/fan_info.h>
#include <Info/gpu_info.h>
#include <Info/battery_info.h>
#include <Info/disk_health_info.h>
#include <Info/update_info.h>
#include <Info/power_profile_info.h>

class InfoManager
{
public:
    static InfoManager *ins();

    int getCpuCoreCount() const;
    QList<int> getCpuPercents() const;
    QList<double> getCpuLoadAvgs() const;
    double getCpuClock() const;

    quint64 getSwapUsed() const;
    quint64 getSwapTotal() const;
    quint64 getMemUsed() const;
    quint64 getMemTotal() const;
    quint64 getMemWired() const;
    quint64 getMemActive() const;
    quint64 getMemInactive() const;
    quint64 getMemCompressed() const;
    quint64 getMemAvailable() const;
    int getMemPressureLevel() const;
    void updateMemoryInfo();

    void updateNetworkBytes();
    quint64 getRXbytes() const;
    quint64 getTXbytes() const;
    QString getDefaultNetworkInterface() const;

    QList<Disk> getDisks() const;
    QList<quint64> getDiskIO();
    void updateDiskInfo();

    QFileInfoList getCrashReports() const;
    QFileInfoList getAppLogs() const;
    QFileInfoList getAppCaches() const;
    QFileInfoList getDevToolCaches() const;
    QFileInfoList getBrokenSymlinks() const;
    QFileInfoList getBrowserPrivacyArtifacts() const;

    void updateProcesses();
    QList<Process> getProcesses() const;
    QString getUserName() const;
    QStringList getUserList() const;
    QStringList getGroupList() const;

    QList<QString> getDevices();
    QList<QString> getFileSystemTypes();

    QList<ThermalSensor> getThermalSensors() const;
    double getThermalTemperature(int index) const;
    bool hasThermalSensors() const;

    QList<FanSensor> getFanSensors() const;
    int getFanSpeed(int index) const;
    bool hasFanSensors() const;

    QList<GpuDevice> getGpuDevices() const;
    void updateGpuInfo();
    bool hasGpu() const;

    BatteryData getBatteryData() const;
    void updateBatteryInfo();
    bool hasBattery() const;

    QList<DriveHealth> getDriveHealth() const;
    void refreshDiskHealth();
    void refreshDiskHealthElevated(const QString &device);
    bool hasDiskHealth() const;
    bool hasSmartctl() const;

    UpdateCheckResult checkForSystemUpdates();
    QStringList updateSources() const;
    bool hasUpdateSources() const;

    PowerProfileData getPowerProfileData() const;
    bool setPowerProfile(const QString &profile);
    bool hasPowerProfiles() const;
    void refreshPowerProfile();

private:
    InfoManager();

    static InfoManager *instance;

    std::unique_ptr<CpuInfo> ci;
    std::unique_ptr<DiskInfo> di;
    std::unique_ptr<MemoryInfo> mi;
    std::unique_ptr<NetworkInfo> ni;
    std::unique_ptr<SystemInfo> si;
    std::unique_ptr<ProcessInfo> pi;
    std::unique_ptr<ThermalInfo> ti;
    std::unique_ptr<FanInfo> fi;
    std::unique_ptr<GpuInfo> gi;
    std::unique_ptr<BatteryInfo> bi;
    std::unique_ptr<DiskHealthInfo> dhi;
    std::unique_ptr<UpdateInfo> upd;
    std::unique_ptr<PowerProfileInfo> ppi;
};

#endif // INFO_MANAGER_H
