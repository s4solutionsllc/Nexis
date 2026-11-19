#include "processes_page.h"
#include "ui_processes_page.h"
#include "utilities.h"
#include "nexis_roles.h"
#include "dpi.h"
#include "pin_sort_filter_proxy_model.h"
#include "kill_button_delegate.h"
#include "process_group_header_delegate.h"
#include "process_alert_dialog.h"
#include "Managers/app_manager.h"
#include "Managers/data_refresh_service.h"
#include "Managers/process_prefs_manager.h"
#include "Services/process_service.h"
#include <QFileInfo>
#include <QItemSelectionModel>
#include <QRegularExpression>
#include <QSet>
#include <QSystemTrayIcon>
#include <Utils/format_util.h>
#include <Utils/process_cpu_util.h>
#include <Utils/process_tree_util.h>
#ifdef Q_OS_MACOS
#include <QFileIconProvider>
#endif

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
    // Order MUST mirror the Col enum in the header. GH#194: the Name column is
    // Linux-only (omitted on macOS), matching the conditional Col_Name entry.
    mHeaders = QStringList {
        "PID",
#ifdef Q_OS_LINUX
        tr("Name"),                              // GH#194: Col_Name (Linux only)
#endif
        tr("Resident Memory"), tr("%Memory"), tr("Virtual Memory"),
        tr("User"), "%CPU", tr("Start Time"), tr("State"), tr("Group"),
        tr("Nice"), tr("CPU Time"), tr("Session"),
        tr("Disk Read/s"), tr("Disk Write/s"), tr("Net Down/s"), tr("Net Up/s"),
        tr("GPU %"), tr("GPU VRAM"),             // FR-115
        tr("Process")                            // Col_Cmd (last data column)
    };

    // slider settings
    ui->sliderRefresh->setRange(1, 10);
    ui->sliderRefresh->setPageStep(1);
    ui->sliderRefresh->setSingleStep(1);

    // Table settings
    mSortFilterModel->setSourceModel(mItemModel);

    mItemModel->setHorizontalHeaderLabels(mHeaders);

    // DS §7: right-align tabular numerics (PID / Resident Memory / %Memory / %CPU).
    const auto rightAlign = QVariant(Qt::AlignRight | Qt::AlignVCenter);
    mItemModel->setHeaderData(Col_Pid, Qt::Horizontal, rightAlign, Qt::TextAlignmentRole);
    mItemModel->setHeaderData(Col_Rss, Qt::Horizontal, rightAlign, Qt::TextAlignmentRole);
    mItemModel->setHeaderData(Col_Pmem, Qt::Horizontal, rightAlign, Qt::TextAlignmentRole);
    mItemModel->setHeaderData(Col_Pcpu, Qt::Horizontal, rightAlign, Qt::TextAlignmentRole);
    mItemModel->setHeaderData(Col_Pcpu, Qt::Horizontal,
        tr("Share of total CPU across all %n logical processor(s)", nullptr,
           ProcessCpuUtil::logicalCpuCount()),
        Qt::ToolTipRole);
    // SSO-15374: GPU % / GPU VRAM are tabular numerics too — same DS §7
    // right-align convention as PID/Resident Memory/%Memory/%CPU above.
    mItemModel->setHeaderData(Col_GpuPct, Qt::Horizontal, rightAlign, Qt::TextAlignmentRole);
    mItemModel->setHeaderData(Col_GpuVram, Qt::Horizontal, rightAlign, Qt::TextAlignmentRole);
    // SSO-15379: Disk/Net rate columns are tabular numerics too — closing a
    // pre-existing gap where these four never got the DS §7 right-align
    // treatment the other numeric columns above already have.
    mItemModel->setHeaderData(Col_DiskRead, Qt::Horizontal, rightAlign, Qt::TextAlignmentRole);
    mItemModel->setHeaderData(Col_DiskWrite, Qt::Horizontal, rightAlign, Qt::TextAlignmentRole);
    mItemModel->setHeaderData(Col_NetDown, Qt::Horizontal, rightAlign, Qt::TextAlignmentRole);
    mItemModel->setHeaderData(Col_NetUp, Qt::Horizontal, rightAlign, Qt::TextAlignmentRole);

    ui->tableProcess->setModel(mSortFilterModel);

    // SSO-15376: two static section headers (Apps above Background), each
    // holding process rows as children. No expand/collapse chrome — the
    // sections aren't collapsible, just grouped.
    ui->tableProcess->setRootIsDecorated(false);
    ui->tableProcess->setItemsExpandable(false);
    ui->tableProcess->setExpandsOnDoubleClick(false);
    ui->tableProcess->setIndentation(0);
    // QTreeView equivalents of the QTableView-only "horizontalHeader*"
    // .ui attributes this page used before the SSO-15376 grouping change.
    ui->tableProcess->header()->setCascadingSectionResizes(false);
    ui->tableProcess->header()->setStretchLastSection(true);

    ui->btnEndProcess->setVisible(false);
    connect(ui->tableProcess->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this](const QItemSelection &selected, const QItemSelection &) {
        ui->btnEndProcess->setVisible(!selected.isEmpty());
    });

    mSortFilterModel->setSortRole(SortRole);
    mSortFilterModel->setDynamicSortFilter(true);
    mSortFilterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    // SSO-15376: without this, a non-matching group-header row hides itself
    // and — by default QSortFilterProxyModel behavior — every child under
    // it too, even children whose own text matches the search. Recursive
    // filtering keeps a group visible whenever any of its children match.
    mSortFilterModel->setRecursiveFilteringEnabled(true);
    mSortFilterModel->sort(Col_Pcpu, Qt::SortOrder::DescendingOrder);

    // DS §7: zebra striping for populated rows (shadow stays on the container).
    ui->tableProcess->setAlternatingRowColors(true);

    ui->tableProcess->header()->setSectionsMovable(true);
    ui->tableProcess->header()->setFixedHeight(Dpi::scale(36));
    ui->tableProcess->header()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui->tableProcess->header()->setCursor(Qt::PointingHandCursor);
    ui->tableProcess->header()->resizeSection(Col_Pid, 70);

    // GH#174: Kill column — fixed narrow width, not shown in header context menu
    // (not in mHeaders so loadHeaderMenu() ignores it), not sortable.
    mItemModel->setHorizontalHeaderItem(kKillCol, new QStandardItem());
    ui->tableProcess->header()->setSectionResizeMode(kKillCol, QHeaderView::Fixed);
    ui->tableProcess->header()->resizeSection(kKillCol, Dpi::scale(30));
    // The kill column is the last section, so stretchLastSection would hand
    // it all the spare width and leave the command line elided at 100px.
    ui->tableProcess->header()->setStretchLastSection(false);
    ui->tableProcess->header()->setSectionResizeMode(Col_Cmd, QHeaderView::Stretch);
    // Size from the header text so it fits at any platform's font metrics.
    ui->tableProcess->header()->resizeSection(Col_Rss,
        ui->tableProcess->header()->fontMetrics().horizontalAdvance(tr("Resident Memory")) + Dpi::scale(40));
    mKillDelegate = new KillButtonDelegate(this);
    ui->tableProcess->setItemDelegateForColumn(kKillCol, mKillDelegate);
    ui->tableProcess->setMouseTracking(true);
    connect(ui->tableProcess, &QTreeView::clicked,
            this, &ProcessesPage::onKillColumnClicked);

    // SSO-15376: Apps/Background section-header rows, DS §3 anatomy painted
    // by ProcessGroupHeaderDelegate. Assigning it to Col_Pid (column 0) is
    // safe for ordinary rows too — the delegate falls through to the default
    // QStyledItemDelegate paint/sizeHint whenever GroupHeaderRole isn't set.
    mGroupHeaderDelegate = new ProcessGroupHeaderDelegate(this);
    ui->tableProcess->setItemDelegateForColumn(Col_Pid, mGroupHeaderDelegate);

    mAppsGroupItem = createGroupHeaderItem(tr("Apps"));
    mBackgroundGroupItem = createGroupHeaderItem(tr("Background"));
    mItemModel->appendRow(mAppsGroupItem);
    mItemModel->appendRow(mBackgroundGroupItem);
    applyGroupHeaderLayout();
    updateGroupHeaderCounts();

    connect(mRefresh, &DataRefreshService::processesUpdated,
            this, &ProcessesPage::onProcessesUpdated);

    // SSO-27153: tree mode. Collapse state is tracked per PID so a new child
    // doesn't pop a branch the user closed back open.
    connect(ui->checkProcessTree, &QCheckBox::toggled,
            this, &ProcessesPage::onProcessTreeToggled);
    connect(ui->tableProcess, &QTreeView::collapsed, this,
            [this](const QModelIndex &idx) { trackCollapseState(idx, true); });
    connect(ui->tableProcess, &QTreeView::expanded, this,
            [this](const QModelIndex &idx) { trackCollapseState(idx, false); });

    ui->tableProcess->header()->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->tableProcess->header(), &QHeaderView::customContextMenuRequested,
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

    // DS §2/§7: shadow lives on the elevated container, never on the table itself.
    ui->processesContainer->setAttribute(Qt::WA_StyledBackground, true);
    Utilities::addDropShadow(ui->processesContainer, 90, 26);

    ui->processesEmptyState->setVisible(false);
    // Keep the header at its natural height; with an empty table it used to
    // soak up the spare vertical space.
    ui->sectionHeaderRow->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    ui->gridLayout->setRowStretch(3, 1);
    connect(ui->btnRefreshNow, &QPushButton::clicked, this, [this]() {
        mRefresh->triggerProcessRefresh();
    });
}

