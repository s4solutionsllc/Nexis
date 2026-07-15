#include "package_tool_macos.h"
#include "Utils/brew_util.h"
#include "Utils/plist_util.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

PackageToolMacOS::PackageToolMacOS()
{
    currentPackageTool = !resolveBrewPath().isEmpty() ? HOMEBREW : UNKNOWN;
}

QString PackageToolMacOS::resolveBrewPath() const
{
    return findBrew();
}

QList<Package> PackageToolMacOS::getPackages()
{
    switch (currentPackageTool) {
    case HOMEBREW:
        return getHomebrewPackages();
    default:
        return {};
    }
}

QFileInfoList PackageToolMacOS::getPackageCaches()
{
    switch (currentPackageTool) {
    case HOMEBREW:
        return getHomebrewCaches();
    default:
        return {};
    }
}

void PackageToolMacOS::uninstallPackages(const QStringList &packages, bool purge)
{
    Q_UNUSED(purge);
    switch (currentPackageTool) {
    case HOMEBREW:
        homebrewRemovePackages(packages);
        break;
    default:
        break;
    }
}

QStringList PackageToolMacOS::dryRunRemovePackages(const QStringList &packages)
{
    switch (currentPackageTool) {
    case HOMEBREW:
        return homebrewDryRunRemove(packages);
    default:
        return {};
    }
}

QStringList PackageToolMacOS::getSnapPackages()
{
    return {};
}

bool PackageToolMacOS::uninstallSnapPackages(const QStringList &packages)
{
    Q_UNUSED(packages);
    return false;
}

/**********
 * HOMEBREW
 **********/

QFileInfoList PackageToolMacOS::getHomebrewCaches()
{
    QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QDir caches(homePath + "/Library/Caches/Homebrew");
    if (!caches.exists()) {
        caches.setPath("/opt/homebrew/var/cache");
    }
    return caches.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
}

QList<Package> PackageToolMacOS::getHomebrewPackages()
{
    QList<Package> packages;

    QString brew = resolveBrewPath();
    if (brew.isEmpty())
        return packages;

    ExecResult result = CommandUtil::execWithStatus(brew, {"info", "--json=v2", "--installed"}, 120000);
    if (!result.ok()) {
        qCritical() << result.error;
        return packages;
    }

    QJsonDocument doc = QJsonDocument::fromJson(result.output.trimmed().toUtf8());

    if (doc.isNull()) {
        qCritical() << "Failed to parse brew info JSON";
        return packages;
    }

    for (const BrewEntry &e : parseBrewJson(doc)) {
        Package pkg;
        pkg.name    = e.identifier;
        pkg.section = e.isCask ? "cask" : "formula";

        if (e.isCask) {
            pkg.description = e.displayName;
            if (!e.description.isEmpty()) {
                if (!pkg.description.isEmpty())
                    pkg.description += QString::fromUtf8(" \u2014 ") + e.description;
                else
                    pkg.description = e.description;
            }
        } else {
            pkg.description = e.description;
        }

        packages.append(pkg);
    }

    return packages;
}

bool PackageToolMacOS::homebrewRemovePackages(QStringList packages)
{
    QString brew = resolveBrewPath();
    if (brew.isEmpty())
        return false;

    packages.insert(0, "uninstall");
    runCommand(brew, packages, 120000);
    return true;
}

QStringList PackageToolMacOS::homebrewDryRunRemove(const QStringList &packages)
{
    QString brew = resolveBrewPath();
    if (brew.isEmpty())
        return packages;

    QStringList wouldRemove;
    for (const QString &pkg : packages) {
        wouldRemove << pkg;
        ExecResult result = CommandUtil::execWithStatus(brew, {"uses", "--installed", pkg});
        if (!result.ok()) {
            qWarning() << "Failed to check brew dependencies for" << pkg << ":" << result.error;
            continue;
        }
        QString deps = result.output.trimmed();
        if (!deps.isEmpty())
            wouldRemove << deps.split('\n');
    }
    return wouldRemove;
}

/**************************
 * macOS native .app bundles
 **************************/

static QList<Package> scanAppDirectory(const QString &dirPath, const QString &section)
{
    QList<Package> apps;
    QDir dir(dirPath);
    if (!dir.exists())
        return apps;

    const QFileInfoList entries = dir.entryInfoList({"*.app"}, QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        const QString appPath = entry.absoluteFilePath();
        const PlistUtil::AppBundleInfo info = PlistUtil::readAppBundleInfo(appPath);

        if (info.bundleId.startsWith("com.apple."))
            continue;

        Package pkg;
        pkg.name = info.displayName.isEmpty()
                       ? entry.completeBaseName()
                       : info.displayName;
        pkg.description = info.version;
        pkg.section = section;
        pkg.path = appPath;
        pkg.bundleId = info.bundleId;   // FR-123: plumbed through for crumbs scanner.

        if (!pkg.name.isEmpty())
            apps.append(pkg);
    }

    return apps;
}

QList<Package> PackageToolMacOS::getInstalledApps()
{
    QList<Package> apps;

    apps.append(scanAppDirectory("/Applications", "applications"));

    QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    apps.append(scanAppDirectory(homePath + "/Applications", "user-applications"));

    std::sort(apps.begin(), apps.end(), [](const Package &a, const Package &b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });

    return apps;
}

