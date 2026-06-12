#ifndef UPDATES_PAGE_H
#define UPDATES_PAGE_H

#include <QWidget>
#include <QTreeWidgetItem>

#include <Info/update_info.h>

class DataRefreshService;
class AppManager;
class SignalMapper;

namespace Ui {
    class UpdatesPage;
}

// SSO-3741 (FW-13): Available-updates surface. Lists outdated packages across
// all detected package managers and supports per-item and "upgrade all" actions.
// Async fetch + progress via DataRefreshService::triggerUpdateCheck(),
// runUpgrade(), and runUpgradeAll().
class UpdatesPage : public QWidget
{
    Q_OBJECT

public:
    explicit UpdatesPage(QWidget *parent = nullptr,
                         DataRefreshService *dataRefreshService = nullptr,
                         AppManager *appManager = nullptr,
                         SignalMapper *signalMapper = nullptr);
    ~UpdatesPage();

private slots:
    void onUpdatesChecked(const UpdateCheckResult &result);
    void onUpgradeStarted(const QString &label);
    void onUpgradeFinished(const QString &label, bool ok, const QString &error);

    void on_btnCheckNow_clicked();
    void on_btnUpgradeSelected_clicked();
    void on_btnUpgradeAll_clicked();
    void on_treeUpdates_itemSelectionChanged();
    void on_txtSearch_textChanged(const QString &val);

private:
    void init();
    void populate(const UpdateCheckResult &result);
    void setLoading(bool loading);
    void setUpgrading(bool upgrading, const QString &label = {});

    Ui::UpdatesPage *ui;
    DataRefreshService *mDrs;
    AppManager *mAppManager;
    SignalMapper *mSignalMapper;

    UpdateCheckResult mLastResult;
    bool mHasResult = false;
};

#endif // UPDATES_PAGE_H
