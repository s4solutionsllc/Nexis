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
#include <Info/boot_analysis_info.h>
#include <Info/startup_info.h>
#ifdef Q_OS_LINUX
#include <Info/psi_info.h>
#include <Info/oomd_snapshot.h>
class OomdInfoLinux;
#endif

class InfoManager
{
public:
    static InfoManager *ins();

    // System / CPU descriptive strings (facade over SystemInfo / CpuInfo).
    // Added in WI-27 so pages don't need to stack-construct platform subclasses.
    QString getHostname() const;
    QString getPlatform() const;
    QString getDistribution() const;
    QString getKernel() const;
    QString getCpuModel() const;
    QString getCpuSpeed() const;
    QString getCpuCoreLabel() const;
    int getCpuPhysicalCoreCount() const;

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
    QStringList getNetworkInterfaceNames() const;
    QString getNetworkInterfaceDisplayName(const QString &name) const;
    NetInterfaceStatsMap getInterfaceStats() const;

    QList<Disk> getDisks() const;
    QList<quint64> getDiskIO();
    void updateDiskInfo();

    // FR-101: thread-safe pair. collectDiskInfo() walks QStorageInfo without
    // mutating the cache (safe to call from a worker); setDisks() assigns
    // the result (must be called on the UI thread).
    QList<Disk> collectDiskInfo() const;
    void setDisks(QList<Disk> disks);

    QFileInfoList getCrashReports() const;
    QFileInfoList getAppLogs() const;
    QFileInfoList getAppCaches() const;
    QFileInfoList getDevToolCaches() const;
    QFileInfoList getBrokenSymlinks() const;
    QFileInfoList getBrowserPrivacyArtifacts() const;

    void updateProcesses();
    // WI-21: thread-safe publish pair, mirrors collectDiskInfo()/setDisks()
    // and collectDriveHealth()/setDriveHealth() (WI-03). collectProcesses()
    // runs the per-PID walk + any forks (`ps` on macOS) into a local list,
    // safe to call from a QtConcurrent worker; setProcessList() assigns the
    // cache on the UI thread (DataRefreshService hops via invokeMethod).
    QList<Process> collectProcesses();
    void setProcessList(QList<Process> processes);
    QList<Process> getProcesses() const;
    QString getUserName() const;
    QStringList getUserList() const;
    QStringList getGroupList() const;

    // FR-108 / FR-115: toggle per-PID I/O and GPU collection based on
    // Processes-page column visibility. Skips /proc/<pid>/io reads (Linux),
    // the nettop fork (macOS), and /proc/<pid>/fdinfo + nvidia-smi pmon
    // (Linux) when the matching columns are all hidden.
    void setCollectProcessDiskIO(bool enabled);
    void setCollectProcessNetIO(bool enabled);
    void setCollectProcessGpu(bool enabled);

    // SSO-15379: lets ProcessesPage distinguish "no network activity" from
    // "collection isn't actually working" (missing CAP_BPF/root, no eBPF
    // support at build time, nethogs not installed, ...).
    ProcessInfo::NetIoStatus getProcessNetIoStatus() const;
    QString getProcessNetIoStatusDetail() const;

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
    QString getGpuDiagnosticReport() const;

    BatteryData getBatteryData() const;
    BatteryData getBatteryData(int index) const;
    int batteryCount() const;
    void updateBatteryInfo();
    bool hasBattery() const;

    QList<DriveHealth> getDriveHealth() const;
    // WI-03: thread-safe publish pair, mirrors collectDiskInfo()/setDisks().
    // collectDriveHealth() runs the smartctl fork into a local list (safe to
    // call from a QtConcurrent worker); setDriveHealth() assigns the cache and
    // must be called on the UI thread.
    QList<DriveHealth> collectDriveHealth();
    void setDriveHealth(QList<DriveHealth> drives);
    void refreshDiskHealthElevated(const QString &device);
    void refreshDiskHealthElevatedBatch(const QStringList &devices, bool applySetcap);
    bool hasDiskHealth() const;
    bool hasSmartctl() const;

    UpdateCheckResult checkForSystemUpdates();
    QStringList updateSources() const;
    bool hasUpdateSources() const;

    PowerProfileData getPowerProfileData() const;
    bool setPowerProfile(const QString &profile);
    bool hasPowerProfiles() const;
    void refreshPowerProfile();

    // Boot analysis / startup item providers — owned platform-specific
    // subclasses. Pages and services should use these instead of
    // stack-constructing `*Linux` / `*MacOS` subclasses directly.
    BootAnalysisInfo *bootAnalysisInfo() const;
    StartupInfo      *startupInfo() const;

#ifdef Q_OS_LINUX
    void updateCpuPsi();
    PsiSnapshot getCpuPsi() const;

    // FW-11 (SSO-3739): systemd-oomd / cgroup v2 observability.
    void updateOomdInfo();
    OomdSnapshot getOomdSnapshot() const;
    bool hasOomd() const;
#endif

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
    std::unique_ptr<BootAnalysisInfo> bai;
    std::unique_ptr<StartupInfo> sui;
#ifdef Q_OS_LINUX
    std::unique_ptr<PsiInfo> psii;
    std::unique_ptr<OomdInfoLinux> oomd;
#endif
};

#endif // INFO_MANAGER_H
