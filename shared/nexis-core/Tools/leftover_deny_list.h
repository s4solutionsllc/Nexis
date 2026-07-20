#ifndef LEFTOVER_DENY_LIST_H
#define LEFTOVER_DENY_LIST_H

#include <QDateTime>
#include <QString>
#include <QStringList>

#include "nexis-core_global.h"

// CISO §2 (SSO-15373): centralized deny-list shared by the uninstaller and
// orphan scanner on both platforms. Every deletion path must pass through
// isDenied() after canonicalization — never check the raw/pre-symlink path.
//
// CISO §3 (SSO-15373): append-only audit log per deletion. Call logDeletion()
// immediately before each trash/delete call so the record exists even if the
// operation partially fails.
namespace LeftoverDenyList {

// Returns true iff canonicalPath is on the CISO deny-list for the current
// platform. The check runs on the resolved (post-symlink) absolute path.
// When this returns true the caller MUST abort with no "delete anyway" option.
NEXISCORESHARED_EXPORT bool isDenied(const QString &canonicalPath);

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
