#ifndef PACKAGE_TOOL_MACOS_H
#define PACKAGE_TOOL_MACOS_H

#include <Tools/package_tool_shared.h>
#include "Utils/command_util.h"
#include "Utils/file_util.h"

class PackageToolMacOS : public PackageTool
{
public:
    PackageToolMacOS();

    QList<Package> getPackages() override;
    QFileInfoList getPackageCaches() override;
    void uninstallPackages(const QStringList &packages, bool purge = false) override;
    QStringList dryRunRemovePackages(const QStringList &packages) override;

    QStringList getSnapPackages() override;
    bool uninstallSnapPackages(const QStringList &packages) override;

    QList<Package> getInstalledApps() override;
    bool trashApps(const QStringList &appPaths) override;

private:
    QFileInfoList getHomebrewCaches();
    QList<Package> getHomebrewPackages();
    bool homebrewRemovePackages(QStringList packages);
    QStringList homebrewDryRunRemove(const QStringList &packages);
};

#endif // PACKAGE_TOOL_MACOS_H
