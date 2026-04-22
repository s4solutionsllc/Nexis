#include "resources_page.h"
#include "ui_resources_page.h"
#include "utilities.h"
#include "Managers/data_refresh_service.h"
#include <QFile>

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

    init();
}

void ResourcesPage::init()
{
    mChartCpu->setYMax(100);
    mChartMemory->setYMax(100);

    // GPU chart — only if GPU(s) detected
    if (im->hasGpu()) {
        int gpuCount = im->getGpuDevices().size();
        mChartGpu = new HistoryChart(tr("History of GPU"), gpuCount, nullptr, this);
        mChartGpu->setYMax(100);
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

    // Insert disk health chart after Network, before Disk Launcher
    if (mChartDiskHealth)
        widgets.append(mChartDiskHealth);

#ifdef Q_OS_LINUX
    if (QFile::exists(QStringLiteral("/proc/pressure/cpu"))) {
        mChartPsiCpu = new HistoryChart(tr("CPU Pressure Stall (some)"), 3, nullptr, this);
        mChartPsiCpu->setYMax(1.0);
        widgets.append(mChartPsiCpu);
    }
#endif

    for (QWidget *widget : widgets) {
        ui->chartsLayout->addWidget(widget);
    }

    Utilities::addDropShadow(widgets, 40);

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
#endif

    // Disk Usage Analyzer launcher (FR-23)
    mDiskLauncher = new DiskUsageLauncherWidget(this);
    ui->chartsLayout->addWidget(mDiskLauncher);
    Utilities::addDropShadow(mDiskLauncher, 40);
}

void ResourcesPage::onDiskIOUpdated(const QList<quint64> &io)
{
    static quint64 l_readBytes  = io.at(0);
    static quint64 l_writeBytes = io.at(1);

    if (!mActive) {
        l_readBytes  = io.at(0);
        l_writeBytes = io.at(1);
        return;
    }

    static int second = 0;
    static quint64 maxY = (1L << 10) * 100; // 100 KIBI

    QVector<QSplineSeries*> seriesList = mChartDiskReadWrite->getSeriesList();

    for (int j = 0; j < seriesList.count(); ++j) {
        for (int i = 0; i < (second < 61 ? second : 61); ++i) {
            seriesList.at(j)->replace(i, (i+1), seriesList.at(j)->at(i).y());
        }

        if(second > 61) seriesList.at(j)->removePoints(61, 1);
    }

    quint64 readBytes  = io.at(0);
    quint64 writeBytes = io.at(1);

    quint64 d_readByte = (readBytes - l_readBytes);
    quint64 d_writeByte = (writeBytes - l_writeBytes);

    seriesList.at(0)->insert(0, QPointF(0, d_readByte));
    seriesList.at(0)->setName(tr("Read: %1/s Total: %2")
                              .arg(FormatUtil::formatBytes(d_readByte))
                              .arg(FormatUtil::formatBytes(readBytes)));


    seriesList.at(1)->insert(0, QPointF(0, d_writeByte));
    seriesList.at(1)->setName(tr("Write: %1/s Total: %2")
                              .arg(FormatUtil::formatBytes(d_writeByte))
                              .arg(FormatUtil::formatBytes(writeBytes)));

    maxY = qMax(qMax(maxY, d_readByte), d_writeByte);

    l_readBytes  = readBytes;
    l_writeBytes = writeBytes;

    mChartDiskReadWrite->setYMax(maxY);

    mChartDiskReadWrite->setSeriesList(seriesList);

    mChartDiskReadWrite->setCategoryAxisYLabels();

    second++;
}

void ResourcesPage::onCpuUpdated(const QList<int> &percents, double clockGHz,
                                   const QList<double> &loadAvgs)
{
    Q_UNUSED(clockGHz)
    if (!mActive) return;

    // --- CPU per-core chart ---
    {
        static int second = 0;

        QVector<QSplineSeries *> seriesList = mChartCpu->getSeriesList();

        for (int j = 0; j < seriesList.count(); j++){
            int p = percents.at(j+1);

            for (int i = 0; i < (second < 61 ? second : 61); i++)
                seriesList.at(j)->replace(i, (i+1), seriesList.at(j)->at(i).y());

            seriesList.at(j)->insert(0, QPointF(0, p));

            seriesList.at(j)->setName(QString("CPU%1: %2%").arg(j+1).arg(p));

            if(second > 61) seriesList.at(j)->removePoints(61, 1);
        }

        mChartCpu->setSeriesList(seriesList);

        second++;
    }

    // --- CPU load average chart ---
    {
        static int second = 0;
        static int maxAvg = im->getCpuCoreCount();
        static int minutes[] = {1, 5, 15};

        QVector<QSplineSeries*> seriesList = mChartCpuLoadAvg->getSeriesList();

        for (int j = 0; j < seriesList.count(); ++j) {
            double avg = loadAvgs.at(j);

            for (int i = 0; i < (second < 61 ? second : 61); ++i) {
                seriesList.at(j)->replace(i, (i+1), seriesList.at(j)->at(i).y());
            }

            seriesList.at(j)->insert(0, QPointF(0, avg));

            seriesList.at(j)->setName(tr("%1 Minute Average: %2")
                                      .arg(minutes[j])
                                      .arg(avg));

            if (second > 61) seriesList.at(j)->removePoints(61, 1);

            maxAvg = qMax((int)ceil(avg), maxAvg);
        }

        mChartCpuLoadAvg->setYMax(maxAvg);

        mChartCpuLoadAvg->setSeriesList(seriesList);

        second++;
    }
}

void ResourcesPage::onNetworkUpdated(quint64 rxBytes, quint64 txBytes)
{
    static quint64 l_RXbytes = rxBytes;
    static quint64 l_TXbytes = txBytes;

    if (!mActive) {
        l_RXbytes = rxBytes;
        l_TXbytes = txBytes;
        return;
    }

    static int second = 0;

    QVector<QSplineSeries *> seriesList = mChartNetwork->getSeriesList();

    // points swap
    for (int j = 0; j < seriesList.count(); j++) {
        for (int i = 0; i < (second < 61 ? second : 61); i++)
            seriesList.at(j)->replace(i, (i+1), seriesList.at(j)->at(i).y());

        if(second > 61) seriesList.at(j)->removePoints(61, 1);
    }
    static quint64 max_RXbytes = 1L << 20; // 1 MEBI
    static quint64 max_TXbytes = 1L << 20; // 1 MEBI

    quint64 d_RXbytes = (rxBytes - l_RXbytes);
    quint64 d_TXbytes = (txBytes - l_TXbytes);

    QString downText = FormatUtil::formatBytes(d_RXbytes);
    QString upText   = FormatUtil::formatBytes(d_TXbytes);

    // Download
    seriesList.at(0)->insert(0, QPointF(0, d_RXbytes));
    seriesList.at(0)->setName(tr("Download: %1/s Total: %2")
                              .arg(downText)
                              .arg(FormatUtil::formatBytes(rxBytes)));

    seriesList.at(1)->insert(0, QPointF(0, d_TXbytes));
    seriesList.at(1)->setName(tr("Upload: %1/s  Total: %2")
                              .arg(upText)
                              .arg(FormatUtil::formatBytes(txBytes)));

    max_RXbytes = qMax(max_RXbytes, d_RXbytes);
    max_TXbytes = qMax(max_TXbytes, d_TXbytes);

    int max = qMax(max_RXbytes, max_TXbytes);

    l_RXbytes = rxBytes;
    l_TXbytes = txBytes;

    mChartNetwork->setYMax(max);

    mChartNetwork->setSeriesList(seriesList);

    mChartNetwork->setCategoryAxisYLabels();

    second++;
}

void ResourcesPage::onMemoryUpdated(const MemorySnapshot &snap)
{
    if (!mActive) return;

    static int second = 0;

    QVector<QSplineSeries *> seriesList = mChartMemory->getSeriesList();

    // Shift existing points right for all series
    for (int j = 0; j < seriesList.count(); j++) {
        for (int i = 0; i < (second < 61 ? second : 61); i++)
            seriesList.at(j)->replace(i, (i+1), seriesList.at(j)->at(i).y());

        if(second > 61)
            seriesList.at(j)->removePoints(61, 1);
    }

    // Series 0: Swap
    double swapPct = 0;
    if (snap.swapTotal)
        swapPct = ((double)snap.swapUsed / (double)snap.swapTotal) * 100.0;

    seriesList.at(0)->insert(0, QPointF(0, swapPct));
    seriesList.at(0)->setName(tr("Swap: %1 (%2%) %3")
                              .arg(FormatUtil::formatBytes(snap.swapUsed))
                              .arg(QString::asprintf("%.1f", swapPct))
                              .arg(FormatUtil::formatBytes(snap.swapTotal)));

    // Series 1: Memory Used
    double memPct = snap.total ? ((double)snap.used / (double)snap.total) * 100.0 : 0;

    seriesList.at(1)->insert(0, QPointF(0, memPct));
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
        seriesList.at(2)->insert(0, QPointF(0, pct2));
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
        seriesList.at(3)->insert(0, QPointF(0, pct3));
        seriesList.at(3)->setName(name3);
    }

    mChartMemory->setSeriesList(seriesList);

    second++;
}

