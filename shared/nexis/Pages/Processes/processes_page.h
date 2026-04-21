#ifndef PROCESSESPAGE_H
#define PROCESSESPAGE_H

#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QDebug>
#include <QScrollBar>
#include <QMenu>
#include <QAction>
#include <QHash>

#include "nexis_page.h"
#include "Managers/info_manager.h"

class DataRefreshService;
class ProcessService;
class PinSortFilterProxyModel;

namespace Ui {
    class ProcessesPage;
}

class ProcessesPage : public NexisPage
{
    Q_OBJECT

public:
    explicit ProcessesPage(QWidget *parent = nullptr,
                           InfoManager *infoManager = nullptr,
                           DataRefreshService *refreshService = nullptr,
                           ProcessService *processService = nullptr);
    ~ProcessesPage();

    void onPageActivated() override;
    void onPageDeactivated() override;

private slots:
    void init();
    void onProcessesUpdated(const QList<Process> &processes, const QString &userName);
    void loadHeaderMenu();
    QList<QStandardItem *> createRow(const Process &proc);
    void updateRow(int row, const Process &proc);
    void on_txtProcessSearch_textChanged(const QString &val);
    void on_sliderRefresh_valueChanged(const int &i);
    void on_btnEndProcess_clicked();
    void on_tableProcess_customContextMenuRequested(const QPoint &pos);

    // FR-116
    void onRowContextMenu(const QPoint &pos);
    void onPinPrefsChanged();

private:
    // FR-108: tell InfoManager whether to collect per-PID disk/net I/O based
    // on the current column visibility. Called from init() and after the
    // header context menu toggles a column.
    void updateProcessIoCollection();

    // FR-116: refresh the pinned-role on every row from ProcessPrefsManager.
    void refreshPinnedRoles();

    // FR-116: evaluate thresholds and fire tray notifications with per-
    // (name, metric) hysteresis.
    void evaluateThresholdAlerts(const QList<Process> &processes);

private:
    Ui::ProcessesPage *ui;

    QStandardItemModel *mItemModel;
    PinSortFilterProxyModel *mSortFilterModel;
    QModelIndex mSelectedRowModel;
    QStringList mHeaders;
    QMenu mHeaderMenu;
    QHash<pid_t, int> mPidToRow;
    QHash<pid_t, QString> mPidToName;   // FR-116
    QHash<QString, bool> mAlertArmed;   // FR-116: "<name>::cpu" / "<name>::mem"
    InfoManager *im;
    DataRefreshService *mRefresh;
    ProcessService *mProcessService;
};

#endif // PROCESSESPAGE_H
