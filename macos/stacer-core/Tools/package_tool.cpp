#include "package_tool.h"

#include <QDebug>
#include <QHash>
#include <QStandardPaths>

const PackageTools PackageTool::currentPackageTool =
        CommandUtil::isExecutable("brew") ? HOMEBREW :
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

    try {
        // Get list of installed formulae with descriptions
        QString output = CommandUtil::exec("brew", {"list", "--formula", "-1"}).trimmed();
        QStringList formulae = output.split('\n', Qt::SkipEmptyParts);

        for (const QString &name : formulae) {
            Package pkg;
            pkg.name = name.trimmed();
            pkg.section = "formula";
            pkg.description = ""; // Description fetched on-demand would be too slow
            if (!pkg.name.isEmpty())
                packages.append(pkg);
        }

        // Also get casks
        output = CommandUtil::exec("brew", {"list", "--cask", "-1"}).trimmed();
        QStringList casks = output.split('\n', Qt::SkipEmptyParts);

        for (const QString &name : casks) {
            Package pkg;
            pkg.name = name.trimmed();
            pkg.section = "cask";
            pkg.description = "";
            if (!pkg.name.isEmpty())
                packages.append(pkg);
        }
    } catch (QString &ex) {
        qCritical() << ex;
    }

    return packages;
}

bool PackageTool::homebrewRemovePackages(QStringList packages)
{
    try {
        packages.insert(0, "uninstall");
        CommandUtil::exec("brew", packages);
        return true;
    } catch (QString &ex) {
        qCritical() << ex;
    }
    return false;
}

QStringList PackageTool::homebrewDryRunRemove(const QStringList &packages)
{
    // Homebrew doesn't have a built-in dry-run for uninstall
    // We can check for dependents that would be affected
    QStringList wouldRemove;
    for (const QString &pkg : packages) {
        wouldRemove << pkg;
        try {
            QString deps = CommandUtil::exec("brew", {"uses", "--installed", pkg}).trimmed();
            if (!deps.isEmpty()) {
                wouldRemove << deps.split('\n');
            }
        } catch (...) {}
    }
    return wouldRemove;
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
