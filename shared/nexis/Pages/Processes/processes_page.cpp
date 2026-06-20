#include "processes_page.h"
#include "ui_processes_page.h"
#include "utilities.h"
#include "nexis_roles.h"
#include "dpi.h"
#include "pin_sort_filter_proxy_model.h"
#include "kill_button_delegate.h"
#include "process_alert_dialog.h"
#include "Managers/app_manager.h"
#include "Managers/data_refresh_service.h"
#include "Managers/process_prefs_manager.h"
#include "Services/process_service.h"
#include <QItemSelectionModel>
#include <QRegularExpression>
#include <QSystemTrayIcon>
#include <Utils/format_util.h>

ProcessesPage::~ProcessesPage()
{
    delete ui;
}

ProcessesPage::ProcessesPage(QWidget *parent, InfoManager *infoManager,
                               DataRefreshService *refreshService,
                               ProcessService *processService) :
  NexisPage(parent),
  ui(new Ui::ProcessesPage),
  mItemModel(new QStandardItemModel(this)),
  mSortFilterModel(new PinSortFilterProxyModel(this)),
  im(infoManager ? infoManager : InfoManager::ins()),
  mRefresh(refreshService ? refreshService : DataRefreshService::ins()),
  mProcessService(processService ? processService : ProcessService::ins())
{
    ui->setupUi(this);

    init();
}

void ProcessesPage::init()
{
    mHeaders = QStringList {
        "PID", tr("Resident Memory"), tr("%Memory"), tr("Virtual Memory"),
        tr("User"), "%CPU", tr("Start Time"), tr("State"), tr("Group"),
        tr("Nice"), tr("CPU Time"), tr("Session"),
        tr("Disk Read/s"), tr("Disk Write/s"), tr("Net Down/s"), tr("Net Up/s"),
        tr("GPU %"), tr("GPU VRAM"),            // FR-115 (indices 16, 17)
        tr("Process")                            // index 18; Kill icon is 19 (kKillCol, not in mHeaders)
    };

    // slider settings
    ui->sliderRefresh->setRange(1, 10);
    ui->sliderRefresh->setPageStep(1);
    ui->sliderRefresh->setSingleStep(1);

    // Table settings
    mSortFilterModel->setSourceModel(mItemModel);

    mItemModel->setHorizontalHeaderLabels(mHeaders);

    ui->tableProcess->setModel(mSortFilterModel);

    ui->btnEndProcess->setVisible(false);
    connect(ui->tableProcess->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this](const QItemSelection &selected, const QItemSelection &) {
        ui->btnEndProcess->setVisible(!selected.isEmpty());
    });

    mSortFilterModel->setSortRole(SortRole);
    mSortFilterModel->setDynamicSortFilter(true);
    mSortFilterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    mSortFilterModel->sort(5, Qt::SortOrder::DescendingOrder);

    ui->tableProcess->horizontalHeader()->setSectionsMovable(true);
    ui->tableProcess->horizontalHeader()->setFixedHeight(Dpi::scale(36));
    ui->tableProcess->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui->tableProcess->horizontalHeader()->setCursor(Qt::PointingHandCursor);
    ui->tableProcess->horizontalHeader()->resizeSection(0, 70);

    // GH#174: Kill column — fixed narrow width, not shown in header context menu
    // (not in mHeaders so loadHeaderMenu() ignores it), not sortable.
    mItemModel->setHorizontalHeaderItem(kKillCol, new QStandardItem());
    ui->tableProcess->horizontalHeader()->setSectionResizeMode(kKillCol, QHeaderView::Fixed);
    ui->tableProcess->horizontalHeader()->resizeSection(kKillCol, Dpi::scale(30));
    mKillDelegate = new KillButtonDelegate(this);
    ui->tableProcess->setItemDelegateForColumn(kKillCol, mKillDelegate);
    ui->tableProcess->setMouseTracking(true);
    connect(ui->tableProcess, &QTableView::clicked,
            this, &ProcessesPage::onKillColumnClicked);

    connect(mRefresh, &DataRefreshService::processesUpdated,
            this, &ProcessesPage::onProcessesUpdated);

    ui->tableProcess->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->tableProcess->horizontalHeader(), &QHeaderView::customContextMenuRequested,
        this, &ProcessesPage::on_tableProcess_customContextMenuRequested);

    // FR-116: row-level context menu (distinct from the header menu above).
    ui->tableProcess->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tableProcess, &QWidget::customContextMenuRequested,
            this, &ProcessesPage::onRowContextMenu);

    // FR-116: re-render when pins/thresholds change from elsewhere.
    connect(ProcessPrefsManager::ins(), &ProcessPrefsManager::changed,
            this, &ProcessesPage::onPinPrefsChanged);

    loadHeaderMenu();

    // FR-108: apply the initial column-visibility state to ProcessInfo
    // before the first tick so we don't pay the /proc/<pid>/io walk or the
    // nettop fork when their columns are hidden (which is the default).
    updateProcessIoCollection();

    Utilities::addDropShadow(ui->btnEndProcess, 60);
    Utilities::addDropShadow(ui->tableProcess, 55);
}

