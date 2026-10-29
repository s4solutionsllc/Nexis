#include "resources_page.h"
#include "nexis_page.h"
#include "ui_resources_page.h"
#include "utilities.h"
#include "signal_mapper.h"
#include "Managers/data_refresh_service.h"
#include "Managers/resource_history_service.h"
#include "Utils/rate_counter_util.h"
#include <QFile>

#ifdef Q_OS_LINUX
#include "Pages/Helpers/oom_kills_widget.h"
#include "Pages/Helpers/power_draw_widget.h"
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
    mChartCpu(new HistoryChart(tr("History of CPU"), im->getCpuCoreCount(), nullptr, this)),
    mChartCpuLoadAvg(new HistoryChart(tr("History of CPU Load Averages"), 3, nullptr, this)),
    mChartDiskReadWrite(new HistoryChart(tr("History of Disk Read Write"), 2, new QCategoryAxis, this)),
    mChartMemory(new HistoryChart(tr("History of Memory"), 4, nullptr, this)),
    mChartNetwork(new HistoryChart(tr("History of Network"), 2, new QCategoryAxis, this)),
    mChartGpu(nullptr),
    mChartDiskHealth(nullptr),
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
    mChartCpu->setYMax(100);
    mChartMemory->setYMax(100);

    // SSO-26718: one RollingSeries per chart, replacing the shift-and-insert
    // loop each handler used to carry. Default window (60s) / capacity (256)
    // match every chart's existing 0-60 axis range.
    mRollingCpu = std::make_unique<RollingSeries>(toXYSeries(mChartCpu->getSeriesList()));
    mRollingCpuLoadAvg = std::make_unique<RollingSeries>(toXYSeries(mChartCpuLoadAvg->getSeriesList()));
    mRollingDiskIO = std::make_unique<RollingSeries>(toXYSeries(mChartDiskReadWrite->getSeriesList()));
    mRollingMemory = std::make_unique<RollingSeries>(toXYSeries(mChartMemory->getSeriesList()));
    mRollingNetwork = std::make_unique<RollingSeries>(toXYSeries(mChartNetwork->getSeriesList()));

    // GPU chart — only if GPU(s) detected
    if (im->hasGpu()) {
        int gpuCount = im->getGpuDevices().size();
        mChartGpu = new HistoryChart(tr("History of GPU"), gpuCount, nullptr, this);
        mChartGpu->setYMax(100);
        mRollingGpu = std::make_unique<RollingSeries>(toXYSeries(mChartGpu->getSeriesList()));
    }

    // Disk health temperature chart is created lazily on first
    // diskHealthUpdated signal — drive discovery is async (FR-96) so drive
    // data may not be available at construction time.
    if (im->hasDiskHealth())
        ensureDiskHealthChart(im->getDriveHealth());

    QList<QWidget*> widgets = { mChartCpu, mChartCpuLoadAvg, mChartDiskReadWrite, mChartMemory, mChartNetwork };

    // Insert GPU chart after CPU charts
    if (mChartGpu)
        widgets.insert(2, mChartGpu);  // after CPU Load Avg, before Disk R/W

    // SSO-10930 / GH#220: historical view, right after the live Network
    // chart and before the disk-health / PSI / OOM / power cards below.
    mResourceHistory = new ResourceHistoryPanel(this);
    {
        const int netIdx = widgets.indexOf(mChartNetwork);
        widgets.insert(netIdx + 1, mResourceHistory);
    }

    // Insert disk health chart after Network, before Disk Launcher
    if (mChartDiskHealth)
        widgets.append(mChartDiskHealth);

#ifdef Q_OS_LINUX
    if (QFile::exists(QStringLiteral("/proc/pressure/cpu"))) {
        mChartPsiCpu = new HistoryChart(tr("CPU Pressure Stall (some)"), 3, nullptr, this);
        mChartPsiCpu->setYMax(1.0);
        mRollingPsi = std::make_unique<RollingSeries>(toXYSeries(mChartPsiCpu->getSeriesList()));
        widgets.append(mChartPsiCpu);
    }

    // FW-11 (SSO-3739): OOM-kill observability panel. Always constructed —
    // the widget hides itself on hosts without any oomd / cgroup v2 signal,
    // so this costs nothing on, say, a stock macOS-port build target.
    mOomKills = new OomKillsWidget(this);
    widgets.append(mOomKills);

    // SSO-15378: package power via powercap/RAPL. Hides itself on hosts
    // without any RAPL zones (VMs, non-x86, unsupported CPUs).
    mPowerDraw = new PowerDrawWidget(this);
    widgets.append(mPowerDraw);
