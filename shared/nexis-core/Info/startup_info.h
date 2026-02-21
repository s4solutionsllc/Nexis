#ifndef STARTUP_INFO_H
#define STARTUP_INFO_H

#include <QString>
#include <QList>

struct StartupAppData {
    QString name;
    QString filePath;
    QString iconPath;
    bool enabled = true;
};

class StartupInfo
{
public:
    virtual ~StartupInfo() = default;

    virtual QList<StartupAppData> getStartupApps() const = 0;
    virtual QString autostartPath() const = 0;
    virtual bool isAutostartDisabled() const = 0;
};

#endif // STARTUP_INFO_H
