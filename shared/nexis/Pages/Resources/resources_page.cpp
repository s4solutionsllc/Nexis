#include "resources_page.h"
#include "nexis_page.h"
#include "ui_resources_page.h"
#include "utilities.h"
#include "signal_mapper.h"
#include "dpi.h"
#include "Managers/data_refresh_service.h"
#include "Managers/resource_history_service.h"
#include "Managers/setting_manager.h"
#include "Utils/rate_counter_util.h"
#include <QFile>
#include <QButtonGroup>

#ifdef Q_OS_LINUX
#include "Pages/Helpers/oom_kills_widget.h"
#endif
#include "Pages/Helpers/cpu_core_detail_widget.h"

namespace {
// QSplineSeries IS-A QXYSeries; RollingSeries only needs the base interface.
QVector<QXYSeries *> toXYSeries(const QVector<QSplineSeries *> &series)
{
    QVector<QXYSeries *> out;
    out.reserve(series.size());
    for (QSplineSeries *s : series)
        out.append(s);
    return out;
}

// SSO-26719: 2 columns above this width, collapsing to 1 below it — wider
// than SystemCleanerPage's threshold since these cards carry a chart, not
// just a subtitle line.
constexpr int kTwoColumnThreshold = 1100;
}

ResourcesPage::~ResourcesPage()
{
    delete ui;
}

ResourcesPage::ResourcesPage(QWidget *parent, InfoManager *infoManager,
                               DataRefreshService *refreshService) :
    NexisPage(parent),
    ui(new Ui::ResourcesPage),
    im(infoManager ? infoManager : InfoManager::ins()),
    mRefresh(refreshService ? refreshService : DataRefreshService::ins()),
    mChartCpu(new HistoryChart(tr("CPU"), 1, nullptr, this)),
    mChartLoadPressure(new HistoryChart(tr("Load & Pressure"), 3, nullptr, this)),
    mChartDiskReadWrite(new HistoryChart(tr("Disk I/O"), 2, new QCategoryAxis, this)),
    // SSO-26721: 3 series — Used, Swap, one platform series (Compressed on
    // macOS, Cached on Linux). Wired (macOS) / Active (Linux) moved to the
    // header tooltip; Linux's Available dropped — it's close to the inverse
    // of Used.
    mChartMemory(new HistoryChart(tr("Memory"), 3, nullptr, this)),
    mChartNetwork(new HistoryChart(tr("Network"), 2, new QCategoryAxis, this)),
    mDiskLauncher(nullptr),
    mActive(false)
{
    ui->setupUi(this);
    ui->verticalLayout->setSpacing(PageScaffold::pageSpacing());
    ui->verticalLayout->insertWidget(0, PageScaffold::buildHeader(
        tr("Resources"), tr("Live history of CPU, memory, disk and network"), this).row);

    init();
}

