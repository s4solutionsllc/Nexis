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
//
// `homeRootOverride`: when non-empty, used as the $HOME boundary instead of
// re-deriving it via QStandardPaths::HomeLocation. Callers that have already
// resolved an effective home root (e.g. a dependency-injected fake root in
// tests) should pass it here — QStandardPaths::HomeLocation ignores $HOME
// env-var overrides on Apple platforms (unlike QDir::homePath(), which the
// scanners use to build their scan roots), so re-deriving it independently
// can diverge from the root the caller actually operated against.
NEXISCORESHARED_EXPORT bool isSafe(const QString &path, const QString &homeRootOverride = QString());

// SSO-27095: the System Cleaner / CleanerML delete gate. Same canonical-path
// resolution and the same hard-denied OS roots and credential stores as
// isSafe(), but without the uninstaller-only rules that would veto the
// cleaner's own targets: no $HOME confinement (the cleaner legitimately
// clears /var/cache, /var/log, /var/crash) and no per-path dpkg/rpm
// ownership probe (a process spawn per file in a recursive walk). /usr/local
// is exempt from the /usr rule on both platforms.
//
// Returns false for an empty path, a path that canonicalizes to "/", and any
// path whose canonical form is under a denied root. Like isSafe(), there is no
// user-facing bypass: callers must skip and report a false result.
NEXISCORESHARED_EXPORT bool isSafeForCleaning(const QString &path);

} // namespace LifecycleDenyList

#endif // LIFECYCLE_DENY_LIST_H
