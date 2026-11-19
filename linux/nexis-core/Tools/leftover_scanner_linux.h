#ifndef LEFTOVER_SCANNER_LINUX_H
#define LEFTOVER_SCANNER_LINUX_H

#include <QList>
#include <QString>
#include <QStringList>

#include "nexis-core_global.h"

// SSO-15385: scan the standard freedesktop.org user data directories for
// leftover files associated with a package name. Returns candidates sorted
// by size descending so the review dialog shows the largest first.
//
// Matching policy: conservative leaf-name match (packageName exact, or
// packageName followed by a literal '.' — e.g. "firefox.desktop",
// "org.mozilla.firefox.conf"). Hyphen/underscore/space are NOT treated as
// separators: they are how distinct sibling packages are named (firefox vs
// firefox-esr, code vs code-insiders), so a prefix match on those would
// delete an unrelated package's data. Deep content inspection is out of
// scope to avoid false positives.
//
// All returned paths have been canonicalized (QFileInfo::canonicalFilePath)
// and checked against the CISO §2 deny-list. Deny-listed paths are silently
// excluded — they will never appear in results.
namespace LeftoverScannerLinux {

struct NEXISCORESHARED_EXPORT LeftoverCandidate {
    QString path;
    QString canonicalPath;
    quint64 sizeBytes = 0;
    QString category;       // "Config", "Cache", "Data", "Autostart"
    QString matchedName;    // which packageName in the input matched
    QString sharedWith;     // SSO-27151: still-installed sibling that also claims it
    QString sharedReason;   // SSO-27151: user-facing reason; empty = not shared
};

// Scan for leftovers belonging to any of the supplied package / app names.
// packageNames may include the raw dpkg/rpm/flatpak/snap package name.
// On Flatpak the app id (e.g. "org.mozilla.firefox") is also accepted.
//
// SSO-27151: `installedIds` lists the apps still installed. A candidate that
// one of them also claims (see SharedDataGuard) is still returned but carries
// sharedWith / sharedReason so the review UI can exclude it by default.
NEXISCORESHARED_EXPORT QList<LeftoverCandidate> scanLeftovers(const QStringList &packageNames,
                                                              const QStringList &installedIds = {});

} // namespace LeftoverScannerLinux

#endif // LEFTOVER_SCANNER_LINUX_H