// ───────── SSO-15376: Apps/Background grouping helpers ─────────

QStandardItem *ProcessesPage::createGroupHeaderItem(const QString &title)
{
    QStandardItem *item = new QStandardItem(title);
    // Enabled but not selectable/editable: clicks land on it (the kill-icon
    // and context-menu handlers already guard on column/GroupHeaderRole),
    // but it can't be selected, dragged into "End Process", or renamed.
    item->setFlags(Qt::ItemIsEnabled);
    item->setData(true, GroupHeaderRole);
    return item;
}

QStandardItem *ProcessesPage::groupItemFor(bool isAppProcess) const
{
    return isAppProcess ? mAppsGroupItem : mBackgroundGroupItem;
}

void ProcessesPage::updateGroupHeaderCounts()
{
    if (mAppsGroupItem)
        mAppsGroupItem->setText(tr("Apps (%1)").arg(mAppsGroupItem->rowCount()));
    if (mBackgroundGroupItem)
        mBackgroundGroupItem->setText(tr("Background (%1)").arg(mBackgroundGroupItem->rowCount()));
}

QModelIndex ProcessesPage::proxyIndexForPid(pid_t pid) const
{
    QStandardItem *item = mPidToRow.value(pid, nullptr);
    return item ? mSortFilterModel->mapFromSource(item->index()) : QModelIndex();
}

