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

    // SSO-15386 T3: multi-signal orphan-leftover scanner.
    QList<OrphanLeftover> findOrphanLeftovers() override;

    // SSO-15384 / CISO §4: returns true when any process whose executable path
    // is inside bundlePath is currently running.
    bool isAppRunning(const QString &bundlePath) const override;

    // SSO-15566 / CISO §4: graceful quit (never SIGKILL) of every running
    // instance of the app at bundlePath. See macos/nexis-core/Tools/
    // app_quit_helper.mm for the NSRunningApplication::terminate call.
    bool quitApp(const QString &bundlePath) override;

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
