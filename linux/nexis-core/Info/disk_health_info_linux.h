#ifndef DISK_HEALTH_INFO_LINUX_H
#define DISK_HEALTH_INFO_LINUX_H

#include <Info/disk_health_info.h>

class DiskHealthInfoLinux : public DiskHealthInfo
{
public:
    DiskHealthInfoLinux();

    void refreshHealth() override;
    void refreshHealthElevated(const QString &device) override;
    void refreshHealthElevatedBatch(const QStringList &devices,
                                     bool applySetcap,
                                     const QString &smartctlPath) override;

protected:
    void discoverDrives() override;
};

#endif // DISK_HEALTH_INFO_LINUX_H