void ResourcesPage::onGpuUpdated(const QList<GpuDevice> &gpus)
{
    if (!mChartGpu || !mActive)
        return;

    static int second = 0;

    QVector<QSplineSeries *> seriesList = mChartGpu->getSeriesList();

    for (int j = 0; j < seriesList.count() && j < gpus.size(); j++) {
        const GpuDevice &gpu = gpus.at(j);
        int util = qMax(0, gpu.utilization);  // -1 → 0 for chart data point

        for (int i = 0; i < (second < 61 ? second : 61); i++)
            seriesList.at(j)->replace(i, (i+1), seriesList.at(j)->at(i).y());

        seriesList.at(j)->insert(0, QPointF(0, util));

        QString label = (gpu.utilization < 0)
            ? QString("%1: N/A").arg(gpu.name)
            : QString("%1: %2%").arg(gpu.name).arg(util);
        seriesList.at(j)->setName(label);

        if (second > 61) seriesList.at(j)->removePoints(61, 1);
    }

    mChartGpu->setSeriesList(seriesList);

    second++;
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
    mChartDiskHealth->setYMax(100);

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
    Utilities::addDropShadow(mChartDiskHealth, 40);
}

void ResourcesPage::onDiskHealthUpdated(const QList<DriveHealth> &drives)
{
    // FR-96: create the chart on first data arrival if it wasn't built
    // during construction (discovery was still running then).
    if (!mChartDiskHealth)
        ensureDiskHealthChart(drives);

    if (!mChartDiskHealth || !mActive)
        return;

    static int tick = 0;

    QVector<QSplineSeries *> seriesList = mChartDiskHealth->getSeriesList();

    int seriesIdx = 0;
    for (const DriveHealth &d : drives) {
        if (d.temperatureCelsius < 0)
            continue;
        if (seriesIdx >= seriesList.count())
            break;

        double temp = d.temperatureCelsius;

        for (int i = 0; i < (tick < 61 ? tick : 61); i++)
            seriesList.at(seriesIdx)->replace(i, (i + 1), seriesList.at(seriesIdx)->at(i).y());

        seriesList.at(seriesIdx)->insert(0, QPointF(0, temp));

        QString model = d.model.isEmpty() ? d.deviceName : d.model;
        seriesList.at(seriesIdx)->setName(QString("%1: %2 \u00B0C")
                                          .arg(model)
                                          .arg(temp, 0, 'f', 0));

        if (tick > 61) seriesList.at(seriesIdx)->removePoints(61, 1);

        seriesIdx++;
    }

    mChartDiskHealth->setSeriesList(seriesList);

    tick++;
}

