#include "package_tool.h"

#include <QDebug>
#include <QHash>
#include <QRegularExpression>

#ifdef Q_OS_LINUX

const PackageTool::PackageTools PackageTool::currentPackageTool =
        CommandUtil::isExecutable("apt-get") ? PackageTool::APT :
        CommandUtil::isExecutable("dnf")     ? PackageTool::DNF :
        CommandUtil::isExecutable("yum")     ? PackageTool::YUM :
        CommandUtil::isExecutable("pacman")  ? PackageTool::PACMAN :
        CommandUtil::isExecutable("zypper")  ? PackageTool::ZYPPER :
                                               PackageTool::UNKNOWN;

#elif defined(Q_OS_MACOS)

const PackageTool::PackageTools PackageTool::currentPackageTool =
        CommandUtil::isExecutable("brew") ? PackageTool::HOMEBREW :
                                            PackageTool::UNKNOWN;

#endif

/***********
 * DPKG (Linux only)
 ***********/
#ifdef Q_OS_LINUX

QFileInfoList PackageTool::getDpkgPackageCaches()
{
    QDir caches("/var/cache/apt/archives/");
    return caches.entryInfoList(QDir::Files);
}

QList<Package> PackageTool::getDpkgPackages()
{
    QList<Package> packages;

    try {
        QString output = CommandUtil::exec("bash", {"-c",
            "dpkg-query -W -f '${Package}\\t${Section}\\t${binary:Summary}\\n' 2> /dev/null"})
                .trimmed();

        const QStringList lines = output.split('\n');
        for (const QString &line : lines) {
            QStringList parts = line.split('\t');
            if (parts.size() < 3)
                continue;

            Package pkg;
            pkg.name = parts.at(0).trimmed();
            pkg.section = parts.at(1).trimmed();
            pkg.description = parts.at(2).trimmed();

            if (!pkg.name.isEmpty())
                packages.append(pkg);
        }
    } catch(QString &ex) {
        qCritical() << ex;
    }

    return packages;
}

bool PackageTool::dpkgRemovePackages(QStringList packages)
{
    try {
        packages.insert(0, "remove");
        packages.insert(1, "-y");

        CommandUtil::sudoExec("apt-get", packages);

        return true;

    } catch(QString &ex) {
        qCritical() << ex;
    }

    return false;
}

/**********
 * RPM
 **********/
QList<Package> PackageTool::getRpmPackages()
{
    QList<Package> packages;

    try {
        QString output = CommandUtil::exec("bash", {"-c",
            "rpm -qa --queryformat '%{NAME}\\t%{GROUP}\\t%{SUMMARY}\\n' 2> /dev/null"})
                .trimmed();

        const QStringList lines = output.split('\n');
        for (const QString &line : lines) {
            QStringList parts = line.split('\t');
            if (parts.size() < 3)
                continue;

            Package pkg;
            pkg.name = parts.at(0).trimmed();
            pkg.section = parts.at(1).trimmed();
            pkg.description = parts.at(2).trimmed();

            if (!pkg.name.isEmpty())
                packages.append(pkg);
        }
    } catch(QString &ex) {
        qCritical() << ex;
    }

    return packages;
}

bool PackageTool::dnfRemovePackages(QStringList packages)
{
    try {
        packages.insert(0, "remove");
        packages.insert(1, "-y");

        CommandUtil::sudoExec("dnf", packages);

        return true;

    } catch(QString &ex) {
        qCritical() << ex;
    }

    return false;
}

bool PackageTool::yumRemovePackages(QStringList packages)
{
    try {
        packages.insert(0, "remove");
        packages.insert(1, "-y");

        CommandUtil::sudoExec("yum", packages);

        return true;

    } catch(QString &ex) {
        qCritical() << ex;
    }

    return false;
}

/**********
 * PACMAN
 **********/
QFileInfoList PackageTool::getPacmanPackageCaches()
{
    QDir caches("/var/cache/pacman/pkg/");

    return caches.entryInfoList(QDir::Files);
}