void ResourcesPage::init()
{
    // --- CPU: one "Total" line; per-core moves to the "Per-core" toggle ---
    mChartCpu->setYMax(100);
    mChartCpu->getSeriesList().at(0)->setName(tr("Total"));
    mRollingCpu = std::make_unique<RollingSeries>(toXYSeries(mChartCpu->getSeriesList()));

    mPerCoreToggle = mChartCpu->addHeaderToggle(tr("Per-core"));
    mCoreDetail = new CpuCoreDetailWidget(mChartCpu);
    const int perCorePage = mChartCpu->addBodyPage(mCoreDetail);
    connect(mPerCoreToggle, &QToolButton::toggled, this, [this, perCorePage](bool checked) {
        mChartCpu->setBodyPage(checked ? perCorePage : 0);
    });

    // --- Load & Pressure: load averages; PSI folds in below as a status line ---
    {
        QVector<QSplineSeries *> series = mChartLoadPressure->getSeriesList();
        static const QString names[] = { tr("1m"), tr("5m"), tr("15m") };
        for (int i = 0; i < series.count() && i < 3; ++i)
            series.at(i)->setName(names[i]);
    }
    mRollingLoadPressure = std::make_unique<RollingSeries>(toXYSeries(mChartLoadPressure->getSeriesList()));

    // --- Memory ---
    mChartMemory->setYMax(100);
    mRollingMemory = std::make_unique<RollingSeries>(toXYSeries(mChartMemory->getSeriesList()));

    // --- Disk I/O: legends show series names only; rate + (B6) disk temp
    // move to the header ---
    {
        QVector<QSplineSeries *> series = mChartDiskReadWrite->getSeriesList();
        series.at(0)->setName(tr("Read"));
        series.at(1)->setName(tr("Write"));
    }
    mRollingDiskIO = std::make_unique<RollingSeries>(toXYSeries(mChartDiskReadWrite->getSeriesList()));

    // --- Network: no "Total" anywhere here — that word is reserved for the
    // cumulative bytes-since-boot figure on the separate Network Usage page ---
    {
        QVector<QSplineSeries *> series = mChartNetwork->getSeriesList();
        series.at(0)->setName(tr("Download"));
        series.at(1)->setName(tr("Upload"));
    }
    mRollingNetwork = std::make_unique<RollingSeries>(toXYSeries(mChartNetwork->getSeriesList()));

    // --- GPU (only if detected) ---
    if (im->hasGpu()) {
        const int gpuCount = im->getGpuDevices().size();
        mChartGpu = new HistoryChart(tr("GPU"), gpuCount, nullptr, this);
        mChartGpu->setYMax(100);
        mRollingGpu = std::make_unique<RollingSeries>(toXYSeries(mChartGpu->getSeriesList()));
    }

    // SSO-26719: six cards, fixed order, defined in exactly one place.
    mCardOrder = { mChartCpu, mChartLoadPressure, mChartMemory };
    if (mChartGpu)
        mCardOrder.append(mChartGpu);
    mCardOrder.append(mChartDiskReadWrite);
    mCardOrder.append(mChartNetwork);

    buildLiveHistorySwitch();
    buildCardGrid();

    // SSO-26775 stage 2: each card reports its own expand/collapse request;
    // ResourcesPage (which owns the grid) does the relayout.
    for (QWidget *card : std::as_const(mCardOrder)) {
        connect(static_cast<HistoryChart *>(card), &HistoryChart::expandToggled,
                this, [this, card](bool expanded) { onCardExpandToggled(card, expanded); });
    }

    // DS §2/§3 (NEX-Resources): elevated container + accent-bar header for
    // every subsystem card.
    mChartCpu->setElevated(QStringLiteral("cpu"));
    mChartLoadPressure->setElevated(QStringLiteral("cpu"));
    if (mChartGpu)
        mChartGpu->setElevated(QStringLiteral("gpu"));
    mChartDiskReadWrite->setElevated(QStringLiteral("disk"));
    mChartMemory->setElevated(QStringLiteral("memory"));
    mChartNetwork->setElevated(QStringLiteral("network"));

#ifdef Q_OS_LINUX
    // SSO-26721: cpu/memory/io `some avg10` plotted as 3 lines on the Load &
    // Pressure card's secondary (0-100%) axis, replacing the old CPU-only
    // text status line. Only created when CPU PSI is present (kernel 4.20+,
    // CONFIG_PSI=y) — zero cost when absent, since it's never subscribed
    // either; memory/io each independently hide their own line in
    // onPsiUpdated() if their file is missing, rather than hiding the card.
    if (QFile::exists(QStringLiteral("/proc/pressure/cpu"))) {
        mPsiSeries = mChartLoadPressure->addSecondaryAxisSeries(3, 0, 100, tr("PSI"));
        static const QString psiNames[] = { tr("PSI CPU"), tr("PSI Memory"), tr("PSI IO") };
        for (int i = 0; i < mPsiSeries.count(); ++i)
            mPsiSeries.at(i)->setName(psiNames[i]);
        mRollingPsi = std::make_unique<RollingSeries>(toXYSeries(mPsiSeries));
    }

    // FW-11 (SSO-3739): OOM-kill status line, folded into the Memory card.
    // Always constructed — it hides itself on hosts without any oomd /
    // cgroup v2 signal, so this costs nothing on, say, a macOS-port build.
    mOomKills = new OomKillsWidget(mChartMemory);
    {
        QWidget *extras = mChartMemory->extrasRow();
        static_cast<QVBoxLayout *>(extras->layout())->addWidget(mOomKills);
        extras->setVisible(true);
    }
#endif

    // Subscribe to DataRefreshService signals
    connect(mRefresh, &DataRefreshService::cpuUpdated,
            this, &ResourcesPage::onCpuUpdated);
    connect(mRefresh, &DataRefreshService::memoryUpdated,
            this, &ResourcesPage::onMemoryUpdated);
    connect(mRefresh, &DataRefreshService::networkUpdated,
            this, &ResourcesPage::onNetworkUpdated);
    connect(mRefresh, &DataRefreshService::diskIOUpdated,
            this, &ResourcesPage::onDiskIOUpdated);

    if (mChartGpu)
        connect(mRefresh, &DataRefreshService::gpuUpdated,
                this, &ResourcesPage::onGpuUpdated);

    // Always subscribe to diskHealthUpdated: even if no drives were present
    // at construction, the first async discovery (FR-96) will fire this and
    // the Disk I/O header's temperature text can be filled in then.
    connect(mRefresh, &DataRefreshService::diskHealthUpdated,
            this, &ResourcesPage::onDiskHealthUpdated);

#ifdef Q_OS_LINUX
    if (!mPsiSeries.isEmpty())
        connect(mRefresh, &DataRefreshService::psiUpdated,
                this, &ResourcesPage::onPsiUpdated);
    connect(mRefresh, &DataRefreshService::oomdUpdated,
            this, &ResourcesPage::onOomdUpdated);
    connect(mRefresh, &DataRefreshService::powerUpdated,
            this, &ResourcesPage::onPowerUpdated);
#endif

    // Disk Usage Analyzer launcher (FR-23) — the operator declined removing
    // it (SSO-26716 item D); kept outside the six-card grid as a compact
    // full-width row below it, behaviour unchanged.
    mDiskLauncher = new DiskUsageLauncherWidget(this);
    ui->chartsLayout->addWidget(mDiskLauncher);
    mDiskLauncher->setElevated(QStringLiteral("disk"));

    // SSO-10930 / GH#220: historical view, now behind the Live/History
    // switch (item C) instead of stacked below the live charts. Starts
    // hidden — Live is the default mode.
    mResourceHistory = new ResourceHistoryPanel(this);
    mResourceHistory->setElevated(QStringLiteral("cpu"));
    mResourceHistory->setVisible(false);
    ui->chartsLayout->addWidget(mResourceHistory);

    connect(mResourceHistory, &ResourceHistoryPanel::openSettingsRequested, this, [] {
        emit SignalMapper::ins()->sigNavigateToPage(QStringLiteral("settings"));
    });
    connect(SignalMapper::ins(), &SignalMapper::sigResourceHistoryToggled,
            mResourceHistory, &ResourceHistoryPanel::refresh);
    connect(ResourceHistoryService::ins(), &ResourceHistoryService::historyChanged,
            mResourceHistory, &ResourceHistoryPanel::refresh);

    // SSO-26717 (B5): DataRefreshService::pause() (app minimized/backgrounded)
    // stops every timer, so no disk-I/O/network ticks arrive at all for the
    // duration — unlike page deactivation, where another subscribed page can
    // keep ticks flowing. Invalidate the baselines here too so the first
    // sample after resume re-baselines instead of plotting a delta spanning
    // the whole paused interval.
    connect(SignalMapper::ins(), &SignalMapper::sigAppVisibilityChanged,
            this, [this](bool visible) {
        if (!visible)
            invalidateIOBaselines();
    });

    // SSO-26720: apply the persisted Live/History mode now that mCardGrid,
    // mDiskLauncher and mResourceHistory all exist — buildLiveHistorySwitch()
    // only set the buttons' checked state, deliberately without firing
    // setLiveModeActive() this early.
    setLiveModeActive(mBtnLive->isChecked());
}

