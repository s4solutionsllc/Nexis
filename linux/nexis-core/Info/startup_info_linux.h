#ifndef STARTUP_INFO_LINUX_H
#define STARTUP_INFO_LINUX_H

#include <Info/startup_info.h>

class StartupInfoLinux : public StartupInfo
{
public:
    QList<StartupAppData> getStartupApps() const override;
    QString autostartPath() const override;
    bool isAutostartDisabled() const override;
};

#endif // STARTUP_INFO_LINUX_H
