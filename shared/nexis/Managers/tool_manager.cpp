#include "tool_manager.h"

#ifdef Q_OS_MACOS
#include <Tools/service_tool_macos.h>
#include <Tools/package_tool_macos.h>
#include <Tools/apt_source_tool_macos.h>
#include <Tools/gnome_settings_tool_macos.h>
#else
#include <Tools/service_tool_linux.h>
#include <Tools/package_tool_linux.h>
#include <Tools/apt_source_tool_linux.h>
#include <Tools/gnome_settings_tool_linux.h>
#endif

ToolManager *ToolManager::instance = nullptr;

ToolManager::ToolManager()
{
#ifdef Q_OS_MACOS
    mServiceTool   = std::make_unique<ServiceToolMacOS>();
    mPackageTool   = std::make_unique<PackageToolMacOS>();
    mAptSourceTool = std::make_unique<AptSourceToolMacOS>();
    mGnomeSettings = std::make_unique<GnomeSettingsToolMacOS>();
#else
    mServiceTool   = std::make_unique<ServiceToolLinux>();
    mPackageTool   = std::make_unique<PackageToolLinux>();
    mAptSourceTool = std::make_unique<AptSourceToolLinux>();
    mGnomeSettings = std::make_unique<GnomeSettingsToolLinux>();
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
    return mGnomeSettings->isAvailable()
        && mGnomeSettings->schemaExists(GnomeSchema::INTERFACE);
}

/*
 * APT Source / Homebrew Taps
 */
bool ToolManager::checkSourceRepository() const
{
    return mAptSourceTool->checkSourceRepository();
}

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

void ToolManager::addAPTRepository(const QString &repository, const bool isSource)
{
    mAptSourceTool->addRepository(repository, isSource);
}
