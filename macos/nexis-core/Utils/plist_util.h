#ifndef PLIST_UTIL_H
#define PLIST_UTIL_H

#include <QString>

#include "nexis-core_global.h"

// macOS-only plist helpers. Today we only need Info.plist fields for app
// bundles; if new call sites emerge we can extend here rather than
// duplicating `plutil -convert json` blocks across the codebase.
namespace PlistUtil {

struct NEXISCORESHARED_EXPORT AppBundleInfo {
    QString bundleId;
    QString displayName;
    QString version;
    QString suFeedUrl;      // SUFeedURL — empty if not a Sparkle app
    QString suPublicEDKey;  // SUPublicEDKey — base64 Ed25519 public key
};

// Reads <appPath>/Contents/Info.plist via /usr/bin/plutil -convert json -o -.
// Missing files or parse errors return a default-constructed AppBundleInfo
// (all empty strings) — callers can check bundleId.isEmpty().
NEXISCORESHARED_EXPORT AppBundleInfo readAppBundleInfo(const QString &appPath);

} // namespace PlistUtil

#endif // PLIST_UTIL_H
