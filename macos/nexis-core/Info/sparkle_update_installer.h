#ifndef SPARKLE_UPDATE_INSTALLER_H
#define SPARKLE_UPDATE_INSTALLER_H

#include <QByteArray>
#include <QString>
#include <functional>

#include "nexis-core_global.h"
#include "sparkle_signature_verifier.h"
#include <Info/update_info.h>

// ============================================================
// SECURITY CRITICAL — CTO / CISO REVIEW REQUIRED (SSO-17776,
// design gate SSO-17775, GO-WITH-CONDITIONS).
//
// This is the call site that turns SparkleSignatureVerifier from dead code
// into an RCE-adjacent control. It owns steps 2-5 of the design doc's §4
// TOCTOU-safe pipeline (verify → write → quarantine → exec), the §5
// fail-closed contract at the caller layer, the §6 unprivileged-execution
// and format-scope decisions, and the §7 downgrade/replay gate. Any change
// here needs the same review bar as sparkle_signature_verifier.h/.cpp.
//
// This class never touches the network — SparkleUpdateDownloader owns that
// — so verifyAndInstall() can be exercised deterministically in tests
// against synthetic bytes, including the AC4 hard-gate spy-launcher test
// (zero launcher invocations for every non-Valid verifier Result).
// ============================================================

namespace SparkleUpdateInstaller {

enum class NEXISCORESHARED_EXPORT Outcome {
    Installed,                // .pkg handed to the OS opener (Installer.app)
    RevealedForManualInstall, // .zip expanded + revealed in Finder; NOT installed
    BlockedVerificationFailed,
    BlockedDowngrade,
    BlockedUnsupportedFormat,
    BlockedInternalError,     // write/quarantine/extract failure, not a verify failure
};

struct NEXISCORESHARED_EXPORT InstallResult {
    Outcome outcome = Outcome::BlockedInternalError;
    SparkleSignatureVerifier::Result verifyResult = SparkleSignatureVerifier::Result::InternalError;
    QString message; // user-facing summary; safe to show as-is

    // True for every Blocked* outcome — i.e. every case where the caller
    // must fall back to today's QDesktopServices::openUrl(entry.enclosureUrl)
    // behavior (design doc §5, §8).
    bool shouldFallBackToBrowserOpen() const
    {
        return outcome == Outcome::BlockedVerificationFailed
            || outcome == Outcome::BlockedDowngrade
            || outcome == Outcome::BlockedUnsupportedFormat
            || outcome == Outcome::BlockedInternalError;
    }
};

// Test seams. Defaults call the real OS integration (`/usr/bin/open`, the
// same LaunchServices/NSWorkspace opener QDesktopServices::openUrl wraps —
// nexis-core links no Qt Gui module, so it cannot call that class directly);
// tests override these to spy on invocation without a live installer prompt.
using Launcher = std::function<bool(const QString &artifactPath)>;
using Revealer = std::function<bool(const QString &folderPath)>;

NEXISCORESHARED_EXPORT bool defaultLaunch(const QString &artifactPath);
NEXISCORESHARED_EXPORT bool defaultReveal(const QString &folderPath);

// Runs the full verify → downgrade-check → write → quarantine → exec
// pipeline against already-downloaded bytes for `entry`.
//
//   entry.enclosureUrl                  — selects format (.pkg / .zip)
//   entry.edSignature / entry.dsaSignature / entry.publicKey
//                                        — fed to SparkleSignatureVerifier
//   entry.bundleId / entry.installedVersion
//                                        — feed the §7 floor-version gate
//   entry.version                       — appcast's unsigned claimed
//                                          version; NOT used as the gate
//                                          (kept only for the message text)
//
// Only SparkleSignatureVerifier::Result::Valid can reach disk or `launcher`/
// `revealer` — every other Result returns a Blocked* outcome before a
// single byte is written (§5 / AC4).
NEXISCORESHARED_EXPORT InstallResult verifyAndInstall(
    const UpdateEntry &entry,
    const QByteArray &downloadedBytes,
    const Launcher &launcher = defaultLaunch,
    const Revealer &revealer = defaultReveal);

// Sweeps stale temp artifacts left behind by a crashed/force-quit prior run
// (design doc §4 cleanup correction, §10.3 — QDesktopServices::openUrl gives
// no waitable process handle, so normal cleanup is auto-remove-on-quit and
// this sweep covers the crash case). Call once at startup. No-op on
// non-macOS builds.
NEXISCORESHARED_EXPORT void sweepStaleArtifacts();

} // namespace SparkleUpdateInstaller

#endif // SPARKLE_UPDATE_INSTALLER_H
