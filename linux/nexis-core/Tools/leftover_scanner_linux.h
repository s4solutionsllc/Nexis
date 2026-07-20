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
// Matching policy: conservative leaf-name prefix match (packageName exact,
// or packageName followed by a non-alphanumeric separator). Deep content
// inspection is out of scope to avoid false positives.
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
};

// Scan for leftovers belonging to any of the supplied package / app names.
// packageNames may include the raw dpkg/rpm/flatpak/snap package name.
// On Flatpak the app id (e.g. "org.mozilla.firefox") is also accepted.
NEXISCORESHARED_EXPORT QList<LeftoverCandidate> scanLeftovers(const QStringList &packageNames);

} // namespace LeftoverScannerLinux

#endif // LEFTOVER_SCANNER_LINUX_H
