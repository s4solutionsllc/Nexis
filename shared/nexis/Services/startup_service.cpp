#include "startup_service.h"

#include <QDir>

#ifdef Q_OS_MACOS
#include "startup_info_macos.h"
#include <Utils/command_util.h>
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

#ifdef Q_OS_MACOS
    // Watch system-level directories too (read-only items)
    const QStringList sysDirs = {
        QStringLiteral("/Library/LaunchAgents"),
        QStringLiteral("/Library/LaunchDaemons")
    };
    for (const QString &d : sysDirs) {
        if (QDir(d).exists())
            mWatcher.addPath(d);
    }
#endif

    connect(&mWatcher, &QFileSystemWatcher::directoryChanged,
            this, &StartupService::appsChanged);
}

QList<StartupAppData> StartupService::getApps() const
{
    return mInfo->getStartupApps();
}

QList<StartupAppData> StartupService::getAllLoginItems() const
{
    return mInfo->getAllLoginItems();
}

QString StartupService::autostartPath() const
{
    return mInfo->autostartPath();
}

bool StartupService::isAutostartDisabled() const
{
    return mInfo->isAutostartDisabled();
}

#ifdef Q_OS_MACOS
QList<BtmRecord> StartupService::getBtmRecords(QString *error) const
{
    auto *macInfo = static_cast<StartupInfoMacOS*>(mInfo.get());
    return macInfo->getBtmRecords(error);
}

bool StartupService::resetBtm(QString *error)
{
    // resetbtm needs root and runs to completion in ~1s, but the AppleScript
    // prompt may take much longer — give it 5 minutes per WI-21 / SSO-3383.
    ExecResult res = CommandUtil::sudoExecWithStatus(
        QStringLiteral("sfltool"), {QStringLiteral("resetbtm")}, QByteArray(),
        5 * 60 * 1000);

    if (!res.ok()) {
        if (error) {
            *error = res.error.isEmpty()
                ? tr("sfltool resetbtm failed (exit %1).").arg(res.exitCode)
                : res.error;
        }
        return false;
    }

    emit appsChanged();
    return true;
}
#endif