#endif

    // SSO-15378: compact per-core utilization/frequency detail — cross
    // platform, unlike the RAPL card above.
    mCoreDetail = new CpuCoreDetailWidget(this);
    widgets.append(mCoreDetail);

    for (QWidget *widget : widgets) {
        ui->chartsLayout->addWidget(widget);
    }

    // DS §2/§3 (NEX-Resources): elevated container + accent-bar header for
    // every history-chart band. SSO-13731 (PR #247) covered CPU, CPU Load
    // Averages, GPU, Disk Read Write; SSO-14437 extends the same treatment to
    // the remaining tiles named in notes/resources_remaining.md (Memory,
    // Network, Disk Temperature — elevated below; PSI and OOM Kills —
    // elevated further down since they're conditional/Linux-only).
    mChartCpu->setElevated(QStringLiteral("cpu"));
    mChartCpuLoadAvg->setElevated(QStringLiteral("cpu"));
    if (mChartGpu)
        mChartGpu->setElevated(QStringLiteral("gpu"));
    mChartDiskReadWrite->setElevated(QStringLiteral("disk"));
    mChartMemory->setElevated(QStringLiteral("memory"));
    mChartNetwork->setElevated(QStringLiteral("network"));
    mResourceHistory->setElevated(QStringLiteral("cpu"));
    // mChartDiskHealth, if already constructed at this point, was elevated
    // inside ensureDiskHealthChart() above — that single call site also
    // handles the lazy (FR-96) creation path from onDiskHealthUpdated().

#ifdef Q_OS_LINUX
    if (mChartPsiCpu)
        mChartPsiCpu->setElevated(QStringLiteral("cpu"));
    if (mOomKills)
        mOomKills->setElevated(QStringLiteral("memory"));
    if (mPowerDraw)
        mPowerDraw->setElevated(QStringLiteral("cpu"));
#endif
    if (mCoreDetail)
        mCoreDetail->setElevated(QStringLiteral("cpu"));

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
    // we can build the chart then.
    connect(mRefresh, &DataRefreshService::diskHealthUpdated,
            this, &ResourcesPage::onDiskHealthUpdated);

#ifdef Q_OS_LINUX
    if (mChartPsiCpu)
        connect(mRefresh, &DataRefreshService::psiUpdated,
                this, &ResourcesPage::onPsiUpdated);
    if (mOomKills)
        connect(mRefresh, &DataRefreshService::oomdUpdated,
                this, &ResourcesPage::onOomdUpdated);
    if (mPowerDraw)
        connect(mRefresh, &DataRefreshService::powerUpdated,
                this, &ResourcesPage::onPowerUpdated);
#endif

    // Disk Usage Analyzer launcher (FR-23)
    mDiskLauncher = new DiskUsageLauncherWidget(this);
    ui->chartsLayout->addWidget(mDiskLauncher);
    mDiskLauncher->setElevated(QStringLiteral("disk"));

    // SSO-10930 / GH#220
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
}

void ResourcesPage::invalidateIOBaselines()
{
    mDiskIOBaselineValid = false;
    mNetworkBaselineValid = false;
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

    seriesList.at(0)->setName(tr("Read: %1/s Total: %2")
                              .arg(FormatUtil::formatBytes(d_readByte))
                              .arg(FormatUtil::formatBytes(readBytes)));

    seriesList.at(1)->setName(tr("Write: %1/s Total: %2")
                              .arg(FormatUtil::formatBytes(d_writeByte))
                              .arg(FormatUtil::formatBytes(writeBytes)));

    mLastReadBytes  = readBytes;
    mLastWriteBytes = writeBytes;

    // SSO-26718 (B9): floor matches the old static's initial 100 KIBI, but
    // the max now shrinks once a spike's point ages out of the window.
    const double maxY = mRollingDiskIO->windowYMax(double((1L << 10) * 100));
    mChartDiskReadWrite->setYMax(maxY);

    mChartDiskReadWrite->setSeriesList(seriesList);

    mChartDiskReadWrite->setCategoryAxisYLabels();
}

