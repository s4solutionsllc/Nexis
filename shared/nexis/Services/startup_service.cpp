#include "startup_service.h"

#include <QDir>

#ifdef Q_OS_MACOS
#include "startup_info_macos.h"
#else
#include "startup_info_linux.h"
#endif

StartupService *StartupService::instance = nullptr;

StartupService *StartupService::ins()
{
    if (!instance)
        instance = new StartupService;
    return instance;
}

StartupService::StartupService(QObject *parent)
    : QObject(parent)
{
#ifdef Q_OS_MACOS
    mInfo = std::make_unique<StartupInfoMacOS>();
#else
    mInfo = std::make_unique<StartupInfoLinux>();
#endif

    QString path = mInfo->autostartPath();

    // Ensure the autostart directory exists
    if (!QDir(path).exists()) {
        QDir().mkdir(path);
    }

    mWatcher.addPath(path);
    connect(&mWatcher, &QFileSystemWatcher::directoryChanged,
            this, &StartupService::appsChanged);
}

QList<StartupAppData> StartupService::getApps() const
{
    return mInfo->getStartupApps();
}

QString StartupService::autostartPath() const
{
    return mInfo->autostartPath();
}

bool StartupService::isAutostartDisabled() const
{
    return mInfo->isAutostartDisabled();
}
