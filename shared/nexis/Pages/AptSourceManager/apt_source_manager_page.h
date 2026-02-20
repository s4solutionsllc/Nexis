#ifndef APTSourceManagerPage_PAGE_H
#define APTSourceManagerPage_PAGE_H

#include <QWidget>
#include <QListWidgetItem>

#include "apt_source_repository_item.h"
#include "apt_source_edit.h"
#include "Managers/info_manager.h"

class ToolManager;
class SignalMapper;

#ifdef Q_OS_MAC
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QtConcurrent>
#include "Managers/tool_manager.h"
#include "signal_mapper.h"
#endif

namespace Ui {
class APTSourceManagerPage;
}

class APTSourceManagerPage : public QWidget
{
    Q_OBJECT

public:
    explicit APTSourceManagerPage(QWidget *parent = nullptr,
                                  ToolManager *toolManager = nullptr,
                                  SignalMapper *signalMapper = nullptr);
    ~APTSourceManagerPage();

public:
    static APTSourcePtr selectedAptSource;

#ifdef Q_OS_MAC
signals:
    void brewPackagesLoaded();
#endif

private slots:
    void loadAptSources();
    void changeElementsVisible(const bool checked);
    void on_btnAddAPTSourceRepository_clicked(bool checked);
    void on_listWidgetAptSources_itemClicked(QListWidgetItem *item);
    void on_listWidgetAptSources_itemDoubleClicked(QListWidgetItem *item);
    void on_txtSearchAptSource_textChanged(const QString &val);
    void on_btnDeleteAptSource_clicked();
    void on_btnEditAptSource_clicked();
    void on_btnCancel_clicked();

#ifdef Q_OS_MAC
    void fetchBrewPackages();
    void onBrewPackagesLoaded();
    void onTreeItemChanged(QTreeWidgetItem *item, int column);
    QStringList getSelectedBrewPackages();
    void updateBrewUninstallButton();
#endif

private:
    void init();

private:
    Ui::APTSourceManagerPage *ui;
    ToolManager *mToolManager;
    SignalMapper *mSignalMapper;

    QSharedPointer<APTSourceEdit> mAptSourceEditDialog;

#ifdef Q_OS_MAC
    QTreeWidget *mTreeWidget = nullptr;
    QList<Package> mBrewPackages;
#endif
};

#endif
