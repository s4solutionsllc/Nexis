#ifndef SHARED_DATA_GUARD_H
#define SHARED_DATA_GUARD_H

#include <QString>
#include <QStringList>

#include "nexis-core_global.h"

// SSO-27151: uninstaller shared-data guard. The leftover scanners match by
// id prefix, so removing "com.example.App" also matches the data of a
// still-installed "com.example.App.beta", and channel variants (stable /
// beta / esr / insiders) conventionally share the unsuffixed profile name.
// Deleting those would break an app the user kept. This module decides, from
// names alone and with no filesystem access, whether a matched leftover is
// also claimed by another installed app.
//
// Ids are macOS bundle identifiers or Linux package / Flatpak / Snap names.
// Comparison is case-insensitive.
namespace SharedDataGuard {

struct NEXISCORESHARED_EXPORT Claim {
    QString siblingId;   // the still-installed app that also claims the leftover
    QString reason;      // user-facing explanation; empty when not shared

    bool isShared() const { return !siblingId.isEmpty(); }
};

// Strips one trailing release-channel token ("beta", "esr", "insiders", ...)
// so "com.google.Chrome.beta" and "firefox-esr" reduce to their family name.
// Returns `id` unchanged when it carries no channel token.
NEXISCORESHARED_EXPORT QString familyBase(const QString &id);

// True when `a` and `b` are different ids of the same app family.
NEXISCORESHARED_EXPORT bool areVariants(const QString &a, const QString &b);

// Decide whether the leftover named `leaf` (a leaf file/directory name), which
// the scanner matched to the uninstalled id `matchedId`, is also claimed by an
// app in `installedIds` (apps still installed *now*).
//
// A leftover is shared when either
//  - it belongs more specifically (a longer id, or an equal one) to an
//    installed app than to `matchedId` — e.g. leaf "com.foo.App.beta.plist"
//    matched to "com.foo.App" belongs to the installed "com.foo.App.beta"; or
//  - `matchedId` is the unsuffixed id of its family and an installed variant
//    of that family remains — channel variants share the unsuffixed profile.
NEXISCORESHARED_EXPORT Claim classify(const QString &leaf,
                                      const QString &matchedId,
                                      const QStringList &installedIds);

} // namespace SharedDataGuard

#endif // SHARED_DATA_GUARD_H
