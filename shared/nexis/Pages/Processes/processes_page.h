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
#include "kill_button_delegate.h"

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

    // GH#174: inline kill icon
    void onKillColumnClicked(const QModelIndex &index);

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

    // Logical column indices for the process table. GH#194: the Name column
    // (Linux /proc/<pid>/comm — a short process name distinct from the full
    // command line) exists only on Linux; on macOS it is omitted entirely so
    // there is no permanently-empty column. Defining it conditionally keeps
    // every downstream index correct on both platforms — on macOS the enum
    // collapses to the historical 0..18 layout. The Name column sits right
    // after PID; Command stays last (it is the wide, variable-width column and
    // the search filter target).
    enum Col {
        Col_Pid = 0,
#ifdef Q_OS_LINUX
        Col_Name,
#endif
        Col_Rss,
        Col_Pmem,
        Col_Vsize,
        Col_User,
        Col_Pcpu,
        Col_StartTime,
        Col_State,
        Col_Group,
        Col_Nice,
        Col_CpuTime,
        Col_Session,
        Col_DiskRead,
        Col_DiskWrite,
        Col_NetDown,
        Col_NetUp,
        Col_GpuPct,
        Col_GpuVram,
        Col_Cmd,
        Col_Count   // number of data columns (kill icon follows)
    };

    static constexpr int kKillCol = Col_Count; // GH#174: per-row kill icon column

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
    KillButtonDelegate *mKillDelegate = nullptr; // GH#174
};

#endif // PROCESSESPAGE_H
