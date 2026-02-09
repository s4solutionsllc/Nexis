#ifndef PACKAGE_TOOL_H
#define PACKAGE_TOOL_H

#include "package_tool_shared.h"
#include "stacer-core_global.h"

#include "Utils/command_util.h"
#include "Utils/file_util.h"

class STACERCORESHARED_EXPORT PackageTool
{
public:
    // APT
    static QFileInfoList getDpkgPackageCaches();
    static QList<Package> getDpkgPackages();
    static bool dpkgRemovePackages(QStringList packages, bool purge = false);
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

    // Snap
    static QStringList getSnapPackages();
    static bool snapRemovePackages(QStringList packages);

    static QString friendlySectionName(const QString &section);

    static const PackageTools currentPackageTool;
};

#endif // PACKAGE_TOOL_H