void ResourcesPage::buildLiveHistorySwitch()
{
    auto *row = new QWidget(this);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 4);
    layout->setSpacing(6);
    layout->addStretch();

    // SSO-26720: remember the last selected mode across sessions. Set the
    // initial checked state before the toggled() connections below so this
    // doesn't fire setLiveModeActive() before mCardGrid/mResourceHistory
    // exist — init() applies the persisted mode explicitly once everything
    // is built.
    const bool startInHistory = SettingManager::ins()->getResourcesHistoryModeActive();

    mBtnLive = new QToolButton(row);
    // Named so the screenshot test can switch modes before capturing
    // resources_history.png (SSO-26720).
    mBtnLive->setObjectName("resourcesLiveModeButton");
    mBtnLive->setText(tr("Live"));
    mBtnLive->setCheckable(true);
    mBtnLive->setChecked(!startInHistory);
    mBtnLive->setAutoRaise(true); // BUG-52
    mBtnLive->setCursor(Qt::PointingHandCursor);

    mBtnHistory = new QToolButton(row);
    mBtnHistory->setObjectName("resourcesHistoryModeButton");
    mBtnHistory->setText(tr("History"));
    mBtnHistory->setCheckable(true);
    mBtnHistory->setChecked(startInHistory);
    mBtnHistory->setAutoRaise(true); // BUG-52
    mBtnHistory->setCursor(Qt::PointingHandCursor);

    auto *group = new QButtonGroup(row);
    group->setExclusive(true);
    group->addButton(mBtnLive);
    group->addButton(mBtnHistory);

    layout->addWidget(mBtnLive);
    layout->addWidget(mBtnHistory);

    connect(mBtnLive, &QToolButton::toggled, this, [this](bool checked) {
        if (checked) setLiveModeActive(true);
    });
    connect(mBtnHistory, &QToolButton::toggled, this, [this](bool checked) {
        if (checked) setLiveModeActive(false);
    });

    ui->chartsLayout->addWidget(row);
}

