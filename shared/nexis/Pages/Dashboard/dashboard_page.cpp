#include "dashboard_page.h"
#include "ui_dashboard_page.h"

#include "utilities.h"
#include "Managers/app_manager.h"
#include "signal_mapper.h"

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
                             SignalMapper *signalMapper) :
    QWidget(parent),
    ui(new Ui::DashboardPage),
    mCpuBar(new CircleBar(tr("CPU"), {"#2ec27e", "#26a269"}, this)),
    mMemBar(new CircleBar(tr("MEMORY"), {"#E95420", "#c64516"}, this)),
    mDiskBar(new CircleBar(tr("DISK"), {"#e01b24", "#c01c28"}, this)),
    mTempBar(new CircleBar(tr("TEMP"), {"#1c71d8", "#1a5fb4"}, this)),
    mDownloadBar(new LineBar(tr("DOWNLOAD"), this)),
    mUploadBar(new LineBar(tr("UPLOAD"), this)),
    mTimer(new QTimer(this)),
    im(infoManager ? infoManager : InfoManager::ins()),
    mSettingManager(settingManager ? settingManager : SettingManager::ins()),
    mAppManager(appManager ? appManager : AppManager::ins()),
    mSignalMapper(signalMapper ? signalMapper : SignalMapper::ins()),
    mSelectedSensorIndex(0),
    mGpuBar(new CircleBar(tr("GPU"), {"#813d9c", "#613583"}, this)),
    mSelectedGpuIndex(0),
    mBatteryBar(new CircleBar(tr("BATTERY"), {"#f5c211", "#e5a50a"}, this)),
    mDiskHealthBar(new CircleBar(tr("DISK HEALTH"), {"#26a69a", "#00897b"}, this)),
    mKioskButton(new QPushButton(this))
{
    ui->setupUi(this);

    init();
}

