#include "tool_manager.h"

ToolManager *ToolManager::instance = NULL;

ToolManager *ToolManager::ins()
{
    if(! instance) {
        instance = new ToolManager;
    }

    return instance;
}

/*
 * Services
 */
QList<Service> ToolManager::getServices() const
{
    return ServiceTool::getServicesWithSystemctl();
}

bool ToolManager::changeServiceStatus(const QString &sname, bool status) const
{
    return ServiceTool::changeServiceStatus(sname, status);
}

bool ToolManager::changeServiceActive(const QString &sname, bool status) const
{
    return ServiceTool::changeServiceActive(sname, status);
}

bool ToolManager::serviceIsActive(const QString &sname) const
{
    return ServiceTool::serviceIsActive(sname);
}

bool ToolManager::serviceIsEnabled(const QString &sname) const
{
    return ServiceTool::serviceIsEnabled(sname);
}

/*
 * Packages
 */
QList<Package> ToolManager::getPackages() const
{
    switch (PackageTool::currentPackageTool) {
    case APT:
        return PackageTool::getDpkgPackages();
    case YUM:
    case DNF:
        return PackageTool::getRpmPackages();
    case PACMAN:
        return PackageTool::getPacmanPackages();
    default:
        return QList<Package>();
    }
}

QStringList ToolManager::getSnapPackages() const
{
    return PackageTool::getSnapPackages();
}

bool ToolManager::uninstallSnapPackages(const QStringList packages)
{
    return PackageTool::snapRemovePackages(packages);
}

QStringList ToolManager::dryRunRemovePackages(const QStringList &packages)
{
    switch (PackageTool::currentPackageTool) {
    case APT:
        return PackageTool::dpkgDryRunRemove(packages);
    case YUM:
    case DNF:
        return PackageTool::rpmDryRunRemove(packages);
    case PACMAN:
        return PackageTool::pacmanDryRunRemove(packages);
    default:
        return QStringList();
    }
}

QFileInfoList ToolManager::getPackageCaches() const
{
    switch (PackageTool::currentPackageTool) {
    case APT:
        return PackageTool::getDpkgPackageCaches();
        break;
    case YUM:
    case DNF:
        return PackageTool::getPacmanPackageCaches();
        break;
    case PACMAN:
        return PackageTool::getPacmanPackageCaches();
        break;
    default:
        return QFileInfoList();
        break;
    }
}

void ToolManager::uninstallPackages(const QStringList &packages, bool purge)
{
    switch (PackageTool::currentPackageTool) {
    case APT:
        PackageTool::dpkgRemovePackages(packages, purge);
        break;
    case YUM:
        PackageTool::yumRemovePackages(packages);
        break;
    case DNF:
        PackageTool::dnfRemovePackages(packages);
        break;
    case PACMAN:
        PackageTool::pacmanRemovePackages(packages);
        break;
    default:
        break;
    }
}

/*
 * GNOME Settings
 */
bool ToolManager::checkGnomeSettings() const
{
    return GnomeSettingsTool::isAvailable() && GnomeSettingsTool::schemaExists(GnomeSchema::INTERFACE);
}

/*
 * APT Source
 */
bool ToolManager::checkSourceRepository() const
{
    return AptSourceTool::checkSourceRepository();
}

QList<APTSourcePtr> ToolManager::getSourceList() const
{
    return AptSourceTool::getSourceList();
}

void ToolManager::removeAPTSource(const APTSourcePtr source)
{
    AptSourceTool::removeAPTSource(source);
}

void ToolManager::changeAPTStatus(const APTSourcePtr aptSource, const bool status)
{
    AptSourceTool::changeStatus(aptSource, status);
}

void ToolManager::changeAPTSource(const APTSourcePtr aptSource, const QString newSource)
{
    AptSourceTool::changeSource(aptSource, newSource);
}

void ToolManager::addAPTRepository(const QString &repository, const bool isSource)
{
    AptSourceTool::addRepository(repository, isSource);
}
