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

    // SSO-15385: enumerate/uninstall installed Flatpak apps (distinct from
    // getUnusedFlatpakRuntimes(), which cleans up unreferenced runtimes).
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

    // SSO-15385: scan XDG user dirs for leftover files from removed packages.
    QList<AppLeftover> findAppLeftovers(const Package &app) override;
    // Move each path in `paths` to the freedesktop.org Trash. Returns true
    // iff every path was trashed successfully; aborts on deny-list hit.
    bool trashLeftovers(const QStringList &paths) override;

    // SSO-15428 (SSO-15373 §5): multi-signal orphan-leftover scanner — scans
    // ~/.config, ~/.cache, ~/.local/share and requires >= 3 of 4 independent
    // signals before including a result. See package_tool_shared.h for the
    // OrphanLeftover / OrphanSignal structs and the confidence-bar rationale.
    QList<OrphanLeftover> findOrphanLeftovers() override;

    // FW-07 (SSO-3735): APT 3.1 history / why surface.
    bool aptHistorySupported();
    AptVersion aptVersion();
    QList<AptHistoryEntry> getAptHistory();
    AptHistoryEntry getAptHistoryInfo(int transactionId);
    QStringList aptWhy(const QString &package, bool whyNot = false);
    bool aptHistoryUndo(int transactionId);
    bool aptHistoryRedo(int transactionId);
    bool aptHistoryRollback(int transactionId);

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

    QList<OrphanPackage> getAptOrphans();
    QList<OrphanPackage> getDnfOrphans();
    QList<OrphanPackage> getPacmanOrphans();
};

#endif // PACKAGE_TOOL_LINUX_H