void ProcessesPage::updateProcessIoCollection()
{
    const auto *header = ui->tableProcess->horizontalHeader();
    // Columns: 12=Disk Read/s, 13=Disk Write/s, 14=Net Down/s, 15=Net Up/s,
    //          16=GPU %, 17=GPU VRAM.
    const bool diskVisible = !header->isSectionHidden(12) || !header->isSectionHidden(13);
    const bool netVisible  = !header->isSectionHidden(14) || !header->isSectionHidden(15);
    const bool gpuVisible  = !header->isSectionHidden(16) || !header->isSectionHidden(17);
    im->setCollectProcessDiskIO(diskVisible);
    im->setCollectProcessNetIO(netVisible);
    im->setCollectProcessGpu(gpuVisible);
}

void ProcessesPage::loadHeaderMenu()
{
    int i = 0;
    QList<QAction*> actionList;
    actionList.reserve(mHeaders.size());
    for (const QString &header : mHeaders) {
        QAction *action = new QAction(header,&mHeaderMenu);
        action->setCheckable(true);
        action->setChecked(true);
        action->setData(i++);
        actionList.push_back(action);

    }
    mHeaderMenu.addActions(actionList);
    // exclude headers — GPU %/VRAM (16,17) hidden by default, same pattern as disk/net I/O.
    QList<int> hiddenHeaders = { 3, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17 };

    QList<QAction*> actions = mHeaderMenu.actions();
    for (const int i : hiddenHeaders) {
        if (i < mHeaders.count()) {
            ui->tableProcess->horizontalHeader()->setSectionHidden(i, true);
            actions.at(i)->setChecked(false);
        }
    }
}

