#ifndef SPARKLE_UPDATE_FLOOR_STORE_H
#define SPARKLE_UPDATE_FLOOR_STORE_H

#include <QString>

#include "nexis-core_global.h"

// SSO-17776 design doc §7: persisted per-app floor version, the concrete
// mechanism behind the downgrade/replay mitigation (T3/T4 in §1). Keyed by
// CFBundleIdentifier — never display name, which is spoofable/collidable
// across apps.
//
// Binding rule: ratchetAfterVerifiedInstall() must only ever be called after
// a real SparkleSignatureVerifier::Result::Valid verify AND a successful
// install — never from an appcast's unsigned sparkle:version string.
// Ratcheting on unverified metadata lets an attacker "poison" the floor with
// a fake high version and DoS a later legitimate update.
class NEXISCORESHARED_EXPORT SparkleUpdateFloorStore
{
public:
    // Persisted floor for bundleId, or an empty string if none recorded yet.
    static QString floorVersion(const QString &bundleId);

    // First-observation initialization: writes ONLY if no floor is recorded
    // yet, so an already-tracked app's floor is never reset backward. Safe
    // to call on every scan.
    static void initializeIfAbsent(const QString &bundleId, const QString &installedVersion);

    // Ratchets the floor to newVersion if and only if newVersion is greater
    // than the current floor (or no floor exists yet). See the binding rule
    // above — callers must only reach this after a verified, installed
    // update.
    static void ratchetAfterVerifiedInstall(const QString &bundleId, const QString &newVersion);

    // Test-only seam: redirects the backing store to an arbitrary ini path
    // instead of the real AppDataLocation, so tests don't touch the user's
    // real Nexis app-support directory. Pass an empty string to restore the
    // default location.
    static void setBackingFilePathForTesting(const QString &path);

private:
    SparkleUpdateFloorStore() = default;
};

#endif // SPARKLE_UPDATE_FLOOR_STORE_H
