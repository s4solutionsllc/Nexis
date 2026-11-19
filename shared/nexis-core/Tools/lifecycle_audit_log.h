#ifndef LIFECYCLE_AUDIT_LOG_H
#define LIFECYCLE_AUDIT_LOG_H

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

#include "nexis-core_global.h"

// SSO-15386 / SSO-15373 (CISO safety controls, §3): append-only audit trail
// for every deletion performed by the lifecycle manager (paired-uninstaller
// leftover cleanup and the orphan-leftover scanner). Metadata only — never
// file contents, so a leftover config that happens to contain a secret is
// never captured by the log itself.
//
// This log is the data source for the "Recently removed" history view
// (System Cleaner → History). System Cleaner runs (interactive, scheduled,
// headless) write into it too, one entry per top-level path they touched, so
// the view covers both lifecycle cleanup and cleaner runs.
namespace LifecycleAuditLog {

enum class Action {
    MovedToTrash,
    PermanentlyDeleted,
    Skipped,   // cleaner left the path alone; skipReason says why
    Restored,  // a MovedToTrash entry was put back (links via trashDestination)
};

// Values of Entry::runSource. Empty means a lifecycle-manager entry
// (paired-uninstaller leftovers / orphan scanner), which has no "run".
namespace RunSource {
inline const QString Interactive = QStringLiteral("interactive");
inline const QString Scheduled = QStringLiteral("scheduled");
inline const QString Headless = QStringLiteral("headless");
} // namespace RunSource

struct NEXISCORESHARED_EXPORT Entry {
    QDateTime timestamp;
    QString batchId;              // links multi-file operations together
    QString originalPath;         // path as discovered by the scanner
    QString canonicalPath;        // post-symlink-resolution
    Action action = Action::MovedToTrash;
    QString trashDestination;     // populated only when action == MovedToTrash
    QStringList matchingRuleIds;  // uninstaller: the app bundle id; orphan
                                   // scanner: the heuristic rule id(s) that matched
    int confidenceScore = -1;     // orphan-scanner confidence score; -1 = n/a
                                   // (paired-uninstaller leftovers have no score)
    quint64 sizeBytes = 0;
    QString nexisVersion;
    QString processStopAction;    // optional; empty if no process was stopped
                                   // as part of this batch
    QString runSource;            // System Cleaner runs only: a RunSource value
    QString category;             // System Cleaner runs only: cleaner category
                                   // display name the path belongs to
    QString skipReason;           // populated only when action == Skipped
};

// One batch of entries as shown in the history view. Restored entries are
// folded into `restoredDestinations` rather than listed as items.
struct NEXISCORESHARED_EXPORT Run {
    QString batchId;
    QDateTime timestamp;          // earliest entry in the batch
    QString runSource;
    QList<Entry> items;           // everything except Restored entries
    QStringList restoredDestinations;
    quint64 bytesFreed = 0;       // MovedToTrash + PermanentlyDeleted
    int removedCount = 0;
    int skippedCount = 0;

    bool isRestorable(const Entry &item) const;
};

struct NEXISCORESHARED_EXPORT RestoreResult {
    enum class Status {
        Restored,
        NotRestorable,       // not a trashed entry, or not inside a Trash dir
        TrashItemMissing,    // the Trash copy is gone (emptied / restored by hand)
        DestinationExists,   // something already lives at the original path
        Failed,              // the move itself failed
    };
    Status status = Status::Failed;
    QString message;         // user-facing, translated

    bool ok() const { return status == Status::Restored; }
};

// Appends one entry to the on-disk log (JSON Lines, one JSON object per
// line). Creates the log file and its parent directory with user-only
// (0600 / 0700) permissions on first write. Returns false if the entry
// could not be written.
NEXISCORESHARED_EXPORT bool append(const Entry &entry);

// Appends several entries in one open/close of the log. Returns false if any
// entry could not be written.
NEXISCORESHARED_EXPORT bool appendAll(const QList<Entry> &entries);

// Reads every entry currently on disk, oldest first. Malformed lines are
// skipped rather than aborting the read.
NEXISCORESHARED_EXPORT QList<Entry> readAll();

// Prunes entries older than 90 days, but always keeps at least the most
// recent `minRetainedCount` entries regardless of age — CISO §3 requires
// "at minimum the last N operations or 90 days, whichever is longer".
NEXISCORESHARED_EXPORT void prune(int minRetainedCount = 200);

// Groups entries by batchId into runs, newest first.
NEXISCORESHARED_EXPORT QList<Run> groupIntoRuns(const QList<Entry> &entries);

// Moves a MovedToTrash entry back to its original path. Refuses — leaving
// everything untouched — when the Trash copy is gone or something already
// exists at the original path. On success appends a Restored entry.
NEXISCORESHARED_EXPORT RestoreResult restore(const Entry &entry);

// Path to the underlying log file
// (QStandardPaths::AppDataLocation + "/lifecycle_audit.jsonl"). Exposed
// for tests and the future "Recently removed" UI.
NEXISCORESHARED_EXPORT QString logFilePath();

} // namespace LifecycleAuditLog

#endif // LIFECYCLE_AUDIT_LOG_H
