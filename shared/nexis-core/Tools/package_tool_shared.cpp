// Shared PackageTool methods — implementations that are identical across platforms.
// Platform-specific methods live in linux/nexis-core/Tools/package_tool.cpp
// and macos/nexis-core/Tools/package_tool.cpp.

#include "package_tool_shared.h"

#include <QHash>
#include <QRegularExpression>

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
        // macOS Homebrew sections (harmless on Linux — they just won't match)
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

/********************
 * Static Parsers
 ********************/

QList<StaleSnapRevision> PackageTool::parseSnapListAll(const QString &output)
{
    QList<StaleSnapRevision> revisions;
    const QStringList lines = output.trimmed().split('\n');

    // snap list --all output:
    // Name      Version  Rev   Tracking       Publisher  Notes
    // firefox   130.0    4259  latest/stable  mozilla✓   -
    // firefox   129.0.1  4173  latest/stable  mozilla✓   disabled
    for (const QString &line : lines) {
        if (!line.contains("disabled"))
            continue;

        static const QRegularExpression re(R"(^(\S+)\s+\S+\s+(\d+)\s+)");
        QRegularExpressionMatch match = re.match(line);
        if (!match.hasMatch())
            continue;

        StaleSnapRevision rev;
        rev.name = match.captured(1);
        rev.revision = match.captured(2);
        rev.filePath = QString("/var/lib/snapd/snaps/%1_%2.snap")
                           .arg(rev.name, rev.revision);
        // Size is populated by the caller from QFileInfo
        revisions.append(rev);
    }

    return revisions;
}

QList<OrphanPackage> PackageTool::parseAptAutoremoveDryRun(const QString &output)
{
    QList<OrphanPackage> orphans;
    const QStringList lines = output.trimmed().split('\n');

    // apt-get autoremove --dry-run output contains lines like:
    //   Remv libfoo-dev [1.2.3-1]
    static const QRegularExpression re(R"(^Remv\s+(\S+))");
    for (const QString &line : lines) {
        QRegularExpressionMatch match = re.match(line);
        if (match.hasMatch()) {
            OrphanPackage pkg;
            pkg.name = match.captured(1);
            orphans.append(pkg);
        }
    }

    return orphans;
}

QList<OrphanPackage> PackageTool::parsePacmanOrphans(const QString &output)
{
    QList<OrphanPackage> orphans;
    const QStringList lines = output.trimmed().split('\n');

    // pacman -Qdtq outputs one package name per line
    for (const QString &line : lines) {
        QString name = line.trimmed();
        if (!name.isEmpty()) {
            OrphanPackage pkg;
            pkg.name = name;
            orphans.append(pkg);
        }
    }

    return orphans;
}

QList<OrphanPackage> PackageTool::parseDnfAutoremoveDryRun(const QString &output)
{
    QList<OrphanPackage> orphans;
    const QStringList lines = output.trimmed().split('\n');

    // dnf autoremove --assumeno output has a section:
    //   Removing:
    //    package-name   arch   version   repo   size
    //   ...
    //   Transaction Summary
    bool inRemoveSection = false;
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();

        if (trimmed.startsWith("Removing:") || trimmed.startsWith("Removing dependent packages:") ||
            trimmed.startsWith("Removing unused dependencies:")) {
            inRemoveSection = true;
            continue;
        }

        if (trimmed.startsWith("Transaction Summary") || trimmed.startsWith("Install ") ||
            trimmed.isEmpty() || trimmed.startsWith("=")) {
            if (inRemoveSection && (trimmed.startsWith("Transaction Summary") || trimmed.startsWith("=")))
                inRemoveSection = false;
            continue;
        }

        if (inRemoveSection) {
            QString name = trimmed.split(QRegularExpression("\\s+")).first();
            if (!name.isEmpty() && !name.startsWith("-")) {
                OrphanPackage pkg;
                pkg.name = name;
                orphans.append(pkg);
            }
        }
    }

    return orphans;
}

QList<OrphanPackage> PackageTool::parseBrewAutoremoveDryRun(const QString &output)
{
    QList<OrphanPackage> orphans;
    const QStringList lines = output.trimmed().split('\n');

    // brew autoremove --dry-run output:
    //   ==> Would uninstall:
    //   libfoo
    //   libbar
    bool inUninstallSection = false;
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();

        if (trimmed.contains("Would uninstall") || trimmed.contains("would uninstall")) {
            inUninstallSection = true;
            continue;
        }

        if (inUninstallSection) {
            if (trimmed.isEmpty() || trimmed.startsWith("==>"))
                break;
            OrphanPackage pkg;
            pkg.name = trimmed;
            orphans.append(pkg);
        }
    }

    return orphans;
}
