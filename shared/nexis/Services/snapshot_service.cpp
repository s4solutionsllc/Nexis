#include "snapshot_service.h"

#include "Utils/command_util.h"

#include <QDebug>

SnapshotService *SnapshotService::instance = nullptr;

SnapshotService *SnapshotService::ins()
{
    if (!instance)
        instance = new SnapshotService;
    return instance;
}

bool SnapshotService::isAvailable() const
{
#ifdef Q_OS_MACOS
    return CommandUtil::isExecutable("tmutil");
#else
    return CommandUtil::isExecutable("timeshift");
#endif
}

QString SnapshotService::toolDisplayName() const
{
    if (!isAvailable())
        return QString();
#ifdef Q_OS_MACOS
    return QStringLiteral("APFS snapshot");
#else
    return QStringLiteral("Timeshift");
#endif
}

bool SnapshotService::takeSnapshot(const QString &reason)
{
    if (!isAvailable()) {
        qWarning() << "SnapshotService: tool unavailable — skipping snapshot";
        return false;
    }

#ifdef Q_OS_MACOS
    // tmutil localsnapshot doesn't require elevation for user snapshots on
    // APFS volumes. Output looks like "Created local snapshot with date:
    // 2026-04-21-143012".
    Q_UNUSED(reason)
    const ExecResult result = CommandUtil::execWithStatus("tmutil", {"localsnapshot"}, {}, 60000);
    if (!result.ok()) {
        qWarning() << "SnapshotService: tmutil failed:" << result.error;
        return false;
    }
    qInfo() << "SnapshotService (macOS):" << result.output.trimmed();
    return !result.output.trimmed().isEmpty();
#else
    // Timeshift needs root. sudoExecWithStatus uses pkexec so the user gets a
    // GUI prompt. The --comments flag is annotation only; --create is the
    // action. ok() is authoritative for whether the snapshot was created.
    const ExecResult result = CommandUtil::sudoExecWithStatus(
        "timeshift",
        {"--create", "--comments", reason});
    if (!result.ok()) {
        qWarning() << "SnapshotService: timeshift --create failed:" << result.error;
        return false;
    }
    qInfo() << "SnapshotService (Linux):" << result.output;
    return true;
#endif
}
