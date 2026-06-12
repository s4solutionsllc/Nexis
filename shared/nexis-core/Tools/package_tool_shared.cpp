// Shared PackageTool methods — implementations that are identical across platforms.
// Platform-specific methods live in linux/nexis-core/Tools/package_tool.cpp
// and macos/nexis-core/Tools/package_tool.cpp.

#include "package_tool_shared.h"

#include <QDebug>
#include <QHash>
#include <QRegularExpression>
#include <QSet>

#include "Utils/command_util.h"

// WI-33: default impls delegate to CommandUtil. Test subclasses override
// these to capture (cmd, args) instead of actually running anything.
bool PackageTool::runSudoCommand(const QString &cmd, const QStringList &args)
{
    try {
        CommandUtil::sudoExec(cmd, args);
        return true;
    } catch (const QString &ex) {
        qCritical() << ex;
        return false;
    }
}

QString PackageTool::runCommand(const QString &cmd,
                                const QStringList &args,
                                int timeoutMs)
{
    try {
        return CommandUtil::exec(cmd, args, {}, timeoutMs);
    } catch (const QString &ex) {
        qCritical() << ex;
        return QString();
    }
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

// FW-07 (SSO-3735): APT 3.1 history + why parsers.
//
// APT 3.1 mirrors dnf's transaction-history surface. The textual output isn't
// strictly machine-formatted (no --json yet), so the parsers are forgiving:
// they skip header/separator decoration, recognise the few field labels APT
// is documented to print, and otherwise treat each section as a list of
// package strings. Tests in tests/core/test_apt_history.cpp lock the
// expected outputs from a 26.04 box.

AptVersion PackageTool::parseAptVersion(const QString &output)
{
    AptVersion v;
    // Typical "apt --version" output:
    //   apt 3.1.0 (amd64)
    //   apt 2.8.5 (amd64)
    // Some distributions prefix with "apt-get". Be flexible on the leading word.
    static const QRegularExpression re(R"((?:^|\s)(\d+)\.(\d+)(?:\.(\d+))?)");
    QRegularExpressionMatch m = re.match(output);
    if (!m.hasMatch())
        return v;
    v.major = m.captured(1).toInt();
    v.minor = m.captured(2).toInt();
    v.patch = m.captured(3).isEmpty() ? 0 : m.captured(3).toInt();
    v.valid = true;
    return v;
}

QList<AptHistoryEntry> PackageTool::parseAptHistoryList(const QString &output)
{
    QList<AptHistoryEntry> entries;
    const QStringList lines = output.split('\n');

    // apt history-list prints a dnf-style table:
    //   ID | Date and time        | Operation | Command line
    //   ---+----------------------+-----------+--------------------------
    //   12 | 2026-06-10 14:32:11  | install   | apt install firefox
    //   11 | 2026-06-09 09:15:02  | upgrade   | apt upgrade
    // The exact column count is not stable across versions, so split on '|'
    // and require at least an integer id + a non-empty operation.
    for (const QString &raw : lines) {
        QString line = raw.trimmed();
        if (line.isEmpty())
            continue;
        // skip separator and header rows
        if (line.contains(QRegularExpression(R"(^[-+]+$)")))
            continue;
        QStringList cells = line.split('|');
        for (QString &c : cells)
            c = c.trimmed();
        if (cells.size() < 3)
            continue;
        bool ok = false;
        int id = cells.at(0).toInt(&ok);
        if (!ok)
            continue;
        AptHistoryEntry e;
        e.id = id;
        e.dateTime = cells.at(1);
        e.operation = cells.at(2);
        if (cells.size() >= 4)
            e.commandLine = cells.at(3);
        entries.append(e);
    }
    return entries;
}

AptHistoryEntry PackageTool::parseAptHistoryInfo(const QString &output)
{
    AptHistoryEntry e;
    const QStringList lines = output.split('\n');

    // apt history-info <id> prints a labelled header followed by per-state
    // sections (Installed:, Upgraded:, Removed:, Purged:, Reinstalled:,
    // Downgraded:). Within a section each indented line begins with the
    // package name; trailing version/arch tokens may follow.
    static const QRegularExpression labelRe(R"(^([A-Za-z][\w\s]*?)\s*:\s*(.*)$)");
    static const QSet<QString> sectionLabels = {
        QStringLiteral("Installed"),
        QStringLiteral("Upgraded"),
        QStringLiteral("Removed"),
        QStringLiteral("Purged"),
        QStringLiteral("Reinstalled"),
        QStringLiteral("Downgraded"),
    };

    QString currentSection;
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty()) {
            currentSection.clear();
            continue;
        }

        // Indented continuation of a section: take the first token as pkg name.
        if (raw.startsWith(' ') || raw.startsWith('\t')) {
            if (!currentSection.isEmpty()) {
                const QString name = line.split(QRegularExpression(R"(\s+)")).first();
                // strip :arch suffix so the model holds just the source name
                e.packages << name.section(':', 0, 0);
                continue;
            }
        }

        QRegularExpressionMatch m = labelRe.match(line);
        if (!m.hasMatch()) {
            currentSection.clear();
            continue;
        }
        const QString key = m.captured(1).trimmed();
        const QString val = m.captured(2).trimmed();

        if (key.compare("Transaction ID", Qt::CaseInsensitive) == 0
            || key.compare("ID", Qt::CaseInsensitive) == 0) {
            e.id = val.toInt();
            currentSection.clear();
        } else if (key.compare("Begin time", Qt::CaseInsensitive) == 0
                   || key.compare("Start time", Qt::CaseInsensitive) == 0
                   || key.compare("Date", Qt::CaseInsensitive) == 0) {
            e.dateTime = val;
            currentSection.clear();
        } else if (key.compare("Operation", Qt::CaseInsensitive) == 0
                   || key.compare("Action", Qt::CaseInsensitive) == 0) {
            e.operation = val;
            currentSection.clear();
        } else if (key.compare("Command line", Qt::CaseInsensitive) == 0
                   || key.compare("Command", Qt::CaseInsensitive) == 0) {
            e.commandLine = val;
            currentSection.clear();
        } else if (key.compare("User", Qt::CaseInsensitive) == 0
                   || key.compare("Requested by", Qt::CaseInsensitive) == 0) {
            e.user = val;
            currentSection.clear();
        } else if (sectionLabels.contains(key)) {
            currentSection = key;
            // Inline list on the same line, e.g. "Installed: firefox libfoo"
            if (!val.isEmpty()) {
                for (const QString &tok : val.split(QRegularExpression(R"([\s,]+)"),
                                                    Qt::SkipEmptyParts))
                    e.packages << tok.section(':', 0, 0);
            }
        } else {
            currentSection.clear();
        }
    }

    // Dedupe while preserving order — Installed/Upgraded sections often repeat.
    QStringList uniq;
    QSet<QString> seen;
    for (const QString &p : e.packages) {
        if (!p.isEmpty() && !seen.contains(p)) {
            uniq << p;
            seen.insert(p);
        }
    }
    e.packages = uniq;
    return e;
}

QStringList PackageTool::parseAptWhy(const QString &output)
{
    QStringList reasons;
    const QStringList lines = output.split('\n');

    // apt why / why-not output forms:
    //   firefox  Installed by user
    //   firefox  Required by:
    //     thunderbird depends on firefox
    //   No reason.
    //
    // We collect every non-empty, non-header line trimmed of leading whitespace
    // and surface them verbatim — the UI just shows them as an explanation list.
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith("Reading package lists")
            || line.startsWith("Building dependency tree")
            || line.startsWith("Reading state information"))
            continue;
        reasons << line;
    }
    return reasons;
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