#ifdef Q_OS_LINUX
void ResourcesPage::onPsiUpdated(const PsiSnapshot &snap)
{
    if (!mChartPsiCpu || !mActive || !snap.available)
        return;

    static int second = 0;

    QVector<QSplineSeries*> seriesList = mChartPsiCpu->getSeriesList();

    for (int j = 0; j < seriesList.count(); ++j) {
        for (int i = 0; i < (second < 61 ? second : 61); ++i)
            seriesList.at(j)->replace(i, i + 1, seriesList.at(j)->at(i).y());
        if (second > 61)
            seriesList.at(j)->removePoints(61, 1);
    }

    const double values[3] = { snap.someAvg10, snap.someAvg60, snap.someAvg300 };
    const char  *labels[3] = { "avg10", "avg60", "avg300" };

    for (int i = 0; i < 3; ++i) {
        seriesList.at(i)->insert(0, QPointF(0, values[i]));
        seriesList.at(i)->setName(
            tr("CPU %1: %2%").arg(QLatin1String(labels[i]))
                             .arg(values[i], 0, 'f', 1));
    }

    // Auto-scale Y: expand when data exceeds the current ceiling, floor at 1%.
    static double maxPsi = 1.0;
    double tickMax = qMax({snap.someAvg10, snap.someAvg60, snap.someAvg300});
    if (tickMax * 1.25 > maxPsi) {
        maxPsi = tickMax * 1.25;
        mChartPsiCpu->setYMax(maxPsi);
    }

    mChartPsiCpu->setSeriesList(seriesList);
    second++;
}
#endif

void ResourcesPage::onPageActivated()
{
    mActive = true;

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
#endif

    // Kick an immediate disk health refresh so the temperature chart populates
    // on first open rather than waiting up to 30 s for the slow tick.
    mRefresh->triggerDiskHealthCheck();
}

void ResourcesPage::onPageDeactivated()
{
    mActive = false;

    mRefresh->unsubscribe(DataRefreshService::Signal::Cpu);
    mRefresh->unsubscribe(DataRefreshService::Signal::Memory);
    mRefresh->unsubscribe(DataRefreshService::Signal::Network);
    mRefresh->unsubscribe(DataRefreshService::Signal::DiskIO);
    mRefresh->unsubscribe(DataRefreshService::Signal::Gpu);
#ifdef Q_OS_LINUX
    if (mChartPsiCpu)
        mRefresh->unsubscribe(DataRefreshService::Signal::Psi);
#endif
}
