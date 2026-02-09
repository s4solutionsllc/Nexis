#include "dashboard_page.h"
#include "ui_dashboard_page.h"

#include "utilities.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>

DashboardPage::~DashboardPage()
{
    delete ui;
}

DashboardPage::DashboardPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DashboardPage),
    mCpuBar(new CircleBar(tr("CPU"), {"#2ec27e", "#26a269"}, this)),
    mMemBar(new CircleBar(tr("MEMORY"), {"#E95420", "#c64516"}, this)),
    mDiskBar(new CircleBar(tr("DISK"), {"#e01b24", "#c01c28"}, this)),
    mTempBar(new CircleBar(tr("TEMP"), {"#1c71d8", "#1a5fb4"}, this)),
    mDownloadBar(new LineBar(tr("DOWNLOAD"), this)),
    mUploadBar(new LineBar(tr("UPLOAD"), this)),
    mTimer(new QTimer(this)),
    im(InfoManager::ins()),
    mSettingManager(SettingManager::ins()),
    mSelectedSensorIndex(0),
    mGpuBar(new CircleBar(tr("GPU"), {"#813d9c", "#613583"}, this)),
    mSelectedGpuIndex(0)
{
    ui->setupUi(this);

    init();

    systemInformationInit();
}

void DashboardPage::init()
{
    // Circle bars (row 0)
    ui->circleBarsLayout->addWidget(mCpuBar);
    ui->circleBarsLayout->addWidget(mMemBar);
    ui->circleBarsLayout->addWidget(mDiskBar);

    // Line bars (stacked in col 2)
    ui->lineBarsLayout->addWidget(mDownloadBar);
    ui->lineBarsLayout->addWidget(mUploadBar);

    // Temperature gauge (row 1, col 1) — graceful degradation
    if (im->hasThermalSensors()) {
        QList<ThermalSensor> sensors = im->getThermalSensors();
        for (const ThermalSensor &s : sensors)
            ui->cmbTempSensor->addItem(s.label);

        ui->tempContainerLayout->addWidget(mTempBar);

        connect(ui->cmbTempSensor, &QComboBox::currentIndexChanged,
                this, &DashboardPage::onTempSensorChanged);
        connect(mTimer, &QTimer::timeout, this, &DashboardPage::updateTempBar);
    } else {
        ui->tempContainer->hide();
        mTempBar->hide();       // prevent orphan widget rendering at (0,0)
    }

    // GPU gauge — graceful degradation
    if (im->hasGpu()) {
        QList<GpuDevice> gpus = im->getGpuDevices();
        for (const GpuDevice &g : gpus)
            ui->cmbGpuDevice->addItem(g.name);

        // Hide the combo box if there's only one GPU
        if (gpus.size() <= 1)
            ui->cmbGpuDevice->hide();

        ui->gpuContainerLayout->addWidget(mGpuBar);

        connect(ui->cmbGpuDevice, &QComboBox::currentIndexChanged,
                this, &DashboardPage::onGpuDeviceChanged);
        connect(mTimer, &QTimer::timeout, this, &DashboardPage::updateGpuBar);
    } else {
        ui->gpuContainer->hide();
        mGpuBar->hide();
    }

    // connections
    connect(mTimer, &QTimer::timeout, this, &DashboardPage::updateCpuBar);
    connect(mTimer, &QTimer::timeout, this, &DashboardPage::updateMemoryBar);
    connect(mTimer, &QTimer::timeout, this, &DashboardPage::updateNetworkBar);

    QTimer *timerDisk = new QTimer(this);
    connect(timerDisk, &QTimer::timeout, this, &DashboardPage::updateDiskBar);
    timerDisk->start(5 * 1000);

    mTimer->start(1 * 1000);

    // initialization
    updateCpuBar();
    updateMemoryBar();
    updateDiskBar();
    updateNetworkBar();
    if (im->hasThermalSensors())
        updateTempBar();
    if (im->hasGpu())
        updateGpuBar();

    ui->widgetUpdateBar->hide();

    // check update
    checkUpdate();
    connect(this, &DashboardPage::sigShowUpdateBar, ui->widgetUpdateBar, &QWidget::show);

    QList<QWidget*> widgets = {
        mCpuBar, mMemBar, mDiskBar, mDownloadBar, mUploadBar
    };
    if (im->hasThermalSensors())
        widgets.append(mTempBar);
    if (im->hasGpu())
        widgets.append(mGpuBar);

    Utilities::addDropShadow(widgets, 60);
}

