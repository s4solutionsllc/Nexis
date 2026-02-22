#include "dashboard_page.h"
#include "ui_dashboard_page.h"

#include "utilities.h"
#include "Managers/app_manager.h"
#include "Managers/data_refresh_service.h"
#include "signal_mapper.h"

#ifdef Q_OS_MACOS
#include <Info/system_info_macos.h>
#else
#include <Info/system_info_linux.h>
#endif

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QVersionNumber>

DashboardPage::~DashboardPage()
{
    delete ui;
}

DashboardPage::DashboardPage(QWidget *parent, InfoManager *infoManager,
                             SettingManager *settingManager, AppManager *appManager,
                             SignalMapper *signalMapper, DataRefreshService *refreshService) :
    QWidget(parent),
    ui(new Ui::DashboardPage),
    mCpuTile(new MetricTile(tr("CPU"), "@cpuColor", this)),
    mMemTile(new MetricTile(tr("MEMORY"), "@memoryColor", this)),
    mDiskTile(new DiskTile("@diskColor", "@color02", this)),
    mTempTile(new MetricTile(tr("TEMP"), "@tempColor", this)),
    mGpuTile(new MetricTile(tr("GPU"), "@gpuColor", this)),
    mBatteryTile(new MetricTile(tr("BATTERY"), "@batteryColor", this)),
    mDiskHealthTile(new MetricTile(tr("DISK HEALTH"), "@diskHealthColor", this)),
    mNetworkTile(new NetworkTile("@networkColor", this)),
    mCmbGpuDevice(new QComboBox(this)),
    im(infoManager ? infoManager : InfoManager::ins()),
    mSettingManager(settingManager ? settingManager : SettingManager::ins()),
    mAppManager(appManager ? appManager : AppManager::ins()),
    mSignalMapper(signalMapper ? signalMapper : SignalMapper::ins()),
    mRefresh(refreshService ? refreshService : DataRefreshService::ins()),
    mDiskMenu(new QMenu(this)),
    mTempSensorMenu(new QMenu(this)),
    mSelectedSensorIndex(0),
    mSelectedGpuIndex(0),
    mKioskButton(new QPushButton(this))
{
    ui->setupUi(this);

    init();
}

