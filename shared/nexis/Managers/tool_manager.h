#ifndef TOOL_MANAGER_H
#define TOOL_MANAGER_H

#include <memory>

#include <Tools/service_tool.h>
#include <Tools/package_tool_shared.h>
#include <Tools/apt_source_tool.h>
#include <Tools/gnome_settings_tool.h>
#include <Tools/docker_tool.h>

class ToolManager
{
public:
    static ToolManager *ins();

    QList<Service> getServices() const;
    QList<Package> getPackages() const;
    QStringList getSnapPackages() const;
    QFileInfoList getPackageCaches() const;

    bool changeServiceStatus(const QString &sname, bool status) const;
    bool changeServiceActive(const QString &sname, bool status) const;
    bool serviceIsActive(const QString &sname) const;
    bool serviceIsEnabled(const QString &sname) const;

    void uninstallPackages(const QStringList &packages, bool purge = false);
    bool uninstallSnapPackages(const QStringList packages);
    QStringList dryRunRemovePackages(const QStringList &packages);

    // macOS native .app bundles
    QList<Package> getInstalledApps() const;
    bool trashApps(const QStringList &appPaths);

    // Snap/Flatpak revision cleanup (FR-79)
    QList<StaleSnapRevision> getStaleSnapRevisions() const;
    bool removeStaleSnapRevisions(const QList<StaleSnapRevision> &revisions);
    QStringList getUnusedFlatpakRuntimes() const;
    bool removeUnusedFlatpakRuntimes();

    // Orphan packages (FR-80)
    QList<OrphanPackage> getOrphanPackages() const;
    bool removeOrphanPackages();

    bool checkGnomeSettings() const;
    bool checkDocker() const;

    bool checkSourceRepository() const;
    QList<APTSourcePtr> getSourceList() const;
    void removeAPTSource(const APTSourcePtr source);
    void changeAPTStatus(const APTSourcePtr aptSource, const bool status);
    void changeAPTSource(const APTSourcePtr aptSource, const APTSourcePtr newSource);
    void addAPTRepository(const QString &repository, const bool isSource);

    GnomeSettingsTool *gnomeSettings() const { return mGnomeSettings.get(); }
    PackageTool *packageTool() const { return mPackageTool.get(); }

private:
    ToolManager();
    static ToolManager *instance;

    std::unique_ptr<ServiceTool> mServiceTool;
    std::unique_ptr<PackageTool> mPackageTool;
    std::unique_ptr<AptSourceTool> mAptSourceTool;
    std::unique_ptr<GnomeSettingsTool> mGnomeSettings;
};

#endif // TOOL_MANAGER_H
