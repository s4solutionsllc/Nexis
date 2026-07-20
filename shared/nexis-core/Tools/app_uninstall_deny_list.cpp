#include "app_uninstall_deny_list.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace AppUninstallDenyList {

namespace {

// Returns the canonical (real, symlink-resolved) form of path.
// If the path doesn't exist yet we canonicalize as much as possible.
static QString canonicalize(const QString &path)
{
    QFileInfo fi(path);
    const QString canonical = fi.canonicalFilePath();
    // canonicalFilePath() returns "" for non-existent paths; fall back to
    // cleaned absolute path so callers still get deny-list coverage.
    return canonical.isEmpty() ? QDir::cleanPath(QFileInfo(path).absoluteFilePath()) : canonical;
}

static bool hasPrefix(const QString &path, const QString &prefix)
{
    // Ensure we match path components, not substrings: "/usr2" must not match "/usr".
    if (!path.startsWith(prefix))
        return false;
    if (path.length() == prefix.length())
        return true;
    return path.at(prefix.length()) == QLatin1Char('/');
}

} // namespace

bool isSafeToDelete(const QString &canonicalPath, const QString &bundleId)
{
    // SSO-15373 §2: com.apple.* bundle identifiers are always deny-listed.
    if (!bundleId.isEmpty() && bundleId.startsWith(QLatin1String("com.apple.")))
        return false;

    const QString path = canonicalize(canonicalPath);

    // SSO-15373 §2 — macOS hard deny-list (system and privileged locations).
    // These must never be deleted even if a heuristic matches.
    static const QStringList kDeniedPrefixes = {
        QStringLiteral("/System"),
        QStringLiteral("/Library/LaunchDaemons"),
        QStringLiteral("/Library/LaunchAgents"),
        QStringLiteral("/Library/PrivilegedHelperTools"),
        QStringLiteral("/usr"),
        QStringLiteral("/bin"),
        QStringLiteral("/sbin"),
        QStringLiteral("/opt/homebrew"),
        QStringLiteral("/usr/local/Cellar"),
        QStringLiteral("/usr/local/opt"),
        // Credential-store paths.
        QStringLiteral("/Library/Keychains"),
    };

    for (const QString &prefix : kDeniedPrefixes) {
        if (hasPrefix(path, prefix))
            return false;
    }

    // Per-user credential stores.
    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    static const QStringList kDeniedUserSuffixes = {
        QStringLiteral("/Library/Keychains"),
        QStringLiteral("/Library/Saved Application State"),   // system-managed, not app data
    };
    // NOTE: ~/Library/Saved Application State is in scope for the leftover
    // scanner when it contains an entry matching the uninstalled app's bundle id.
    // The saved state for *other* bundles must never be touched, but the
    // scanner already filters by exact bundle-id prefix, so only the target
    // app's own <bundleId>.savedState directory reaches this check.
    // We therefore allow that specific sub-path while still denying the root.
    for (const QString &suffix : kDeniedUserSuffixes) {
        const QString denied = home + suffix;
        // Allow delete of a bundle-id-scoped subdirectory (e.g.
        // ~/Library/Saved Application State/com.example.App.savedState)
        // but deny the root itself.
        if (path == denied)
            return false;
    }

    // Credential stores under home.
    const QStringList kDeniedHomeExact = {
        home + QLatin1String("/Library/Keychains"),
        home + QLatin1String("/.local/share/keyrings"),   // GNOME Keyring (cross-platform guard)
    };
    for (const QString &denied : kDeniedHomeExact) {
        if (hasPrefix(path, denied))
            return false;
    }

    // Deny paths outside $HOME entirely (Nexis never deletes system-wide files).
    if (!path.startsWith(home + QLatin1Char('/')) && path != home)
        return false;

    return true;
}

} // namespace AppUninstallDenyList