void ProcessesPage::onProcessesUpdated(const QList<Process> &processes, const QString &userName)
{
    QModelIndexList selecteds = ui->tableProcess->selectionModel()->selectedRows();
    pid_t selectedPid = 0;
    if (!selecteds.isEmpty())
        selectedPid = selecteds.first().data(SortRole).toInt();

    bool showAll = ui->checkAllProcesses->isChecked();
    QSet<pid_t> incomingPids;

    for (const Process &proc : processes) {
        if (!showAll && userName != proc.getUname())
            continue;

        pid_t pid = proc.getPid();
        incomingPids.insert(pid);
        mPidToName.insert(pid, proc.getCmd());   // FR-116

        auto it = mPidToRow.find(pid);
        if (it != mPidToRow.end()) {
            updateRow(it.value(), proc);
        } else {
            int newRow = mItemModel->rowCount();
            mItemModel->appendRow(createRow(proc));
            mPidToRow.insert(pid, newRow);
        }
    }

    // Remove exited processes (iterate in reverse to keep row indices valid)
    QList<int> rowsToRemove;
    for (auto it = mPidToRow.begin(); it != mPidToRow.end(); ++it) {
        if (!incomingPids.contains(it.key()))
            rowsToRemove.append(it.value());
    }
    std::sort(rowsToRemove.begin(), rowsToRemove.end(), std::greater<int>());
    for (int row : rowsToRemove) {
        pid_t pid = mItemModel->item(row, 0)->data(SortRole).toLongLong();
        mItemModel->removeRow(row);
        mPidToRow.remove(pid);
        mPidToName.remove(pid);   // FR-116
    }

    // Rebuild PID→row map after removals (row indices shifted)
    mPidToRow.clear();
    for (int i = 0; i < mItemModel->rowCount(); ++i) {
        pid_t pid = mItemModel->item(i, 0)->data(SortRole).toLongLong();
        mPidToRow.insert(pid, i);
    }

    ui->lblProcessTitle->setText(tr("Processes (%1)").arg(mItemModel->rowCount()));

    // Restore selection
    if (selectedPid) {
        for (int i = 0; i < mSortFilterModel->rowCount(); ++i) {
            if (mSortFilterModel->index(i, 0).data(SortRole).toInt() == selectedPid) {
                ui->tableProcess->selectRow(i);
                mSelectedRowModel = mSortFilterModel->index(i, 0);
                break;
            }
        }
    } else {
        mSelectedRowModel = QModelIndex();
    }

    // FR-116: fire tray notifications on threshold breach.
    evaluateThresholdAlerts(processes);
}