void DashboardPage::init()
{
    // Bento grid layout (mockup-aligned):
    //  Row 0: HeroCard(CPU+Memory) (colspan 2) | Disk | Network
    //  Row 1: GPU* | Temp* | Battery* | DiskHealth*
    // * = conditional tiles

    int row = 0;
    int col = 0;

    // Row 0: Hero card (CPU + Memory combined) + Disk + Network
    mHeroCard = new HeroCard(mCpuTile, mMemTile, this);
    ui->bentoGrid->addWidget(mHeroCard, 0, 0, 1, 2);
    ui->bentoGrid->addWidget(mDiskTile, 0, 2);
    ui->bentoGrid->addWidget(mNetworkTile, 0, 3);

    // Set Hero display mode for CPU and Memory tiles
    mCpuTile->setDisplayMode(MetricTile::Hero);
    mMemTile->setDisplayMode(MetricTile::Hero);

    // Row 1: remaining tiles placed dynamically based on available hardware
    row = 1;
    col = 0;

    if (im->hasGpu()) {
        mGpuTile->setDisplayMode(MetricTile::Large);
        ui->bentoGrid->addWidget(mGpuTile, row, col++);
    } else {
        mGpuTile->hide();
    }

    if (im->hasThermalSensors()) {
        mTempTile->setDisplayMode(MetricTile::Large);
        ui->bentoGrid->addWidget(mTempTile, row, col++);
    } else {
        mTempTile->hide();
    }

    if (im->hasBattery()) {
        mBatteryTile->setDisplayMode(MetricTile::Large);
        ui->bentoGrid->addWidget(mBatteryTile, row, col++);
    } else {
        mBatteryTile->hide();
    }

    if (im->hasDiskHealth()) {
        mDiskHealthTile->setDisplayMode(MetricTile::Large);
        ui->bentoGrid->addWidget(mDiskHealthTile, row, col++);
    } else {
        mDiskHealthTile->hide();
    }

    // If only a few tiles in row 1, let them stretch
    for (int c = 0; c < 4; ++c)
        ui->bentoGrid->setColumnStretch(c, 1);

    // Temperature sensor gear menu
    if (im->hasThermalSensors()) {
        QList<ThermalSensor> sensors = im->getThermalSensors();

        mTempSensorMenu->setObjectName("tempSensorMenu");
        for (int i = 0; i < sensors.size(); ++i) {
            QAction *action = mTempSensorMenu->addAction(sensors.at(i).label);
            action->setData(i);
            action->setCheckable(true);
        }

        QString savedSensorId = mSettingManager->getTempSensorId();
        if (!savedSensorId.isEmpty()) {
            for (int i = 0; i < sensors.size(); ++i) {
                if (sensors.at(i).id == savedSensorId) {
                    mSelectedSensorIndex = i;
                    break;
                }
            }
        }

        for (QAction *a : mTempSensorMenu->actions())
            a->setChecked(a->data().toInt() == mSelectedSensorIndex);

        mTempTile->gearButton()->setMenu(mTempSensorMenu);
        mTempTile->gearButton()->setPopupMode(QToolButton::InstantPopup);
        mTempTile->setGearVisible(sensors.size() >= 2);

        connect(mTempSensorMenu, &QMenu::triggered,
                this, &DashboardPage::onTempSensorSelected);
        connect(mRefresh, &DataRefreshService::tempUpdated,
                this, &DashboardPage::updateTempTile);
    }

    // GPU device combo box
    if (im->hasGpu()) {
        QList<GpuDevice> gpus = im->getGpuDevices();
        for (const GpuDevice &g : gpus)
            mCmbGpuDevice->addItem(g.name);

        QString savedGpuId = mSettingManager->getGpuDeviceId();
        if (!savedGpuId.isEmpty()) {
            for (int i = 0; i < gpus.size(); ++i) {
                if (gpus.at(i).name == savedGpuId) {
                    mCmbGpuDevice->setCurrentIndex(i);
                    mSelectedGpuIndex = i;
                    break;
                }
            }
        }

        if (gpus.size() <= 1)
            mCmbGpuDevice->hide();
        else {
            mCmbGpuDevice->setObjectName("cmbGpuDevice");
            mCmbGpuDevice->setCursor(Qt::PointingHandCursor);
            mCmbGpuDevice->setFocusPolicy(Qt::NoFocus);
            mCmbGpuDevice->setMaximumWidth(140);
            mGpuTile->layout()->addWidget(mCmbGpuDevice);
        }

        connect(mCmbGpuDevice, &QComboBox::currentIndexChanged,
                this, &DashboardPage::onGpuDeviceChanged);
        connect(mRefresh, &DataRefreshService::gpuUpdated,
                this, &DashboardPage::onGpuUpdated);
    } else {
        mCmbGpuDevice->hide();
    }

    // Battery gauge
    if (im->hasBattery()) {
        connect(mRefresh, &DataRefreshService::batteryUpdated,
                this, &DashboardPage::onBatteryUpdated);
    }

    // Disk health gauge
    if (im->hasDiskHealth()) {
        connect(mRefresh, &DataRefreshService::diskHealthUpdated,
                this, &DashboardPage::onDiskHealthUpdated);
    }

    // Set CPU model + core info as subtitle
    {
#ifdef Q_OS_MACOS
        SystemInfoMacOS sysInfo;
#else
        SystemInfoLinux sysInfo;
#endif
        QString cpuModel = sysInfo.getCpuModel();
        int coreCount = im->getCpuCoreCount();
        if (!cpuModel.isEmpty()) {
            QString cpuSubtitle = cpuModel;
            if (coreCount > 0)
                cpuSubtitle += QString(" \u2022 %1C").arg(coreCount);
            mCpuTile->setSubtitle(cpuSubtitle);
        }
    }

    // Core data signals
    connect(mRefresh, &DataRefreshService::cpuUpdated,
            this, &DashboardPage::onCpuUpdated);
    connect(mRefresh, &DataRefreshService::memoryUpdated,
            this, &DashboardPage::onMemoryUpdated);
    connect(mRefresh, &DataRefreshService::networkUpdated,
            this, &DashboardPage::onNetworkUpdated);
    connect(mRefresh, &DataRefreshService::diskUsageUpdated,
            this, &DashboardPage::onDiskUsageUpdated);

    // Disk selector gear menu
    mDiskMenu->setObjectName("diskSelectorMenu");
    mDiskTile->gearButton()->setMenu(mDiskMenu);
    mDiskTile->gearButton()->setPopupMode(QToolButton::InstantPopup);
    connect(mDiskMenu, &QMenu::triggered, this, &DashboardPage::onDiskSelected);

    // Set network interface name
    QString ifName = im->getDefaultNetworkInterface();
    if (!ifName.isEmpty())
        mNetworkTile->setInterfaceName(ifName);

    // Update bar
    ui->widgetUpdateBar->hide();
    checkUpdate();
    connect(this, &DashboardPage::sigShowUpdateBar, ui->widgetUpdateBar, &QWidget::show);

    // Drop shadows on tiles
    QList<QWidget*> widgets = {
        mHeroCard, mDiskTile, mNetworkTile
    };
    if (im->hasThermalSensors())
        widgets.append(mTempTile);
    if (im->hasGpu())
        widgets.append(mGpuTile);
    if (im->hasBattery())
        widgets.append(mBatteryTile);
    if (im->hasDiskHealth())
        widgets.append(mDiskHealthTile);

    Utilities::addDropShadow(widgets, 80);

    // System summary card
    buildSystemSummary();

    // Status footer
    ui->lblFooterRight->setText(
        QString("Nexis v%1 \u2022 Refreshing every 1s")
            .arg(qApp->applicationVersion()));

    // Kiosk mode toggle button (floating, top-right)
    mKioskButton->setFixedSize(32, 32);
    mKioskButton->setIcon(QIcon(":/static/themes/common/img/fullscreen.svg"));
    mKioskButton->setIconSize(QSize(16, 16));
    mKioskButton->setToolTip(tr("Enter Kiosk Mode (F11)"));
    mKioskButton->setCursor(Qt::PointingHandCursor);
    mKioskButton->setFocusPolicy(Qt::NoFocus);
    mKioskButton->setObjectName("btnKioskToggle");
    mKioskButton->raise();

    connect(mKioskButton, &QPushButton::clicked, this, [this]() {
        emit mSignalMapper->sigKioskToggleRequested();
    });
    connect(mSignalMapper, &SignalMapper::sigKioskModeChanged,
            this, &DashboardPage::onKioskModeChanged);
}

