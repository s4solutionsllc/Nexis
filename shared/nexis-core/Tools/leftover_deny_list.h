#ifndef LEFTOVER_DENY_LIST_H
#define LEFTOVER_DENY_LIST_H

#include <QDateTime>
#include <QString>
#include <QStringList>

#include "nexis-core_global.h"

// CISO §3 (SSO-15373): append-only audit log per deletion. Call logDeletion()
// immediately before each trash/delete call so the record exists even if the
// operation partially fails.
//
// The deny-list check itself (CISO §2) lives in LifecycleDenyList::isSafe()
// (shared/nexis-core/Tools/lifecycle_deny_list.h) — that is the single
// centralized "is this path safe to delete" check shared with the orphan
// scanner (SSO-15386). This module only handles audit logging; do not add a
// second deny-list implementation here.
namespace LeftoverDenyList {

// Audit-log record written per-item before each deletion (CISO §3).
// batchId links related multi-file operations. matchRule is the heuristic
// or package name that triggered the match.
struct NEXISCORESHARED_EXPORT AuditEntry {
    QString batchId;
    QString originalPath;
    QString canonicalPath;
    QString action;       // "trash_pending" or "trash" (final) or "delete"
    QString trashDest;    // populated for trash operations
    QString matchRule;
    quint64 sizeBytes = 0;
    QString nexisVersion;
    QDateTime timestamp;  // wall-clock time the entry was written (UTC)
};

NEXISCORESHARED_EXPORT void logDeletion(const AuditEntry &entry);

// Returns the path to the current audit log file.
NEXISCORESHARED_EXPORT QString auditLogPath();

} // namespace LeftoverDenyList

#endif // LEFTOVER_DENY_LIST_H
