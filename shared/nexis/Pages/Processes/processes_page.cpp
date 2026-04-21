#include "processes_page.h"
#include "ui_processes_page.h"
#include "utilities.h"
#include "nexis_roles.h"
#include "dpi.h"
#include "Managers/data_refresh_service.h"
#include "Services/process_service.h"
#include <QRegularExpression>

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
  mSortFilterModel(new QSortFilterProxyModel(this)),
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
        tr("Process")
    };

    // slider settings
    ui->sliderRefresh->setRange(1, 10);
    ui->sliderRefresh->setPageStep(1);
    ui->sliderRefresh->setSingleStep(1);

    // Table settings
    mSortFilterModel->setSourceModel(mItemModel);

    mItemModel->setHorizontalHeaderLabels(mHeaders);

    ui->tableProcess->setModel(mSortFilterModel);
    mSortFilterModel->setSortRole(SortRole);
    mSortFilterModel->setDynamicSortFilter(true);
    mSortFilterModel->sort(5, Qt::SortOrder::DescendingOrder);

    ui->tableProcess->horizontalHeader()->setSectionsMovable(true);
    ui->tableProcess->horizontalHeader()->setFixedHeight(Dpi::scale(36));
    ui->tableProcess->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui->tableProcess->horizontalHeader()->setCursor(Qt::PointingHandCursor);
    ui->tableProcess->horizontalHeader()->resizeSection(0, 70);

    connect(mRefresh, &DataRefreshService::processesUpdated,
            this, &ProcessesPage::onProcessesUpdated);

    ui->tableProcess->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->tableProcess->horizontalHeader(), &QHeaderView::customContextMenuRequested,
        this, &ProcessesPage::on_tableProcess_customContextMenuRequested);

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
    // Columns: 12=Disk Read/s, 13=Disk Write/s, 14=Net Down/s, 15=Net Up/s.
    const bool diskVisible = !header->isSectionHidden(12) || !header->isSectionHidden(13);
    const bool netVisible  = !header->isSectionHidden(14) || !header->isSectionHidden(15);
    im->setCollectProcessDiskIO(diskVisible);
    im->setCollectProcessNetIO(netVisible);
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
    // exclude headers
    QList<int> hiddenHeaders = { 3, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

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
}

QList<QStandardItem*> ProcessesPage::createRow(const Process &proc)
{
    QList<QStandardItem*> row;

    int data = SortRole;

    QStandardItem *pid_i = new QStandardItem(QString::number(proc.getPid()));
    pid_i->setData(proc.getPid(), data);
    pid_i->setData(proc.getPid(), Qt::ToolTipRole);

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

    QStandardItem *cmd_i = new QStandardItem(proc.getCmd());
    cmd_i->setData(proc.getCmd(), data);
    cmd_i->setData(QString("<p>%1</p>").arg(proc.getCmd()), Qt::ToolTipRole);

    row << pid_i << rss_i << pmem_i << vsize_i << uname_i << pcpu_i
        << starttime_i << state_i << group_i << nice_i << cpuTime_i
        << session_i << diskRead_i << diskWrite_i << netDown_i << netUp_i
        << cmd_i;

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

    setCell(16, proc.getCmd(), proc.getCmd(), QString("<p>%1</p>").arg(proc.getCmd()));
}

void ProcessesPage::on_txtProcessSearch_textChanged(const QString &val)
{
    QString pattern = QRegularExpression::wildcardToRegularExpression(val);
    QRegularExpression query(pattern, QRegularExpression::CaseInsensitiveOption);

    mSortFilterModel->setFilterKeyColumn(mHeaders.count() - 1); // process name
    mSortFilterModel->setFilterRegularExpression(query);
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
