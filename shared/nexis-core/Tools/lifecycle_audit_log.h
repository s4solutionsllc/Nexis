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
// This log is the intended data source for a future "Recently removed"
// restore view; this header only covers the writer/reader/prune API.
namespace LifecycleAuditLog {

enum class Action {
    MovedToTrash,
    PermanentlyDeleted,
};

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
};

// Appends one entry to the on-disk log (JSON Lines, one JSON object per
// line). Creates the log file and its parent directory with user-only
// (0600 / 0700) permissions on first write. Returns false if the entry
// could not be written.
NEXISCORESHARED_EXPORT bool append(const Entry &entry);

// Reads every entry currently on disk, oldest first. Malformed lines are
// skipped rather than aborting the read.
NEXISCORESHARED_EXPORT QList<Entry> readAll();

// Prunes entries older than 90 days, but always keeps at least the most
// recent `minRetainedCount` entries regardless of age — CISO §3 requires
// "at minimum the last N operations or 90 days, whichever is longer".
NEXISCORESHARED_EXPORT void prune(int minRetainedCount = 200);

// Path to the underlying log file
// (QStandardPaths::AppDataLocation + "/lifecycle_audit.jsonl"). Exposed
// for tests and the future "Recently removed" UI.
NEXISCORESHARED_EXPORT QString logFilePath();

} // namespace LifecycleAuditLog

#endif // LIFECYCLE_AUDIT_LOG_H
