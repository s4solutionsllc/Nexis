#ifndef SYSTEM_INFO_LINUX_H
#define SYSTEM_INFO_LINUX_H

#include <Info/system_info.h>

class SystemInfoLinux : public SystemInfo
{
public:
    SystemInfoLinux();

    QFileInfoList getCrashReports() const override;
    QFileInfoList getAppLogs() const override;
    QFileInfoList getAppCaches() const override;
    QFileInfoList getDevToolCaches() const override;
    QFileInfoList getBrokenSymlinks() const override;

    QStringList getUserList() const override;
    QStringList getGroupList() const override;
};

#endif // SYSTEM_INFO_LINUX_H
