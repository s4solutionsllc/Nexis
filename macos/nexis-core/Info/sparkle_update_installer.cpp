// SECURITY CRITICAL — CTO / CISO REVIEW REQUIRED (SSO-17776, SSO-17775).
// See sparkle_update_installer.h for the design doc references this file
// implements and the fail-closed contract it must uphold.

#include "sparkle_update_installer.h"

#include "sparkle_embedded_version.h"
#include "sparkle_update_floor_store.h"

#include <Utils/command_util.h>
#include <Utils/macos_xattr_util.h>

#include <QDesktopServices>
#include <QDir>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QUrl>
#include <QUuid>
#include <QVersionNumber>
#include <QDebug>

namespace SparkleUpdateInstaller {

namespace {

// Deliberately leaked for the process lifetime — see the header comment on
// verifyAndInstall() / design doc §4 step 5 & §10.3: QDesktopServices::
// openUrl hands off to LaunchServices with no waitable completion handle, so
// there is no safe "the opener is done with this file" moment to delete on.
// Static destruction at normal process exit is what "auto-remove on quit"
// means here; sweepStaleArtifacts() covers the crash/force-quit case.
QList<QTemporaryFile *> &liveArtifactFiles()
{
    static QList<QTemporaryFile *> files;
    return files;
}

QList<QTemporaryDir *> &liveArtifactDirs()
{
    static QList<QTemporaryDir *> dirs;
    return dirs;
}

// Private, unpredictable, per-user temp root — never the shared /tmp. On
// macOS QStandardPaths::TempLocation resolves to the per-user $TMPDIR (mode
// 0700), so a different-user local attacker cannot pre-stage or race a path
// under it (design doc §4; same-user local attacker is the accepted T5
// residual per §8).
QString privateTempRoot()
{
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation)
         + QLatin1String("/NexisSparkleUpdate");
}

QString uniqueSubPath(const QString &prefix)
{
    return privateTempRoot() + QLatin1Char('/') + prefix + QLatin1Char('-')
         + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

} // namespace

bool defaultLaunch(const QString &artifactPath)
{
    return QDesktopServices::openUrl(QUrl::fromLocalFile(artifactPath));
}

bool defaultReveal(const QString &folderPath)
{
    return QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
}

InstallResult verifyAndInstall(const UpdateEntry &entry,
                                const QByteArray &downloadedBytes,
                                const Launcher &launcher,
                                const Revealer &revealer)
{
    InstallResult result;

    // §8 gate 7 / product brief scope decision: format scope is checked
    // first. An unsupported format (.dmg, anything else) always falls back
    // to browser-open, independent of signature validity.
    const SparkleEmbeddedVersion::Format format =
        SparkleEmbeddedVersion::formatFromUrl(entry.enclosureUrl);
    if (format == SparkleEmbeddedVersion::Format::Unsupported) {
        result.outcome = Outcome::BlockedUnsupportedFormat;
        result.message = QStringLiteral(
            "This update's file type isn't eligible for verified install yet "
            "— opening the publisher's page instead.");
        return result;
    }

    // §5 / AC4 hard gate: only Result::Valid may proceed past this point.
    // Nothing has touched disk yet and `launcher`/`revealer` have not been
    // called — every other Result (Invalid, MissingKey, MissingSignature,
    // DecodingError, InternalError, UnsupportedLegacyDsa) returns here.
    const SparkleSignatureVerifier::Result verifyResult = SparkleSignatureVerifier::verify(
        downloadedBytes, entry.edSignature, entry.dsaSignature, entry.publicKey);
    result.verifyResult = verifyResult;
    if (verifyResult != SparkleSignatureVerifier::Result::Valid) {
        result.outcome = Outcome::BlockedVerificationFailed;
        result.message = QStringLiteral(
            "This update couldn't be verified (%1) — opening the publisher's "
            "page instead.").arg(SparkleSignatureVerifier::resultDescription(verifyResult));
        return result;
    }

    // §4 step 3: write the exact verified bytes to a private, unpredictable
    // QTemporaryFile. Nothing above this line has written to disk.
    QDir().mkpath(privateTempRoot());
    const QString suffix = (format == SparkleEmbeddedVersion::Format::Pkg)
                          ? QStringLiteral(".pkg") : QStringLiteral(".zip");
    auto *artifactFile = new QTemporaryFile(
        privateTempRoot() + QLatin1String("/update-XXXXXX") + suffix);
    artifactFile->setAutoRemove(true);
    if (!artifactFile->open() ||
        artifactFile->write(downloadedBytes) != downloadedBytes.size()) {
        result.outcome = Outcome::BlockedInternalError;
        result.message = QStringLiteral(
            "Couldn't save the verified update — opening the publisher's page instead.");
        delete artifactFile;
        return result;
    }
    artifactFile->flush();
    const QString artifactPath = artifactFile->fileName();
    liveArtifactFiles().append(artifactFile); // see comment above

    // §7: embedded-version cross-check, read from the verified bytes on
    // disk — never trust the appcast's unsigned sparkle:version (that's
    // exactly what a downgrade/replay attacker lies about).
    QString embeddedVersion;
    QString revealDir; // populated for the .zip case only

    if (format == SparkleEmbeddedVersion::Format::Pkg) {
        const QString expandDir = uniqueSubPath(QStringLiteral("pkg-expand"));
        embeddedVersion = SparkleEmbeddedVersion::extractPkgVersion(artifactPath, expandDir);
        QDir(expandDir).removeRecursively(); // disposable — never shown to the user
    } else {
        auto *expandDir = new QTemporaryDir(uniqueSubPath(QStringLiteral("zip-expand")));
        if (expandDir->isValid()) {
            const ExecResult r = CommandUtil::execWithStatus(
                QStringLiteral("/usr/bin/unzip"),
                {"-o", artifactPath, "-d", expandDir->path()}, 120000);
            if (r.ok())
                embeddedVersion = SparkleEmbeddedVersion::readAppVersionFromExpandedZip(expandDir->path());
        }
        if (embeddedVersion.isEmpty()) {
            delete expandDir; // nothing to reveal; cleans itself up
        } else {
            revealDir = expandDir->path();
            liveArtifactDirs().append(expandDir); // kept alive alongside artifactFile
        }
    }

    if (embeddedVersion.isEmpty()) {
        result.outcome = Outcome::BlockedInternalError;
        result.message = QStringLiteral(
            "Couldn't read the update's real version — opening the publisher's page instead.");
        return result;
    }

    if (!entry.bundleId.isEmpty())
        SparkleUpdateFloorStore::initializeIfAbsent(entry.bundleId, entry.installedVersion);
    const QString floor = entry.bundleId.isEmpty()
                         ? entry.installedVersion
                         : SparkleUpdateFloorStore::floorVersion(entry.bundleId);
    const QVersionNumber floorVer = QVersionNumber::fromString(floor);
    const QVersionNumber embeddedVer = QVersionNumber::fromString(embeddedVersion);
    if (!floorVer.isNull() && embeddedVer <= floorVer) {
        result.outcome = Outcome::BlockedDowngrade;
        result.message = QStringLiteral(
            "This update's real version (%1) isn't newer than what's already "
            "installed — blocking a possible downgrade or replay. Opening the "
            "publisher's page instead.").arg(embeddedVersion);
        return result;
    }

    // §6 gate 8: quarantine must be set explicitly — self-downloaded bytes
    // carry no com.apple.quarantine automatically. MacOsXattrUtil::
    // stripQuarantine is the wrong helper here (design doc §6, §10.1); this
    // ADDS the attribute. A failure here does not block: ed25519
    // verification above is still the primary control, and Gatekeeper is
    // defense-in-depth only (§8 residual risk).
    if (!MacOsXattrUtil::setQuarantine(artifactPath))
        qWarning() << "SparkleUpdateInstaller: failed to set quarantine on" << artifactPath;

    if (format == SparkleEmbeddedVersion::Format::Pkg) {
        // §6: unprivileged execution only — hand the verified path to the
        // OS's normal unprivileged open mechanism. Nexis never invokes
        // sudoExecWithStatus/osascript-with-administrator-privileges for
        // this feature; a flat .pkg targeting system locations may still
        // prompt for admin rights itself via Installer.app, which is
        // OS-mediated and in scope (design doc §6 "elevation, precisely
        // stated").
        if (!launcher(artifactPath)) {
            result.outcome = Outcome::BlockedInternalError;
            result.message = QStringLiteral(
                "Couldn't open the installer — opening the publisher's page instead.");
            return result;
        }
        if (!entry.bundleId.isEmpty())
            SparkleUpdateFloorStore::ratchetAfterVerifiedInstall(entry.bundleId, embeddedVersion);
        result.outcome = Outcome::Installed;
        result.message = QStringLiteral("Update verified and opened in Installer.");
        return result;
    }

    // .zip: verified-reveal only (design doc §6 option 2 / AC7). openUrl on
    // a .zip would only expand it into our own private temp dir and leave
    // /Applications untouched, so we reveal the already-expanded bundle in
    // Finder instead and leave the copy-to-Applications step to the user.
    // This must never be presented as a completed update.
    if (!revealer(revealDir)) {
        result.outcome = Outcome::BlockedInternalError;
        result.message = QStringLiteral(
            "Couldn't reveal the verified update — opening the publisher's page instead.");
        return result;
    }
    if (!entry.bundleId.isEmpty())
        SparkleUpdateFloorStore::ratchetAfterVerifiedInstall(entry.bundleId, embeddedVersion);
    result.outcome = Outcome::RevealedForManualInstall;
    result.message = QStringLiteral(
        "Update verified. The app was extracted to a temporary folder for "
        "you to drag into Applications — Nexis did not install it automatically.");
    return result;
}

void sweepStaleArtifacts()
{
    QDir dir(privateTempRoot());
    if (dir.exists())
        dir.removeRecursively();
}

} // namespace SparkleUpdateInstaller
