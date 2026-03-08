#include "package_tool_macos.h"
#include "Utils/brew_util.h"

#include <QDebug>
#include <QStandardPaths>

PackageToolMacOS::PackageToolMacOS()
{
    currentPackageTool = !findBrew().isEmpty() ? HOMEBREW : UNKNOWN;
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

    QString brew = findBrew();
    if (brew.isEmpty())
        return packages;

    try {
        QString jsonOutput = CommandUtil::exec(brew, {"info", "--json=v2", "--installed"}, {}, 120000).trimmed();
        QJsonDocument doc = QJsonDocument::fromJson(jsonOutput.toUtf8());

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
    } catch (const QString &ex) {
        qCritical() << ex;
    }

    return packages;
}

bool PackageToolMacOS::homebrewRemovePackages(QStringList packages)
{
    QString brew = findBrew();
    if (brew.isEmpty())
        return false;

    try {
        packages.insert(0, "uninstall");
        CommandUtil::exec(brew, packages, {}, 120000);
        return true;
    } catch (const QString &ex) {
        qCritical() << ex;
    }
    return false;
}

QStringList PackageToolMacOS::homebrewDryRunRemove(const QStringList &packages)
{
    QString brew = findBrew();
    if (brew.isEmpty())
        return packages;

    QStringList wouldRemove;
    for (const QString &pkg : packages) {
        wouldRemove << pkg;
        try {
            QString deps = CommandUtil::exec(brew, {"uses", "--installed", pkg}).trimmed();
            if (!deps.isEmpty()) {
                wouldRemove << deps.split('\n');
            }
        } catch (...) { qWarning() << "Failed to check brew dependencies for" << pkg; }
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
        QString appPath = entry.absoluteFilePath();
        QString plistPath = appPath + "/Contents/Info.plist";

        if (!QFileInfo::exists(plistPath))
            continue;

        QString bundleId, displayName, version;
        try {
            QString json = CommandUtil::exec("/usr/bin/plutil",
                {"-convert", "json", "-o", "-", plistPath}).trimmed();
            QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
            if (!doc.isNull()) {
                QJsonObject obj = doc.object();
                bundleId    = obj.value("CFBundleIdentifier").toString();
                displayName = obj.value("CFBundleDisplayName").toString();
                if (displayName.isEmpty())
                    displayName = obj.value("CFBundleName").toString();
                version = obj.value("CFBundleShortVersionString").toString();
            }
        } catch (...) { qWarning() << "Failed to parse Info.plist for" << appPath; }

        if (bundleId.startsWith("com.apple."))
            continue;

        Package pkg;
        pkg.name = displayName.isEmpty()
                       ? entry.completeBaseName()
                       : displayName;
        pkg.description = version;
        pkg.section = section;
        pkg.path = appPath;

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
    bool allOk = true;
    for (const QString &path : appPaths) {
        try {
            QString script = QString("tell application \"Finder\" to delete POSIX file \"%1\"")
                                 .arg(path);
            CommandUtil::exec("/usr/bin/osascript", {"-e", script});
        } catch (const QString &ex) {
            qCritical() << "Failed to trash:" << path << ex;
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
    QString brew = findBrew();
    if (brew.isEmpty())
        return {};

    try {
        QString output = CommandUtil::exec(brew, {"autoremove", "--dry-run"}, {}, 60000).trimmed();
        return PackageTool::parseBrewAutoremoveDryRun(output);
    } catch (const QString &ex) {
        qCritical() << "Failed to get brew orphans:" << ex;
    }
    return {};
}

bool PackageToolMacOS::removeOrphanPackages()
{
    QString brew = findBrew();
    if (brew.isEmpty())
        return false;

    try {
        CommandUtil::exec(brew, {"autoremove"}, {}, 120000);
        return true;
    } catch (const QString &ex) {
        qCritical() << "Failed to brew autoremove:" << ex;
    }
    return false;
}

// friendlySectionName() is in shared/nexis-core/Tools/package_tool_shared.cpp