void ProcessesPage::applyGroupHeaderLayout()
{
    // SSO-15376: the view's model is the proxy (setModel(mSortFilterModel)
    // above), not mItemModel directly — setFirstColumnSpanned()/setExpanded()
    // take indices in the *view's* model, so source-model indices from
    // QStandardItem::row()/index() must be mapped through the proxy first.
    // Getting this wrong doesn't crash; it silently leaves every process row
    // uncollapsed-but-never-shown, since setItemsExpandable(false) removes
    // any user affordance to expand them manually.
    for (QStandardItem *group : {mAppsGroupItem, mBackgroundGroupItem}) {
        const QModelIndex proxyIdx = mSortFilterModel->mapFromSource(group->index());
        if (!proxyIdx.isValid())
            continue;
        ui->tableProcess->setFirstColumnSpanned(proxyIdx.row(), proxyIdx.parent(), true);
        ui->tableProcess->setExpanded(proxyIdx, true);
    }
}

QStandardItem *ProcessesPage::rowParentOf(QStandardItem *item) const
{
    return item->parent() ? item->parent() : mItemModel->invisibleRootItem();
}

bool ProcessesPage::isDescendantOf(QStandardItem *maybeDescendant, QStandardItem *item)
{
    for (QStandardItem *p = maybeDescendant->parent(); p; p = p->parent()) {
        if (p == item)
            return true;
    }
    return false;
}

void ProcessesPage::trackCollapseState(const QModelIndex &proxyIndex, bool collapsed)
{
    const QModelIndex src = mSortFilterModel->mapToSource(proxyIndex).siblingAtColumn(0);
    if (!src.isValid() || src.data(GroupHeaderRole).toBool())
        return;
    const pid_t pid = src.data(SortRole).toLongLong();
    if (collapsed)
        mCollapsedPids.insert(pid);
    else
        mCollapsedPids.remove(pid);
}

