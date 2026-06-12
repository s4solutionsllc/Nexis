#ifndef OOMD_SNAPSHOT_H
#define OOMD_SNAPSHOT_H

#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QString>

// FW-11 (SSO-3739): systemd-oomd / cgroup v2 observability snapshot.
//
// Designed to live in shared/ so the GUI signal (DataRefreshService::oomdUpdated)
// can carry it across threads without dragging in any Linux-only headers.
// Producers live in linux/nexis-core/Info/oomd_info* — on macOS the snapshot
// type still compiles but no producer wires it up.

struct OomdEvent
{
    QDateTime when;          // wall-clock from the journal entry, may be invalid
    QString   unit;          // systemd unit that hosted the cgroup (e.g. "user@1000.service")
    QString   cgroupPath;    // best-effort cgroup path (e.g. "/user.slice/user-1000.slice/...")
    QString   reason;        // free-form ("memory-pressure", "swap-used", etc)
    int       tasksKilled = 0;
};

struct OomdSnapshot
{
    // True when at least one source produced data — even when systemd-oomd is
    // masked we may still have a kernel-level oom_kill count from cgroup v2
    // memory.events. UI hides the panel entirely when !available.
    bool available = false;

    // True iff the kernel exposes the cgroup v2 unified hierarchy. Used by the
    // panel to distinguish "v2-but-no-oomd" from "v1 host (unsupported)".
    bool cgroupV2 = false;

    QString loadState;     // "loaded" / "not-found" / "masked" / ""
    QString activeState;   // "active" / "inactive" / "failed" / ""

    // From `systemctl show systemd-oomd.service`. These are session-wide
    // cumulative counters (reset across reboot). 0 when oomd never ran.
    quint64 oomKills = 0;
    quint64 managedOomKills = 0;

    // From /sys/fs/cgroup/memory.events `oom_kill` row (kernel-side fallback;
    // counts kills that hit the system root memcg regardless of oomd state).
    quint64 systemOomKill = 0;

    // Most recent OOM events parsed from `journalctl -u systemd-oomd.service`,
    // newest first; capped at ~16. Empty when journalctl is unavailable or
    // oomd has not killed anything since the last journal rotation.
    QList<OomdEvent> recentEvents;
};

Q_DECLARE_METATYPE(OomdEvent)
Q_DECLARE_METATYPE(OomdSnapshot)

#endif // OOMD_SNAPSHOT_H
