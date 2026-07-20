#include "leftover_deny_list.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QStandardPaths>
#include <QTextStream>
#include <QUuid>

namespace LeftoverDenyList {

namespace {

// CISO §2: platform deny-list roots. All checks are against canonicalized paths.
bool isDeniedLinux(const QString &path)
{
    // Anything outside $HOME is denied unless on an explicit reviewed allowlist.
    const QString home = QDir::homePath();
    if (!path.startsWith(home + QLatin1Char('/')))
        return true;

    // Credential stores.
    const QStringList credentialRoots = {
        home + "/.local/share/keyrings",
        home + "/.gnupg",
        home + "/.ssh",
        home + "/.pki",
        home + "/.local/share/kwalletd",
    };
    for (const QString &root : credentialRoots) {
        if (path == root || path.startsWith(root + QLatin1Char('/')))
            return true;
    }

    // System-level paths that may appear under $HOME via bind-mounts/symlinks.
    static const QStringList systemPrefixes = {
        "/etc", "/usr", "/bin", "/sbin", "/lib", "/lib64",
        "/boot", "/root", "/run/systemd", "/proc", "/sys",
    };
    for (const QString &prefix : systemPrefixes) {
        if (path == prefix || path.startsWith(prefix + QLatin1Char('/')))
            return true;
    }

    return false;
}

bool isDeniedMacOS(const QString &path)
{
    static const QStringList deniedPrefixes = {
        "/System",
        "/Library/LaunchDaemons",
        "/Library/LaunchAgents",
        "/Library/PrivilegedHelperTools",
        "/usr",
        "/bin",
        "/sbin",
        "/opt/homebrew",
        "/usr/local/Cellar",
        "/usr/local/opt",
    };
    for (const QString &prefix : deniedPrefixes) {
        if (path == prefix || path.startsWith(prefix + QLatin1Char('/')))
            return true;
    }

    // Credential stores.
    const QString home = QDir::homePath();
    const QStringList credentialRoots = {
        home + "/Library/Keychains",
    };
    for (const QString &root : credentialRoots) {
        if (path == root || path.startsWith(root + QLatin1Char('/')))
            return true;
    }

    return false;
}

} // namespace

bool isDenied(const QString &canonicalPath)
{
#if defined(Q_OS_LINUX)
    return isDeniedLinux(canonicalPath);
#elif defined(Q_OS_MACOS)
    return isDeniedMacOS(canonicalPath);
#else
    Q_UNUSED(canonicalPath)
    return true; // safe default on unknown platforms
#endif
}

QString auditLogPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                        + QLatin1String("/audit");
    QDir().mkpath(dir);
    return dir + QLatin1String("/leftover_deletions.log");
}

void logDeletion(const AuditEntry &entry)
{
    static QMutex mutex;
    QMutexLocker lock(&mutex);

    QFile f(auditLogPath());
    if (!f.open(QIODevice::Append | QIODevice::Text))
        return;

    const QDateTime timestamp = entry.timestamp.isValid()
        ? entry.timestamp
        : QDateTime::currentDateTimeUtc();

    QTextStream out(&f);
    out << "time=" << timestamp.toString(Qt::ISODateWithMs)
        << " batch=" << entry.batchId
        << " original=" << entry.originalPath
        << " canonical=" << entry.canonicalPath
        << " action=" << entry.action
        << " trash_dest=" << entry.trashDest
        << " rule=" << entry.matchRule
        << " size=" << entry.sizeBytes
        << " nexis=" << entry.nexisVersion
        << "\n";
}

} // namespace LeftoverDenyList
