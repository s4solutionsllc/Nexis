#include "package_service.h"
#include "Managers/tool_manager.h"
#include "signal_mapper.h"
#include <QThreadPool>

PackageService *PackageService::instance = nullptr;

PackageService *PackageService::ins()
{
    if (!instance)
        instance = new PackageService;
    return instance;
}

PackageService::PackageService(QObject *parent, ToolManager *toolManager, SignalMapper *signalMapper)
    : QObject(parent),
      mToolManager(toolManager ? toolManager : ToolManager::ins()),
      mSignalMapper(signalMapper ? signalMapper : SignalMapper::ins())
{
}

void PackageService::fetchPackages()
{
    QThreadPool::globalInstance()->start([this]() {
#ifdef Q_OS_MAC
        QList<Package> packages = mToolManager->getInstalledApps();
#else
        QList<Package> packages = mToolManager->getPackages();
#endif
        emit packagesFetched(packages);
    });
}

void PackageService::fetchSnapPackages()
{
    QThreadPool::globalInstance()->start([this]() {
        QStringList packages = mToolManager->getSnapPackages();
        emit snapPackagesFetched(packages);
    });
}

void PackageService::uninstallPackages(const QStringList &packages, bool purge)
{
    QThreadPool::globalInstance()->start([this, packages, purge]() {
        emit mSignalMapper->sigUninstallStarted();
        mToolManager->uninstallPackages(packages, purge);
        emit mSignalMapper->sigUninstallFinished();
    });
}

void PackageService::uninstallSnapPackages(const QStringList &packages)
{
    QThreadPool::globalInstance()->start([this, packages]() {
        emit mSignalMapper->sigUninstallStarted();
        mToolManager->uninstallSnapPackages(packages);
        emit mSignalMapper->sigUninstallFinished();
    });
}

void PackageService::trashApps(const QStringList &appPaths)
{
    QThreadPool::globalInstance()->start([this, appPaths]() {
        emit mSignalMapper->sigUninstallStarted();
        mToolManager->trashApps(appPaths);
        emit mSignalMapper->sigUninstallFinished();
    });
}

void PackageService::fetchOrphanPackages()
{
    QThreadPool::globalInstance()->start([this]() {
        QList<OrphanPackage> packages = mToolManager->getOrphanPackages();
        emit orphanPackagesFetched(packages);
    });
}

void PackageService::removeOrphanPackages()
{
    QThreadPool::globalInstance()->start([this]() {
        emit mSignalMapper->sigUninstallStarted();
        mToolManager->removeOrphanPackages();
        emit mSignalMapper->sigUninstallFinished();
    });
}

QStringList PackageService::dryRunRemovePackages(const QStringList &packages)
{
    return mToolManager->dryRunRemovePackages(packages);
}
