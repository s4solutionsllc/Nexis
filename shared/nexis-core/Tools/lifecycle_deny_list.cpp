#include "lifecycle_deny_list.h"

#include "Utils/command_util.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace LifecycleDenyList {

namespace {

// Resolves symlinks when the path exists. When it doesn't (a broken symlink,
// or a target already removed by the time the caller checks) the leaf cannot
// be resolved, so resolve its parent directory and re-attach the leaf — a
// dangling link inside a directory that is itself a symlink to a denied
// location must still be judged by where it really lives. Falls back to the
// cleaned absolute path when even the parent does not resolve.
QString canonicalOrAbsolute(const QString &path)
{
    QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    if (!canonical.isEmpty())
        return canonical;

    const QString parent = QFileInfo(info.absolutePath()).canonicalFilePath();
    if (!parent.isEmpty() && !info.fileName().isEmpty())
        return QDir::cleanPath(parent + QLatin1Char('/') + info.fileName());
    return QDir::cleanPath(info.absoluteFilePath());
}

bool isUnderAny(const QString &canonicalPath, const QStringList &roots)
{
    for (const QString &root : roots) {
        if (canonicalPath == root)
            return true;
        if (canonicalPath.startsWith(root + QLatin1Char('/')))
            return true;
    }
    return false;
}

#if defined(Q_OS_MAC)

const QStringList &deniedRoots()
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
    return kDeniedRoots;
}

bool isSafeMacOS(const QString &canonicalPath, const QString &homeRoot)
{
    if (isUnderAny(canonicalPath, deniedRoots()))
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
    // (Defense in Depth, CISO §2 follow-up from SSO-15771). `homeRoot` is
    // resolved by isSafe() below (real $HOME, or a caller-supplied override).
    if (!homeRoot.isEmpty() && !canonicalPath.startsWith(homeRoot + QLatin1Char('/')) && canonicalPath != homeRoot)
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

const QStringList &deniedRoots()
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
    return kDeniedRoots;
}

bool isSafeLinux(const QString &canonicalPath, const QString &homeRoot)
{
    if (isUnderAny(canonicalPath, deniedRoots()))
        return false;

    // Credential stores: GNOME Keyring and common browser cookie/token
    // stores under the user's data dir.
    if (canonicalPath.contains(QLatin1String("/.local/share/keyrings/")))
        return false;

    // Must stay inside $HOME unless explicitly allowlisted elsewhere by
    // the caller — this function only ever narrows, never widens, so
    // "outside HOME" is a deny by default. `homeRoot` is resolved by isSafe()
    // below (real $HOME, or a caller-supplied override).
    if (!homeRoot.isEmpty() && !canonicalPath.startsWith(homeRoot + QLatin1Char('/')) && canonicalPath != homeRoot)
        return false;

    // Any path owned by an installed package is managed by the package
    // manager, not the lifecycle scanner — refuse and let dpkg/rpm own it.
    if (pathOwnedByPackage(canonicalPath))
        return false;

    return true;
}

#endif

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)

// Credential stores that no cleaner action may touch, whichever directory the
// canonical path sits under.
bool isCredentialStore(const QString &canonicalPath)
{
#if defined(Q_OS_MAC)
    return canonicalPath.contains(QLatin1String("/Library/Keychains/"));
#else
    return canonicalPath.contains(QLatin1String("/.local/share/keyrings/"));
#endif
}

// The cleaner's own roots (/var/cache, /var/log, /var/crash, the user's
// caches) legitimately sit outside $HOME and outside any package's files, so
// the uninstaller's $HOME-confinement and package-ownership rules would veto
// the whole job. /usr/local is the admin-managed prefix (the broken-symlink
// scan walks /usr/local/bin) rather than OS-owned, so it is carved out of the
// /usr rule; the macOS Homebrew roots stay denied because they are listed
// explicitly.
bool isSafeForCleaningCanonical(const QString &canonicalPath)
{
    if (isCredentialStore(canonicalPath))
        return false;

    const bool underUsrLocal = isUnderAny(canonicalPath, {QStringLiteral("/usr/local")});
    for (const QString &root : deniedRoots()) {
        if (underUsrLocal && root == QLatin1String("/usr"))
            continue;
        if (isUnderAny(canonicalPath, {root}))
            return false;
    }
    return true;
}

#endif

} // namespace

bool isSafeForCleaning(const QString &path)
{
    if (path.isEmpty())
        return false;

    const QString canonical = canonicalOrAbsolute(path);
    if (canonical.isEmpty() || canonical == QLatin1String("/"))
        return false;

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    return isSafeForCleaningCanonical(canonical);
#else
    return false;
#endif
}

bool isSafe(const QString &path, const QString &homeRootOverride)
{
    if (path.isEmpty())
        return false;

    const QString canonical = canonicalOrAbsolute(path);
    if (canonical.isEmpty() || canonical == QLatin1String("/"))
        return false;

    // Canonicalize the home root so the prefix check below works correctly
    // when homeRootOverride comes from a QTemporaryDir (e.g. /tmp/xxx) and
    // the tested path resolves through a symlink (e.g. /tmp → /private/tmp on
    // macOS), as well as when QStandardPaths returns a non-canonical path.
    const QString rawHome = !homeRootOverride.isEmpty()
        ? homeRootOverride
        : QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    const QString homeRoot = canonicalOrAbsolute(rawHome);

#if defined(Q_OS_MAC)
    return isSafeMacOS(canonical, homeRoot);
#elif defined(Q_OS_LINUX)
    return isSafeLinux(canonical, homeRoot);
#else
    Q_UNUSED(canonical);
    Q_UNUSED(homeRoot);
    return false;
#endif
}

} // namespace LifecycleDenyList
