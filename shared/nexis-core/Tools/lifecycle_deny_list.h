#ifndef LIFECYCLE_DENY_LIST_H
#define LIFECYCLE_DENY_LIST_H

#include <QString>

#include "nexis-core_global.h"

// SSO-15386 / SSO-15373 (CISO safety controls, §2): a single, centralized
// "is this path safe to delete" check shared by the paired-uninstaller
// leftover cleanup (trashLeftovers()) and the orphan-leftover scanner.
//
// Deliberately conservative and non-overridable — callers must skip (and
// log) any path this returns false for. There is no user-facing bypass for
// this class of block; that is a product requirement, not an implementation
// detail, so do not add one.
namespace LifecycleDenyList {

// Resolves `path` to its canonical (symlink-free) form and checks it against
// the platform deny-list below. A literal-string check alone is insufficient
// — a symlink inside a scanned, otherwise-safe directory can point out to a
// denied location (e.g. /System), so the check always runs against the
// resolved path, never the input string.
//
// Returns false (unsafe / deny) for:
//  - an empty path, or a path that canonicalizes to "/"
//  - any path under a platform deny-list location (see .cpp for the list)
//
// Returns true only when the canonicalized path is outside every denied
// location for the current platform.
NEXISCORESHARED_EXPORT bool isSafe(const QString &path);

} // namespace LifecycleDenyList

#endif // LIFECYCLE_DENY_LIST_H