void DashboardPage::buildSystemSummary()
{
#ifdef Q_OS_MACOS
    SystemInfoMacOS sysInfo;
#else
    SystemInfoLinux sysInfo;
#endif

    ui->systemSummary->setObjectName("systemSummaryCard");

    while (ui->summaryLayout->count() > 0) {
        QLayoutItem *item = ui->summaryLayout->takeAt(0);
        delete item;
    }

    auto *lblTitle = new QLabel(tr("SYSTEM"), ui->systemSummary);
    lblTitle->setObjectName("summaryLabel");
    ui->summaryLayout->addWidget(lblTitle);

    auto *summaryWidget = new QWidget(ui->systemSummary);
    auto *vbox = new QVBoxLayout(summaryWidget);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(2);

    mSummaryHostname = sysInfo.getHostname();
    mSummaryOs = sysInfo.getDistribution();
    mSummaryCpu = sysInfo.getCpuModel();
    mSummaryRam = FormatUtil::formatBytes(im->getMemTotal()) + " RAM";

    auto *lblLine1 = new QLabel(ui->systemSummary);
    lblLine1->setObjectName("summaryValue");
    mSummaryLabels.append(lblLine1);
    vbox->addWidget(lblLine1);

    ui->summaryLayout->addWidget(summaryWidget);
    ui->summaryLayout->addStretch();

    refreshSummaryColors();

    connect(mSignalMapper, &SignalMapper::sigChangedAppTheme, this, &DashboardPage::refreshSummaryColors);
}

void DashboardPage::refreshSummaryColors()
{
    QSettings *sv = mAppManager->getStyleValues();
    if (!sv || mSummaryLabels.isEmpty())
        return;

    QString tertiaryText = sv->value("@tertiaryText").toString();
    mSummaryLabels.first()->setText(
        QString("<b>%1</b> <span style='color: %5;'>\u2022 %2 \u2022 %3 \u2022 %4</span>")
            .arg(mSummaryHostname, mSummaryOs, mSummaryCpu, mSummaryRam, tertiaryText));
}

void DashboardPage::checkUpdate()
{
    QNetworkAccessManager * nam = new QNetworkAccessManager(this);
    const QNetworkRequest updateCheckRequest(QUrl("https://api.github.com/repos/lsimpsonsfdc/Nexis/releases/latest"));
    connect(nam,&QNetworkAccessManager::finished,this,[this](QNetworkReply * reply){
        if(reply->error()==QNetworkReply::NoError)
        {
            const QString requestResult= reply->readAll();
            const QJsonDocument result = QJsonDocument::fromJson(requestResult.toUtf8());
            const QRegularExpression ex("([0-9]\\.[0-9]\\.[0-9])");
            QRegularExpressionMatch match = ex.match(result.object().value("tag_name").toString());

            if (match.hasMatch())
            {
                const QVersionNumber remote = QVersionNumber::fromString(match.captured());
                const QVersionNumber local  = QVersionNumber::fromString(qApp->applicationVersion());

                if (!remote.isNull() && !local.isNull() && local < remote) {
                    emit sigShowUpdateBar();
                }
            }
        }

    });
    nam->get(updateCheckRequest);
}

