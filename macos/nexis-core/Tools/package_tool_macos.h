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

    // Flatpak apps are Linux-only; macOS never surfaces any.
    QStringList getFlatpakPackages() override;
    bool uninstallFlatpakPackages(const QStringList &refs) override;

    QList<Package> getInstalledApps() override;
    bool trashApps(const QStringList &appPaths) override;

    QList<StaleSnapRevision> getStaleSnapRevisions() override;
    bool removeStaleSnapRevisions(const QList<StaleSnapRevision> &revisions) override;
    QStringList getUnusedFlatpakRuntimes() override;
    bool removeUnusedFlatpakRuntimes() override;
    QList<OrphanPackage> getOrphanPackages() override;
    bool removeOrphanPackages() override;

    // FW-18: leftover artifact scanner for macOS app uninstalls.
    QList<AppLeftover> findAppLeftovers(const Package &app) override;
    bool trashLeftovers(const QStringList &paths) override;

protected:
    // WI-33: seam for tests — overridden to return a fixed fake path so the
    // uninstall paths reach the runCommand seam even when brew isn't installed.
    virtual QString resolveBrewPath() const;

private:
    QFileInfoList getHomebrewCaches();
    QList<Package> getHomebrewPackages();
    bool homebrewRemovePackages(QStringList packages);
    QStringList homebrewDryRunRemove(const QStringList &packages);
};

#endif // PACKAGE_TOOL_MACOS_H
