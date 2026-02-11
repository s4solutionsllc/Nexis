#ifndef UNINSTALLERPAGE_H
#define UNINSTALLERPAGE_H

#include <QWidget>
#include <QListWidgetItem>
#include <QTreeWidgetItem>
#include <QtConcurrent>

#include "Managers/tool_manager.h"
#include "Managers/app_manager.h"
#include "signal_mapper.h"

namespace Ui {
    class UninstallerPage;
}

class UninstallerPage : public QWidget
{
    Q_OBJECT

public:
    explicit UninstallerPage(QWidget *parent = 0);
    ~UninstallerPage();

signals:
    void packagesLoadedS();
    void snapPackagesLoadedS();

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
    void fetchPackages();
    void fetchSnapPackages();
    void onPackagesLoaded();
    void onSnapPackagesLoaded();
    void on_btnSystemPackages_clicked();
    void on_btnSnapPackages_clicked();

    void on_listWidgetSnapPackages_itemClicked(QListWidgetItem *item);
    void onTreeItemChanged(QTreeWidgetItem *item, int column);

private:
    Ui::UninstallerPage *ui;

    ToolManager *tm;

    // Thread-safe package data (written on worker, read on main thread)
    QList<Package> mPackages;
    QStringList mSnapPackages;

    // Track background tasks so they can be awaited on shutdown (BUG-05)
    QFuture<void> mFetchFuture;
    QFuture<void> mFetchSnapFuture;
    QFuture<void> mUninstallFuture;
};

#endif // UNINSTALLERPAGE_H