void ResourcesPage::onCpuUpdated(const QList<int> &percents, double clockGHz,
                                   const QList<double> &loadAvgs)
{
    Q_UNUSED(clockGHz)
    if (!mActive) return;

    advanceDiskTempChart();

    // --- CPU per-core chart ---
    {
        QVector<QSplineSeries *> seriesList = mChartCpu->getSeriesList();

        // SSO-3380 / WI-18: DataRefreshService gates the emit on a non-empty
        // percents list, but guard the per-core indexing here too — `.at(j+1)`
        // is UB in release if the producer ever returns fewer entries than
        // chart series. The load-avg block below is safe (loadAvgs is always
        // 3 elements) and runs regardless.
        const int neededPercents = seriesList.count() + 1; // index 0 + per-core
        const bool percentsOk = percents.size() >= neededPercents;

        if (percentsOk) {
            QVector<double> values;
            values.reserve(seriesList.count());
            for (int j = 0; j < seriesList.count(); j++) {
                int p = percents.at(j + 1);
                values.append(p);
                seriesList.at(j)->setName(QString("CPU%1: %2%").arg(j + 1).arg(p));
            }
            mRollingCpu->push(values);
            mChartCpu->setSeriesList(seriesList);
        }
    }

    // --- CPU load average chart ---
    {
        static const int minutes[] = {1, 5, 15};

        QVector<QSplineSeries*> seriesList = mChartCpuLoadAvg->getSeriesList();

        QVector<double> values;
        values.reserve(seriesList.count());
        for (int j = 0; j < seriesList.count(); ++j) {
            double avg = loadAvgs.at(j);
            values.append(avg);
            seriesList.at(j)->setName(tr("%1 Minute Average: %2")
                                      .arg(minutes[j])
                                      .arg(avg));
        }
        mRollingCpuLoadAvg->push(values);

        // SSO-26718 (B9): floor matches the old static's initial core count,
        // but the max now shrinks once a spike's point ages out of the window.
        const double maxAvg = mRollingCpuLoadAvg->windowYMax(im->getCpuCoreCount());
        mChartCpuLoadAvg->setYMax(maxAvg);

        mChartCpuLoadAvg->setSeriesList(seriesList);
    }

    // --- Per-core detail list (SSO-15378) ---
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

    seriesList.at(0)->setName(tr("Download: %1/s Total: %2")
                              .arg(FormatUtil::formatBytes(d_RXbytes))
                              .arg(FormatUtil::formatBytes(rxBytes)));

    seriesList.at(1)->setName(tr("Upload: %1/s  Total: %2")
                              .arg(FormatUtil::formatBytes(d_TXbytes))
                              .arg(FormatUtil::formatBytes(txBytes)));

    mLastRxBytes = rxBytes;
    mLastTxBytes = txBytes;

    // SSO-26718 (B9): floor matches the old statics' initial 1 MEBI, but the
    // max now shrinks once a spike's point ages out of the window.
    const double max = mRollingNetwork->windowYMax(double(1L << 20));
    mChartNetwork->setYMax(max);

    mChartNetwork->setSeriesList(seriesList);

    mChartNetwork->setCategoryAxisYLabels();
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
    seriesList.at(0)->setName(tr("Swap: %1 (%2%) %3")
                              .arg(FormatUtil::formatBytes(snap.swapUsed))
                              .arg(QString::asprintf("%.1f", swapPct))
                              .arg(FormatUtil::formatBytes(snap.swapTotal)));

    // Series 1: Memory Used
    double memPct = snap.total ? ((double)snap.used / (double)snap.total) * 100.0 : 0;

    values.append(memPct);
    seriesList.at(1)->setName(tr("Memory: %1 (%2%) %3")
                              .arg(FormatUtil::formatBytes(snap.used))
                              .arg(QString::asprintf("%.1f", memPct))
                              .arg(FormatUtil::formatBytes(snap.total)));

    // FR-57: Series 2 — Wired % (macOS) or Available % (Linux)
    if (seriesList.count() > 2) {
        double pct2 = 0;
        QString name2;
        if (snap.wired > 0) {
            pct2 = snap.total ? ((double)snap.wired / (double)snap.total) * 100.0 : 0;
            name2 = tr("Wired: %1 (%2%)")
                .arg(FormatUtil::formatBytes(snap.wired))
                .arg(QString::asprintf("%.1f", pct2));
        } else {
            pct2 = snap.total ? ((double)snap.available / (double)snap.total) * 100.0 : 0;
            name2 = tr("Available: %1 (%2%)")
                .arg(FormatUtil::formatBytes(snap.available))
                .arg(QString::asprintf("%.1f", pct2));
        }
        values.append(pct2);
        seriesList.at(2)->setName(name2);
    }

    // FR-57: Series 3 — Compressed % (macOS) or Active % (Linux)
    if (seriesList.count() > 3) {
        double pct3 = 0;
        QString name3;
        if (snap.compressed > 0 || snap.wired > 0) {
            pct3 = snap.total ? ((double)snap.compressed / (double)snap.total) * 100.0 : 0;
            name3 = tr("Compressed: %1 (%2%)")
                .arg(FormatUtil::formatBytes(snap.compressed))
                .arg(QString::asprintf("%.1f", pct3));
        } else {
            pct3 = snap.total ? ((double)snap.active / (double)snap.total) * 100.0 : 0;
            name3 = tr("Active: %1 (%2%)")
                .arg(FormatUtil::formatBytes(snap.active))
                .arg(QString::asprintf("%.1f", pct3));
        }
        values.append(pct3);
        seriesList.at(3)->setName(name3);
    }

    mRollingMemory->push(values);
    mChartMemory->setSeriesList(seriesList);
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
    for (int j = 0; j < count; j++) {
        const GpuDevice &gpu = gpus.at(j);
        int util = qMax(0, gpu.utilization);  // -1 → 0 for chart data point
        values.append(util);

        QString label = (gpu.utilization < 0)
            ? QString("%1: N/A").arg(gpu.name)
            : QString("%1: %2%").arg(gpu.name).arg(util);
        seriesList.at(j)->setName(label);
    }

    mRollingGpu->push(values);
    mChartGpu->setSeriesList(seriesList);
}

