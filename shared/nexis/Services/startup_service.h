#ifndef STARTUP_SERVICE_H
#define STARTUP_SERVICE_H

#include <QObject>
#include <QFileSystemWatcher>
#include <memory>

#include <Info/startup_info.h>

class StartupService : public QObject
{
    Q_OBJECT

public:
    static StartupService *ins();

    QList<StartupAppData> getApps() const;
    QList<StartupAppData> getAllLoginItems() const;
    QString autostartPath() const;
    bool isAutostartDisabled() const;

signals:
    void appsChanged();

private:
    explicit StartupService(QObject *parent = nullptr);
    static StartupService *instance;

    std::unique_ptr<StartupInfo> mInfo;
    QFileSystemWatcher mWatcher;
};

#endif // STARTUP_SERVICE_H
