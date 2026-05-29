#include "package_tool_linux.h"

#include <QDebug>
#include <QRegularExpression>

PackageToolLinux::PackageToolLinux()
{
    currentPackageTool =
        (CommandUtil::isExecutable("apt-get") && CommandUtil::isExecutable("rpm")
            && !CommandUtil::isExecutable("dpkg")) ? APT_RPM :
        CommandUtil::isExecutable("apt-get") ? APT :
        CommandUtil::isExecutable("dnf")     ? DNF :
        CommandUtil::isExecutable("yum")     ? YUM :
        CommandUtil::isExecutable("pacman")  ? PACMAN :
        CommandUtil::isExecutable("zypper")  ? ZYPPER :
                                               UNKNOWN;
}

QList<Package> PackageToolLinux::getPackages()
{
    switch (currentPackageTool) {
    case APT:
        return getDpkgPackages();
    case APT_RPM:
    case YUM:
    case DNF:
        return getRpmPackages();
    case PACMAN:
        return getPacmanPackages();
    default:
        return {};
    }
}

QFileInfoList PackageToolLinux::getPackageCaches()
{
    switch (currentPackageTool) {
    case APT:
    case APT_RPM:
        return getDpkgPackageCaches();
    case YUM:
    case DNF:
        return getYumDnfPackageCaches();
    case PACMAN:
        return getPacmanPackageCaches();
    default:
        return {};
    }
}

void PackageToolLinux::uninstallPackages(const QStringList &packages, bool purge)
{
    switch (currentPackageTool) {
    case APT:
    case APT_RPM:
        dpkgRemovePackages(packages, purge);
        break;
    case YUM:
        yumRemovePackages(packages);
        break;
    case DNF:
        dnfRemovePackages(packages);
        break;
    case PACMAN:
        pacmanRemovePackages(packages);
        break;
    default:
        break;
    }
}

QStringList PackageToolLinux::dryRunRemovePackages(const QStringList &packages)
{
    switch (currentPackageTool) {
    case APT:
    case APT_RPM:
        return dpkgDryRunRemove(packages);
    case YUM:
    case DNF:
        return rpmDryRunRemove(packages);
    case PACMAN:
        return pacmanDryRunRemove(packages);
    default:
        return {};
    }
}

QStringList PackageToolLinux::getSnapPackages()
{
    QStringList packageList = {};

    if (CommandUtil::isExecutable("snap")) {
        try {
            packageList = CommandUtil::exec("snap", {"list"})
                    .trimmed()
                    .split('\n');

            packageList.removeFirst();

            for (int i = 0; i < packageList.count(); ++i)
                packageList[i] = packageList.at(i).split(QRegularExpression("\\s+")).first();

        } catch (QString &ex) {
            qCritical() << ex;
        }
    }

    return packageList;
}

bool PackageToolLinux::uninstallSnapPackages(const QStringList &packages)
{
    try {
        QStringList args = packages;
        args.insert(0, "remove");
        qDebug() << args;

        CommandUtil::sudoExec("snap", args);

        return true;

    } catch(QString &ex) {
        qCritical() << ex;
    }

    return false;
}

QList<Package> PackageToolLinux::getInstalledApps()
{
    return {};
}

bool PackageToolLinux::trashApps(const QStringList &)
{
    return false;
}

/***********
 * DPKG
 ***********/

QFileInfoList PackageToolLinux::getDpkgPackageCaches()
{
    QDir caches("/var/cache/apt/archives/");
    return caches.entryInfoList(QDir::Files);
}

QFileInfoList PackageToolLinux::getYumDnfPackageCaches()
{
    QFileInfoList caches;

    QDir dnfCache("/var/cache/dnf/");
    if (dnfCache.exists())
        caches.append(dnfCache.entryInfoList(QDir::Files, QDir::Size));

    QDir yumCache("/var/cache/yum/");
    if (yumCache.exists())
        caches.append(yumCache.entryInfoList(QDir::Files, QDir::Size));

    return caches;
}