void ResourcesPage::setLiveModeActive(bool live)
{
    // SSO-26775 stage 2: switching to History mode hides the whole card
    // grid, including the expanded card itself — collapse it first so it
    // doesn't come back still-expanded (and its siblings still hidden) the
    // next time Live mode is selected.
    if (!live && mExpandedCard) {
        static_cast<HistoryChart *>(mExpandedCard)->resetExpandState();
        collapseExpandedCard();
    }

    mCardGrid->setVisible(live);
    if (mDiskLauncher)
        mDiskLauncher->setVisible(live);
    if (mResourceHistory) {
        mResourceHistory->setVisible(!live);
        if (!live)
            mResourceHistory->refresh();
    }
    // SSO-26720: remember the mode across sessions.
    SettingManager::ins()->setResourcesHistoryModeActive(!live);
}

void ResourcesPage::buildCardGrid()
{
    mCardGrid = new QWidget(this);
    // Named so tests can find it by objectName.
    mCardGrid->setObjectName("resourcesCardGrid");
    mCardGridLayout = new QGridLayout(mCardGrid);
    mCardGridLayout->setContentsMargins(0, 0, 0, 0);
    mCardGridLayout->setSpacing(10);

    ui->chartsLayout->addWidget(mCardGrid);

    mCardCols = 0; // force the first reflow to actually place every card
    reflowCardGrid(2);
}

void ResourcesPage::reflowCardGrid(int cols)
{
    if (!mCardGridLayout || mCardOrder.isEmpty())
        return;
    mCardCols = cols;

    for (QWidget *card : std::as_const(mCardOrder))
        mCardGridLayout->removeWidget(card);

    for (int i = 0; i < mCardOrder.size(); ++i)
        mCardGridLayout->addWidget(mCardOrder[i], i / cols, i % cols);

    mCardGridLayout->setColumnStretch(0, 1);
    mCardGridLayout->setColumnStretch(1, cols > 1 ? 1 : 0);

    // Clear the previous stretch row before assigning a new one — QGridLayout
    // never shrinks its tracked row count, so a stale stretch factor on a row
    // that now holds a card would leave a gap above it.
    if (mCardStretchRow >= 0)
        mCardGridLayout->setRowStretch(mCardStretchRow, 0);
    mCardStretchRow = (mCardOrder.size() + cols - 1) / cols;
    mCardGridLayout->setRowStretch(mCardStretchRow, 1);
}

void ResourcesPage::onCardExpandToggled(QWidget *card, bool expanded)
{
    if (expanded)
        expandCard(card);
    else
        collapseExpandedCard();
}

void ResourcesPage::expandCard(QWidget *card)
{
    if (!mCardGridLayout || !mCardOrder.contains(card) || mExpandedCard == card)
        return;

    mExpandedCard = card;
    for (QWidget *w : std::as_const(mCardOrder))
        w->setVisible(w == card);
    placeExpandedCard();
}

void ResourcesPage::collapseExpandedCard()
{
    if (!mExpandedCard)
        return;

    mExpandedCard = nullptr;
    for (QWidget *w : std::as_const(mCardOrder))
        w->setVisible(true);
    // Restores exactly the layout reflowCardGrid() would already have in
    // place, had the card never expanded.
    reflowCardGrid(mCardCols);
}