void DashboardPage::init()
{
    // Circle bars (row 0)
    ui->circleBarsLayout->addWidget(mCpuBar);
    ui->circleBarsLayout->addWidget(mMemBar);
    ui->circleBarsLayout->addWidget(mDiskBar);

    // Disk health gauge (row 0) — graceful degradation
    if (im->hasDiskHealth()) {
        ui->circleBarsLayout->addWidget(mDiskHealthBar);
    } else {
        mDiskHealthBar->hide();
    }

    // Line bars (stacked in col 2)
    ui->lineBarsLayout->addWidget(mDownloadBar);
    ui->lineBarsLayout->addWidget(mUploadBar);

    // Temperature gauge (row 1, col 1) — graceful degradation
    if (im->hasThermalSensors()) {
        QList<ThermalSensor> sensors = im->getThermalSensors();
        for (const ThermalSensor &s : sensors)
            ui->cmbTempSensor->addItem(s.label);

        // Restore saved sensor selection
        QString savedSensorId = mSettingManager->getTempSensorId();
        if (!savedSensorId.isEmpty()) {
            for (int i = 0; i < sensors.size(); ++i) {
                if (sensors.at(i).id == savedSensorId) {
                    ui->cmbTempSensor->setCurrentIndex(i);
                    mSelectedSensorIndex = i;
                    break;
                }
            }
        }

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

        // Restore saved GPU selection
        QString savedGpuId = mSettingManager->getGpuDeviceId();
        if (!savedGpuId.isEmpty()) {
            for (int i = 0; i < gpus.size(); ++i) {
                if (gpus.at(i).name == savedGpuId) {
                    ui->cmbGpuDevice->setCurrentIndex(i);
                    mSelectedGpuIndex = i;
                    break;
                }
            }
        }

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

    // Battery gauge — graceful degradation
    if (im->hasBattery()) {
        ui->batteryContainerLayout->addWidget(mBatteryBar);
        connect(mTimer, &QTimer::timeout, this, &DashboardPage::updateBatteryBar);
    } else {
        ui->batteryContainer->hide();
        mBatteryBar->hide();
    }

    // Disk health gauge — 30s refresh (subprocess calls are expensive)
    if (im->hasDiskHealth()) {
        QTimer *timerDiskHealth = new QTimer(this);
        connect(timerDiskHealth, &QTimer::timeout, this, [this]() {
            im->refreshDiskHealth();
            updateDiskHealthBar();
        });
        timerDiskHealth->start(30 * 1000);
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
    if (im->hasBattery())
        updateBatteryBar();
    if (im->hasDiskHealth())
        updateDiskHealthBar();

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
    if (im->hasBattery())
        widgets.append(mBatteryBar);
    if (im->hasDiskHealth())
        widgets.append(mDiskHealthBar);

    Utilities::addDropShadow(widgets, 60);

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

                // Only show the update bar when the remote release is newer
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


void DashboardPage::updateCpuBar()
{
    int cpuUsedPercent = im->getCpuPercents().at(0);
    double cpuCurrentClockGHz = im->getCpuClock()/1000.0;

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
            mAppManager->getTrayIcon()->showMessage(tr("High Memory Usage"),
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

    const QList<Disk> allDisks = im->getDisks();
    if (!allDisks.isEmpty()) {
        const Disk *disk = nullptr;
        QString selectedDiskName = mSettingManager->getDiskName();
        for (const Disk &d : allDisks) {
            if (d.name.trimmed() == selectedDiskName.trimmed())
                disk = &d;
        }

        if (!disk) {
            for (const Disk &d : allDisks)
                if (d.name.trimmed() == QStorageInfo::root().displayName().trimmed())
                    disk = &d;
            if (!disk)
                disk = &allDisks.at(0);
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

    QList<ThermalSensor> sensors = im->getThermalSensors();
    if (index >= 0 && index < sensors.size())
        mSettingManager->setTempSensorId(sensors.at(index).id);

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

    QList<GpuDevice> gpus = im->getGpuDevices();
    if (index >= 0 && index < gpus.size())
        mSettingManager->setGpuDeviceId(gpus.at(index).name);

    updateGpuBar();
}

void DashboardPage::updateBatteryBar()
{
    im->updateBatteryInfo();
    BatteryData bat = im->getBatteryData();

    if (!bat.hasBattery)
        return;

    int displayValue = (bat.healthPercent >= 0) ? bat.healthPercent : bat.chargePercent;
    displayValue = qBound(0, displayValue, 100);

    QString label;
    if (bat.healthPercent >= 0 && bat.cycleCount >= 0)
        label = QString("%1%\n%2 %3").arg(bat.healthPercent).arg(bat.cycleCount).arg(tr("cycles"));
    else if (bat.healthPercent >= 0)
        label = QString("%1%").arg(bat.healthPercent);
    else
        label = QString("%1%").arg(bat.chargePercent);

    mBatteryBar->setValue(displayValue, label);

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

void DashboardPage::updateDiskHealthBar()
{
    QList<DriveHealth> drives = im->getDriveHealth();
    if (drives.isEmpty())
        return;

    int worstHealth = 100;
    QString worstVerdict = "Good";
    QString worstModel;
    bool hasAnyHealth = false;

    for (const DriveHealth &d : drives) {
        if (d.healthPercent >= 0) {
            hasAnyHealth = true;
            if (d.healthPercent < worstHealth) {
                worstHealth = d.healthPercent;
                worstVerdict = d.healthVerdict;
                worstModel = d.model.isEmpty() ? d.deviceName : d.model;
            }
        } else if (!d.smartPassed) {
            hasAnyHealth = true;
            worstHealth = 0;
            worstVerdict = "Critical";
            worstModel = d.model.isEmpty() ? d.deviceName : d.model;
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

    QString label;
    if (drives.size() > 1 && !worstModel.isEmpty())
        label = QString("%1%\n%2").arg(displayPercent).arg(worstVerdict);
    else
        label = QString("%1%\n%2").arg(displayPercent).arg(worstVerdict);

    mDiskHealthBar->setValue(displayPercent, label);

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
