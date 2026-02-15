#include "package_tool.h"

#include <QDebug>
#include <QRegularExpression>

const PackageTools PackageTool::currentPackageTool =
        CommandUtil::isExecutable("apt-get") ? APT :
        CommandUtil::isExecutable("dnf")     ? DNF :
        CommandUtil::isExecutable("yum")     ? YUM :
        CommandUtil::isExecutable("pacman")  ? PACMAN :
        CommandUtil::isExecutable("zypper")  ? ZYPPER :
                                               UNKNOWN;

/***********
 * DPKG
 ***********/

QFileInfoList PackageTool::getDpkgPackageCaches()
{
    QDir caches("/var/cache/apt/archives/");
    return caches.entryInfoList(QDir::Files);
}

QFileInfoList PackageTool::getYumDnfPackageCaches()
{
    QFileInfoList caches;

    // DNF cache (Fedora 22+, RHEL 8+, etc.)
    QDir dnfCache("/var/cache/dnf/");
    if (dnfCache.exists())
        caches.append(dnfCache.entryInfoList(QDir::Files, QDir::Size));

    // YUM cache (older Fedora, RHEL 7 and below, CentOS)
    QDir yumCache("/var/cache/yum/");
    if (yumCache.exists())
        caches.append(yumCache.entryInfoList(QDir::Files, QDir::Size));

    return caches;
}

QList<Package> PackageTool::getDpkgPackages()
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

bool PackageTool::dpkgRemovePackages(QStringList packages, bool purge)
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
QList<Package> PackageTool::getRpmPackages()
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

// friendlySectionName() is in shared/nexis-core/Tools/package_tool_shared.cpp
