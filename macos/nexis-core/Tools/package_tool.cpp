#include "package_tool.h"

#include <QDebug>
#include <QHash>
#include <QStandardPaths>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

// Homebrew binary path — checked at well-known locations since GUI apps
// don't inherit the user's shell PATH.
static QString findBrew()
{
    for (const QString &path : {"/opt/homebrew/bin/brew", "/usr/local/bin/brew"}) {
        if (QFileInfo(path).isExecutable())
            return path;
    }
    return QString();
}

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
        // Single call to get metadata (name + description) for all installed packages
        QString jsonOutput = CommandUtil::exec(brew, {"info", "--json=v2", "--installed"}).trimmed();
        QJsonDocument doc = QJsonDocument::fromJson(jsonOutput.toUtf8());

        if (doc.isNull()) {
            qCritical() << "Failed to parse brew info JSON";
            return packages;
        }

        QJsonObject root = doc.object();

        // Installed formulae
        QJsonArray formulae = root.value("formulae").toArray();
        for (const QJsonValue &val : formulae) {
            QJsonObject obj = val.toObject();
            Package pkg;
            pkg.name = obj.value("name").toString();
            pkg.section = "formula";
            pkg.description = obj.value("desc").toString();
            if (!pkg.name.isEmpty())
                packages.append(pkg);
        }

        // Installed casks
        QJsonArray casks = root.value("casks").toArray();
        for (const QJsonValue &val : casks) {
            QJsonObject obj = val.toObject();
            Package pkg;
            pkg.name = obj.value("token").toString();
            pkg.section = "cask";
            // Cask "name" is the human-friendly name (e.g. "AltTab")
            QJsonArray names = obj.value("name").toArray();
            pkg.description = names.isEmpty() ? "" : names.first().toString();
            // Append the description text if available
            QString desc = obj.value("desc").toString();
            if (!desc.isEmpty()) {
                if (!pkg.description.isEmpty())
                    pkg.description += QString::fromUtf8(" \u2014 ") + desc;
                else
                    pkg.description = desc;
            }
            if (!pkg.name.isEmpty())
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

/********************
 * Section Names
 ********************/
QString PackageTool::friendlySectionName(const QString &section)
{
    static const QHash<QString, QString> map = {
        {"libs", "Libraries"}, {"libdevel", "Development Libraries"},
        {"python", "Python"}, {"perl", "Perl"}, {"ruby", "Ruby"},
        {"net", "Network"}, {"web", "Web"},
        {"admin", "Administration"}, {"utils", "Utilities"},
        {"text", "Text Processing"}, {"editors", "Editors"},
        {"devel", "Development"}, {"debug", "Debug"},
        {"doc", "Documentation"}, {"fonts", "Fonts"},
        {"games", "Games"}, {"gnome", "GNOME"},
        {"graphics", "Graphics"}, {"sound", "Sound & Audio"},
        {"video", "Video"}, {"mail", "Mail"},
        {"math", "Mathematics"}, {"science", "Science"},
        {"database", "Database"}, {"httpd", "Web Server"},
        {"interpreters", "Interpreters"}, {"kernel", "Kernel"},
        {"misc", "Miscellaneous"}, {"oldlibs", "Legacy Libraries"},
        {"x11", "X11"}, {"xfce", "Xfce"},
        {"kde", "KDE"}, {"java", "Java"},
        {"comm", "Communication"}, {"electronics", "Electronics"},
        {"embedded", "Embedded"}, {"otherosfs", "Other OS & FS"},
        {"shells", "Shells"}, {"localization", "Localization"},
        {"introspection", "Introspection"}, {"cli-mono", "Mono/.NET"},
        {"vcs", "Version Control"}, {"zope", "Zope"},
        {"php", "PHP"}, {"lisp", "Lisp"},
        {"ocaml", "OCaml"}, {"haskell", "Haskell"},
        {"rust", "Rust"}, {"golang", "Go"},
        // macOS Homebrew sections
        {"formula", "Homebrew Formula"}, {"cask", "Homebrew Cask"},
        // macOS native application sections
        {"applications", "Applications"}, {"user-applications", "User Applications"},
    };

    // Exact match
    if (map.contains(section))
        return map.value(section);

    // Handle composite sections like "universe/libs"
    QString last = section.section('/', -1);
    if (map.contains(last))
        return map.value(last);

    // Capitalize as fallback
    if (section.isEmpty())
        return "Other";

    QString f = section;
    f[0] = f[0].toUpper();
    return f;
}