void ProcessesPage::onProcessTreeToggled(bool enabled)
{
    ui->tableProcess->setRootIsDecorated(enabled);
    ui->tableProcess->setItemsExpandable(enabled);
    ui->tableProcess->setExpandsOnDoubleClick(enabled);
    ui->tableProcess->setIndentation(enabled ? Dpi::scale(18) : 0);
    mSortFilterModel->setTreeMode(enabled);

    if (!mLastProcesses.isEmpty())
        onProcessesUpdated(mLastProcesses, mLastUserName);
    if (!enabled)
        applyGroupHeaderLayout();
}

QIcon ProcessesPage::resolveProcessIcon(const Process &proc) const
{
    static const QIcon genericIcon(QStringLiteral(":/static/themes/common/img/package.png"));

    const QString hint = proc.getIconHint();
    if (hint.isEmpty())
        return genericIcon;

    QIcon icon;
#ifdef Q_OS_MACOS
    // macOS: hint is the .app bundle path — same resolution StartupApp
    // already uses for macOS autostart entries (startup_app.cpp).
    if (hint.endsWith(QLatin1String(".app")) && QFileInfo::exists(hint)) {
        QFileIconProvider iconProvider;
        icon = iconProvider.icon(QFileInfo(hint));
    }
#else
    // Linux: hint is an XDG Icon= value — a freedesktop theme name or an
    // absolute path. Try the path first, then fall back to theme lookup.
    if (QFileInfo::exists(hint))
        icon = QIcon(hint);
    if (icon.isNull())
        icon = QIcon::fromTheme(hint, genericIcon);
#endif
    return icon.isNull() ? genericIcon : icon;
}

void ProcessesPage::updateProcessIoCollection()
{
    const auto *header = ui->tableProcess->header();
    const bool diskVisible = !header->isSectionHidden(Col_DiskRead) || !header->isSectionHidden(Col_DiskWrite);
    const bool netVisible  = !header->isSectionHidden(Col_NetDown) || !header->isSectionHidden(Col_NetUp);
    const bool gpuVisible  = !header->isSectionHidden(Col_GpuPct) || !header->isSectionHidden(Col_GpuVram);
    im->setCollectProcessDiskIO(diskVisible);
    im->setCollectProcessNetIO(netVisible);
    im->setCollectProcessGpu(gpuVisible);

    updateNetIoAvailabilityMessage(netVisible);
}

void ProcessesPage::updateNetIoAvailabilityMessage(bool netVisible)
{
    // SSO-15379: acceptance criterion — never let missing tool/permission
    // show up as silent blank/zero data. The em-dash sentinel already
    // covers "unavailable", but a header tooltip explains *why* instead of
    // leaving the user to guess.
    QString tooltip;
    if (netVisible) {
        switch (im->processNetIoAvailability()) {
        case ProcessInfo::NetIoAvailability::ToolMissing:
            tooltip = tr("Per-process network usage requires nethogs. "
                         "Install it (e.g. `apt install nethogs`) and reopen this page.");
            break;
        case ProcessInfo::NetIoAvailability::PermissionDenied:
            tooltip = tr("Per-process network usage needs elevated permissions. "
                         "Run Nexis as root, or grant nethogs capture rights "
                         "(e.g. `sudo setcap cap_net_raw,cap_net_admin+ep $(which nethogs)`).");
            break;
        case ProcessInfo::NetIoAvailability::Available:
            break;
        }
    }

    mItemModel->setHeaderData(Col_NetDown, Qt::Horizontal, tooltip, Qt::ToolTipRole);
    mItemModel->setHeaderData(Col_NetUp, Qt::Horizontal, tooltip, Qt::ToolTipRole);
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
    // Columns hidden by default (GPU %/VRAM, disk/net I/O, and the verbose
    // columns). The Name column (GH#194) is intentionally NOT hidden — it is a
    // primary identity column shown by default on Linux.
    QList<int> hiddenHeaders = {
        Col_Vsize, Col_StartTime, Col_State, Col_Group, Col_Nice,
        Col_CpuTime, Col_Session, Col_DiskRead, Col_DiskWrite,
        Col_NetDown, Col_NetUp, Col_GpuPct, Col_GpuVram
    };

    QList<QAction*> actions = mHeaderMenu.actions();
    for (const int i : hiddenHeaders) {
        if (i < mHeaders.count()) {
            ui->tableProcess->header()->setSectionHidden(i, true);
            actions.at(i)->setChecked(false);
        }
    }
}