void ResourcesPage::placeExpandedCard()
{
    if (!mExpandedCard || !mCardGridLayout)
        return;

    mCardGridLayout->removeWidget(mExpandedCard);

    // Clear the trailing stretch row reflowCardGrid() left behind — the
    // expanded card gets that stretch itself below, and a stale factor on a
    // now-empty row would otherwise reserve dead space under it.
    if (mCardStretchRow >= 0)
        mCardGridLayout->setRowStretch(mCardStretchRow, 0);
    mCardStretchRow = -1;

    const int span = qMax(mCardCols, 1);
    mCardGridLayout->addWidget(mExpandedCard, 0, 0, 1, span);
    mCardGridLayout->setColumnStretch(0, 1);
    mCardGridLayout->setColumnStretch(1, span > 1 ? 1 : 0);
    mCardGridLayout->setRowStretch(0, 1);
}

void ResourcesPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateCardGridForWidth();
}

void ResourcesPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // Mirrors resizeEvent()'s reflow — a widget can receive its first real
    // geometry as part of becoming the stacked widget's current page without
    // a QResizeEvent ever firing.
    updateCardGridForWidth();
}

void ResourcesPage::updateCardGridForWidth()
{
    const int cols = width() >= Dpi::scale(kTwoColumnThreshold) ? 2 : 1;
    if (cols == mCardCols)
        return;

    // SSO-26775 stage 2: a resize must not silently drop the expanded card
    // back into the normal grid — re-span it across the new column count
    // instead of reflowing every card.
    if (mExpandedCard) {
        mCardCols = cols;
        placeExpandedCard();
    } else {
        reflowCardGrid(cols);
    }
}

void ResourcesPage::invalidateIOBaselines()
{
    mDiskIOBaselineValid = false;
    mNetworkBaselineValid = false;
}

void ResourcesPage::refreshDiskIOHeader()
{
    QString secondary = mLastDiskWriteText;
    if (!mLastDiskTempText.isEmpty()) {
        secondary = secondary.isEmpty() ? mLastDiskTempText
                                         : secondary + QStringLiteral(" · ") + mLastDiskTempText;
    }
    mChartDiskReadWrite->setHeaderValue(mLastDiskReadText, secondary);
}

void ResourcesPage::refreshCpuHeader()
{
    mChartCpu->setHeaderValue(mLastCpuPercentText, mLastPowerText);
}

void ResourcesPage::onDiskIOUpdated(const QList<quint64> &io)
{
    const quint64 readBytes  = io.at(0);
    const quint64 writeBytes = io.at(1);

    // SSO-26717 (B5): re-baseline (rather than plot) whenever the page is
    // inactive, or the very first tick after it was reactivated / the app
    // resumed from DataRefreshService::pause() — otherwise that first delta
    // would span however long sampling was actually stopped.
    if (!mActive || !mDiskIOBaselineValid) {
        mLastReadBytes  = readBytes;
        mLastWriteBytes = writeBytes;
        mDiskIOBaselineValid = true;
        return;
    }

    // SSO-26717 (B1): counters can step backwards (device re-enumeration, a
    // sample that fails and reports 0). Unsigned subtraction would wrap to
    // ~16 EiB/s and latch the Y axis at that scale for the rest of the
    // session — guard via the helper shared with onNetworkUpdated().
    const quint64 d_readByte  = RateCounterUtil::guardedDelta(mLastReadBytes, readBytes);
    const quint64 d_writeByte = RateCounterUtil::guardedDelta(mLastWriteBytes, writeBytes);

    QVector<QSplineSeries*> seriesList = mChartDiskReadWrite->getSeriesList();
    mRollingDiskIO->push({ double(d_readByte), double(d_writeByte) });

    mLastReadBytes  = readBytes;
    mLastWriteBytes = writeBytes;

    // SSO-26718 (B9): floor matches the old static's initial 100 KIBI, but
    // the max now shrinks once a spike's point ages out of the window.
    const double maxY = mRollingDiskIO->windowYMax(double((1L << 10) * 100));
    mChartDiskReadWrite->setYMax(maxY);

    mChartDiskReadWrite->setSeriesList(seriesList);
    mChartDiskReadWrite->setCategoryAxisYLabels();

    mLastDiskReadText  = tr("%1/s").arg(FormatUtil::formatBytes(d_readByte));
    mLastDiskWriteText = tr("Write %1/s").arg(FormatUtil::formatBytes(d_writeByte));
    refreshDiskIOHeader();
}

