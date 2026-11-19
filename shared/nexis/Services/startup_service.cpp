#include "startup_service.h"

#include <QDir>
#include <QStandardPaths>
#include <Utils/command_util.h>

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
    , mPersistence(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
                   + QStringLiteral("/persistence_baseline.json"))
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

QList<StartupAppData> StartupService::getCronJobs() const
{
    QList<StartupAppData> jobs;
    if (!CommandUtil::isExecutable(QStringLiteral("crontab")))
        return jobs;

    // Exit 1 with "no crontab for <user>" is the normal empty case.
    const ExecResult res = CommandUtil::execWithStatus(
        QStringLiteral("crontab"), {QStringLiteral("-l")}, 5000);
    if (!res.ok())
        return jobs;

    for (const CronEntry &cron : PersistenceWatch::parseCrontab(res.output))
        jobs << PersistenceWatch::itemForCron(cron);
    return jobs;
}

static QList<PersistenceEntry> persistenceEntries(const QList<StartupAppData> &items)
{
    QList<PersistenceEntry> entries;
    entries.reserve(items.size());
    for (const StartupAppData &d : items)
        entries << PersistenceWatch::entryFor(d);
    return entries;
}

PersistenceDiff StartupService::evaluatePersistence(const QList<StartupAppData> &items)
{
    return mPersistence.evaluate(persistenceEntries(items)).diff;
}

void StartupService::acknowledgePersistence(const QList<StartupAppData> &items,
                                            const QSet<QString> &keys)
{
    mPersistence.acknowledge(persistenceEntries(items), keys);
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
