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

#ifndef Q_OS_MACOS
    // APT 3.1 transaction history (FW-07 / SSO-3735). All methods are no-ops
    // on macOS — apt history is APT-specific. On Linux they no-op when apt < 3.1.
    bool isAptHistorySupported() const;
    void fetchAptHistory();
    void fetchAptHistoryInfo(int transactionId);
    void fetchAptWhy(const QString &package, bool whyNot);
    void aptHistoryUndo(int transactionId);
    void aptHistoryRollback(int transactionId);
#endif

signals:
    void packagesFetched(QList<Package> packages);
    void snapPackagesFetched(QStringList packages);
    void orphanPackagesFetched(QList<OrphanPackage> packages);
#ifndef Q_OS_MACOS
    void aptHistoryFetched(QList<AptHistoryEntry> entries);
    void aptHistoryInfoFetched(AptHistoryEntry entry);
    void aptWhyFetched(QString package, bool whyNot, QStringList reasons);
#endif

private:
    static PackageService *instance;
    ToolManager *mToolManager;
    SignalMapper *mSignalMapper;
};

#endif // PACKAGE_SERVICE_H