void ResourcesPage::onCpuUpdated(const QList<int> &percents, double clockGHz,
                                   const QList<double> &loadAvgs)
{
    Q_UNUSED(clockGHz)
    if (!mActive) return;

    // --- CPU total line (per-core moved to the "Per-core" toggle view) ---
    if (!percents.isEmpty()) {
        const int totalPercent = percents.at(0);
        mRollingCpu->push({ double(totalPercent) });
        mChartCpu->setSeriesList(mChartCpu->getSeriesList());

        mLastCpuPercentText = tr("%1%").arg(totalPercent);
        refreshCpuHeader();
    }

    // --- Load & Pressure: load averages ---
    {
        QVector<QSplineSeries*> seriesList = mChartLoadPressure->getSeriesList();

        QVector<double> values;
        values.reserve(seriesList.count());
        for (int j = 0; j < seriesList.count() && j < loadAvgs.count(); ++j)
            values.append(loadAvgs.at(j));
        mRollingLoadPressure->push(values);

        // SSO-26718 (B9): floor matches the old static's initial core count,
        // but the max now shrinks once a spike's point ages out of the window.
        const double maxAvg = mRollingLoadPressure->windowYMax(im->getCpuCoreCount());
        mChartLoadPressure->setYMax(maxAvg);

        mChartLoadPressure->setSeriesList(seriesList);

        // SSO-26721: a dashed core-count reference line makes load average
        // meaningful across core counts — users compare raw load numbers
        // with `uptime`/`top`, which report against core count too, rather
        // than a normalized ratio.
        mChartLoadPressure->setReferenceLine(im->getCpuCoreCount(), tr("Cores"));

        if (!loadAvgs.isEmpty())
            mChartLoadPressure->setHeaderValue(QString::asprintf("%.2f", loadAvgs.at(0)));
    }

    // --- Per-core detail (toggle view) ---
    if (mCoreDetail)
        mCoreDetail->onCpuUpdated(percents, im->getCpuClocks());
}

void ResourcesPage::onNetworkUpdated(quint64 rxBytes, quint64 txBytes)
{
    // SSO-26717 (B5): see onDiskIOUpdated() above — same re-baseline-on-gap
    // treatment for the network counters.
    if (!mActive || !mNetworkBaselineValid) {
        mLastRxBytes = rxBytes;
        mLastTxBytes = txBytes;
        mNetworkBaselineValid = true;
        return;
    }

    // SSO-26717 (B1): same backwards-step guard as disk I/O — plain unsigned
    // subtraction here could wrap to ~16 EiB/s and latch the Y axis at that
    // scale for the rest of the session.
    const quint64 d_RXbytes = RateCounterUtil::guardedDelta(mLastRxBytes, rxBytes);
    const quint64 d_TXbytes = RateCounterUtil::guardedDelta(mLastTxBytes, txBytes);

    QVector<QSplineSeries *> seriesList = mChartNetwork->getSeriesList();
    mRollingNetwork->push({ double(d_RXbytes), double(d_TXbytes) });

    mLastRxBytes = rxBytes;
    mLastTxBytes = txBytes;

    // SSO-26718 (B9): floor matches the old statics' initial 1 MEBI, but the
    // max now shrinks once a spike's point ages out of the window.
    const double max = mRollingNetwork->windowYMax(double(1L << 20));
    mChartNetwork->setYMax(max);

    mChartNetwork->setSeriesList(seriesList);
    mChartNetwork->setCategoryAxisYLabels();

    // SSO-26719: hero = current download rate; secondary = upload rate.
    // Deliberately not labelled "Total" anywhere here — that word is
    // reserved for the cumulative bytes-since-boot figure on the separate
    // Network Usage page.
    mChartNetwork->setHeaderValue(tr("%1/s").arg(FormatUtil::formatBytes(d_RXbytes)),
                                  tr("↑ %1/s").arg(FormatUtil::formatBytes(d_TXbytes)));
}

