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
    case HOMEBREW:
        return PackageTool::getHomebrewPackages();
    default:
        return QList<Package>();
    }
}

QStringList ToolManager::getSnapPackages() const
{
    return QStringList();
}

bool ToolManager::uninstallSnapPackages(const QStringList packages)
{
    Q_UNUSED(packages);
    return false;
}

QStringList ToolManager::dryRunRemovePackages(const QStringList &packages)
{
    switch (PackageTool::currentPackageTool) {
    case HOMEBREW:
        return PackageTool::homebrewDryRunRemove(packages);
    default:
        return QStringList();
    }
}

QFileInfoList ToolManager::getPackageCaches() const
{
    switch (PackageTool::currentPackageTool) {
    case HOMEBREW:
        return PackageTool::getHomebrewCaches();
        break;
    default:
        return QFileInfoList();
        break;
    }
}

void ToolManager::uninstallPackages(const QStringList &packages, bool purge)
{
    Q_UNUSED(purge); // Homebrew always removes config files
    switch (PackageTool::currentPackageTool) {
    case HOMEBREW:
        PackageTool::homebrewRemovePackages(packages);
        break;
    default:
        break;
    }
}

/*
 * macOS System Settings — not yet implemented; hide the tab for now.
 */
bool ToolManager::checkGnomeSettings() const
{
    return false;
}

/*
 * Homebrew Taps
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