void DashboardPage::checkUpdate()
{
    QNetworkAccessManager * nam = new QNetworkAccessManager(this);
    const QNetworkRequest updateCheckRequest(QUrl("https://api.github.com/repos/lsimpsonsfdc/Stacer/releases/latest"));
    connect(nam,&QNetworkAccessManager::finished,this,[this](QNetworkReply * reply){
        if(reply->error()==QNetworkReply::NoError)
        {
            const QString requestResult= reply->readAll();
            const QJsonDocument result = QJsonDocument::fromJson(requestResult.toUtf8());
            const QRegularExpression ex("([0-9]\\.[0-9]\\.[0-9])");
            QRegularExpressionMatch match = ex.match(result.object().value("tag_name").toString());

            if (match.hasMatch())
            {
                const QString version = match.captured();

                if (qApp->applicationVersion() != version) {
                    emit sigShowUpdateBar();
                }
            }
        }

    });
    nam->get(updateCheckRequest);
}

void DashboardPage::on_btnDownloadUpdate_clicked()
{
    QDesktopServices::openUrl(QUrl("https://github.com/lsimpsonsfdc/Stacer/releases/latest"));
}

void DashboardPage::systemInformationInit()
{
    // get system information
    SystemInfo sysInfo;

    QStringList infos;
    infos
        << tr("Hostname: %1").arg(sysInfo.getHostname())
        << tr("Platform: %1").arg(sysInfo.getPlatform())
        << tr("Distribution: %1").arg(sysInfo.getDistribution())
        << tr("Kernel Release: %1").arg(sysInfo.getKernel())
        << tr("CPU Model: %1").arg(sysInfo.getCpuModel())
        << tr("CPU Core: %1").arg(sysInfo.getCpuCore())
        << tr("CPU Speed: %1").arg(sysInfo.getCpuSpeed());

    QStringListModel *systemInfoModel = new QStringListModel(infos,ui->listViewSystemInfo);
    const auto oldModel = ui->listViewSystemInfo->selectionModel();
    delete  oldModel;
    ui->listViewSystemInfo->setModel(systemInfoModel);
}

void DashboardPage::updateCpuBar()
{
    int cpuUsedPercent = im->getCpuPercents().at(0);
    double cpuCurrentClockGHz = im->getCpuClock()/1000.0;

    // alert message
    int cpuAlerPercent = mSettingManager->getCpuAlertPercent();
    if (cpuAlerPercent > 0) {
        static bool isShow = true;
        if (cpuUsedPercent > cpuAlerPercent && isShow) {
            AppManager::ins()->getTrayIcon()->showMessage(tr("High CPU Usage"),
                                                          tr("The amount of CPU used is over %1%.").arg(cpuAlerPercent),
                                                          QSystemTrayIcon::Warning);
            isShow = false;
        } else if (cpuUsedPercent < cpuAlerPercent) {
            isShow = true;
        }
    }

    QString cpuLabel;
    if (cpuCurrentClockGHz > 0.01)
        cpuLabel = QString("%1 GHz\n%2%").arg(cpuCurrentClockGHz, 0, 'f', 2).arg(cpuUsedPercent);
    else
        cpuLabel = QString("%1%").arg(cpuUsedPercent);

    mCpuBar->setValue(cpuUsedPercent, cpuLabel);
}

void DashboardPage::updateMemoryBar()
{
    im->updateMemoryInfo();

    int memUsedPercent = 0;
    if (im->getMemTotal()) {
        memUsedPercent = ((double)im->getMemUsed() / (double)im->getMemTotal()) * 100.0;
    }

    QString f_memUsed  = FormatUtil::formatBytes(im->getMemUsed());
    QString f_memTotal = FormatUtil::formatBytes(im->getMemTotal());

    // alert message
    int memoryAlertPercent = mSettingManager->getMemoryAlertPercent();
    if (memoryAlertPercent > 0) {
        static bool isShow = true;
        if (memUsedPercent > memoryAlertPercent && isShow) {
            AppManager::ins()->getTrayIcon()->showMessage(tr("High Memory Usage"),
                                                          tr("The amount of memory used is over %1%.").arg(memoryAlertPercent),
                                                          QSystemTrayIcon::Warning);
            isShow = false;
        } else if (memUsedPercent < memoryAlertPercent) {
            isShow = true;
        }
    }

    mMemBar->setValue(memUsedPercent, QString("%1 / %2")
                     .arg(f_memUsed)
                     .arg(f_memTotal));
}

