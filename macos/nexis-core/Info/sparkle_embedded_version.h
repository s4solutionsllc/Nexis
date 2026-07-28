#ifndef SPARKLE_EMBEDDED_VERSION_H
#define SPARKLE_EMBEDDED_VERSION_H

#include <QString>

#include "nexis-core_global.h"

// SSO-17776 design doc §6: the v1 enclosure format scope is limited to flat
// .pkg and .zip-containing-.app — both admit a cheap, non-executing way to
// read the real embedded version, needed for the §7 downgrade cross-check.
// Anything else (.dmg included) falls back to browser-open.
namespace SparkleEmbeddedVersion {

enum class NEXISCORESHARED_EXPORT Format {
    Pkg,
    Zip,
    Unsupported,
};

// Determined from the enclosure URL's file extension only — cheap, and does
// not require any bytes to have been downloaded.
NEXISCORESHARED_EXPORT Format formatFromUrl(const QString &enclosureUrl);

// Reads the real embedded version from an already-verified, on-disk .pkg via
// `pkgutil --expand` (does not run preinstall/postinstall scripts) into
// expandDir, then parses PackageInfo (component package) or Distribution
// (product archive) XML for the first version= attribute. expandDir must
// NOT already exist — pkgutil creates it. Returns empty on any failure;
// callers MUST treat empty as "cannot prove this isn't a downgrade" and
// block (design doc §7.4), not as "no version info, proceed anyway".
NEXISCORESHARED_EXPORT QString extractPkgVersion(const QString &pkgPath,
                                                  const QString &expandDir);

// Locates the single top-level "*.app" bundle directly under expandedDir
// (a directory a .zip has already been fully expanded into). Returns an
// empty string if zero or more than one top-level .app is found — an
// ambiguous zip is treated as untrustworthy for version extraction, not
// guessed at.
NEXISCORESHARED_EXPORT QString findTopLevelAppBundle(const QString &expandedDir);

// Reads CFBundleShortVersionString from the Info.plist of the single
// top-level .app under expandedDir. Returns empty on any failure (no
// bundle, ambiguous bundle, missing/malformed Info.plist).
NEXISCORESHARED_EXPORT QString readAppVersionFromExpandedZip(const QString &expandedDir);

} // namespace SparkleEmbeddedVersion

#endif // SPARKLE_EMBEDDED_VERSION_H
