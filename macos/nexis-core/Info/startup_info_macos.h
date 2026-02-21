#ifndef STARTUP_INFO_MACOS_H
#define STARTUP_INFO_MACOS_H

#include <Info/startup_info.h>

class StartupInfoMacOS : public StartupInfo
{
public:
    QList<StartupAppData> getStartupApps() const override;
    QString autostartPath() const override;
    bool isAutostartDisabled() const override;
};

#endif // STARTUP_INFO_MACOS_H