void DashboardPage::on_btnDownloadUpdate_clicked()
{
    QDesktopServices::openUrl(QUrl("https://github.com/lsimpsonsfdc/Nexis/releases/latest"));
}

void DashboardPage::onCpuUpdated(const QList<int> &percents, double clockGHz,
                                  const QList<double> &loadAvgs)
{
    Q_UNUSED(loadAvgs)

    int cpuUsedPercent = percents.at(0);

    // alert message
    int cpuAlerPercent = mSettingManager->getCpuAlertPercent();
    if (cpuAlerPercent > 0) {
        static bool isShow = true;
        if (cpuUsedPercent > cpuAlerPercent && isShow) {
            mAppManager->getTrayIcon()->showMessage(tr("High CPU Usage"),
                                                          tr("The amount of CPU used is over %1%.").arg(cpuAlerPercent),
                                                          QSystemTrayIcon::Warning);
            isShow = false;
        } else if (cpuUsedPercent < cpuAlerPercent) {
            isShow = true;
        }
    }

    QString valueText = QString("%1%").arg(cpuUsedPercent);

    mCpuTile->setValue(cpuUsedPercent, valueText);
    mCpuTile->addDataPoint(cpuUsedPercent);

    if (clockGHz > 0.00001)
        mCpuTile->setSecondaryValue(QString("%1 GHz").arg(clockGHz, 0, 'f', 2));
}

void DashboardPage::onMemoryUpdated(quint64 used, quint64 total,
                                     quint64 swapUsed, quint64 swapTotal)
{
    int memUsedPercent = 0;
    if (total) {
        memUsedPercent = ((double)used / (double)total) * 100.0;
    }

    QString f_memUsed  = FormatUtil::formatBytes(used);
    QString f_memTotal = FormatUtil::formatBytes(total);

    // alert message
    int memoryAlertPercent = mSettingManager->getMemoryAlertPercent();
    if (memoryAlertPercent > 0) {
        static bool isShow = true;
        if (memUsedPercent > memoryAlertPercent && isShow) {
            mAppManager->getTrayIcon()->showMessage(tr("High Memory Usage"),
                                                          tr("The amount of memory used is over %1%.").arg(memoryAlertPercent),
                                                          QSystemTrayIcon::Warning);
            isShow = false;
        } else if (memUsedPercent < memoryAlertPercent) {
            isShow = true;
        }
    }

    mMemTile->setValue(memUsedPercent, QString("%1%").arg(memUsedPercent));
    mMemTile->addDataPoint(memUsedPercent);
    mMemTile->setSecondaryValue(QString("%1 / %2").arg(f_memUsed, f_memTotal));

    QString swapSubtitle = QString("Swap: %1 / %2")
        .arg(FormatUtil::formatBytes(swapUsed), FormatUtil::formatBytes(swapTotal));
    mMemTile->setSubtitle(swapSubtitle);
}

void DashboardPage::onDiskUsageUpdated(const QList<Disk> &disks)
{
    if (disks.isEmpty())
        return;

    mCachedDisks = disks;

    // Rebuild gear menu with current disk list
    mDiskMenu->clear();
    for (const Disk &d : disks) {
        QAction *action = mDiskMenu->addAction(d.name);
        action->setData(d.name);
        action->setCheckable(true);
    }
    mDiskTile->setGearVisible(disks.size() >= 2);

    const Disk *disk = nullptr;
    QString selectedDiskName = mSettingManager->getDiskName();
    for (const Disk &d : disks) {
        if (d.name.trimmed() == selectedDiskName.trimmed())
            disk = &d;
    }

    if (!disk) {
        for (const Disk &d : disks)
            if (d.name.trimmed() == QStorageInfo::root().displayName().trimmed())
                disk = &d;
        if (!disk)
            disk = &disks.at(0);
    }

    // Mark the selected disk in the gear menu
    for (QAction *a : mDiskMenu->actions())
        a->setChecked(a->data().toString() == disk->name);

    int diskPercent = 0;
    if (disk->size > 0) {
        diskPercent = ((double) disk->used / (double) disk->size) * 100.0;
    }

    // alert message
    int diskAlertPercent = mSettingManager->getDiskAlertPercent();
    if (diskAlertPercent > 0) {
        static bool isShow = true;
        if (diskPercent > diskAlertPercent && isShow) {
            mAppManager->getTrayIcon()->showMessage(tr("High Disk Usage"),
                                                          tr("The amount of disk used is over %1%.").arg(diskAlertPercent),
                                                          QSystemTrayIcon::Warning);
            isShow = false;
        } else if (diskPercent < diskAlertPercent) {
            isShow = true;
        }
    }

    QString sizeText = FormatUtil::formatBytes(disk->size);
    QString usedText = FormatUtil::formatBytes(disk->used);

    mDiskTile->setValue(diskPercent, usedText, sizeText);
}

