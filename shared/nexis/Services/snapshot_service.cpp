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
    try {
        const QString output = CommandUtil::exec("tmutil", {"localsnapshot"}, {}, 60000).trimmed();
        qInfo() << "SnapshotService (macOS):" << output;
        return !output.isEmpty();
    } catch (const QString &err) {
        qWarning() << "SnapshotService: tmutil failed:" << err;
        return false;
    }
#else
    // Timeshift needs root. sudoExec uses pkexec so the user gets a GUI
    // prompt. The --comments flag is annotation only; --create is the action.
    const QString result = CommandUtil::sudoExec(
        "timeshift",
        {"--create", "--comments", reason});
    if (result.isEmpty()) {
        qWarning() << "SnapshotService: timeshift --create returned empty — snapshot may have failed";
        return false;
    }
    qInfo() << "SnapshotService (Linux):" << result;
    return true;
#endif
}