QList<QStandardItem*> ProcessesPage::createRow(const Process &proc)
{
    QList<QStandardItem*> row;

    int data = SortRole;

    QStandardItem *pid_i = new QStandardItem(QString::number(proc.getPid()));
    pid_i->setData(proc.getPid(), data);
    pid_i->setData(proc.getPid(), Qt::ToolTipRole);
    // FR-116: pin role used by PinSortFilterProxyModel::lessThan.
    pid_i->setData(ProcessPrefsManager::ins()->isPinned(proc.getCmd()),
                   PinSortFilterProxyModel::PinnedRole);

    QStandardItem *rss_i = new QStandardItem(FormatUtil::formatBytes(proc.getRss()));
    rss_i->setData(proc.getRss(), data);
    rss_i->setData(FormatUtil::formatBytes(proc.getRss()), Qt::ToolTipRole);

    QStandardItem *pmem_i = new QStandardItem(QString::number(proc.getPmem()));
    pmem_i->setData(proc.getPmem(), data);
    pmem_i->setData(proc.getPmem(), Qt::ToolTipRole);

    QStandardItem *vsize_i = new QStandardItem(FormatUtil::formatBytes(proc.getVsize()));
    vsize_i->setData(proc.getVsize(), data);
    vsize_i->setData(FormatUtil::formatBytes(proc.getVsize()), Qt::ToolTipRole);

    QStandardItem *uname_i = new QStandardItem(proc.getUname());
    uname_i->setData(proc.getUname(), data);
    uname_i->setData(proc.getUname(), Qt::ToolTipRole);

    QStandardItem *pcpu_i = new QStandardItem(QString::number(proc.getPcpu()));
    pcpu_i->setData(proc.getPcpu(), data);
    pcpu_i->setData(proc.getPcpu(), Qt::ToolTipRole);

    QStandardItem *starttime_i = new QStandardItem(proc.getStartTime());
    starttime_i->setData(proc.getStartTime(), data);
    starttime_i->setData(proc.getStartTime(), Qt::ToolTipRole);

    QStandardItem *state_i = new QStandardItem(proc.getState());
    state_i->setData(proc.getState(), data);
    state_i->setData(proc.getState(), Qt::ToolTipRole);

    QStandardItem *group_i = new QStandardItem(proc.getGroup());
    group_i->setData(proc.getGroup(), data);
    group_i->setData(proc.getGroup(), Qt::ToolTipRole);

    QStandardItem *nice_i = new QStandardItem(QString::number(proc.getNice()));
    nice_i->setData(proc.getNice(), data);
    nice_i->setData(proc.getNice(), Qt::ToolTipRole);

    QStandardItem *cpuTime_i = new QStandardItem(proc.getCpuTime());
    cpuTime_i->setData(proc.getCpuTime(), data);
    cpuTime_i->setData(proc.getCpuTime(), Qt::ToolTipRole);

    QStandardItem *session_i = new QStandardItem(proc.getSession());
    session_i->setData(proc.getSession(), data);
    session_i->setData(proc.getSession(), Qt::ToolTipRole);

    // Disk Read/s
    QString diskReadText = proc.getDiskReadRate() < 0
        ? QString::fromUtf8("\u2014")
        : FormatUtil::formatBytes(static_cast<quint64>(proc.getDiskReadRate())) + "/s";
    QStandardItem *diskRead_i = new QStandardItem(diskReadText);
    diskRead_i->setData(proc.getDiskReadRate(), data);
    diskRead_i->setData(diskReadText, Qt::ToolTipRole);

    // Disk Write/s
    QString diskWriteText = proc.getDiskWriteRate() < 0
        ? QString::fromUtf8("\u2014")
        : FormatUtil::formatBytes(static_cast<quint64>(proc.getDiskWriteRate())) + "/s";
    QStandardItem *diskWrite_i = new QStandardItem(diskWriteText);
    diskWrite_i->setData(proc.getDiskWriteRate(), data);
    diskWrite_i->setData(diskWriteText, Qt::ToolTipRole);

    // Net Down/s
    QString netDownText = proc.getNetDownRate() < 0
        ? QString::fromUtf8("\u2014")
        : FormatUtil::formatBytes(static_cast<quint64>(proc.getNetDownRate())) + "/s";
    QStandardItem *netDown_i = new QStandardItem(netDownText);
    netDown_i->setData(proc.getNetDownRate(), data);
    netDown_i->setData(netDownText, Qt::ToolTipRole);

    // Net Up/s
    QString netUpText = proc.getNetUpRate() < 0
        ? QString::fromUtf8("\u2014")
        : FormatUtil::formatBytes(static_cast<quint64>(proc.getNetUpRate())) + "/s";
    QStandardItem *netUp_i = new QStandardItem(netUpText);
    netUp_i->setData(proc.getNetUpRate(), data);
    netUp_i->setData(netUpText, Qt::ToolTipRole);

    // FR-115: GPU %
    QString gpuPctText = proc.getGpuPercent() < 0
        ? QString::fromUtf8("\u2014")
        : QString::number(proc.getGpuPercent(), 'f', 0) + "%";
    QStandardItem *gpuPct_i = new QStandardItem(gpuPctText);
    gpuPct_i->setData(proc.getGpuPercent(), data);
    gpuPct_i->setData(gpuPctText, Qt::ToolTipRole);

    // FR-115: GPU VRAM
    QString gpuVramText = proc.getGpuVramBytes() < 0
        ? QString::fromUtf8("\u2014")
        : FormatUtil::formatBytes(static_cast<quint64>(proc.getGpuVramBytes()));
    QStandardItem *gpuVram_i = new QStandardItem(gpuVramText);
    gpuVram_i->setData(proc.getGpuVramBytes(), data);
    gpuVram_i->setData(gpuVramText, Qt::ToolTipRole);

    QStandardItem *cmd_i = new QStandardItem(proc.getCmd());
    cmd_i->setData(proc.getCmd(), data);
    cmd_i->setData(QString("<p>%1</p>").arg(proc.getCmd()), Qt::ToolTipRole);
    cmd_i->setFont(QFont(QStringLiteral("JetBrains Mono")));

    // GH#174: kill icon column — enabled but not selectable so clicking it
    // kills without selecting the row. PID is read from column 0 on click.
    QStandardItem *kill_i = new QStandardItem();
    kill_i->setFlags(Qt::ItemIsEnabled);
    kill_i->setData(tr("Kill process"), Qt::ToolTipRole);

    row << pid_i << rss_i << pmem_i << vsize_i << uname_i << pcpu_i
        << starttime_i << state_i << group_i << nice_i << cpuTime_i
        << session_i << diskRead_i << diskWrite_i << netDown_i << netUp_i
        << gpuPct_i << gpuVram_i
        << cmd_i << kill_i;

    return row;
}

