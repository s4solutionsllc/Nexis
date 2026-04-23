#ifndef STARTUP_INFO_H
#define STARTUP_INFO_H

#include <QString>
#include <QList>

enum class LoginItemCategory { UserAgent, SystemAgent, SystemDaemon };

struct StartupAppData {
    QString name;
    QString filePath;
    QString iconPath;
    QString identifier;
    bool enabled = true;
    bool readOnly = false;
    LoginItemCategory category = LoginItemCategory::UserAgent;
};

class StartupInfo
{
public:
    virtual ~StartupInfo() = default;

    virtual QList<StartupAppData> getStartupApps() const = 0;
    virtual QList<StartupAppData> getAllLoginItems() const { return getStartupApps(); }
    virtual QString autostartPath() const = 0;
    virtual bool isAutostartDisabled() const = 0;
};

#endif // STARTUP_INFO_H