void ResourcesPage::ensureDiskHealthChart(const QList<DriveHealth> &drives)
{
    if (mChartDiskHealth)
        return;

    int tempDriveCount = 0;
    for (const DriveHealth &d : drives) {
        if (d.temperatureCelsius >= 0)
            tempDriveCount++;
    }
    if (tempDriveCount == 0)
        return;

    mChartDiskHealth = new HistoryChart(tr("History of Disk Temperature"),
                                        tempDriveCount, nullptr, this);
    // 0–100 flattens a drive that lives between 30 and 60 °C into a straight
    // line at the bottom third; 20–90 covers idle to thermal-throttle.
    mChartDiskHealth->setYRange(20, 90);
    mRollingDiskHealth = std::make_unique<RollingSeries>(toXYSeries(mChartDiskHealth->getSeriesList()));

    // Insert at the end of the charts layout (after Network), before
    // the DiskUsageLauncher if it already exists.
    if (mDiskLauncher) {
        int idx = ui->chartsLayout->indexOf(mDiskLauncher);
        if (idx >= 0) {
            ui->chartsLayout->insertWidget(idx, mChartDiskHealth);
        } else {
            ui->chartsLayout->addWidget(mChartDiskHealth);
        }
    } else {
        ui->chartsLayout->addWidget(mChartDiskHealth);
    }
    // Single call site for both the construction-time and lazy (FR-96)
    // creation paths (see setElevated() comment in init()).
    mChartDiskHealth->setElevated(QStringLiteral("temp"));
}

void ResourcesPage::onDiskHealthUpdated(const QList<DriveHealth> &drives)
{
    // FR-96: create the chart on first data arrival if it wasn't built
    // during construction (discovery was still running then).
    if (!mChartDiskHealth)
        ensureDiskHealthChart(drives);

    if (!mChartDiskHealth)
        return;

    // Remember the reading even while the page is hidden.
    const bool firstReading = mLastDiskTemps.isEmpty();
    mLastDiskTemps.clear();
    mLastDiskNames.clear();
    for (const DriveHealth &d : drives) {
        if (d.temperatureCelsius < 0)
            continue;
        mLastDiskTemps.append(d.temperatureCelsius);
        mLastDiskNames.append(d.model.isEmpty() ? d.deviceName : d.model);
    }

    if (firstReading && mActive)
        advanceDiskTempChart();
}