void ProcessesPage::updateRow(int row, const Process &proc)
{
    int d = SortRole;
    auto setCell = [&](int col, const QString &display, const QVariant &sort, const QVariant &tip) {
        QStandardItem *item = mItemModel->item(row, col);
        if (item) {
            item->setText(display);
            item->setData(sort, d);
            item->setData(tip, Qt::ToolTipRole);
        }
    };

    setCell(0,  QString::number(proc.getPid()), proc.getPid(), proc.getPid());
    // FR-116: refresh pinned role on the updated row.
    if (auto *pidItem = mItemModel->item(row, 0))
        pidItem->setData(ProcessPrefsManager::ins()->isPinned(proc.getCmd()),
                         PinSortFilterProxyModel::PinnedRole);
    setCell(1,  FormatUtil::formatBytes(proc.getRss()), proc.getRss(), FormatUtil::formatBytes(proc.getRss()));
    setCell(2,  QString::number(proc.getPmem()), proc.getPmem(), proc.getPmem());
    setCell(3,  FormatUtil::formatBytes(proc.getVsize()), proc.getVsize(), FormatUtil::formatBytes(proc.getVsize()));
    setCell(4,  proc.getUname(), proc.getUname(), proc.getUname());
    setCell(5,  QString::number(proc.getPcpu()), proc.getPcpu(), proc.getPcpu());
    setCell(6,  proc.getStartTime(), proc.getStartTime(), proc.getStartTime());
    setCell(7,  proc.getState(), proc.getState(), proc.getState());
    setCell(8,  proc.getGroup(), proc.getGroup(), proc.getGroup());
    setCell(9,  QString::number(proc.getNice()), proc.getNice(), proc.getNice());
    setCell(10, proc.getCpuTime(), proc.getCpuTime(), proc.getCpuTime());
    setCell(11, proc.getSession(), proc.getSession(), proc.getSession());

    QString diskReadText = proc.getDiskReadRate() < 0
        ? QString::fromUtf8("\u2014")
        : FormatUtil::formatBytes(static_cast<quint64>(proc.getDiskReadRate())) + "/s";
    setCell(12, diskReadText, proc.getDiskReadRate(), diskReadText);

    QString diskWriteText = proc.getDiskWriteRate() < 0
        ? QString::fromUtf8("\u2014")
        : FormatUtil::formatBytes(static_cast<quint64>(proc.getDiskWriteRate())) + "/s";
    setCell(13, diskWriteText, proc.getDiskWriteRate(), diskWriteText);

    QString netDownText = proc.getNetDownRate() < 0
        ? QString::fromUtf8("\u2014")
        : FormatUtil::formatBytes(static_cast<quint64>(proc.getNetDownRate())) + "/s";
    setCell(14, netDownText, proc.getNetDownRate(), netDownText);

    QString netUpText = proc.getNetUpRate() < 0
        ? QString::fromUtf8("\u2014")
        : FormatUtil::formatBytes(static_cast<quint64>(proc.getNetUpRate())) + "/s";
    setCell(15, netUpText, proc.getNetUpRate(), netUpText);

    QString gpuPctText = proc.getGpuPercent() < 0
        ? QString::fromUtf8("\u2014")
        : QString::number(proc.getGpuPercent(), 'f', 0) + "%";
    setCell(16, gpuPctText, proc.getGpuPercent(), gpuPctText);

    QString gpuVramText = proc.getGpuVramBytes() < 0
        ? QString::fromUtf8("\u2014")
        : FormatUtil::formatBytes(static_cast<quint64>(proc.getGpuVramBytes()));
    setCell(17, gpuVramText, proc.getGpuVramBytes(), gpuVramText);

    setCell(18, proc.getCmd(), proc.getCmd(), QString("<p>%1</p>").arg(proc.getCmd()));
    if (auto *item = mItemModel->item(row, 18))
        item->setFont(QFont(QStringLiteral("JetBrains Mono")));
}

