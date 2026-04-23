#ifndef STARTUP_INFO_MACOS_H
#define STARTUP_INFO_MACOS_H

#include <Info/startup_info.h>

class StartupInfoMacOS : public StartupInfo
{
public:
    QList<StartupAppData> getStartupApps() const override;
    QList<StartupAppData> getAllLoginItems() const override;
    QString autostartPath() const override;
    bool isAutostartDisabled() const override;

private:
    QList<StartupAppData> loadPlistDir(const QString &dirPath,
                                       LoginItemCategory category,
                                       bool readOnly) const;
    QSet<QString> queryLaunchctlDisabled() const;
};

#endif // STARTUP_INFO_MACOS_H