void ResourcesPage::onMemoryUpdated(const MemorySnapshot &snap)
{
    if (!mActive) return;

    QVector<QSplineSeries *> seriesList = mChartMemory->getSeriesList();

    QVector<double> values;
    values.reserve(seriesList.count());

    // Series 0: Swap
    double swapPct = 0;
    if (snap.swapTotal)
        swapPct = ((double)snap.swapUsed / (double)snap.swapTotal) * 100.0;
    values.append(swapPct);

    // Series 1: Memory Used
    double memPct = snap.total ? ((double)snap.used / (double)snap.total) * 100.0 : 0;
    values.append(memPct);

    // SSO-26721 (report §4): one platform series instead of two — macOS →
    // Compressed, Linux → Cached (Linux's memory_info.cpp never sets wired,
    // so snap.wired > 0 is still the reliable mac/Linux tell used elsewhere
    // in this file). Wired (macOS) / Active (Linux) move to the header
    // tooltip instead of their own line; Linux's Available is dropped
    // entirely — it's close to the inverse of Used and added nothing the
    // Used line didn't already show.
    double otherPct = 0;
    QString otherName;
    double droppedPct = 0;
    QString droppedName;
    const bool isMac = snap.wired > 0;
    if (isMac) {
        otherPct = snap.total ? ((double)snap.compressed / (double)snap.total) * 100.0 : 0;
        otherName = tr("Compressed");
        droppedPct = snap.total ? ((double)snap.wired / (double)snap.total) * 100.0 : 0;
        droppedName = tr("Wired");
    } else {
        otherPct = snap.total ? ((double)snap.cached / (double)snap.total) * 100.0 : 0;
        otherName = tr("Cached");
        droppedPct = snap.total ? ((double)snap.active / (double)snap.total) * 100.0 : 0;
        droppedName = tr("Active");
    }
    if (seriesList.count() > 2) {
        values.append(otherPct);
        seriesList.at(2)->setName(otherName);
    }

    // Series names 0/1 are constant ("Swap"/"Memory") — SSO-26719: legends
    // show series names only now, the live percentage moved to the header.
    seriesList.at(0)->setName(tr("Swap"));
    seriesList.at(1)->setName(tr("Memory"));

    mRollingMemory->push(values);
    mChartMemory->setSeriesList(seriesList);

    mChartMemory->setHeaderValue(QString::asprintf("%.0f%%", memPct));
    mChartMemory->setHeaderTooltip(tr("%1: %2%").arg(droppedName).arg(droppedPct, 0, 'f', 0));
}

void ResourcesPage::onGpuUpdated(const QList<GpuDevice> &gpus)
{
    if (!mChartGpu || !mActive)
        return;

    QVector<QSplineSeries *> seriesList = mChartGpu->getSeriesList();

    // SSO-26717 (B4) / SSO-26718: a GPU dropping out (gpus.size() <
    // seriesList.count()) just means fewer values get pushed this tick —
    // RollingSeries ages the remaining series without a new point rather
    // than drifting them out of sync with each other.
    const int count = qMin(seriesList.count(), gpus.size());

    QVector<double> values;
    values.reserve(count);
    int utilSum = 0;
    int utilCount = 0;
    for (int j = 0; j < count; j++) {
        const GpuDevice &gpu = gpus.at(j);
        const int util = qMax(0, gpu.utilization); // -1 → 0 for chart data point
        values.append(util);
        seriesList.at(j)->setName(gpu.name);
        if (gpu.utilization >= 0) {
            utilSum += util;
            utilCount++;
        }
    }

    mRollingGpu->push(values);
    mChartGpu->setSeriesList(seriesList);

    if (utilCount > 0)
        mChartGpu->setHeaderValue(tr("%1%").arg(utilSum / utilCount));
}

void ResourcesPage::onDiskHealthUpdated(const QList<DriveHealth> &drives)
{
    // B6 (by construction): the header shows one representative drive, so
    // there's no timing-dependent chart position to fix.
    mLastDiskTempText.clear();
    for (const DriveHealth &d : drives) {
        if (d.temperatureCelsius < 0)
            continue;
        const QString name = d.model.isEmpty() ? d.deviceName : d.model;
        mLastDiskTempText = QString("%1 %2°C").arg(name).arg(d.temperatureCelsius, 0, 'f', 0);
        break;
    }
    refreshDiskIOHeader();
}

#ifdef Q_OS_LINUX
void ResourcesPage::onPsiUpdated(const PsiSnapshot &cpu, const PsiSnapshot &memory, const PsiSnapshot &io)
{
    if (mPsiSeries.isEmpty())
        return;

    if (!mActive) {
        for (QSplineSeries *s : std::as_const(mPsiSeries))
            s->setVisible(false);
        return;
    }

    // SSO-26721: "a missing file hides that line, not the card" — each of
    // the 3 lines independently follows its own snapshot's `available`
    // flag. Push 0 for an unavailable metric rather than skipping it in
    // the values vector: RollingSeries::push() only supports dropping
    // *trailing* series by passing fewer values, not a gap in the middle,
    // and the hidden line's buffered zero never renders anyway.
    const PsiSnapshot *snaps[3] = { &cpu, &memory, &io };
    QVector<double> values;
    values.reserve(mPsiSeries.count());
    for (int i = 0; i < mPsiSeries.count() && i < 3; ++i) {
        mPsiSeries.at(i)->setVisible(snaps[i]->available);
        values.append(snaps[i]->available ? snaps[i]->someAvg10 : 0.0);
    }
    mRollingPsi->push(values);
    mChartLoadPressure->setSeriesList(mChartLoadPressure->getSeriesList());
}

