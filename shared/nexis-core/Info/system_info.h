#ifndef SYSTEMINFO_H
#define SYSTEMINFO_H

#include "Utils/file_util.h"
#include "Utils/format_util.h"
#include "Utils/command_util.h"

#include "nexis-core_global.h"

class NEXISCORESHARED_EXPORT SystemInfo
{
public:
    virtual ~SystemInfo() = default;

    QString getHostname() const;
    QString getPlatform() const;
    QString getDistribution() const;
    QString getKernel() const;
    QString getCpuModel() const;
    QString getCpuSpeed() const;
    QString getCpuCore() const;
    QString getUsername() const;

    virtual QFileInfoList getCrashReports() const = 0;
    virtual QFileInfoList getAppLogs() const = 0;
    virtual QFileInfoList getAppCaches() const = 0;
    virtual QFileInfoList getDevToolCaches() const = 0;
    virtual QFileInfoList getBrokenSymlinks() const = 0;

    virtual QStringList getUserList() const = 0;
    virtual QStringList getGroupList() const = 0;

protected:
    QString cpuCore;
    QString cpuModel;
    QString cpuSpeed;
    QString username;
};

#endif // SYSTEMINFO_H