bool PackageToolMacOS::trashApps(const QStringList &appPaths)
{
    // SSO-3366 / audit S1: the previous implementation interpolated `path`
    // into an AppleScript source string via `tell application "Finder" to
    // delete POSIX file "%1"` and ran it through `osascript -e`. A bundle
    // whose name contains a double quote (legal on macOS) terminates the
    // string literal and lets attacker-controlled AppleScript follow,
    // including `do shell script`. QFile::moveToTrash binds the path
    // through NSFileManager::trashItemAtURL: on macOS, which takes an
    // NSURL — no shell, no AppleScript parsing surface — so metacharacters
    // in the bundle name are treated as path data, not code.
    bool allOk = true;
    for (const QString &path : appPaths) {
        QString trashedPath;
        if (!QFile::moveToTrash(path, &trashedPath)) {
            qCritical() << "Failed to trash:" << path;
            allOk = false;
        }
    }
    return allOk;
}

/**************************
 * FW-18: leftover artifact scanner
 *
 * Safety contract: every candidate is matched against the exact bundle id
 * (e.g. "com.example.MyApp") or the exact bundle-id-prefixed plist filename.
 * App name is never used as a substring filter to prevent false positives on
 * unrelated bundles that happen to share a word.
 **************************/

static quint64 pathSizeBytes(const QString &path)
{
    QFileInfo fi(path);
    if (!fi.exists())
        return 0;
    if (fi.isFile())
        return static_cast<quint64>(fi.size());

    quint64 total = 0;
    QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += static_cast<quint64>(it.fileInfo().size());
    }
    return total;
}

// Check for <base>/<bundleId> directories and <base>/<bundleId>.plist-style files.
// `nameFilters` is a list of QDir name filters, e.g. {"com.example.App", "com.example.App.*"}.
// Only entries whose names exactly equal bundleId or start with bundleId followed by '.' are
// accepted — no loose substring matching.
static void collectMatches(QList<AppLeftover> &out,
                           const QString &baseDir,
                           const QString &bundleId,
                           const QString &category)
{
    if (bundleId.isEmpty())
        return;

    QDir dir(baseDir);
    if (!dir.exists())
        return;

    // Match: exact name OR name starting with "<bundleId>." (for .plist, .savedState, etc.)
    const QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::Hidden
                                                    | QDir::NoDotAndDotDot);
    for (const QFileInfo &e : entries) {
        const QString name = e.fileName();
        if (name == bundleId || name.startsWith(bundleId + QLatin1Char('.'))) {
            AppLeftover leftover;
            leftover.path = e.absoluteFilePath();
            leftover.category = category;
            leftover.size = pathSizeBytes(leftover.path);
            out.append(leftover);
        }
    }
}

QList<AppLeftover> PackageToolMacOS::findAppLeftovers(const Package &app)
{
    if (app.bundleId.isEmpty())
        return {};

    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    const QString lib  = home + QLatin1String("/Library");

    QList<AppLeftover> leftovers;

    collectMatches(leftovers, lib + QLatin1String("/Application Support"),
                   app.bundleId, QStringLiteral("Application Support"));
    collectMatches(leftovers, lib + QLatin1String("/Caches"),
                   app.bundleId, QStringLiteral("Caches"));
    collectMatches(leftovers, lib + QLatin1String("/Preferences"),
                   app.bundleId, QStringLiteral("Preferences"));
    collectMatches(leftovers, lib + QLatin1String("/Logs"),
                   app.bundleId, QStringLiteral("Logs"));
    collectMatches(leftovers, lib + QLatin1String("/Containers"),
                   app.bundleId, QStringLiteral("Containers"));
    collectMatches(leftovers, lib + QLatin1String("/Saved Application State"),
                   app.bundleId, QStringLiteral("Saved Application State"));
    collectMatches(leftovers, lib + QLatin1String("/LaunchAgents"),
                   app.bundleId, QStringLiteral("LaunchAgents"));

    return leftovers;
}

bool PackageToolMacOS::trashLeftovers(const QStringList &paths)
{
    // Same safe trash path as trashApps() — QFile::moveToTrash uses
    // NSFileManager::trashItemAtURL: which takes an NSURL, not an AppleScript
    // source string, so metacharacters in file names are data, not code.
    bool allOk = true;
    for (const QString &path : paths) {
        QString trashedPath;
        if (!QFile::moveToTrash(path, &trashedPath)) {
            qCritical() << "Failed to trash leftover:" << path;
            allOk = false;
        }
    }
    return allOk;
}

/**********
 * STALE SNAP/FLATPAK (not applicable on macOS)
 **********/
QList<StaleSnapRevision> PackageToolMacOS::getStaleSnapRevisions() { return {}; }
bool PackageToolMacOS::removeStaleSnapRevisions(const QList<StaleSnapRevision> &) { return false; }
QStringList PackageToolMacOS::getUnusedFlatpakRuntimes() { return {}; }
bool PackageToolMacOS::removeUnusedFlatpakRuntimes() { return false; }

/**********
 * ORPHAN PACKAGES (FR-80) — brew autoremove
 **********/
QList<OrphanPackage> PackageToolMacOS::getOrphanPackages()
{
    QString brew = resolveBrewPath();
    if (brew.isEmpty())
        return {};

    ExecResult result = CommandUtil::execWithStatus(brew, {"autoremove", "--dry-run"}, 60000);
    if (!result.ok()) {
        qCritical() << "Failed to get brew orphans:" << result.error;
        return {};
    }
    return PackageTool::parseBrewAutoremoveDryRun(result.output.trimmed());
}

bool PackageToolMacOS::removeOrphanPackages()
{
    QString brew = resolveBrewPath();
    if (brew.isEmpty())
        return false;

    runCommand(brew, {"autoremove"}, 120000);
    return true;
}

// friendlySectionName() is in shared/nexis-core/Tools/package_tool_shared.cpp