QList<Package> PackageTool::getPacmanPackages()
{
    QList<Package> packages;

    try {
        QString output = CommandUtil::exec("bash", {"-c", "pacman -Qi 2> /dev/null"})
                .trimmed();

        const QStringList lines = output.split('\n');
        Package pkg;
        for (const QString &line : lines) {
            if (line.trimmed().isEmpty()) {
                if (!pkg.name.isEmpty())
                    packages.append(pkg);
                pkg = Package();
                continue;
            }
            int colonPos = line.indexOf(':');
            if (colonPos < 0)
                continue;
            QString key = line.left(colonPos).trimmed();
            QString val = line.mid(colonPos + 1).trimmed();
            if (key == "Name")
                pkg.name = val;
            else if (key == "Description")
                pkg.description = val;
            else if (key == "Groups")
                pkg.section = (val == "None") ? "misc" : val;
        }
        // Last block
        if (!pkg.name.isEmpty())
            packages.append(pkg);
    } catch(QString &ex) {
        qCritical() << ex;
    }

    return packages;
}

bool PackageTool::pacmanRemovePackages(QStringList packages)
{
    try {
        packages.push_back("--noconfirm");
        packages.push_back("-R");

        CommandUtil::sudoExec("pacman", packages);

        return true;

    } catch(QString &ex) {
        qCritical() << ex;
    }

    return false;
}

/**********
 * SNAP
 **********/
QStringList PackageTool::getSnapPackages()
{
    QStringList packageList = {};

    if (CommandUtil::isExecutable("snap")) {
        try {
            packageList = CommandUtil::exec("snap", {"list"})
                    .trimmed()
                    .split('\n');

            packageList.removeFirst(); // remove titles e.g name, version

            for (int i = 0; i < packageList.count(); ++i)
                packageList[i] = packageList.at(i).split(QRegularExpression("\\s+")).first();

        } catch (QString &ex) {
            qCritical() << ex;
        }
    }

    return packageList;
}

bool PackageTool::snapRemovePackages(QStringList packages)
{
    try {
        packages.insert(0, "remove");
        qDebug() << packages;

        CommandUtil::sudoExec("snap", packages);

        return true;

    } catch(QString &ex) {
        qCritical() << ex;
    }

    return false;
}

/**********
 * DRY-RUN
 **********/
QStringList PackageTool::dpkgDryRunRemove(const QStringList &packages)
{
    QStringList wouldRemove;
    try {
        QStringList args = {"-c", QString("apt-get remove --dry-run %1 2>&1").arg(packages.join(' '))};
        QString output = CommandUtil::exec("bash", args);

        static QRegularExpression re("^Remv\\s+(\\S+)");
        const QStringList lines = output.split('\n');
        for (const QString &line : lines) {
            QRegularExpressionMatch match = re.match(line);
            if (match.hasMatch())
                wouldRemove << match.captured(1);
        }
    } catch (QString &ex) {
        qCritical() << ex;
    }
    return wouldRemove;
}

QStringList PackageTool::rpmDryRunRemove(const QStringList &packages)
{
    QStringList wouldRemove;
    try {
        QStringList args = packages;
        args.insert(0, "remove");
        args.insert(1, "--assumeno");
        QString output = CommandUtil::exec("dnf", args);

        bool inRemoveSection = false;
        const QStringList lines = output.split('\n');
        for (const QString &line : lines) {
            QString trimmed = line.trimmed();
            if (trimmed.startsWith("Removing:") || trimmed.startsWith("Removing dependent packages:"))
                inRemoveSection = true;
            else if (trimmed.startsWith("Transaction Summary") || trimmed.isEmpty())
                inRemoveSection = false;
            else if (inRemoveSection) {
                QString name = trimmed.split(QRegularExpression("\\s+")).first();
                if (!name.isEmpty())
                    wouldRemove << name;
            }
        }
    } catch (QString &ex) {
        qCritical() << ex;
    }
    return wouldRemove;
}

QStringList PackageTool::pacmanDryRunRemove(const QStringList &packages)
{
    QStringList wouldRemove;
    try {
        QStringList args = packages;
        args.insert(0, "-Rs");
        args.insert(1, "--print");
        QString output = CommandUtil::exec("pacman", args);

        const QStringList lines = output.trimmed().split('\n');
        for (const QString &line : lines) {
            QString name = line.section('/', -1).section('-', 0, 0);
            if (!name.isEmpty())
                wouldRemove << name;
        }
    } catch (QString &ex) {
        qCritical() << ex;
    }
    return wouldRemove;
}

#endif // Q_OS_LINUX

/**********
 * HOMEBREW (macOS only)
 **********/
#ifdef Q_OS_MACOS

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

#endif // Q_OS_MACOS

/********************
 * Section Names (cross-platform)
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