void ProcessesPage::on_txtProcessSearch_textChanged(const QString &val)
{
    mSortFilterModel->setFilterKeyColumn(mHeaders.count() - 1);
    mSortFilterModel->setFilterFixedString(val);
}

void ProcessesPage::on_sliderRefresh_valueChanged(const int &i)
{
    ui->lblRefresh->setText(tr("Refresh (%1)").arg(i));
    mRefresh->setProcessRefreshInterval(i * 1000);
}

void ProcessesPage::on_btnEndProcess_clicked()
{
    pid_t pid = mSelectedRowModel.data(SortRole).toInt();

    if (pid) {
        QString selectedUname = mSortFilterModel->index(mSelectedRowModel.row(), 4).data(SortRole).toString();
        mProcessService->killProcess(pid, selectedUname, im->getUserName());
    }
}

// GH#174: per-row kill icon — clicking column kKillCol kills the process
// directly without requiring the user to select the row first.
void ProcessesPage::onKillColumnClicked(const QModelIndex &proxyIndex)
{
    if (!proxyIndex.isValid() || proxyIndex.column() != kKillCol)
        return;

    const QModelIndex src = mSortFilterModel->mapToSource(proxyIndex);
    QStandardItem *pidItem = mItemModel->item(src.row(), 0);
    if (!pidItem)
        return;

    const pid_t pid = pidItem->data(SortRole).toLongLong();
    if (!pid)
        return;

    QStandardItem *unameItem = mItemModel->item(src.row(), 4);
    const QString uname = unameItem ? unameItem->data(SortRole).toString() : QString();
    mProcessService->killProcess(pid, uname, im->getUserName());
}

void ProcessesPage::on_tableProcess_customContextMenuRequested(const QPoint &pos)
{
    QPoint globalPos = ui->tableProcess->mapToGlobal(pos);

    QAction *action = mHeaderMenu.exec(globalPos);

    if (action) {
        ui->tableProcess->horizontalHeader()->setSectionHidden(action->data().toInt(), ! action->isChecked());
        updateProcessIoCollection();
    }
}

void ProcessesPage::onPageActivated()
{
    mRefresh->resumeProcessTimer();
}

void ProcessesPage::onPageDeactivated()
{
    mRefresh->pauseProcessTimer();
}

// ───────── FR-116: row context menu + pin prefs ─────────