void DashboardPage::onDiskSelected(QAction *action)
{
    QString diskName = action->data().toString();
    mSettingManager->setDiskName(diskName);

    for (const Disk &d : mCachedDisks) {
        if (d.name == diskName) {
            int percent = 0;
            if (d.size > 0)
                percent = static_cast<int>((double)d.used / (double)d.size * 100.0);
            mDiskTile->setValue(percent,
                               FormatUtil::formatBytes(d.used),
                               FormatUtil::formatBytes(d.size));

            for (QAction *a : mDiskMenu->actions())
                a->setChecked(a->data().toString() == diskName);
            break;
        }
    }
}

void DashboardPage::onNetworkUpdated(quint64 rxBytes, quint64 txBytes)
{
    static quint64 l_RXbytes = rxBytes;
    static quint64 l_TXbytes = txBytes;

    quint64 d_RXbytes = (rxBytes - l_RXbytes);
    quint64 d_TXbytes = (txBytes - l_TXbytes);

    mNetworkTile->setValues(d_RXbytes, d_TXbytes, rxBytes, txBytes);

    l_RXbytes = rxBytes;
    l_TXbytes = txBytes;
}

void DashboardPage::updateTempTile()
{
    double temp = im->getThermalTemperature(mSelectedSensorIndex);
    int percent = qBound(0, static_cast<int>(temp), 100);

    double tempF = temp * 9.0 / 5.0 + 32.0;
    mTempTile->setValue(percent, QString("%1\u00B0C").arg(temp, 0, 'f', 1));
    mTempTile->addDataPoint(temp);
}

void DashboardPage::onTempSensorSelected(QAction *action)
{
    int index = action->data().toInt();
    mSelectedSensorIndex = index;

    QList<ThermalSensor> sensors = im->getThermalSensors();
    if (index >= 0 && index < sensors.size())
        mSettingManager->setTempSensorId(sensors.at(index).id);

    for (QAction *a : mTempSensorMenu->actions())
        a->setChecked(a->data().toInt() == index);

    updateTempTile();
}

void DashboardPage::onGpuUpdated(const QList<GpuDevice> &gpus)
{
    if (mSelectedGpuIndex < 0 || mSelectedGpuIndex >= gpus.size())
        return;

    const GpuDevice &gpu = gpus.at(mSelectedGpuIndex);
    int util = qMax(0, gpu.utilization);

    mGpuTile->setValue(util, QString("%1%").arg(util));
    mGpuTile->addDataPoint(util);
}

void DashboardPage::onGpuDeviceChanged(int index)
{
    mSelectedGpuIndex = index;

    QList<GpuDevice> gpus = im->getGpuDevices();
    if (index >= 0 && index < gpus.size())
        mSettingManager->setGpuDeviceId(gpus.at(index).name);

    onGpuUpdated(gpus);
}

