#include "package_tool.h"
#include "Utils/brew_util.h"

#include <QDebug>
#include <QStandardPaths>

const PackageTools PackageTool::currentPackageTool =
        !findBrew().isEmpty() ? HOMEBREW :
                                UNKNOWN;

/**********
 * HOMEBREW
 **********/

QFileInfoList PackageTool::getHomebrewCaches()
{
    QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QDir caches(homePath + "/Library/Caches/Homebrew");
    if (!caches.exists()) {
        // Try the newer location
        caches.setPath("/opt/homebrew/var/cache");
    }
    return caches.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
}

QList<Package> PackageTool::getHomebrewPackages()
{
    QList<Package> packages;

    QString brew = findBrew();
    if (brew.isEmpty())
        return packages;

    try {
        QString jsonOutput = CommandUtil::exec(brew, {"info", "--json=v2", "--installed"}).trimmed();
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
                // For casks, show displayName + description
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

bool PackageTool::homebrewRemovePackages(QStringList packages)
{
    QString brew = findBrew();
    if (brew.isEmpty())
        return false;

    try {
        packages.insert(0, "uninstall");
        CommandUtil::exec(brew, packages);
        return true;
    } catch (const QString &ex) {
        qCritical() << ex;
    }
    return false;
}

QStringList PackageTool::homebrewDryRunRemove(const QStringList &packages)
{
    QString brew = findBrew();
    if (brew.isEmpty())
        return packages;

    // Homebrew doesn't have a built-in dry-run for uninstall
    // We can check for dependents that would be affected
    QStringList wouldRemove;
    for (const QString &pkg : packages) {
        wouldRemove << pkg;
        try {
            QString deps = CommandUtil::exec(brew, {"uses", "--installed", pkg}).trimmed();
            if (!deps.isEmpty()) {
                wouldRemove << deps.split('\n');
            }
        } catch (...) {}
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

        // Parse Info.plist via plutil → JSON
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
        } catch (...) {
            // plutil failed — fall through to folder-name fallback
        }

        // Filter out Apple system apps
        if (bundleId.startsWith("com.apple."))
            continue;

        Package pkg;
        pkg.name = displayName.isEmpty()
                       ? entry.completeBaseName()  // "Slack" from "Slack.app"
                       : displayName;
        pkg.description = version;
        pkg.section = section;
        pkg.path = appPath;

        if (!pkg.name.isEmpty())
            apps.append(pkg);
    }

    return apps;
}

QList<Package> PackageTool::getInstalledApps()
{
    QList<Package> apps;

    // Scan /Applications (system-wide installs)
    apps.append(scanAppDirectory("/Applications", "applications"));

    // Scan ~/Applications (user-specific installs)
    QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    apps.append(scanAppDirectory(homePath + "/Applications", "user-applications"));

    // Sort alphabetically by name
    std::sort(apps.begin(), apps.end(), [](const Package &a, const Package &b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });

    return apps;
}

bool PackageTool::trashApps(const QStringList &appPaths)
{
    bool allOk = true;
    for (const QString &path : appPaths) {
        try {
            // Use Finder to move to Trash (recoverable, handles admin auth automatically)
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

// friendlySectionName() is in shared/nexis-core/Tools/package_tool_shared.cpp
