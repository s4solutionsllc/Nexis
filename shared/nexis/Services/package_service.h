#ifndef PACKAGE_SERVICE_H
#define PACKAGE_SERVICE_H

#include <QObject>
#include <Tools/package_tool_shared.h>

class ToolManager;
class SignalMapper;

class PackageService : public QObject
{
    Q_OBJECT

public:
    static PackageService *ins();

    explicit PackageService(QObject *parent = nullptr,
                            ToolManager *toolManager = nullptr,
                            SignalMapper *signalMapper = nullptr);

    void fetchPackages();
    void fetchSnapPackages();
    void fetchOrphanPackages();

    void uninstallPackages(const QStringList &packages, bool purge);
    void uninstallSnapPackages(const QStringList &packages);
    void trashApps(const QStringList &appPaths);
    void removeOrphanPackages();

    QStringList dryRunRemovePackages(const QStringList &packages);

signals:
    void packagesFetched(QList<Package> packages);
    void snapPackagesFetched(QStringList packages);
    void orphanPackagesFetched(QList<OrphanPackage> packages);

private:
    static PackageService *instance;
    ToolManager *mToolManager;
    SignalMapper *mSignalMapper;
};

#endif // PACKAGE_SERVICE_H