void DashboardPage::onBatteryUpdated(const BatteryData &bat)
{
    if (!bat.hasBattery)
        return;

    int displayValue = (bat.healthPercent >= 0) ? bat.healthPercent : bat.chargePercent;
    displayValue = qBound(0, displayValue, 100);

    QString label;
    if (bat.healthPercent >= 0)
        label = QString("%1%").arg(bat.healthPercent);
    else
        label = QString("%1%").arg(bat.chargePercent);

    QString subtitle;
    if (bat.cycleCount >= 0)
        subtitle = QString("%1 %2").arg(bat.cycleCount).arg(tr("cycles"));

    mBatteryTile->setValue(displayValue, label);
    if (!subtitle.isEmpty())
        mBatteryTile->setSubtitle(subtitle);

    // Battery health alert (inverted: warn when BELOW threshold)
    int alertPercent = mSettingManager->getBatteryAlertPercent();
    if (alertPercent > 0 && bat.healthPercent >= 0) {
        int lastHealth = mSettingManager->getBatteryAlertLastHealth();
        QString snoozedUntilStr = mSettingManager->getBatteryAlertSnoozedUntil();
        bool snoozed = false;
        if (!snoozedUntilStr.isEmpty()) {
            QDateTime snoozedUntil = QDateTime::fromString(snoozedUntilStr, Qt::ISODate);
            snoozed = snoozedUntil.isValid() && QDateTime::currentDateTime() < snoozedUntil;
        }

        bool shouldFire = bat.healthPercent < alertPercent &&
                          !snoozed &&
                          (lastHealth == 0 || bat.healthPercent <= lastHealth - 5);

        if (shouldFire) {
            QString msg = tr("Battery health is %1% (%2).").arg(bat.healthPercent).arg(bat.condition);
            if (bat.cycleCount >= 0)
                msg += QString(" %1 %2.").arg(bat.cycleCount).arg(tr("cycles used"));

            mAppManager->getTrayIcon()->showMessage(
                tr("Battery Health Warning"),
                msg,
                QSystemTrayIcon::Warning);
            mSettingManager->setBatteryAlertLastHealth(bat.healthPercent);
        }
    }
}

void DashboardPage::onDiskHealthUpdated(const QList<DriveHealth> &drives)
{
    if (drives.isEmpty())
        return;

    int worstHealth = 100;
    QString worstVerdict = "Good";
    bool hasAnyHealth = false;

    for (const DriveHealth &d : drives) {
        if (d.healthPercent >= 0) {
            hasAnyHealth = true;
            if (d.healthPercent < worstHealth) {
                worstHealth = d.healthPercent;
                worstVerdict = d.healthVerdict;
            }
        } else if (!d.smartPassed) {
            hasAnyHealth = true;
            worstHealth = 0;
            worstVerdict = "Critical";
        }
    }

    if (!hasAnyHealth) {
        bool allPassed = true;
        for (const DriveHealth &d : drives) {
            if (!d.smartPassed) {
                allPassed = false;
                break;
            }
        }
        worstHealth = allPassed ? 100 : 0;
        worstVerdict = allPassed ? "Good" : "Critical";
    }

    int displayPercent = qBound(0, worstHealth, 100);

    mDiskHealthTile->setValue(displayPercent, QString("%1%").arg(displayPercent));
    mDiskHealthTile->setSubtitle(worstVerdict);

    // Populate drive health on disk tile (first time only)
    static bool diskHealthPopulated = false;
    if (!diskHealthPopulated) {
        for (const DriveHealth &d : drives) {
            QString name = d.model.isEmpty() ? d.deviceName : d.model;
            bool good = (d.healthVerdict == "Good" || d.smartPassed);
            mDiskTile->setDriveHealth(name, d.healthVerdict, d.healthPercent, good);
        }
        diskHealthPopulated = true;
    }

    // Disk health alert
    if (mSettingManager->getDiskHealthAlertEnabled()) {
        bool anyBad = false;
        QString badDrive;
        QString badVerdict;
        for (const DriveHealth &d : drives) {
            if (d.healthVerdict == "Caution" || d.healthVerdict == "Critical") {
                anyBad = true;
                badDrive = d.model.isEmpty() ? d.deviceName : d.model;
                badVerdict = d.healthVerdict;
                break;
            }
        }

        static bool alertShown = false;
        if (anyBad && !alertShown) {
            mAppManager->getTrayIcon()->showMessage(
                tr("Disk Health Warning"),
                tr("%1 status: %2").arg(badDrive, badVerdict),
                QSystemTrayIcon::Warning);
            alertShown = true;
        } else if (!anyBad) {
            alertShown = false;
        }
    }
}

void DashboardPage::onKioskModeChanged(bool enabled)
{
    if (enabled) {
        mKioskButton->setIcon(QIcon(":/static/themes/common/img/fullscreen-exit.svg"));
        mKioskButton->setToolTip(tr("Exit Kiosk Mode (ESC)"));
    } else {
        mKioskButton->setIcon(QIcon(":/static/themes/common/img/fullscreen.svg"));
        mKioskButton->setToolTip(tr("Enter Kiosk Mode (F11)"));
    }
}

void DashboardPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    mKioskButton->move(width() - mKioskButton->width() - 10, 10);
}
