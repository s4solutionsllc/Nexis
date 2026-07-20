#ifndef UNINSTALL_AUDIT_LOG_H
#define UNINSTALL_AUDIT_LOG_H

#include <QString>
#include <QUuid>

#include "nexis-core_global.h"

// SSO-15384 / CISO §3: append-only audit log for every destructive action
// performed by the App Lifecycle Manager.  One JSON-lines file per day under
// ~/Library/Application Support/Nexis/UninstallAuditLog/.
//
// An audit entry records: timestamp, batch id, original path, canonicalized
// path, action, matched bundle id / heuristic rule, size, and Nexis version.
// Entries are never modified after writing.  The log is local and never
// world-writable.
//
// Retention: files older than 90 days are pruned on the next write.
namespace UninstallAuditLog {

enum class Action { MovedToTrash, PermanentlyDeleted };

struct Entry {
    QUuid   batchId;           // links multi-file operations
    QString originalPath;      // as received from the caller
    QString canonicalPath;     // post-symlink-resolution
    Action  action = Action::MovedToTrash;
    QString trashedPath;       // destination in Trash (empty for permanent delete)
    QString matchedRule;       // bundle id for correlated leftovers, rule id for orphans
    quint64 sizeBytes = 0;
    QString nexisVersion;      // populated from QCoreApplication::applicationVersion()
};

// Append a single entry to today's log file.  Creates the log directory the
// first time it is needed.  Thread-safe (file is opened/written/closed each call).
NEXISCORESHARED_EXPORT void append(const Entry &entry);

// Prune log files older than 90 days.  Called automatically by append() once
// per process lifetime.
NEXISCORESHARED_EXPORT void pruneOldLogs();

} // namespace UninstallAuditLog

#endif // UNINSTALL_AUDIT_LOG_H
