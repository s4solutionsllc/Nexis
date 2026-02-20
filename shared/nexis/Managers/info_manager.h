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
#include <Info/gpu_info.h>
#include <Info/battery_info.h>
#include <Info/disk_health_info.h>

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
    void updateMemoryInfo();

    quint64 getRXbytes() const;
    quint64 getTXbytes() const;

    QList<Disk> getDisks() const;
    QList<quint64> getDiskIO();
    void updateDiskInfo();

    QFileInfoList getCrashReports() const;
    QFileInfoList getAppLogs() const;
    QFileInfoList getAppCaches() const;
    QFileInfoList getDevToolCaches() const;

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
    std::unique_ptr<GpuInfo> gi;
    std::unique_ptr<BatteryInfo> bi;
    std::unique_ptr<DiskHealthInfo> dhi;
};

#endif // INFO_MANAGER_H
