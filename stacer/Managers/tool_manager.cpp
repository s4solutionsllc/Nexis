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
#ifdef Q_OS_LINUX
    case PackageTool::PackageTools::APT:
        return PackageTool::getDpkgPackages();
    case PackageTool::PackageTools::YUM:
    case PackageTool::PackageTools::DNF:
        return PackageTool::getRpmPackages();
    case PackageTool::PackageTools::PACMAN:
        return PackageTool::getPacmanPackages();
#endif
#ifdef Q_OS_MACOS
    case PackageTool::PackageTools::HOMEBREW:
        return PackageTool::getHomebrewPackages();
#endif
    default:
        return QList<Package>();
    }
}

QStringList ToolManager::getSnapPackages() const
{
#ifdef Q_OS_LINUX
    return PackageTool::getSnapPackages();
#else
    return QStringList();
#endif
}

bool ToolManager::uninstallSnapPackages(const QStringList packages)
{
#ifdef Q_OS_LINUX
    return PackageTool::snapRemovePackages(packages);
#else
    Q_UNUSED(packages);
    return false;
#endif
}

QStringList ToolManager::dryRunRemovePackages(const QStringList &packages)
{
    switch (PackageTool::currentPackageTool) {
#ifdef Q_OS_LINUX
    case PackageTool::PackageTools::APT:
        return PackageTool::dpkgDryRunRemove(packages);
    case PackageTool::PackageTools::YUM:
    case PackageTool::PackageTools::DNF:
        return PackageTool::rpmDryRunRemove(packages);
    case PackageTool::PackageTools::PACMAN:
        return PackageTool::pacmanDryRunRemove(packages);
#endif
#ifdef Q_OS_MACOS
    case PackageTool::PackageTools::HOMEBREW:
        return PackageTool::homebrewDryRunRemove(packages);
#endif
    default:
        return QStringList();
    }
}

QFileInfoList ToolManager::getPackageCaches() const
{
    switch (PackageTool::currentPackageTool) {
#ifdef Q_OS_LINUX
    case PackageTool::PackageTools::APT:
        return PackageTool::getDpkgPackageCaches();
        break;
    case PackageTool::PackageTools::YUM:
    case PackageTool::PackageTools::DNF:
        return PackageTool::getPacmanPackageCaches();
        break;
    case PackageTool::PackageTools::PACMAN:
        return PackageTool::getPacmanPackageCaches();
        break;
#endif
#ifdef Q_OS_MACOS
    case PackageTool::PackageTools::HOMEBREW:
        return PackageTool::getHomebrewCaches();
        break;
#endif
    default:
        return QFileInfoList();
        break;
    }
}

void ToolManager::uninstallPackages(const QStringList &packages)
{
    switch (PackageTool::currentPackageTool) {
#ifdef Q_OS_LINUX
    case PackageTool::PackageTools::APT:
        PackageTool::dpkgRemovePackages(packages);
        break;
    case PackageTool::PackageTools::YUM:
        PackageTool::yumRemovePackages(packages);
        break;
    case PackageTool::PackageTools::DNF:
        PackageTool::dnfRemovePackages(packages);
        break;
    case PackageTool::PackageTools::PACMAN:
        PackageTool::pacmanRemovePackages(packages);
        break;
#endif
#ifdef Q_OS_MACOS
    case PackageTool::PackageTools::HOMEBREW:
        PackageTool::homebrewRemovePackages(packages);
        break;
#endif
    default:
        break;
    }
}

/*
 * GNOME Settings / macOS System Settings
 */
bool ToolManager::checkGnomeSettings() const
{
#ifdef Q_OS_LINUX
    return GnomeSettingsTool::isAvailable() && GnomeSettingsTool::schemaExists(GnomeSchema::INTERFACE);
#elif defined(Q_OS_MACOS)
    return GnomeSettingsTool::isAvailable();
#else
    return false;
#endif
}

/*
 * APT Source / Homebrew Taps
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
