#ifndef UNINSTALLERPAGE_H
#define UNINSTALLERPAGE_H

#include <QStringList>
#include <QWidget>
#include <QListWidgetItem>
#include <QTableWidget>
#include <QTreeWidgetItem>

#include <Tools/package_tool_shared.h>

class PackageService;
class AppManager;
class SignalMapper;

namespace Ui {
    class UninstallerPage;
}

class UninstallerPage : public QWidget
{
    Q_OBJECT

public:
    explicit UninstallerPage(QWidget *parent = nullptr,
                             PackageService *packageService = nullptr,
                             AppManager *appManager = nullptr,
                             SignalMapper *signalMapper = nullptr);
    ~UninstallerPage();

public slots:
    void uninstallStarted();

private:
    void init();

private slots:
    void setAppCount();
    void on_txtPackageSearch_textChanged(const QString &val);
    void on_btnUninstall_clicked();
    QStringList getSelectedPackages();
    QStringList getSelectedSnapPackages();
    QStringList getSelectedFlatpakPackages();
#ifdef Q_OS_MAC
    QStringList getSelectedAppPaths();
    QStringList getSelectedAppBundleIds();   // FR-123
#endif
    void onPackagesLoaded(QList<Package> packages);
    void onSnapPackagesLoaded(QStringList packages);
    void onFlatpakPackagesLoaded(QStringList packages);
    void onOrphanPackagesLoaded(QList<OrphanPackage> packages);
    void on_btnSystemPackages_clicked();
    void on_btnSnapPackages_clicked();
    void on_btnFlatpakPackages_clicked();
    void on_btnOrphanPackages_clicked();
    // SSO-15429: opens OrphanLeftoversDialog, an on-demand filesystem scan —
    // unlike the buttons above, this is not a navBtnGroup stacked-page toggle.
    void on_btnOrphanLeftovers_clicked();
    void on_btnAptHistory_clicked();

    void on_listWidgetSnapPackages_itemClicked(QListWidgetItem *item);
    void on_listWidgetFlatpakPackages_itemClicked(QListWidgetItem *item);
    void refreshOrphanThemeColors();
    void onTreeItemChanged(QTreeWidgetItem *item, int column);

#ifndef Q_OS_MACOS
    // FW-07 (SSO-3735): APT 3.1 transaction history.
    void onAptHistoryFetched(QList<AptHistoryEntry> entries);
    void onAptWhyFetched(QString package, bool whyNot, QStringList reasons);
    void on_btnAptHistoryUndoLast_clicked();
    void on_btnAptHistoryUndoSelected_clicked();
    void on_btnAptHistoryRollback_clicked();
    void on_btnAptWhy_clicked();
    void on_btnAptWhyNot_clicked();
    void onAptHistorySelectionChanged();
#endif

private:
    Ui::UninstallerPage *ui;

    PackageService *mPackageService;
    AppManager *mAppManager;
    SignalMapper *mSignalMapper;

    // FR-123: bundle ids captured at on_btnUninstall_clicked for the
    // macOS path, consumed when sigUninstallFinished fires. Cleared
    // after the review dialog runs.
    QStringList mPendingCrumbBundleIds;
#ifdef Q_OS_LINUX
    // SSO-15385: package names captured at on_btnUninstall_clicked for the
    // Linux leftover-scan path, consumed when sigUninstallFinished fires.
    QStringList mPendingUninstallPackageNames;
#endif
};

#endif // UNINSTALLERPAGE_H
