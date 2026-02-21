#ifndef UNINSTALLERPAGE_H
#define UNINSTALLERPAGE_H

#include <QWidget>
#include <QListWidgetItem>
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
#ifdef Q_OS_MAC
    QStringList getSelectedAppPaths();
#endif
    void onPackagesLoaded(QList<Package> packages);
    void onSnapPackagesLoaded(QStringList packages);
    void on_btnSystemPackages_clicked();
    void on_btnSnapPackages_clicked();

    void on_listWidgetSnapPackages_itemClicked(QListWidgetItem *item);
    void onTreeItemChanged(QTreeWidgetItem *item, int column);

private:
    Ui::UninstallerPage *ui;

    PackageService *mPackageService;
    AppManager *mAppManager;
    SignalMapper *mSignalMapper;
};

#endif // UNINSTALLERPAGE_H
