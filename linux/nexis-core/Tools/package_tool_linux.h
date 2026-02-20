#ifndef PACKAGE_TOOL_LINUX_H
#define PACKAGE_TOOL_LINUX_H

#include <Tools/package_tool_shared.h>
#include "Utils/command_util.h"
#include "Utils/file_util.h"

class PackageToolLinux : public PackageTool
{
public:
    PackageToolLinux();

    QList<Package> getPackages() override;
    QFileInfoList getPackageCaches() override;
    void uninstallPackages(const QStringList &packages, bool purge = false) override;
    QStringList dryRunRemovePackages(const QStringList &packages) override;

    QStringList getSnapPackages() override;
    bool uninstallSnapPackages(const QStringList &packages) override;

    QList<Package> getInstalledApps() override;
    bool trashApps(const QStringList &appPaths) override;

private:
    QFileInfoList getDpkgPackageCaches();
    QList<Package> getDpkgPackages();
    bool dpkgRemovePackages(QStringList packages, bool purge = false);
    QStringList dpkgDryRunRemove(const QStringList &packages);

    QFileInfoList getYumDnfPackageCaches();
    QList<Package> getRpmPackages();
    bool dnfRemovePackages(QStringList packages);
    bool yumRemovePackages(QStringList packages);
    QStringList rpmDryRunRemove(const QStringList &packages);

    QFileInfoList getPacmanPackageCaches();
    QList<Package> getPacmanPackages();
    bool pacmanRemovePackages(QStringList packages);
    QStringList pacmanDryRunRemove(const QStringList &packages);
};

#endif // PACKAGE_TOOL_LINUX_H
