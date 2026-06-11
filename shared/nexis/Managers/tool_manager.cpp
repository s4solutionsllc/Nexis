#include "tool_manager.h"

#ifdef Q_OS_MACOS
#include <Tools/service_tool_macos.h>
#include <Tools/package_tool_macos.h>
#include <Tools/homebrew_tool_macos.h>
#include <Tools/gnome_settings_tool_macos.h>
#include <Tools/repo_health_checker_macos.h>
#include <Tools/repo_repair_engine_macos.h>
#else
#include <Tools/service_tool_linux.h>
#include <Tools/package_tool_linux.h>
#include <Tools/apt_source_tool_linux.h>
#include <Tools/gnome_settings_tool_linux.h>
#include <Tools/repo_health_checker_linux.h>
#include <Tools/repo_repair_engine_linux.h>
#endif

ToolManager *ToolManager::instance = nullptr;

ToolManager::ToolManager()
{
#ifdef Q_OS_MACOS
    mServiceTool       = std::make_unique<ServiceToolMacOS>();
    mPackageTool       = std::make_unique<PackageToolMacOS>();
    mRepositoryTool    = std::make_unique<HomebrewToolMacOS>();
    mGnomeSettings     = std::make_unique<GnomeSettingsToolMacOS>();
    mRepoHealthChecker = std::make_unique<RepoHealthCheckerMac>();
    mRepoRepairEngine  = std::make_unique<RepoRepairEngineMac>();
#else
    mServiceTool       = std::make_unique<ServiceToolLinux>();
    mPackageTool       = std::make_unique<PackageToolLinux>();
    auto aptTool       = std::make_unique<AptSourceToolLinux>();
    // mAptSourceTool is a non-owning view of the object owned by mRepositoryTool.
    mAptSourceTool     = aptTool.get();
    mRepositoryTool    = std::move(aptTool);
    mGnomeSettings     = std::make_unique<GnomeSettingsToolLinux>();
    mRepoHealthChecker = std::make_unique<RepoHealthCheckerLinux>();
    mRepoRepairEngine  = std::make_unique<RepoRepairEngineLinux>();
#endif
}

ToolManager *ToolManager::ins()
{
    if (!instance) {
        instance = new ToolManager;
    }
    return instance;
}

/*
 * Services
 */
QList<Service> ToolManager::getServices() const
{
    return mServiceTool->getServices();
}

bool ToolManager::changeServiceStatus(const QString &sname, bool status) const
{
    return mServiceTool->changeServiceStatus(sname, status);
}

bool ToolManager::changeServiceActive(const QString &sname, bool status) const
{
    return mServiceTool->changeServiceActive(sname, status);
}

bool ToolManager::serviceIsActive(const QString &sname) const
{
    return mServiceTool->serviceIsActive(sname);
}

bool ToolManager::serviceIsEnabled(const QString &sname) const
{
    return mServiceTool->serviceIsEnabled(sname);
}

/*
 * Packages
 */
QList<Package> ToolManager::getPackages() const
{
    return mPackageTool->getPackages();
}

QStringList ToolManager::getSnapPackages() const
{
    return mPackageTool->getSnapPackages();
}

bool ToolManager::uninstallSnapPackages(const QStringList packages)
{
    return mPackageTool->uninstallSnapPackages(packages);
}

QStringList ToolManager::dryRunRemovePackages(const QStringList &packages)
{
    return mPackageTool->dryRunRemovePackages(packages);
}

QFileInfoList ToolManager::getPackageCaches() const
{
    return mPackageTool->getPackageCaches();
}

void ToolManager::uninstallPackages(const QStringList &packages, bool purge)
{
    mPackageTool->uninstallPackages(packages, purge);
}

/*
 * macOS native .app bundles
 */
QList<Package> ToolManager::getInstalledApps() const
{
    return mPackageTool->getInstalledApps();
}

bool ToolManager::trashApps(const QStringList &appPaths)
{
    return mPackageTool->trashApps(appPaths);
}

/*
 * Snap/Flatpak revision cleanup (FR-79)
 */