void DashboardPage::updateDiskBar()
{
    im->updateDiskInfo();

    if(! im->getDisks().isEmpty()) {
        Disk *disk = nullptr;
        QString selectedDiskName = mSettingManager->getDiskName();
        for (Disk *d: im->getDisks()) {
            if (d->name.trimmed() == selectedDiskName.trimmed())
                disk = d;
        }

        if (! disk) {
            for (Disk *d: im->getDisks())
                if (d->name.trimmed() == QStorageInfo::root().displayName().trimmed())
                    disk = d;
            if (! disk)
                disk = im->getDisks().at(0);
        }

        int diskPercent = 0;
        if (disk->size > 0) {
            diskPercent = ((double) disk->used / (double) disk->size) * 100.0;
        }

        // alert message
        int diskAlertPercent = mSettingManager->getDiskAlertPercent();
        if (diskAlertPercent > 0) {
            static bool isShow = true;
            if (diskPercent > diskAlertPercent && isShow) {
                AppManager::ins()->getTrayIcon()->showMessage(tr("High Disk Usage"),
                                                              tr("The amount of disk used is over %1%.").arg(diskAlertPercent),
                                                              QSystemTrayIcon::Warning);
                isShow = false;
            } else if (diskPercent < diskAlertPercent) {
                isShow = true;
            }
        }

        QString sizeText = FormatUtil::formatBytes(disk->size);
        QString usedText = FormatUtil::formatBytes(disk->used);

        mDiskBar->setValue(diskPercent, QString("%1 / %2")
                          .arg(usedText)
                          .arg(sizeText));
    }
}

void DashboardPage::updateNetworkBar()
{
    static quint64 l_RXbytes = im->getRXbytes();
    static quint64 l_TXbytes = im->getTXbytes();
    static quint64 max_RXbytes = 1L << 20; // 1 MEBI
    static quint64 max_TXbytes = 1L << 20; // 1 MEBI

    quint64 RXbytes = im->getRXbytes();
    quint64 TXbytes = im->getTXbytes();

    quint64 d_RXbytes = (RXbytes - l_RXbytes);
    quint64 d_TXbytes = (TXbytes - l_TXbytes);

    QString downText = FormatUtil::formatBytes(d_RXbytes);
    QString upText   = FormatUtil::formatBytes(d_TXbytes);

    int downPercent = ((double) d_RXbytes / (double) max_RXbytes) * 100.0;
    int upPercent   = ((double) d_TXbytes / (double) max_TXbytes) * 100.0;

    mDownloadBar->setValue(downPercent,
                          QString("%1/s").arg(downText),
                          tr("Total: %1").arg(FormatUtil::formatBytes(RXbytes)));

    mUploadBar->setValue(upPercent,
                        QString("%1/s").arg(upText),
                        tr("Total: %1").arg(FormatUtil::formatBytes(TXbytes)));

    max_RXbytes = qMax(max_RXbytes, d_RXbytes);
    max_TXbytes = qMax(max_TXbytes, d_TXbytes);

    l_RXbytes = RXbytes;
    l_TXbytes = TXbytes;
}

void DashboardPage::updateTempBar()
{
    double temp = im->getThermalTemperature(mSelectedSensorIndex);
    int percent = qBound(0, static_cast<int>(temp), 100);

    double tempF = temp * 9.0 / 5.0 + 32.0;
    mTempBar->setValue(percent, QString("%1 \u00B0C / %2 \u00B0F")
                       .arg(temp, 0, 'f', 1)
                       .arg(tempF, 0, 'f', 1));
}

void DashboardPage::onTempSensorChanged(int index)
{
    mSelectedSensorIndex = index;
    updateTempBar();
}

void DashboardPage::updateGpuBar()
{
    im->updateGpuInfo();

    QList<GpuDevice> gpus = im->getGpuDevices();
    if (mSelectedGpuIndex < 0 || mSelectedGpuIndex >= gpus.size())
        return;

    const GpuDevice &gpu = gpus.at(mSelectedGpuIndex);
    int util = qMax(0, gpu.utilization);  // -1 → 0 for display

    mGpuBar->setValue(util, QString("%1%").arg(util));
}

void DashboardPage::onGpuDeviceChanged(int index)
{
    mSelectedGpuIndex = index;
    updateGpuBar();
}

