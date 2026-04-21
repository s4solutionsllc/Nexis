#ifndef SNAPSHOT_SERVICE_H
#define SNAPSHOT_SERVICE_H

#include <QObject>
#include <QString>

// FR-112: create a system snapshot before risky cleaning operations.
// Linux: Timeshift (requires root elevation via pkexec/sudoExec).
// macOS: APFS local snapshot via `tmutil localsnapshot` (user-level, no elevation).
//
// The SettingKeys::PreCleanSnapshotEnabled toggle gates whether callers
// actually invoke takeSnapshot(); this service itself is silent if the
// tool is not present on the system.
class SnapshotService : public QObject
{
    Q_OBJECT

public:
    static SnapshotService *ins();

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
