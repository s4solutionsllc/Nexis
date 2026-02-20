#include "resources_page.h"
#include "ui_resources_page.h"
#include "utilities.h"
#include "Managers/data_refresh_service.h"

ResourcesPage::~ResourcesPage()
{
    delete ui;
}

ResourcesPage::ResourcesPage(QWidget *parent, InfoManager *infoManager,
                               DataRefreshService *refreshService) :
    QWidget(parent),
    ui(new Ui::ResourcesPage),
    im(infoManager ? infoManager : InfoManager::ins()),
    mRefresh(refreshService ? refreshService : DataRefreshService::ins()),
    mChartCpu(new HistoryChart(tr("History of CPU"), im->getCpuCoreCount(), nullptr, this)),
    mChartCpuLoadAvg(new HistoryChart(tr("History of CPU Load Averages"), 3, nullptr, this)),
    mChartDiskReadWrite(new HistoryChart(tr("History of Disk Read Write"), 2, new QCategoryAxis, this)),
    mChartMemory(new HistoryChart(tr("History of Memory"), 2, nullptr, this)),
    mChartNetwork(new HistoryChart(tr("History of Network"), 2, new QCategoryAxis, this)),
    mChartGpu(nullptr),
    mChartDiskHealth(nullptr),
    mDiskLauncher(nullptr)
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

    // Disk health temperature chart — only if drives with temperature data exist
    if (im->hasDiskHealth()) {
        QList<DriveHealth> drives = im->getDriveHealth();
        int tempDriveCount = 0;
        for (const DriveHealth &d : drives) {
            if (d.temperatureCelsius >= 0)
                tempDriveCount++;
        }
        if (tempDriveCount > 0) {
            mChartDiskHealth = new HistoryChart(tr("History of Disk Temperature"), tempDriveCount, nullptr, this);
            mChartDiskHealth->setYMax(100);
        }
    }

    QList<QWidget*> widgets = { mChartCpu, mChartCpuLoadAvg, mChartDiskReadWrite, mChartMemory, mChartNetwork };

    // Insert GPU chart after CPU charts
    if (mChartGpu)
        widgets.insert(2, mChartGpu);  // after CPU Load Avg, before Disk R/W

    // Insert disk health chart after Network, before Disk Launcher
    if (mChartDiskHealth)
        widgets.append(mChartDiskHealth);

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

    if (mChartDiskHealth)
        connect(mRefresh, &DataRefreshService::diskHealthUpdated,
                this, &ResourcesPage::onDiskHealthUpdated);

    // Disk Usage Analyzer launcher (FR-23)
    mDiskLauncher = new DiskUsageLauncherWidget(this);
    ui->chartsLayout->addWidget(mDiskLauncher);
    Utilities::addDropShadow(mDiskLauncher, 40);
}

void ResourcesPage::onDiskIOUpdated(const QList<quint64> &io)
{
    static int second = 0;

    QVector<QSplineSeries*> seriesList = mChartDiskReadWrite->getSeriesList();

    for (int j = 0; j < seriesList.count(); ++j) {
        for (int i = 0; i < (second < 61 ? second : 61); ++i) {
            seriesList.at(j)->replace(i, (i+1), seriesList.at(j)->at(i).y());
        }

        if(second > 61) seriesList.at(j)->removePoints(61, 1);
    }

    static quint64 l_readBytes  = io.at(0); // last
    static quint64 l_writeBytes = io.at(1); // last
    static quint64 maxY = (1L << 10) * 100; // 100 KIBI

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
    static int second = 0;

    QVector<QSplineSeries *> seriesList = mChartNetwork->getSeriesList();

    // points swap
    for (int j = 0; j < seriesList.count(); j++) {
        for (int i = 0; i < (second < 61 ? second : 61); i++)
            seriesList.at(j)->replace(i, (i+1), seriesList.at(j)->at(i).y());

        if(second > 61) seriesList.at(j)->removePoints(61, 1);
    }

    static quint64 l_RXbytes = rxBytes;
    static quint64 l_TXbytes = txBytes;
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

void ResourcesPage::onMemoryUpdated(quint64 used, quint64 total,
                                      quint64 swapUsed, quint64 swapTotal)
{
    static int second = 0;

    QVector<QSplineSeries *> seriesList = mChartMemory->getSeriesList();

    // points swap
    for (int j = 0; j < seriesList.count(); j++) {
        for (int i = 0; i < (second < 61 ? second : 61); i++)
            seriesList.at(j)->replace(i, (i+1), seriesList.at(j)->at(i).y());

        if(second > 61)
            seriesList.at(j)->removePoints(61, 1);
    }

    // Swap
    double percent = 0;
    if (swapTotal) // arithmetic exception control
        percent = ((double) swapUsed / (double) swapTotal) * 100.0;

    seriesList.at(0)->insert(0, QPointF(0, percent));
    seriesList.at(0)->setName(tr("Swap: %1 (%2%) %3")
                              .arg(FormatUtil::formatBytes(swapUsed))
                              .arg(QString::asprintf("%.1f",percent))
                              .arg(FormatUtil::formatBytes(swapTotal)));

    // Memory
    double percent2 = ((double) used / (double) total) * 100.0;

    seriesList.at(1)->insert(0, QPointF(0, percent2));
    seriesList.at(1)->setName(tr("Memory: %1 (%2%) %3")
                              .arg(FormatUtil::formatBytes(used))
                              .arg(QString::asprintf("%.1f",percent2))
                              .arg(FormatUtil::formatBytes(total)));

    mChartMemory->setSeriesList(seriesList);

    second++;
}

void ResourcesPage::onGpuUpdated(const QList<GpuDevice> &gpus)
{
    if (!mChartGpu)
        return;

    static int second = 0;

    QVector<QSplineSeries *> seriesList = mChartGpu->getSeriesList();

    for (int j = 0; j < seriesList.count() && j < gpus.size(); j++) {
        int util = qMax(0, gpus.at(j).utilization);  // -1 → 0

        for (int i = 0; i < (second < 61 ? second : 61); i++)
            seriesList.at(j)->replace(i, (i+1), seriesList.at(j)->at(i).y());

        seriesList.at(j)->insert(0, QPointF(0, util));

        seriesList.at(j)->setName(QString("%1: %2%")
                                  .arg(gpus.at(j).name)
                                  .arg(util));

        if (second > 61) seriesList.at(j)->removePoints(61, 1);
    }

    mChartGpu->setSeriesList(seriesList);

    second++;
}

void ResourcesPage::onDiskHealthUpdated(const QList<DriveHealth> &drives)
{
    if (!mChartDiskHealth)
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
