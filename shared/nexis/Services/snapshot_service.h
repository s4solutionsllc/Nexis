#ifndef SNAPSHOT_SERVICE_H
#define SNAPSHOT_SERVICE_H

#include <QObject>
#include <QString>
#include <QStringList>

// FR-112: create a system snapshot before risky cleaning operations.
// Linux: Timeshift, else snapper (root config), else a read-only btrfs
//        snapshot of / — each needs root elevation via pkexec/sudoExec.
//        Nexis only uses what is already installed; it never installs a tool.
// macOS: APFS local snapshot via `tmutil localsnapshot` (user-level, no elevation).
//
// The SettingKeys::PreCleanSnapshotEnabled toggle gates whether callers
// actually invoke takeSnapshot(); this service itself is silent if the
// tool is not present on the system.
class SnapshotService : public QObject
{
    Q_OBJECT

public:
    enum class Backend { None, Timeshift, Snapper, Btrfs, ApfsLocal };

    // What the host offers; filled by probeSystem(), constructed directly by tests.
    struct Probe {
        bool timeshift = false;
        bool snapper = false;
        bool snapperRootConfig = false;   // /etc/snapper/configs/root exists
        bool btrfs = false;
        QString rootFsType;               // filesystem type of "/"
    };

    struct Command {
        QString program;
        QStringList args;
    };

    static SnapshotService *ins();

    // Linux preference order: Timeshift, snapper (needs a root config),
    // btrfs (needs the btrfs tool and a btrfs root filesystem).
    static Backend chooseLinuxBackend(const Probe &probe);
    static Probe probeSystem();
    static Command buildCommand(Backend backend, const QString &reason, const QString &timestamp);
    static QString backendDisplayName(Backend backend);
    static QString btrfsSnapshotDir();

    // Active backend on this host, Backend::None if nothing usable is installed.
    Backend backend() const;

    // User-facing sentence for "setting is on but nothing can snapshot".
    QString unavailableMessage() const;

    // Returns true when the platform snapshot tool is present and usable.
    // Cached after first call via CommandUtil::isExecutable (FR-109).
    bool isAvailable() const;

    // Synchronously create a snapshot with the given reason annotation.
    // Safe to call from the UI thread — on Linux it forks timeshift via
    // pkexec so the user may see a password prompt. Returns true on
    // success, false (and qWarning log) on any failure. Callers should
    // proceed with the clean regardless — a failed snapshot must not
    // block the user's requested action.
    bool takeSnapshot(const QString &reason);

    // Name of the underlying tool ("Timeshift" / "APFS snapshot" / "").
    // Used by Settings-page UI to label the toggle accurately per-platform.
    QString toolDisplayName() const;

private:
    static SnapshotService *instance;
    SnapshotService() = default;
};

#endif // SNAPSHOT_SERVICE_H
