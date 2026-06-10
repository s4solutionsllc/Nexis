#ifndef APTSourceManagerPage_PAGE_H
#define APTSourceManagerPage_PAGE_H

#include <QWidget>
#include <QListWidgetItem>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include "apt_source_repository_item.h"
#include "apt_source_edit.h"
#include "Managers/info_manager.h"
#include <Info/update_info.h>
#include <Tools/repo_health_types.h>
#include <Tools/repo_repair_engine.h>

class RepoDetailPanel;
class QSplitter;
class ToolManager;
class SignalMapper;
class DataRefreshService;

namespace Ui {
class APTSourceManagerPage;
}

class APTSourceManagerPage : public QWidget
{
    Q_OBJECT

public:
    explicit APTSourceManagerPage(QWidget *parent = nullptr,
                                  ToolManager *toolManager = nullptr,
                                  SignalMapper *signalMapper = nullptr,
                                  DataRefreshService *refreshService = nullptr);
    ~APTSourceManagerPage();

public:
    static APTSourcePtr selectedAptSource;

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

    void onSystemUpdatesChecked(const UpdateCheckResult &result);
    void onRepoHealthChecked(const RepoHealthCache &cache);
    void onDetailPanelCloseRequested();
    void onRepairActionRequested(const RepoRepairAction &action, const APTSourcePtr &source);
    void onDiagnoseFinished(const DiagnoseResult &result);

private:
    void init();

private:
    Ui::APTSourceManagerPage *ui;
    ToolManager *mToolManager;
    SignalMapper *mSignalMapper;

    QSharedPointer<APTSourceEdit> mAptSourceEditDialog;

    // Available Updates section
    QWidget *mUpdatesSection = nullptr;
    QLabel *mLblUpdatesTitle = nullptr;
    QPushButton *mBtnCheckNow = nullptr;
    QTreeWidget *mUpdatesTree = nullptr;
    DataRefreshService *mRefresh = nullptr;

    // Health dashboard
    RepoDetailPanel *mDetailPanel = nullptr;
    QSplitter *mSplitter = nullptr;
    RepoHealthCache mHealthCache;
    QPushButton *mBtnRefreshHealth = nullptr;
    bool mDiagnoseRunning = false;
    DiagnoseResult mLastDiagnoseResult;
    bool mHasDiagnoseResult = false;
};

#endif
