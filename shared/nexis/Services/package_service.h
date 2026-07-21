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
    void fetchFlatpakPackages();
    void fetchOrphanPackages();

    void uninstallPackages(const QStringList &packages, bool purge);
    void uninstallSnapPackages(const QStringList &packages);
    void uninstallFlatpakPackages(const QStringList &packages);
    void trashApps(const QStringList &appPaths);
    // SSO-15385: move leftover files to the platform Trash (Linux:
    // freedesktop.org Trash; macOS: NSFileManager trashItemAtURL).
    // CISO §1/§2/§3 (SSO-15373) enforcement is inside PackageTool::trashLeftovers.
    bool trashLeftovers(const QStringList &paths);
    void removeOrphanPackages();
    // SSO-15566 / CISO §4: synchronous — called from the UI thread by the
    // pre-uninstall running-process gate and its blocking warn/quit dialog.
    bool isAppRunning(const QString &bundlePath) const;
    bool quitApp(const QString &bundlePath);

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
    void flatpakPackagesFetched(QStringList packages);
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
