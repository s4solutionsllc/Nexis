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

void PackageService::fetchFlatpakPackages()
{
    QThreadPool::globalInstance()->start([this]() {
        QStringList packages = mToolManager->getFlatpakPackages();
        emit flatpakPackagesFetched(packages);
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

void PackageService::uninstallFlatpakPackages(const QStringList &packages)
{
    QThreadPool::globalInstance()->start([this, packages]() {
        emit mSignalMapper->sigUninstallStarted();
        mToolManager->uninstallFlatpakPackages(packages);
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

bool PackageService::trashLeftovers(const QStringList &paths)
{
    return mToolManager->trashLeftovers(paths);
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

#ifndef Q_OS_MACOS
/*
 * APT 3.1 transaction history (FW-07 / SSO-3735)
 */
bool PackageService::isAptHistorySupported() const
{
    return mToolManager->aptHistorySupported();
}

void PackageService::fetchAptHistory()
{
    QThreadPool::globalInstance()->start([this]() {
        QList<AptHistoryEntry> entries = mToolManager->getAptHistory();
        emit aptHistoryFetched(entries);
    });
}

void PackageService::fetchAptHistoryInfo(int transactionId)
{
    QThreadPool::globalInstance()->start([this, transactionId]() {
        AptHistoryEntry entry = mToolManager->getAptHistoryInfo(transactionId);
        emit aptHistoryInfoFetched(entry);
    });
}

void PackageService::fetchAptWhy(const QString &package, bool whyNot)
{
    QThreadPool::globalInstance()->start([this, package, whyNot]() {
        QStringList reasons = mToolManager->aptWhy(package, whyNot);
        emit aptWhyFetched(package, whyNot, reasons);
    });
}

void PackageService::aptHistoryUndo(int transactionId)
{
    QThreadPool::globalInstance()->start([this, transactionId]() {
        emit mSignalMapper->sigUninstallStarted();
        mToolManager->aptHistoryUndo(transactionId);
        emit mSignalMapper->sigUninstallFinished();
    });
}

void PackageService::aptHistoryRollback(int transactionId)
{
    QThreadPool::globalInstance()->start([this, transactionId]() {
        emit mSignalMapper->sigUninstallStarted();
        mToolManager->aptHistoryRollback(transactionId);
        emit mSignalMapper->sigUninstallFinished();
    });
}
#endif