void ProcessesPage::onRowContextMenu(const QPoint &pos)
{
    const QModelIndex idx = ui->tableProcess->indexAt(pos);
    if (!idx.isValid())
        return;

    // Grab the source-model row so we can read the PID directly.
    const int sourceRow = mSortFilterModel->mapToSource(idx).row();
    QStandardItem *pidItem = mItemModel->item(sourceRow, 0);
    if (!pidItem)
        return;

    const pid_t pid = pidItem->data(SortRole).toLongLong();
    const QString name = mPidToName.value(pid);
    if (name.isEmpty())
        return;

    auto *prefs = ProcessPrefsManager::ins();
    const bool pinned = prefs->isPinned(name);

    QMenu menu(this);
    QAction *pinAction = menu.addAction(pinned ? tr("Unpin %1").arg(name)
                                                : tr("Pin %1").arg(name));
    QAction *alertAction = menu.addAction(
        prefs->hasThreshold(name) ? tr("Edit Alert…") : tr("Set Alert…"));
    menu.addSeparator();
    QAction *endAction = menu.addAction(tr("End Process"));

    QAction *chosen = menu.exec(ui->tableProcess->viewport()->mapToGlobal(pos));
    if (!chosen)
        return;

    if (chosen == pinAction) {
        prefs->setPinned(name, !pinned);
    } else if (chosen == alertAction) {
        auto *dlg = new ProcessAlertDialog(name, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->open();
    } else if (chosen == endAction) {
        mSelectedRowModel = mSortFilterModel->index(idx.row(), 0);
        on_btnEndProcess_clicked();
    }
}

void ProcessesPage::onPinPrefsChanged()
{
    refreshPinnedRoles();
}

void ProcessesPage::refreshPinnedRoles()
{
    auto *prefs = ProcessPrefsManager::ins();
    for (int row = 0; row < mItemModel->rowCount(); ++row) {
        QStandardItem *pidItem = mItemModel->item(row, 0);
        if (!pidItem)
            continue;
        const pid_t pid = pidItem->data(SortRole).toLongLong();
        const QString name = mPidToName.value(pid);
        pidItem->setData(prefs->isPinned(name),
                         PinSortFilterProxyModel::PinnedRole);
    }
    // Nudge the proxy to re-sort.
    mSortFilterModel->invalidate();
}

void ProcessesPage::evaluateThresholdAlerts(const QList<Process> &processes)
{
    const auto thresholds = ProcessPrefsManager::ins()->thresholds();
    if (thresholds.isEmpty())
        return;

    // Aggregate RSS + CPU% by process name across all PIDs, plus a count.
    struct Agg {
        quint64 rss = 0;
        double  cpu = 0.0;
        int     count = 0;
    };
    QHash<QString, Agg> agg;
    for (const Process &proc : processes) {
        Agg &a = agg[proc.getCmd()];
        a.rss   += proc.getRss();
        a.cpu   += proc.getPcpu();
        a.count += 1;
    }

    QSystemTrayIcon *tray = AppManager::ins()->getTrayIcon();
    if (!tray)
        return;

    auto fire = [tray, this](const QString &key, const QString &title, const QString &body) {
        if (mAlertArmed.value(key, true)) {   // armed by default
            tray->showMessage(title, body, QSystemTrayIcon::Warning);
            mAlertArmed[key] = false;
        }
    };
    auto disarm = [this](const QString &key) {
        mAlertArmed[key] = true;
    };

    for (const auto &t : thresholds) {
        const Agg &a = agg.value(t.name);
        const QString cpuKey = t.name + QStringLiteral("::cpu");
        const QString memKey = t.name + QStringLiteral("::mem");

        if (t.cpuPercent > 0) {
            if (a.cpu > t.cpuPercent) {
                const QString body = a.count > 1
                    ? tr("%1 (%2 processes) exceeded %3% CPU").arg(t.name).arg(a.count).arg(t.cpuPercent)
                    : tr("%1 exceeded %2% CPU").arg(t.name).arg(t.cpuPercent);
                fire(cpuKey, tr("Process Threshold Alert"), body);
            } else {
                disarm(cpuKey);
            }
        }

        if (t.memoryBytes > 0) {
            if (static_cast<qint64>(a.rss) > t.memoryBytes) {
                const QString body = a.count > 1
                    ? tr("%1 (%2 processes) exceeded %3").arg(t.name).arg(a.count)
                        .arg(FormatUtil::formatBytes(t.memoryBytes))
                    : tr("%1 exceeded %2").arg(t.name,
                        FormatUtil::formatBytes(t.memoryBytes));
                fire(memKey, tr("Process Threshold Alert"), body);
            } else {
                disarm(memKey);
            }
        }
    }
}
