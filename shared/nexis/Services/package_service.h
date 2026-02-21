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

    void uninstallPackages(const QStringList &packages, bool purge);
    void uninstallSnapPackages(const QStringList &packages);
    void trashApps(const QStringList &appPaths);

    QStringList dryRunRemovePackages(const QStringList &packages);

signals:
    void packagesFetched(QList<Package> packages);
    void snapPackagesFetched(QStringList packages);

private:
    static PackageService *instance;
    ToolManager *mToolManager;
    SignalMapper *mSignalMapper;
};

#endif // PACKAGE_SERVICE_H