void ResourcesPage::onOomdUpdated(const OomdSnapshot &snap)
{
    if (mOomKills)
        mOomKills->onOomdUpdated(snap);
}

void ResourcesPage::onPowerUpdated(const RaplPowerSnapshot &snap)
{
    mLastPowerText = snap.available ? tr("%1 W").arg(snap.totalPackageWatts, 0, 'f', 1) : QString();
    refreshCpuHeader();
}
#endif

void ResourcesPage::resetRollingSeries()
{
    // SSO-26718 (B12): clear every chart's buffered history on re-activate
    // rather than leaving a hidden gap mid-"last 60 s" — it starts collecting
    // fresh samples from empty instead of rendering a frozen, stale chart.
    if (mRollingCpu) mRollingCpu->reset();
    if (mRollingLoadPressure) mRollingLoadPressure->reset();
    if (mRollingDiskIO) mRollingDiskIO->reset();
    if (mRollingMemory) mRollingMemory->reset();
    if (mRollingNetwork) mRollingNetwork->reset();
    if (mRollingGpu) mRollingGpu->reset();
#ifdef Q_OS_LINUX
    if (mRollingPsi) mRollingPsi->reset();
#endif
}

void ResourcesPage::onPageActivated()
{
    mActive = true;
    resetRollingSeries();

    // FR-103: subscribe to the signals the resources view renders. Combined
    // with Dashboard subscriptions, these are the only consumers of CPU /
    // memory / network / disk-IO / GPU samples on the fast tick.
    mRefresh->subscribe(DataRefreshService::Signal::Cpu);
    mRefresh->subscribe(DataRefreshService::Signal::Memory);
    mRefresh->subscribe(DataRefreshService::Signal::Network);
    mRefresh->subscribe(DataRefreshService::Signal::DiskIO);
    if (mChartGpu)
        mRefresh->subscribe(DataRefreshService::Signal::Gpu);
#ifdef Q_OS_LINUX
    if (!mPsiSeries.isEmpty())
        mRefresh->subscribe(DataRefreshService::Signal::Psi);
    mRefresh->subscribe(DataRefreshService::Signal::Oomd);
    mRefresh->subscribe(DataRefreshService::Signal::Power);
#endif

    // Kick an immediate disk health refresh so the Disk I/O header's
    // temperature text populates on first open rather than waiting up to
    // 30 s for the slow tick.
    mRefresh->triggerDiskHealthCheck();

    // SSO-10930 / GH#220: pick up any setting/data change made while this
    // page was hidden (e.g. the user enabled history from Settings) — only
    // matters while History mode is the active switch position.
    if (mBtnHistory && mBtnHistory->isChecked() && mResourceHistory)
        mResourceHistory->refresh();
}

void ResourcesPage::onPageDeactivated()
{
    mActive = false;

    // SSO-26717 (B5): don't rely on ticks continuing to arrive while hidden
    // (another page may not be subscribed to Network/DiskIO either) —
    // unconditionally invalidate so onPageActivated()'s first sample always
    // re-baselines rather than risking a delta spanning the whole hidden span.
    invalidateIOBaselines();

    mRefresh->unsubscribe(DataRefreshService::Signal::Cpu);
    mRefresh->unsubscribe(DataRefreshService::Signal::Memory);
    mRefresh->unsubscribe(DataRefreshService::Signal::Network);
    mRefresh->unsubscribe(DataRefreshService::Signal::DiskIO);
    if (mChartGpu)
        mRefresh->unsubscribe(DataRefreshService::Signal::Gpu);
#ifdef Q_OS_LINUX
    if (!mPsiSeries.isEmpty())
        mRefresh->unsubscribe(DataRefreshService::Signal::Psi);
    mRefresh->unsubscribe(DataRefreshService::Signal::Oomd);
    mRefresh->unsubscribe(DataRefreshService::Signal::Power);
#endif
}
