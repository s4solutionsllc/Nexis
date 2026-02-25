#ifndef SYSTEM_INFO_MACOS_H
#define SYSTEM_INFO_MACOS_H

#include <Info/system_info.h>

class SystemInfoMacOS : public SystemInfo
{
public:
    SystemInfoMacOS();

    QFileInfoList getCrashReports() const override;
    QFileInfoList getAppLogs() const override;
    QFileInfoList getAppCaches() const override;
    QFileInfoList getDevToolCaches() const override;
    QFileInfoList getBrokenSymlinks() const override;
    QFileInfoList getBrowserPrivacyArtifacts() const override;

    QStringList getUserList() const override;
    QStringList getGroupList() const override;
};

#endif // SYSTEM_INFO_MACOS_H