void ResourcesPage::advanceDiskTempChart()
{
    if (!mChartDiskHealth || mLastDiskTemps.isEmpty())
        return;

    QVector<QSplineSeries *> seriesList = mChartDiskHealth->getSeriesList();

    // SSO-26717 (B4) / SSO-26718: a drive that temporarily stops reporting a
    // temperature (mLastDiskTemps shrinks) just pushes fewer values this
    // tick \u2014 RollingSeries ages the remaining series without a new point.
    const int count = qMin(seriesList.count(), mLastDiskTemps.count());

    QVector<double> values;
    values.reserve(count);
    for (int idx = 0; idx < count; ++idx) {
        const double temp = mLastDiskTemps.at(idx);
        values.append(temp);
        seriesList.at(idx)->setName(QString("%1: %2 \u00B0C").arg(mLastDiskNames.at(idx)).arg(temp, 0, 'f', 0));
    }

    mRollingDiskHealth->push(values);
    mChartDiskHealth->setSeriesList(seriesList);
}

#ifdef Q_OS_LINUX
void ResourcesPage::onPsiUpdated(const PsiSnapshot &snap)
{
    if (!mChartPsiCpu || !mActive || !snap.available)
        return;

    QVector<QSplineSeries*> seriesList = mChartPsiCpu->getSeriesList();

    const double values[3] = { snap.someAvg10, snap.someAvg60, snap.someAvg300 };
    const char  *labels[3] = { "avg10", "avg60", "avg300" };

    for (int i = 0; i < 3; ++i) {
        seriesList.at(i)->setName(
            tr("CPU %1: %2%").arg(QLatin1String(labels[i]))
                             .arg(values[i], 0, 'f', 1));
    }

    mRollingPsi->push({ values[0], values[1], values[2] });

    // SSO-26718 (B9): auto-scale Y with 25% headroom over the real window
    // max, floored flat at 1% (matching the old static's initial value) —
    // and now shrinks once a spike's point ages out of the window instead of
    // only ever growing.
    const double maxPsi = qMax(1.0, mRollingPsi->windowYMax(0.0) * 1.25);
    mChartPsiCpu->setYMax(maxPsi);

    mChartPsiCpu->setSeriesList(seriesList);
}

void ResourcesPage::onOomdUpdated(const OomdSnapshot &snap)
{
    if (mOomKills)
        mOomKills->onOomdUpdated(snap);
}

void ResourcesPage::onPowerUpdated(const RaplPowerSnapshot &snap)
{
    if (mPowerDraw)
        mPowerDraw->onPowerUpdated(snap);
}
#endif

void ResourcesPage::resetRollingSeries()
{
    // SSO-26718 (B12): clear every chart's buffered history on re-activate
    // rather than leaving a hidden gap mid-"last 60 s" — it starts collecting
    // fresh samples from empty instead of rendering a frozen, stale chart.
    if (mRollingCpu) mRollingCpu->reset();
    if (mRollingCpuLoadAvg) mRollingCpuLoadAvg->reset();
    if (mRollingDiskIO) mRollingDiskIO->reset();
    if (mRollingMemory) mRollingMemory->reset();
    if (mRollingNetwork) mRollingNetwork->reset();
    if (mRollingGpu) mRollingGpu->reset();
    if (mRollingDiskHealth) mRollingDiskHealth->reset();
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
    mRefresh->subscribe(DataRefreshService::Signal::Gpu);
#ifdef Q_OS_LINUX
    if (mChartPsiCpu)
        mRefresh->subscribe(DataRefreshService::Signal::Psi);
    if (mOomKills)
        mRefresh->subscribe(DataRefreshService::Signal::Oomd);
    if (mPowerDraw)
        mRefresh->subscribe(DataRefreshService::Signal::Power);
#endif

    // Kick an immediate disk health refresh so the temperature chart populates
    // on first open rather than waiting up to 30 s for the slow tick.
    mRefresh->triggerDiskHealthCheck();

    // SSO-10930 / GH#220: pick up any setting/data change made while this
    // page was hidden (e.g. the user enabled history from Settings).
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
    mRefresh->unsubscribe(DataRefreshService::Signal::Gpu);
#ifdef Q_OS_LINUX
    if (mChartPsiCpu)
        mRefresh->unsubscribe(DataRefreshService::Signal::Psi);
    if (mOomKills)
        mRefresh->unsubscribe(DataRefreshService::Signal::Oomd);
    if (mPowerDraw)
        mRefresh->unsubscribe(DataRefreshService::Signal::Power);
#endif
}
