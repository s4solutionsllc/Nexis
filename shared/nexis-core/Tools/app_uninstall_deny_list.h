#ifndef APP_UNINSTALL_DENY_LIST_H
#define APP_UNINSTALL_DENY_LIST_H

#include <QString>

#include "nexis-core_global.h"

// SSO-15384 / CISO §2: centralized deny-list check for all app-lifecycle
// destructive operations.  Called by the macOS uninstaller and leftover
// scanner before every trash/delete call.
//
// Contract:
//  - Path MUST be canonicalized (symlinks resolved, trailing slashes stripped)
//    before calling this function.  The check operates on the real path,
//    never the caller's original string.
//  - Returns true when the path is safe to delete.
//  - Returns false (hard-fail, no override) when the path is on the deny-list.
//  - bundleId may be empty; when set, a com.apple.* prefix is an additional
//    hard-fail regardless of path.
namespace AppUninstallDenyList {

NEXISCORESHARED_EXPORT bool isSafeToDelete(const QString &canonicalPath,
                                           const QString &bundleId = {});

} // namespace AppUninstallDenyList

#endif // APP_UNINSTALL_DENY_LIST_H