QList<Package> PackageToolLinux::getDpkgPackages()
{
    QList<Package> packages;

    try {
        QString output = CommandUtil::exec("bash", {"-c",
            "dpkg-query -W -f '${Package}\\t${Section}\\t${binary:Summary}\\n' 2> /dev/null"}, {}, 60000)
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

bool PackageToolLinux::dpkgRemovePackages(QStringList packages, bool purge)
{
    try {
        packages.insert(0, purge ? "purge" : "remove");
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
QList<Package> PackageToolLinux::getRpmPackages()
{
    QList<Package> packages;

    try {
        QString output = CommandUtil::exec("bash", {"-c",
            "rpm -qa --queryformat '%{NAME}\\t%{GROUP}\\t%{SUMMARY}\\n' 2> /dev/null"}, {}, 60000)
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

bool PackageToolLinux::dnfRemovePackages(QStringList packages)
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

bool PackageToolLinux::yumRemovePackages(QStringList packages)
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
QFileInfoList PackageToolLinux::getPacmanPackageCaches()
{
    QDir caches("/var/cache/pacman/pkg/");

    return caches.entryInfoList(QDir::Files);
}

QList<Package> PackageToolLinux::getPacmanPackages()
{
    QList<Package> packages;

    try {
        QString output = CommandUtil::exec("bash", {"-c", "pacman -Qi 2> /dev/null"}, {}, 60000)
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
        if (!pkg.name.isEmpty())
            packages.append(pkg);
    } catch(QString &ex) {
        qCritical() << ex;
    }

    return packages;
}

bool PackageToolLinux::pacmanRemovePackages(QStringList packages)
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
 * DRY-RUN
 **********/
QStringList PackageToolLinux::dpkgDryRunRemove(const QStringList &packages)
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

QStringList PackageToolLinux::rpmDryRunRemove(const QStringList &packages)
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

QStringList PackageToolLinux::pacmanDryRunRemove(const QStringList &packages)
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

/**********
 * STALE SNAP REVISIONS (FR-79)
 **********/
QList<StaleSnapRevision> PackageToolLinux::getStaleSnapRevisions()
{
    if (!CommandUtil::isExecutable("snap"))
        return {};

    try {
        QString output = CommandUtil::exec("snap", {"list", "--all"}).trimmed();
        QList<StaleSnapRevision> revisions = PackageTool::parseSnapListAll(output);

        for (int i = 0; i < revisions.size(); ++i) {
            QFileInfo fi(revisions[i].filePath);
            if (fi.exists())
                revisions[i].size = fi.size();
        }

        return revisions;
    } catch (const QString &ex) {
        qCritical() << "Failed to get stale snap revisions:" << ex;
    }
    return {};
}

bool PackageToolLinux::removeStaleSnapRevisions(const QList<StaleSnapRevision> &revisions)
{
    bool allOk = true;
    for (const StaleSnapRevision &rev : revisions) {
        try {
            CommandUtil::sudoExec("snap", {"remove", rev.name, "--revision=" + rev.revision});
        } catch (const QString &ex) {
            qCritical() << "Failed to remove snap revision:" << rev.name << rev.revision << ex;
            allOk = false;
        }
    }
    return allOk;
}

/**********
 * UNUSED FLATPAK RUNTIMES (FR-79)
 **********/
QStringList PackageToolLinux::getUnusedFlatpakRuntimes()
{
    if (!CommandUtil::isExecutable("flatpak"))
        return {};

    try {
        QString output = CommandUtil::exec("flatpak", {"uninstall", "--unused",
                                                        "--noninteractive"}, {}, 30000).trimmed();
        if (output.isEmpty())
            return {};

        QStringList refs;
        const QStringList lines = output.split('\n');
        for (const QString &line : lines) {
            QString trimmed = line.trimmed();
            if (trimmed.isEmpty() || trimmed.startsWith("ID") || trimmed.contains("---"))
                continue;
            // Lines may start with "1. " numbering or just be ref IDs
            static const QRegularExpression refRe(R"((?:\d+\.\s+)?(\S+))");
            QRegularExpressionMatch match = refRe.match(trimmed);
            if (match.hasMatch()) {
                QString ref = match.captured(1);
                if (ref.contains('.'))
                    refs.append(ref);
            }
        }
        return refs;
    } catch (const QString &ex) {
        qCritical() << "Failed to get unused flatpak runtimes:" << ex;
    }
    return {};
}

bool PackageToolLinux::removeUnusedFlatpakRuntimes()
{
    if (!CommandUtil::isExecutable("flatpak"))
        return false;

    try {
        CommandUtil::exec("flatpak", {"uninstall", "--unused", "-y", "--noninteractive"}, {}, 120000);
        return true;
    } catch (const QString &ex) {
        qCritical() << "Failed to remove unused flatpak runtimes:" << ex;
    }
    return false;
}

/**********
 * ORPHAN PACKAGES (FR-80)
 **********/
QList<OrphanPackage> PackageToolLinux::getOrphanPackages()
{
    switch (currentPackageTool) {
    case APT:
    case APT_RPM:
        return getAptOrphans();
    case DNF:
    case YUM:
        return getDnfOrphans();
    case PACMAN:
        return getPacmanOrphans();
    default:
        return {};
    }
}

bool PackageToolLinux::removeOrphanPackages()
{
    try {
        switch (currentPackageTool) {
        case APT:
        case APT_RPM:
            CommandUtil::sudoExec("apt-get", {"autoremove", "-y"});
            return true;
        case DNF:
            CommandUtil::sudoExec("dnf", {"autoremove", "-y"});
            return true;
        case YUM:
            CommandUtil::sudoExec("yum", {"autoremove", "-y"});
            return true;
        case PACMAN: {
            QString output = CommandUtil::exec("pacman", {"-Qdtq"}).trimmed();
            if (output.isEmpty())
                return true;
            QStringList orphans = output.split('\n');
            QStringList args = {"-Rns", "--noconfirm"};
            args.append(orphans);
            CommandUtil::sudoExec("pacman", args);
            return true;
        }
        default:
            return false;
        }
    } catch (const QString &ex) {
        qCritical() << "Failed to remove orphan packages:" << ex;
    }
    return false;
}

QList<OrphanPackage> PackageToolLinux::getAptOrphans()
{
    try {
        QString output = CommandUtil::exec("bash", {"-c", "LANG=C apt-get autoremove --dry-run 2>&1"}).trimmed();
        QList<OrphanPackage> orphans = PackageTool::parseAptAutoremoveDryRun(output);

        if (orphans.isEmpty())
            return orphans;

        // Bulk auto-install flag — one call for all packages
        try {
            QString autoOut = CommandUtil::exec("bash", {"-c", "LANG=C apt-mark showauto 2>/dev/null"}).trimmed();
            QSet<QString> autoSet;
            for (const QString &line : autoOut.split('\n')) {
                QString pkg = line.trimmed();
                if (!pkg.isEmpty())
                    autoSet.insert(pkg);
            }
            for (OrphanPackage &pkg : orphans)
                pkg.autoInstalled = autoSet.contains(pkg.name);
        } catch (...) {}

        // Per-package reverse dependency count
        for (OrphanPackage &pkg : orphans) {
            try {
                QString rdOut = CommandUtil::exec(
                    "bash",
                    {"-c", QString("LANG=C apt-cache rdepends --installed %1 2>/dev/null").arg(pkg.name)}
                ).trimmed();
                int count = 0;
                bool inSection = false;
                for (const QString &line : rdOut.split('\n')) {
                    if (line.trimmed() == QLatin1String("Reverse Depends:")) {
                        inSection = true;
                        continue;
                    }
                    if (inSection && line.startsWith(QLatin1String("  ")) && !line.trimmed().isEmpty())
                        ++count;
                }
                pkg.reverseDepsCount = count;
            } catch (...) {
                pkg.reverseDepsCount = 0;
            }
        }

        return orphans;
    } catch (const QString &ex) {
        qCritical() << "Failed to get apt orphans:" << ex;
    }
    return {};
}

QList<OrphanPackage> PackageToolLinux::getDnfOrphans()
{
    try {
        QString output = CommandUtil::exec("dnf", {"autoremove", "--assumeno"}).trimmed();
        return PackageTool::parseDnfAutoremoveDryRun(output);
    } catch (const QString &ex) {
        qCritical() << "Failed to get dnf orphans:" << ex;
    }
    return {};
}

QList<OrphanPackage> PackageToolLinux::getPacmanOrphans()
{
    try {
        QString output = CommandUtil::exec("pacman", {"-Qdtq"}).trimmed();
        return PackageTool::parsePacmanOrphans(output);
    } catch (const QString &ex) {
        // pacman -Qdtq returns non-zero if no orphans found
        qDebug() << "No pacman orphans or error:" << ex;
    }
    return {};
}

// friendlySectionName() is in shared/nexis-core/Tools/package_tool_shared.cpp
