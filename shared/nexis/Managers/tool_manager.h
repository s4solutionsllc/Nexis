#ifndef TOOL_MANAGER_H
#define TOOL_MANAGER_H

#include <memory>

#include <Tools/service_tool.h>
#include <Tools/package_tool_shared.h>
#include <Tools/repository_tool.h>
#include <Tools/gnome_settings_tool.h>
#include <Tools/docker_tool.h>
#include <Tools/repo_health_checker.h>
#include <Tools/repo_repair_engine.h>

#ifndef Q_OS_MACOS
#include <Tools/apt_source_tool.h>
#endif

class ToolManager
{
public:
    static ToolManager *ins();

    QList<Service> getServices() const;
    QList<Package> getPackages() const;
    QStringList getSnapPackages() const;
    QStringList getFlatpakPackages() const;
    QFileInfoList getPackageCaches() const;

    bool changeServiceStatus(const QString &sname, bool status) const;
    bool changeServiceActive(const QString &sname, bool status) const;
    bool serviceIsActive(const QString &sname) const;
    bool serviceIsEnabled(const QString &sname) const;

    void uninstallPackages(const QStringList &packages, bool purge = false);
    bool uninstallSnapPackages(const QStringList packages);
    bool uninstallFlatpakPackages(const QStringList packages);
    QStringList dryRunRemovePackages(const QStringList &packages);

    // macOS native .app bundles
    QList<Package> getInstalledApps() const;
    bool trashApps(const QStringList &appPaths);
    bool trashLeftovers(const QStringList &paths);

    // Snap/Flatpak revision cleanup (FR-79)
    QList<StaleSnapRevision> getStaleSnapRevisions() const;
    bool removeStaleSnapRevisions(const QList<StaleSnapRevision> &revisions);
    QStringList getUnusedFlatpakRuntimes() const;
    bool removeUnusedFlatpakRuntimes();

    // Orphan packages (FR-80)
    QList<OrphanPackage> getOrphanPackages() const;
    bool removeOrphanPackages();

#ifndef Q_OS_MACOS
    // APT 3.1 transaction history (FW-07 / SSO-3735)
    bool aptHistorySupported() const;
    QList<AptHistoryEntry> getAptHistory() const;
    AptHistoryEntry getAptHistoryInfo(int transactionId) const;
    QStringList aptWhy(const QString &package, bool whyNot) const;
    bool aptHistoryUndo(int transactionId);
    bool aptHistoryRollback(int transactionId);
#endif

    bool checkGnomeSettings() const;
    bool checkDocker() const;

    // Platform-neutral software-sources surface (APT on Linux, Homebrew on macOS).
    bool checkSourceRepository() const;
    void addRepository(const QString &spec, bool isSource);

    GnomeSettingsTool *gnomeSettings() const { return mGnomeSettings.get(); }
    PackageTool *packageTool() const { return mPackageTool.get(); }
    RepositoryTool *repositoryTool() const { return mRepositoryTool.get(); }
    RepoHealthChecker *repoHealthChecker() const { return mRepoHealthChecker.get(); }
    RepoRepairEngine *repoRepairEngine() const { return mRepoRepairEngine.get(); }

#ifndef Q_OS_MACOS
    // APT-specific surface lives on Linux only — the Homebrew backend doesn't
    // implement these operations, so callers reach them through this accessor
    // instead of through the platform-neutral RepositoryTool.
    AptSourceTool *aptSourceTool() const { return mAptSourceTool; }
    QList<APTSourcePtr> getSourceList() const;
    void removeAPTSource(const APTSourcePtr source);
    void changeAPTStatus(const APTSourcePtr aptSource, const bool status);
    void changeAPTSource(const APTSourcePtr aptSource, const APTSourcePtr newSource);
#endif

private:
    ToolManager();
    static ToolManager *instance;

    std::unique_ptr<ServiceTool> mServiceTool;
    std::unique_ptr<PackageTool> mPackageTool;
    std::unique_ptr<RepositoryTool> mRepositoryTool;
    std::unique_ptr<GnomeSettingsTool> mGnomeSettings;
    std::unique_ptr<RepoHealthChecker> mRepoHealthChecker;
    std::unique_ptr<RepoRepairEngine> mRepoRepairEngine;

#ifndef Q_OS_MACOS
    // Non-owning alias for the APT-specific surface of mRepositoryTool on
    // Linux. Always points at the same object as mRepositoryTool.
    AptSourceTool *mAptSourceTool = nullptr;
#endif
};

#endif // TOOL_MANAGER_H
