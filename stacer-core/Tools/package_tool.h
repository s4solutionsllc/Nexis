#ifndef PACKAGE_TOOL_H
#define PACKAGE_TOOL_H

#include <thread>

#include "Utils/command_util.h"
#include "Utils/file_util.h"

#include "stacer-core_global.h"

struct STACERCORESHARED_EXPORT Package {
    QString name;
    QString description;
    QString section;
};

class STACERCORESHARED_EXPORT PackageTool
{
public:
    enum PackageTools {
        APT,        // debian
        DNF,        // fedora
        YUM,        // fedora
        PACMAN,     // arch
        ZYPPER,     // opensuse
        UNKNOWN
    };

public:
    // APT
    static QFileInfoList getDpkgPackageCaches();
    static QList<Package> getDpkgPackages();
    static bool dpkgRemovePackages(QStringList packages);
    static QStringList dpkgDryRunRemove(const QStringList &packages);

    // DNF - YUM
    static QList<Package> getRpmPackages();
    static bool dnfRemovePackages(QStringList packages);
    static bool yumRemovePackages(QStringList packages);
    static QStringList rpmDryRunRemove(const QStringList &packages);

    // Arch
    static QFileInfoList getPacmanPackageCaches();
    static QList<Package> getPacmanPackages();
    static bool pacmanRemovePackages(QStringList packages);
    static QStringList pacmanDryRunRemove(const QStringList &packages);

    static QString friendlySectionName(const QString &section);

    // Snap
    static QStringList getSnapPackages();
    static bool snapRemovePackages(QStringList packages);

    static const PackageTools currentPackageTool;
};

#endif // PACKAGE_TOOL_H
