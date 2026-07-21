#include "lifecycle_deny_list.h"

#include "Utils/command_util.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace LifecycleDenyList {

namespace {

// Resolves symlinks when the path exists; falls back to the cleaned
// absolute path when it doesn't (e.g. a broken symlink, or a target that
// was already removed by the time the caller checks) so the deny-list
// still applies to whatever literal location was requested.
QString canonicalOrAbsolute(const QString &path)
{
    QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    if (!canonical.isEmpty())
        return canonical;
    return QDir::cleanPath(info.absoluteFilePath());
}

bool isUnderAny(const QString &canonicalPath, const QStringList &deniedRoots)
{
    for (const QString &root : deniedRoots) {
        if (canonicalPath == root)
            return true;
        if (canonicalPath.startsWith(root + QLatin1Char('/')))
            return true;
    }
    return false;
}

#if defined(Q_OS_MAC)

bool isSafeMacOS(const QString &canonicalPath)
{
    static const QStringList kDeniedRoots = {
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
    };
    if (isUnderAny(canonicalPath, kDeniedRoots))
        return false;

    // Any bundle/path with a com.apple.* leaf name (bundle id or plist)
    // is denied regardless of which directory it lives under.
    const QString leaf = QFileInfo(canonicalPath).fileName();
    if (leaf.startsWith(QLatin1String("com.apple.")))
        return false;

    // Credential stores: macOS Keychain files.
    if (canonicalPath.contains(QLatin1String("/Library/Keychains/")))
        return false;

    // Anything owned by root (not the invoking user) is out of scope for
    // an unprivileged lifecycle-manager delete.
    QFileInfo info(canonicalPath);
    if (info.exists() && info.ownerId() == 0)
        return false;

    // Must stay inside $HOME — all current callers (findAppLeftovers,
    // findOrphanLeftovers) source paths from ~/Library, but this rule is the
    // code-level guarantee rather than relying solely on caller convention
    // (Defense in Depth, CISO §2 follow-up from SSO-15771).
    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (!home.isEmpty() && !canonicalPath.startsWith(home + QLatin1Char('/')) && canonicalPath != home)
        return false;

    return true;
}

#elif defined(Q_OS_LINUX)

bool pathOwnedByPackage(const QString &canonicalPath)
{
    // dpkg -S / rpm -qf: argv-safe (CommandUtil), no shell interpolation.
    // A zero exit code means the path is claimed by an installed package;
    // treat that as a deny regardless of which tool reports it.
    if (CommandUtil::isExecutable(QStringLiteral("dpkg"))) {
        const ExecResult r = CommandUtil::execWithStatus(
            QStringLiteral("dpkg"), {QStringLiteral("-S"), canonicalPath});
        if (r.ok())
            return true;
    }
    if (CommandUtil::isExecutable(QStringLiteral("rpm"))) {
        const ExecResult r = CommandUtil::execWithStatus(
            QStringLiteral("rpm"), {QStringLiteral("-qf"), canonicalPath});
        if (r.ok())
            return true;
    }
    return false;
}

bool isSafeLinux(const QString &canonicalPath)
{
    static const QStringList kDeniedRoots = {
        QStringLiteral("/etc"),
        QStringLiteral("/usr"),
        QStringLiteral("/bin"),
        QStringLiteral("/sbin"),
        QStringLiteral("/lib"),
        QStringLiteral("/lib32"),
        QStringLiteral("/lib64"),
        QStringLiteral("/boot"),
        QStringLiteral("/root"),
        QStringLiteral("/etc/systemd"),
        QStringLiteral("/usr/lib/systemd"),
        QStringLiteral("/run/systemd"),
    };
    if (isUnderAny(canonicalPath, kDeniedRoots))
        return false;

    // Credential stores: GNOME Keyring and common browser cookie/token
    // stores under the user's data dir.
    if (canonicalPath.contains(QLatin1String("/.local/share/keyrings/")))
        return false;

    // Must stay inside $HOME unless explicitly allowlisted elsewhere by
    // the caller — this function only ever narrows, never widens, so
    // "outside HOME" is a deny by default.
    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (!home.isEmpty() && !canonicalPath.startsWith(home + QLatin1Char('/')) && canonicalPath != home)
        return false;

    // Any path owned by an installed package is managed by the package
    // manager, not the lifecycle scanner — refuse and let dpkg/rpm own it.
    if (pathOwnedByPackage(canonicalPath))
        return false;

    return true;
}

#endif

} // namespace

bool isSafe(const QString &path)
{
    if (path.isEmpty())
        return false;

    const QString canonical = canonicalOrAbsolute(path);
    if (canonical.isEmpty() || canonical == QLatin1String("/"))
        return false;

#if defined(Q_OS_MAC)
    return isSafeMacOS(canonical);
#elif defined(Q_OS_LINUX)
    return isSafeLinux(canonical);
#else
    Q_UNUSED(canonical);
    return false;
#endif
}

} // namespace LifecycleDenyList
