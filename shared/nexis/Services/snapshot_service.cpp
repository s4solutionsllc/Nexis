#include "snapshot_service.h"

#include "Utils/command_util.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QStorageInfo>

SnapshotService *SnapshotService::instance = nullptr;

SnapshotService *SnapshotService::ins()
{
    if (!instance)
        instance = new SnapshotService;
    return instance;
}

SnapshotService::Backend SnapshotService::chooseLinuxBackend(const Probe &probe)
{
    if (probe.timeshift)
        return Backend::Timeshift;
    if (probe.snapper && probe.snapperRootConfig)
        return Backend::Snapper;
    if (probe.btrfs && probe.rootFsType == QLatin1String("btrfs"))
        return Backend::Btrfs;
    return Backend::None;
}

SnapshotService::Probe SnapshotService::probeSystem()
{
    Probe probe;
    probe.timeshift = CommandUtil::isExecutable("timeshift");
    probe.snapper = CommandUtil::isExecutable("snapper");
    probe.snapperRootConfig = QFile::exists(QStringLiteral("/etc/snapper/configs/root"));
    probe.btrfs = CommandUtil::isExecutable("btrfs");
    probe.rootFsType = QString::fromLatin1(QStorageInfo::root().fileSystemType());
    return probe;
}

QString SnapshotService::btrfsSnapshotDir()
{
    return QStringLiteral("/.nexis-snapshots");
}

SnapshotService::Command SnapshotService::buildCommand(Backend backend, const QString &reason,
                                                       const QString &timestamp)
{
    switch (backend) {
    case Backend::Timeshift:
        // --comments is annotation only; --create is the action.
        return {QStringLiteral("timeshift"), {"--create", "--comments", reason}};
    case Backend::Snapper:
        return {QStringLiteral("snapper"),
                {"-c", "root", "create", "--type", "single", "--cleanup-algorithm", "number",
                 "--description", reason}};
    case Backend::Btrfs:
        // One elevated call: the snapshot directory must exist on the same
        // filesystem as the source. Paths travel as positional parameters so
        // nothing is interpolated into the shell script.
        return {QStringLiteral("sh"),
                {"-c", "mkdir -p \"$1\" && btrfs subvolume snapshot -r / \"$1/$2\"", "sh",
                 btrfsSnapshotDir(), QStringLiteral("nexis-pre-clean-") + timestamp}};
    case Backend::ApfsLocal:
        return {QStringLiteral("tmutil"), {"localsnapshot"}};
    case Backend::None:
        break;
    }
    return {};
}

QString SnapshotService::backendDisplayName(Backend backend)
{
    switch (backend) {
    case Backend::Timeshift: return QStringLiteral("Timeshift");
    case Backend::Snapper:   return QStringLiteral("snapper");
    case Backend::Btrfs:     return QStringLiteral("btrfs snapshot");
    case Backend::ApfsLocal: return QStringLiteral("APFS snapshot");
    case Backend::None:      break;
    }
    return QString();
}

SnapshotService::Backend SnapshotService::backend() const
{
#ifdef Q_OS_MACOS
    return CommandUtil::isExecutable("tmutil") ? Backend::ApfsLocal : Backend::None;
#else
    return chooseLinuxBackend(probeSystem());
#endif
}

bool SnapshotService::isAvailable() const
{
    return backend() != Backend::None;
}

QString SnapshotService::toolDisplayName() const
{
    return backendDisplayName(backend());
}

QString SnapshotService::unavailableMessage() const
{
#ifdef Q_OS_MACOS
    return tr("Restore point skipped: tmutil was not found. The clean continued without one.");
#else
    return tr("Restore point skipped: no snapshot tool found (Timeshift, snapper, or a btrfs root). "
              "The clean continued without one.");
#endif
}

bool SnapshotService::takeSnapshot(const QString &reason)
{
    const Backend be = backend();
    if (be == Backend::None) {
        qWarning() << "SnapshotService: no snapshot backend available — skipping snapshot";
        return false;
    }

    const Command cmd = buildCommand(
        be, reason, QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));

#ifdef Q_OS_MACOS
    // tmutil localsnapshot doesn't require elevation for user snapshots on
    // APFS volumes. Output looks like "Created local snapshot with date:
    // 2026-04-21-143012".
    const ExecResult result = CommandUtil::execWithStatus(cmd.program, cmd.args, {}, 60000);
    if (!result.ok()) {
        qWarning() << "SnapshotService: tmutil failed:" << result.error;
        return false;
    }
    qInfo() << "SnapshotService (macOS):" << result.output.trimmed();
    return !result.output.trimmed().isEmpty();
#else
    // All Linux backends need root. sudoExecWithStatus uses pkexec so the user
    // gets a GUI prompt; ok() is authoritative for whether the snapshot exists.
    const ExecResult result = CommandUtil::sudoExecWithStatus(cmd.program, cmd.args);
    if (!result.ok()) {
        qWarning() << "SnapshotService:" << backendDisplayName(be) << "failed:" << result.error;
        return false;
    }
    qInfo() << "SnapshotService (Linux," << backendDisplayName(be) << "):" << result.output;
    return true;
#endif
}