QList<StaleSnapRevision> ToolManager::getStaleSnapRevisions() const
{
    return mPackageTool->getStaleSnapRevisions();
}

bool ToolManager::removeStaleSnapRevisions(const QList<StaleSnapRevision> &revisions)
{
    return mPackageTool->removeStaleSnapRevisions(revisions);
}

QStringList ToolManager::getUnusedFlatpakRuntimes() const
{
    return mPackageTool->getUnusedFlatpakRuntimes();
}

bool ToolManager::removeUnusedFlatpakRuntimes()
{
    return mPackageTool->removeUnusedFlatpakRuntimes();
}

/*
 * Orphan packages (FR-80)
 */
QList<OrphanPackage> ToolManager::getOrphanPackages() const
{
    return mPackageTool->getOrphanPackages();
}

bool ToolManager::removeOrphanPackages()
{
    return mPackageTool->removeOrphanPackages();
}

#ifndef Q_OS_MACOS
/*
 * APT 3.1 transaction history (FW-07 / SSO-3735)
 *
 * Linux-only — APT history is an APT-specific surface, so we down-cast to
 * the Linux platform tool through a static_cast (mPackageTool always points
 * at a PackageToolLinux on non-macOS builds). The aptHistorySupported() gate
 * means callers can safely no-op when apt < 3.1, which keeps the rest of the
 * UI code free of platform/version conditionals.
 */
bool ToolManager::aptHistorySupported() const
{
    return static_cast<PackageToolLinux*>(mPackageTool.get())->aptHistorySupported();
}

QList<AptHistoryEntry> ToolManager::getAptHistory() const
{
    return static_cast<PackageToolLinux*>(mPackageTool.get())->getAptHistory();
}

AptHistoryEntry ToolManager::getAptHistoryInfo(int transactionId) const
{
    return static_cast<PackageToolLinux*>(mPackageTool.get())->getAptHistoryInfo(transactionId);
}

QStringList ToolManager::aptWhy(const QString &package, bool whyNot) const
{
    return static_cast<PackageToolLinux*>(mPackageTool.get())->aptWhy(package, whyNot);
}

bool ToolManager::aptHistoryUndo(int transactionId)
{
    return static_cast<PackageToolLinux*>(mPackageTool.get())->aptHistoryUndo(transactionId);
}

bool ToolManager::aptHistoryRollback(int transactionId)
{
    return static_cast<PackageToolLinux*>(mPackageTool.get())->aptHistoryRollback(transactionId);
}
#endif

/*
 * Docker
 */
bool ToolManager::checkDocker() const
{
    return DockerTool::isDockerInstalled();
}

/*
 * GNOME / System Settings
 */
bool ToolManager::checkGnomeSettings() const
{
#ifdef Q_OS_MACOS
    // SSO-3391 / WI-29: GNOME Settings has no valid mapping on macOS — the
    // page is hidden in the sidebar and the macOS GnomeSettingsTool is a
    // hard no-op stub. Short-circuit here so a stub regression can't flip
    // availability back on.
    return false;
#else
    return mGnomeSettings->isAvailable()
        && mGnomeSettings->schemaExists(GnomeSchema::INTERFACE);
#endif
}

/*
 * Software sources (APT on Linux, Homebrew on macOS)
 */
bool ToolManager::checkSourceRepository() const
{
    return mRepositoryTool->isAvailable();
}

void ToolManager::addRepository(const QString &spec, bool isSource)
{
    mRepositoryTool->addRepository(spec, isSource);
}

#ifndef Q_OS_MACOS
QList<APTSourcePtr> ToolManager::getSourceList() const
{
    return mAptSourceTool->getSourceList();
}

void ToolManager::removeAPTSource(const APTSourcePtr source)
{
    mAptSourceTool->removeAPTSource(source);
}

void ToolManager::changeAPTStatus(const APTSourcePtr aptSource, const bool status)
{
    mAptSourceTool->changeStatus(aptSource, status);
}

void ToolManager::changeAPTSource(const APTSourcePtr aptSource, const APTSourcePtr newSource)
{
    mAptSourceTool->changeSource(aptSource, newSource);
}
#endif