void ProcessesPage::onProcessesUpdated(const QList<Process> &processes, const QString &userName)
{
    // SSO-15379: re-evaluate on every tick, not just on column toggle — the
    // net-IO backend starts asynchronously on the next collection cycle
    // after the column is shown, so ToolMissing/PermissionDenied can only
    // be known a tick or two after updateProcessIoCollection() ran.
    const auto *header = ui->tableProcess->header();
    const bool netVisible = !header->isSectionHidden(Col_NetDown) || !header->isSectionHidden(Col_NetUp);
    updateNetIoAvailabilityMessage(netVisible);

    QModelIndexList selecteds = ui->tableProcess->selectionModel()->selectedRows();
    pid_t selectedPid = 0;
    if (!selecteds.isEmpty())
        selectedPid = selecteds.first().data(SortRole).toInt();

    mLastProcesses = processes;
    mLastUserName = userName;

    const bool showAll = ui->checkAllProcesses->isChecked();
    const bool treeMode = ui->checkProcessTree->isChecked();
    QList<const Process *> visible;
    QHash<pid_t, pid_t> ppidByPid;
    for (const Process &proc : processes) {
        if (!showAll && userName != proc.getUname())
            continue;
        visible.append(&proc);
        ppidByPid.insert(proc.getPid(), proc.getPpid());
    }
    const QSet<pid_t> incomingPids = QSet<pid_t>(ppidByPid.keyBegin(), ppidByPid.keyEnd());
    const QHash<pid_t, pid_t> treeParents =
        treeMode ? ProcessTreeUtil::resolveParents(ppidByPid) : QHash<pid_t, pid_t>();

    // Pass 1: refresh existing rows in place; build new rows detached, since
    // a child's parent row may not exist yet.
    QHash<pid_t, QList<QStandardItem *>> newRows;
    for (const Process *proc : visible) {
        const pid_t pid = proc->getPid();
        mPidToName.insert(pid, proc->getCmd());   // FR-116

        auto it = mPidToRow.find(pid);
        if (it != mPidToRow.end()) {
            updateRow(it.value(), *proc);
        } else {
            QList<QStandardItem *> row = createRow(*proc);
            mPidToRow.insert(pid, row.first());
            newRows.insert(pid, row);
        }
    }

    // Pass 2: put every row under its target parent — a classified group
    // (SSO-15376) or, in tree mode, its parent process (SSO-27153). A row
    // can change parent between ticks (re-classification, reparenting to
    // init, a mode toggle): takeRow() moves it with its subtree intact.
    QSet<QStandardItem *> expandTargets;
    for (const Process *proc : visible) {
        const pid_t pid = proc->getPid();
        QStandardItem *item = mPidToRow.value(pid);

        QStandardItem *target;
        if (treeMode) {
            const pid_t parentPid = treeParents.value(pid);
            target = parentPid ? mPidToRow.value(parentPid) : mItemModel->invisibleRootItem();
        } else {
            target = groupItemFor(proc->getIsAppProcess());
        }

        auto fresh = newRows.constFind(pid);
        if (fresh != newRows.constEnd()) {
            target->appendRow(fresh.value());
        } else if (rowParentOf(item) != target) {
            // The old structure can still have the target inside this row's
            // subtree (parent/child swapped); lift it out first or the
            // re-parent would form a cycle.
            if (isDescendantOf(target, item)) {
                QStandardItem *targetParent = rowParentOf(target);
                mItemModel->invisibleRootItem()->appendRow(targetParent->takeRow(target->row()));
            }
            QStandardItem *oldParent = rowParentOf(item);
            target->appendRow(oldParent->takeRow(item->row()));
        } else {
            continue;
        }
        if (treeMode && target != mItemModel->invisibleRootItem())
            expandTargets.insert(target);
    }

    // Remove exited processes. Survivors were re-parented away in pass 2, so
    // whatever is still under an exited row has exited too and goes with it:
    // only the topmost exited rows are removed explicitly (removing a row
    // deletes its descendants' items, so touching those would dangle).
    QSet<QStandardItem *> exitedItems;
    QList<pid_t> exitedPids;
    for (auto it = mPidToRow.begin(); it != mPidToRow.end(); ++it) {
        if (!incomingPids.contains(it.key())) {
            exitedPids.append(it.key());
            exitedItems.insert(it.value());
        }
    }
    QList<QStandardItem *> topmostExited;
    for (QStandardItem *item : std::as_const(exitedItems)) {
        bool nested = false;
        for (QStandardItem *p = item->parent(); p && !nested; p = p->parent())
            nested = exitedItems.contains(p);
        if (!nested)
            topmostExited.append(item);
    }
    for (QStandardItem *item : std::as_const(topmostExited))
        rowParentOf(item)->removeRow(item->row());
    for (pid_t pid : exitedPids) {
        mPidToRow.remove(pid);
        mPidToName.remove(pid);   // FR-116
        mCollapsedPids.remove(pid);
    }

    // New branches open by default (like htop); one the user closed stays closed.
    for (QStandardItem *parentItem : std::as_const(expandTargets)) {
        if (mCollapsedPids.contains(static_cast<pid_t>(parentItem->data(SortRole).toLongLong())))
            continue;
        const QModelIndex idx = mSortFilterModel->mapFromSource(parentItem->index());
        if (idx.isValid() && !ui->tableProcess->isExpanded(idx))
            ui->tableProcess->setExpanded(idx, true);
    }

    // DS §5: swap between the process tree and the empty-state affordance
    // based on whether any row survived the filter above.
    const bool isEmpty = incomingPids.isEmpty();
    ui->tableProcess->setVisible(!isEmpty);
    ui->processesEmptyState->setVisible(isEmpty);

    updateGroupHeaderCounts();

    // Restore selection
    if (selectedPid) {
        const QModelIndex idx = proxyIndexForPid(selectedPid);
        if (idx.isValid()) {
            ui->tableProcess->selectionModel()->select(
                idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            ui->tableProcess->setCurrentIndex(idx);
            mSelectedRowModel = idx;
        } else {
            mSelectedRowModel = QModelIndex();
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
    pid_i->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    // FR-116: pin role used by PinSortFilterProxyModel::lessThan.
    pid_i->setData(ProcessPrefsManager::ins()->isPinned(proc.getCmd()),
                   PinSortFilterProxyModel::PinnedRole);
    // SSO-15376: PID is column 0 — the only column guaranteed visible and
    // unhideable on both platforms — so the icon is always on-screen
    // regardless of which columns the user has hidden or scrolled past.
    pid_i->setIcon(resolveProcessIcon(proc));

#ifdef Q_OS_LINUX
    // GH#194: short process name (Linux only).
    QStandardItem *name_i = new QStandardItem(proc.getName());
    name_i->setData(proc.getName(), data);
    name_i->setData(proc.getName(), Qt::ToolTipRole);
#endif

    QStandardItem *rss_i = new QStandardItem(FormatUtil::formatBytes(proc.getRss()));
    rss_i->setData(proc.getRss(), data);
    rss_i->setData(FormatUtil::formatBytes(proc.getRss()), Qt::ToolTipRole);
    rss_i->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

    QStandardItem *pmem_i = new QStandardItem(ProcessCpuUtil::formatPercent(proc.getPmem()));
    pmem_i->setData(proc.getPmem(), data);
    pmem_i->setData(proc.getPmem(), Qt::ToolTipRole);
    pmem_i->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

    QStandardItem *vsize_i = new QStandardItem(FormatUtil::formatBytes(proc.getVsize()));
    vsize_i->setData(proc.getVsize(), data);
    vsize_i->setData(FormatUtil::formatBytes(proc.getVsize()), Qt::ToolTipRole);

    QStandardItem *uname_i = new QStandardItem(proc.getUname());
    uname_i->setData(proc.getUname(), data);
    uname_i->setData(proc.getUname(), Qt::ToolTipRole);

    QStandardItem *pcpu_i = new QStandardItem(ProcessCpuUtil::formatPercent(proc.getPcpu()));
    pcpu_i->setData(proc.getPcpu(), data);
    pcpu_i->setData(proc.getPcpu(), Qt::ToolTipRole);
    pcpu_i->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

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
    gpuPct_i->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // FR-115: GPU VRAM
    QString gpuVramText = proc.getGpuVramBytes() < 0
        ? QString::fromUtf8("\u2014")
        : FormatUtil::formatBytes(static_cast<quint64>(proc.getGpuVramBytes()));
    QStandardItem *gpuVram_i = new QStandardItem(gpuVramText);
    gpuVram_i->setData(proc.getGpuVramBytes(), data);
    gpuVram_i->setData(gpuVramText, Qt::ToolTipRole);
    gpuVram_i->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

    QStandardItem *cmd_i = new QStandardItem(proc.getCmd());
    cmd_i->setData(proc.getCmd(), data);
    cmd_i->setData(QString("<p>%1</p>").arg(proc.getCmd()), Qt::ToolTipRole);
    cmd_i->setFont(QFont(QStringLiteral("JetBrains Mono")));

    // GH#174: kill icon column — enabled but not selectable so clicking it
    // kills without selecting the row. PID is read from column 0 on click.
    QStandardItem *kill_i = new QStandardItem();
    kill_i->setFlags(Qt::ItemIsEnabled);
    kill_i->setData(tr("Kill process"), Qt::ToolTipRole);

    row << pid_i;
#ifdef Q_OS_LINUX
    row << name_i;   // GH#194: Col_Name, right after PID
#endif
    row << rss_i << pmem_i << vsize_i << uname_i << pcpu_i
        << starttime_i << state_i << group_i << nice_i << cpuTime_i
        << session_i << diskRead_i << diskWrite_i << netDown_i << netUp_i
        << gpuPct_i << gpuVram_i
        << cmd_i << kill_i;

    return row;
}

void ProcessesPage::updateRow(QStandardItem *pidItem, const Process &proc)
{
    if (!pidItem)
        return;
    QStandardItem *parentItem = rowParentOf(pidItem);
    const int row = pidItem->row();

    int d = SortRole;
    auto setCell = [&](int col, const QString &display, const QVariant &sort, const QVariant &tip) {
        QStandardItem *item = parentItem->child(row, col);
        if (item) {
            item->setText(display);
            item->setData(sort, d);
            item->setData(tip, Qt::ToolTipRole);
        }
    };

    setCell(Col_Pid, QString::number(proc.getPid()), proc.getPid(), proc.getPid());
    // FR-116: refresh pinned role on the updated row.
    pidItem->setData(ProcessPrefsManager::ins()->isPinned(proc.getCmd()),
                     PinSortFilterProxyModel::PinnedRole);
    // SSO-15376: re-resolve in case classification/icon hint changed.
    pidItem->setIcon(resolveProcessIcon(proc));
#ifdef Q_OS_LINUX
    setCell(Col_Name, proc.getName(), proc.getName(), proc.getName());   // GH#194
#endif
    setCell(Col_Rss,     FormatUtil::formatBytes(proc.getRss()), proc.getRss(), FormatUtil::formatBytes(proc.getRss()));
    setCell(Col_Pmem,    ProcessCpuUtil::formatPercent(proc.getPmem()), proc.getPmem(), proc.getPmem());
    setCell(Col_Vsize,   FormatUtil::formatBytes(proc.getVsize()), proc.getVsize(), FormatUtil::formatBytes(proc.getVsize()));
    setCell(Col_User,    proc.getUname(), proc.getUname(), proc.getUname());
    setCell(Col_Pcpu,    ProcessCpuUtil::formatPercent(proc.getPcpu()), proc.getPcpu(), proc.getPcpu());
    setCell(Col_StartTime, proc.getStartTime(), proc.getStartTime(), proc.getStartTime());
    setCell(Col_State,   proc.getState(), proc.getState(), proc.getState());
    setCell(Col_Group,   proc.getGroup(), proc.getGroup(), proc.getGroup());
    setCell(Col_Nice,    QString::number(proc.getNice()), proc.getNice(), proc.getNice());
    setCell(Col_CpuTime, proc.getCpuTime(), proc.getCpuTime(), proc.getCpuTime());
    setCell(Col_Session, proc.getSession(), proc.getSession(), proc.getSession());

    QString diskReadText = proc.getDiskReadRate() < 0
        ? QString::fromUtf8("\u2014")
        : FormatUtil::formatBytes(static_cast<quint64>(proc.getDiskReadRate())) + "/s";
    setCell(Col_DiskRead, diskReadText, proc.getDiskReadRate(), diskReadText);

    QString diskWriteText = proc.getDiskWriteRate() < 0
        ? QString::fromUtf8("\u2014")
        : FormatUtil::formatBytes(static_cast<quint64>(proc.getDiskWriteRate())) + "/s";
    setCell(Col_DiskWrite, diskWriteText, proc.getDiskWriteRate(), diskWriteText);

    QString netDownText = proc.getNetDownRate() < 0
        ? QString::fromUtf8("\u2014")
        : FormatUtil::formatBytes(static_cast<quint64>(proc.getNetDownRate())) + "/s";
    setCell(Col_NetDown, netDownText, proc.getNetDownRate(), netDownText);

    QString netUpText = proc.getNetUpRate() < 0
        ? QString::fromUtf8("\u2014")
        : FormatUtil::formatBytes(static_cast<quint64>(proc.getNetUpRate())) + "/s";
    setCell(Col_NetUp, netUpText, proc.getNetUpRate(), netUpText);

    QString gpuPctText = proc.getGpuPercent() < 0
        ? QString::fromUtf8("\u2014")
        : QString::number(proc.getGpuPercent(), 'f', 0) + "%";
    setCell(Col_GpuPct, gpuPctText, proc.getGpuPercent(), gpuPctText);

    QString gpuVramText = proc.getGpuVramBytes() < 0
        ? QString::fromUtf8("\u2014")
        : FormatUtil::formatBytes(static_cast<quint64>(proc.getGpuVramBytes()));
    setCell(Col_GpuVram, gpuVramText, proc.getGpuVramBytes(), gpuVramText);

    setCell(Col_Cmd, proc.getCmd(), proc.getCmd(), QString("<p>%1</p>").arg(proc.getCmd()));
    if (auto *item = parentItem->child(row, Col_Cmd))
        item->setFont(QFont(QStringLiteral("JetBrains Mono")));
}

void ProcessesPage::on_txtProcessSearch_textChanged(const QString &val)
{
    mSortFilterModel->setFilterKeyColumn(Col_Cmd);
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
        // SSO-15376: siblingAtColumn() keeps the correct parent (Apps or
        // Background) — reconstructing via row index alone (as before this
        // page had two group parents) would collide with the other group's
        // row at the same index.
        QString selectedUname = mSelectedRowModel.siblingAtColumn(Col_User).data(SortRole).toString();
        mProcessService->killProcess(pid, selectedUname, im->getUserName());
    }
}

// GH#174: per-row kill icon — clicking column kKillCol kills the process
// directly without requiring the user to select the row first.
void ProcessesPage::onKillColumnClicked(const QModelIndex &proxyIndex)
{
    if (!proxyIndex.isValid() || proxyIndex.column() != kKillCol)
        return;

    // SSO-15376: itemFromIndex(), not item(row, col) — the latter only
    // resolves root-level rows, and process rows now live under one of the
    // two group parents, not the root.
    const QModelIndex src = mSortFilterModel->mapToSource(proxyIndex);
    QStandardItem *pidItem = mItemModel->itemFromIndex(src.siblingAtColumn(0));
    if (!pidItem || pidItem->data(GroupHeaderRole).toBool())
        return;

    const pid_t pid = pidItem->data(SortRole).toLongLong();
    if (!pid)
        return;

    QStandardItem *unameItem = mItemModel->itemFromIndex(src.siblingAtColumn(Col_User));
    const QString uname = unameItem ? unameItem->data(SortRole).toString() : QString();
    mProcessService->killProcess(pid, uname, im->getUserName());
}

void ProcessesPage::on_tableProcess_customContextMenuRequested(const QPoint &pos)
{
    QPoint globalPos = ui->tableProcess->mapToGlobal(pos);

    QAction *action = mHeaderMenu.exec(globalPos);

    if (action) {
        ui->tableProcess->header()->setSectionHidden(action->data().toInt(), ! action->isChecked());
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

    // SSO-15376: itemFromIndex(), not item(row, 0) — process rows live
    // under one of the two group parents, not the root. Bail out on a
    // section-header row (no PID, no pin/alert/end-process actions apply).
    const QModelIndex srcIdx = mSortFilterModel->mapToSource(idx);
    QStandardItem *pidItem = mItemModel->itemFromIndex(srcIdx.siblingAtColumn(0));
    if (!pidItem || pidItem->data(GroupHeaderRole).toBool())
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
        // SSO-15376: siblingAtColumn() preserves idx's actual parent group;
        // reconstructing via mSortFilterModel->index(idx.row(), 0) would
        // default to the root parent and could hit the other group's row.
        mSelectedRowModel = idx.siblingAtColumn(Col_Pid);
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
    for (auto it = mPidToRow.constBegin(); it != mPidToRow.constEnd(); ++it)
        it.value()->setData(prefs->isPinned(mPidToName.value(it.key())),
                            PinSortFilterProxyModel::PinnedRole);
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
