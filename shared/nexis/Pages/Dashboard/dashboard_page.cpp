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
    mCpuTile(nullptr),
    mMemTile(nullptr),
    mDiskTile(nullptr),
    mTempTile(nullptr),
    mGpuTile(nullptr),
    mBatteryTile(nullptr),
    mNetworkTile(nullptr),
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
    mKioskButton(new QPushButton(this)),
    mEditButton(new QPushButton(this)),
    mEditToolbar(nullptr),
    mBtnResetLayout(nullptr),
    mBtnDone(nullptr),
    mEditShortcut(nullptr),
    mEditMode(false),
    mKioskMode(false),
    mDragIndicator(nullptr),
    mDragSource(nullptr)
{
    ui->setupUi(this);

    init();
}

void DashboardPage::init()
{
    // Parse saved layout to extract per-tile styles before creating tiles
    QString savedLayout = mSettingManager->getDashboardLayout();
    if (!savedLayout.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(savedLayout.toUtf8());
        QJsonArray arr = doc.array();
        for (const QJsonValue &val : arr) {
            QJsonObject obj = val.toObject();
            QString id = obj["id"].toString();
            QString style = obj["style"].toString();
            if (!style.isEmpty())
                mTileStyles[id] = style;
        }
    }

    // Create tiles using factory with saved styles (or defaults)
    mCpuTile = createTile("cpu", mTileStyles.value("cpu", defaultStyle("cpu")));
    mMemTile = createTile("memory", mTileStyles.value("memory", defaultStyle("memory")));
    mDiskTile = createTile("disk", mTileStyles.value("disk", defaultStyle("disk")));
    mNetworkTile = new NetworkTile("@networkColor", this);
    mGpuTile = createTile("gpu", mTileStyles.value("gpu", defaultStyle("gpu")));
    mTempTile = createTile("temp", mTileStyles.value("temp", defaultStyle("temp")));
    mBatteryTile = createTile("battery", mTileStyles.value("battery", defaultStyle("battery")));

    // Wrap each tile in a DashboardTileWrapper for drag/resize support
    wrapTile("cpu", mCpuTile);
    wrapTile("memory", mMemTile);
    wrapTile("disk", mDiskTile);
    wrapTile("network", mNetworkTile);

    if (im->hasGpu())
        wrapTile("gpu", mGpuTile);
    else
        mGpuTile->hide();

    if (im->hasThermalSensors())
        wrapTile("temp", mTempTile);
    else
        mTempTile->hide();

    if (im->hasBattery())
        wrapTile("battery", mBatteryTile);
    else
        mBatteryTile->hide();

    // Load saved layout or use default, then build the grid
    if (savedLayout.isEmpty())
        deserializeLayout(QString(QJsonDocument(defaultLayout()).toJson()));
    else
        deserializeLayout(savedLayout);

    for (auto it = mTileColors.constBegin(); it != mTileColors.constEnd(); ++it) {
        if (it.key() == "network") {
            if (mNetworkTile)
                mNetworkTile->setColorOverride(it.value());
        } else {
            DashboardTileWrapper *w = findWrapper(it.key());
            if (w) {
                auto *metric = qobject_cast<MetricTileBase*>(w->innerWidget());
                if (metric)
                    metric->setColorOverride(it.value());
            }
        }
    }

    for (auto it = mTileRanges.constBegin(); it != mTileRanges.constEnd(); ++it) {
        DashboardTileWrapper *w = findWrapper(it.key());
        if (w) {
            auto *metric = qobject_cast<MetricTileBase*>(w->innerWidget());
            if (metric)
                metric->setColorRange(it.value());
            w->setCurrentRange(it.value());
        }
    }

    buildGrid();

    // Give the tile grid all available vertical space
    ui->mainLayout->setStretchFactor(ui->bentoGrid, 1);

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
            bool found = false;
            for (int i = 0; i < sensors.size(); ++i) {
                if (sensors.at(i).id == savedSensorId) {
                    mSelectedSensorIndex = i;
                    found = true;
                    break;
                }
            }
            if (!found)
                qWarning("Saved temperature sensor '%s' not found, falling back to first sensor",
                         qPrintable(savedSensorId));
        }

        for (QAction *a : mTempSensorMenu->actions())
            a->setChecked(a->data().toInt() == mSelectedSensorIndex);

        if (mSelectedSensorIndex >= 0 && mSelectedSensorIndex < sensors.size())
            mTempTile->setSubtitle(sensors.at(mSelectedSensorIndex).label);

        setupTileGearMenu("temp", mTempTile);

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
            bool found = false;
            for (int i = 0; i < gpus.size(); ++i) {
                if (gpus.at(i).name == savedGpuId) {
                    mCmbGpuDevice->setCurrentIndex(i);
                    mSelectedGpuIndex = i;
                    found = true;
                    break;
                }
            }
            if (!found)
                qWarning("Saved GPU device '%s' not found, falling back to first device",
                         qPrintable(savedGpuId));
        }

        if (mSelectedGpuIndex >= 0 && mSelectedGpuIndex < gpus.size())
            mGpuTile->setSubtitle(gpus.at(mSelectedGpuIndex).name);

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

    // Disk health data (populates disk tile badges + tray alerts)
    connect(mRefresh, &DataRefreshService::diskHealthUpdated,
            this, &DashboardPage::onDiskHealthUpdated);

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
            mCpuSubtitleBase = cpuModel;
            if (coreCount > 0)
                mCpuSubtitleBase += QString(" \u2022 %1C").arg(coreCount);
            mCpuTile->setSubtitle(mCpuSubtitleBase);
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
    setupTileGearMenu("disk", mDiskTile);
    connect(mDiskMenu, &QMenu::triggered, this, &DashboardPage::onDiskSelected);

    // Set network interface name
    QString ifName = im->getDefaultNetworkInterface();
    if (!ifName.isEmpty())
        mNetworkTile->setInterfaceName(ifName);

    // Update bar
    ui->widgetUpdateBar->hide();
    checkUpdate();
    connect(this, &DashboardPage::sigShowUpdateBar, ui->widgetUpdateBar, &QWidget::show);

    // Drop shadows on tile wrappers
    QList<QWidget*> widgets;
    for (DashboardTileWrapper *w : mTileWrappers)
        widgets.append(w);
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

    // Edit mode toggle button (floating, to the left of kiosk button)
    mEditButton->setFixedSize(32, 32);
    mEditButton->setIcon(QIcon(":/static/themes/common/img/grid-edit.svg"));
    mEditButton->setIconSize(QSize(16, 16));
    mEditButton->setToolTip(tr("Customize Layout (Ctrl+E)"));
    mEditButton->setCursor(Qt::PointingHandCursor);
    mEditButton->setFocusPolicy(Qt::NoFocus);
    mEditButton->setObjectName("btnEditToggle");
    mEditButton->raise();

    connect(mEditButton, &QPushButton::clicked, this, &DashboardPage::toggleEditMode);

    // Ctrl+E shortcut
    mEditShortcut = new QShortcut(QKeySequence("Ctrl+E"), this);
    connect(mEditShortcut, &QShortcut::activated, this, &DashboardPage::toggleEditMode);

    // Edit mode toolbar (hidden by default, shown above bentoGrid)
    mEditToolbar = new QWidget(this);
    mEditToolbar->setObjectName("editToolbar");
    mEditToolbar->setFixedHeight(40);
    mEditToolbar->hide();

    auto *toolbarLayout = new QHBoxLayout(mEditToolbar);
    toolbarLayout->setContentsMargins(12, 4, 12, 4);

    auto *lblCustomize = new QLabel(tr("Customize Layout"), mEditToolbar);
    lblCustomize->setObjectName("editToolbarLabel");
    toolbarLayout->addWidget(lblCustomize);
    toolbarLayout->addStretch();

    mBtnResetLayout = new QPushButton(tr("Reset Layout"), mEditToolbar);
    mBtnResetLayout->setObjectName("btnResetLayout");
    mBtnResetLayout->setCursor(Qt::PointingHandCursor);
    mBtnResetLayout->setFocusPolicy(Qt::NoFocus);
    toolbarLayout->addWidget(mBtnResetLayout);

    mBtnDone = new QPushButton(tr("Done"), mEditToolbar);
    mBtnDone->setObjectName("btnEditDone");
    mBtnDone->setCursor(Qt::PointingHandCursor);
    mBtnDone->setFocusPolicy(Qt::NoFocus);
    toolbarLayout->addWidget(mBtnDone);

    // Insert toolbar at the top of the main layout (before bentoGrid)
    ui->mainLayout->insertWidget(0, mEditToolbar);

    connect(mBtnDone, &QPushButton::clicked, this, &DashboardPage::exitEditMode);
    connect(mBtnResetLayout, &QPushButton::clicked, this, &DashboardPage::onResetLayout);

    // Drag indicator overlay
    mDragIndicator = new QWidget(this);
    mDragIndicator->setObjectName("dragIndicator");
    mDragIndicator->hide();
    mDragIndicator->setAttribute(Qt::WA_TransparentForMouseEvents);
    mDragSource = nullptr;
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

    if (clockGHz > 0.00001 && !mCpuSubtitleBase.isEmpty())
        mCpuTile->setSubtitle(mCpuSubtitleBase + QString(" \u2022 %1 GHz").arg(clockGHz, 0, 'f', 1));
}

void DashboardPage::onMemoryUpdated(const MemorySnapshot &snap)
{
    int memUsedPercent = 0;
    if (snap.total) {
        memUsedPercent = ((double)snap.used / (double)snap.total) * 100.0;
    }

    QString f_memUsed  = FormatUtil::formatBytes(snap.used);
    QString f_memTotal = FormatUtil::formatBytes(snap.total);

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

    // FR-57: Build subtitle with swap info + platform-specific breakdown
    QString subtitle = QString("Swap: %1 / %2")
        .arg(FormatUtil::formatBytes(snap.swapUsed), FormatUtil::formatBytes(snap.swapTotal));

    if (snap.wired > 0 || snap.compressed > 0) {
        // macOS: wired/active/compressed breakdown
        subtitle += QString(" \u2022 W:%1 A:%2 C:%3")
            .arg(FormatUtil::formatBytes(snap.wired),
                 FormatUtil::formatBytes(snap.active),
                 FormatUtil::formatBytes(snap.compressed));
    } else if (snap.available > 0) {
        // Linux: available memory
        subtitle += QString(" \u2022 Avail: %1")
            .arg(FormatUtil::formatBytes(snap.available));
    }

    mMemTile->setSubtitle(subtitle);

    // FR-57: Pressure-based tile color (only when user hasn't set a custom color)
    if (snap.pressureLevel > 0 && mTileColors.value("memory").isEmpty()) {
        QSettings *sv = mAppManager->getStyleValues();
        if (snap.pressureLevel >= 4) {
            QString critColor = sv ? sv->value("@memPressureCritical").toString() : QString("#E05454");
            mMemTile->setColorOverride(critColor);
        } else if (snap.pressureLevel >= 2) {
            QString warnColor = sv ? sv->value("@memPressureWarning").toString() : QString("#FFB347");
            mMemTile->setColorOverride(warnColor);
        } else {
            mMemTile->setColorOverride(QString());   // normal: clear override, use default @memoryColor
        }
    }

    // Update system summary RAM if it was unavailable at init time (BUG-60)
    if (snap.total > 0 && mSummaryRam.startsWith("0")) {
        mSummaryRam = FormatUtil::formatBytes(snap.total) + " RAM";
        refreshSummaryColors();
    }
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

    mDiskTile->setDiskInfo(diskPercent, usedText, sizeText);
    updateDiskHealthBadge();
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
            mDiskTile->setDiskInfo(percent,
                                   FormatUtil::formatBytes(d.used),
                                   FormatUtil::formatBytes(d.size));

            for (QAction *a : mDiskMenu->actions())
                a->setChecked(a->data().toString() == diskName);
            break;
        }
    }

    updateDiskHealthBadge();
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
    if (index >= 0 && index < sensors.size()) {
        mSettingManager->setTempSensorId(sensors.at(index).id);
        mTempTile->setSubtitle(sensors.at(index).label);
    }

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
    if (index >= 0 && index < gpus.size()) {
        mSettingManager->setGpuDeviceId(gpus.at(index).name);
        mGpuTile->setSubtitle(gpus.at(index).name);
    }

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

// Extract the physical device path from a partition/volume device path.
// Linux:  /dev/sda1      → /dev/sda
//         /dev/nvme0n1p1  → /dev/nvme0n1
// macOS:  /dev/disk3s1s1  → /dev/disk3  (synthesized container, may not
//         match the physical device number on APFS systems)
static QString extractBaseDevice(const QString &devicePath)
{
    // NVMe: /dev/nvme0n1p1 → /dev/nvme0n1
    static const QRegularExpression nvmeRe("^(/dev/nvme\\d+n\\d+)p\\d+$");
    auto match = nvmeRe.match(devicePath);
    if (match.hasMatch())
        return match.captured(1);

    // macOS disk: /dev/disk3s1s1 → /dev/disk3
    static const QRegularExpression macRe("^(/dev/disk\\d+)s\\d+.*$");
    match = macRe.match(devicePath);
    if (match.hasMatch())
        return match.captured(1);

    // SATA/SCSI/virtio: /dev/sda1 → /dev/sda, /dev/vda2 → /dev/vda
    static const QRegularExpression sataRe("^(/dev/[a-z]+)\\d+$");
    match = sataRe.match(devicePath);
    if (match.hasMatch())
        return match.captured(1);

    return devicePath;
}

void DashboardPage::updateDiskHealthBadge()
{
    if (mCachedDriveHealth.isEmpty() || mCachedDisks.isEmpty())
        return;

    // Find the currently selected disk
    const Disk *selectedDisk = nullptr;
    QString selectedDiskName = mSettingManager->getDiskName();
    for (const Disk &d : mCachedDisks) {
        if (d.name.trimmed() == selectedDiskName.trimmed()) {
            selectedDisk = &d;
            break;
        }
    }
    if (!selectedDisk) {
        for (const Disk &d : mCachedDisks) {
            if (d.name.trimmed() == QStorageInfo::root().displayName().trimmed()) {
                selectedDisk = &d;
                break;
            }
        }
        if (!selectedDisk)
            selectedDisk = &mCachedDisks.first();
    }

    // Match the selected volume to its physical drive's health data
    QString baseDev = extractBaseDevice(selectedDisk->device);
    const DriveHealth *matched = nullptr;

    for (const DriveHealth &dh : mCachedDriveHealth) {
        if (dh.devicePath == baseDev) {
            matched = &dh;
            break;
        }
    }

    // On macOS, APFS synthesized container numbering often won't match the
    // physical device path. If there's only one physical drive, use it.
    if (!matched && mCachedDriveHealth.size() == 1)
        matched = &mCachedDriveHealth.first();

    // Update the tile
    mDiskTile->clearDriveHealth();
    if (matched) {
        QString name = matched->model.isEmpty() ? matched->deviceName : matched->model;
        bool good = (matched->healthVerdict == "Good" || matched->smartPassed);
        mDiskTile->setDriveHealth(name, matched->healthVerdict, matched->healthPercent, good);
    }
}

void DashboardPage::onDiskHealthUpdated(const QList<DriveHealth> &drives)
{
    if (drives.isEmpty())
        return;

    mCachedDriveHealth = drives;
    updateDiskHealthBadge();

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

void DashboardPage::toggleEditMode()
{
    if (mKioskMode)
        return;

    if (mEditMode)
        exitEditMode();
    else {
        mEditMode = true;
        mEditToolbar->show();
        mKioskButton->hide();
        mEditButton->hide();
        mGearVisibleTiles.clear();
        for (DashboardTileWrapper *w : mTileWrappers) {
            auto *metric = qobject_cast<MetricTileBase*>(w->innerWidget());
            if (metric && metric->gearButton()->isVisible()) {
                mGearVisibleTiles.insert(w->tileId());
                metric->setGearVisible(false);
            }
            w->setEditMode(true);
        }
        for (QWidget *ph : mPlaceholders)
            ph->setVisible(true);
    }
}

void DashboardPage::exitEditMode()
{
    mEditMode = false;
    mEditToolbar->hide();
    mKioskButton->show();
    mEditButton->show();
    mKioskButton->raise();
    mEditButton->raise();
    for (DashboardTileWrapper *w : mTileWrappers) {
        w->setEditMode(false);
        if (mGearVisibleTiles.contains(w->tileId())) {
            auto *metric = qobject_cast<MetricTileBase*>(w->innerWidget());
            if (metric)
                metric->setGearVisible(true);
        }
    }
    mGearVisibleTiles.clear();
    for (QWidget *ph : mPlaceholders)
        ph->setVisible(false);
    QJsonDocument doc(serializeLayout());
    mSettingManager->setDashboardLayout(QString(doc.toJson(QJsonDocument::Compact)));
}

void DashboardPage::onResetLayout()
{
    mHiddenTiles.clear();
    mTileColors.clear();
    mTileRanges.clear();

    // Reset styles to defaults
    mTileStyles.clear();

    // Re-create tiles that aren't on their default style
    for (DashboardTileWrapper *w : mTileWrappers) {
        QString id = w->tileId();
        QString defStyle = defaultStyle(id);
        if (w->currentStyle() != defStyle && !availableStyles(id).isEmpty()) {
            // Detach GPU combo box if needed
            if (id == "gpu" && mCmbGpuDevice->parentWidget() == mGpuTile) {
                mGpuTile->layout()->removeWidget(mCmbGpuDevice);
                mCmbGpuDevice->setParent(this);
            }

            MetricTileBase *newTile = createTile(id, defStyle);
            w->setInnerWidget(newTile);
            w->setCurrentStyle(defStyle);

            if (id == "cpu") mCpuTile = newTile;
            else if (id == "memory") mMemTile = newTile;
            else if (id == "disk") mDiskTile = newTile;
            else if (id == "temp") mTempTile = newTile;
            else if (id == "gpu") mGpuTile = newTile;
            else if (id == "battery") mBatteryTile = newTile;

            setupTileGearMenu(id, newTile);

            if (id == "gpu" && im->hasGpu() && im->getGpuDevices().size() > 1)
                newTile->layout()->addWidget(mCmbGpuDevice);

            w->clearCustomizationSection();
            setupCustomizationMenu(w, defStyle);
        }
    }

    mSettingManager->clearDashboardLayout();
    deserializeLayout(QString(QJsonDocument(defaultLayout()).toJson()));
    buildGrid();
}

void DashboardPage::onKioskModeChanged(bool enabled)
{
    mKioskMode = enabled;
    if (enabled) {
        if (mEditMode)
            exitEditMode();
        mEditButton->hide();
        mEditShortcut->setEnabled(false);
        mKioskButton->setIcon(QIcon(":/static/themes/common/img/fullscreen-exit.svg"));
        mKioskButton->setToolTip(tr("Exit Kiosk Mode (ESC)"));
    } else {
        mEditButton->show();
        mEditButton->raise();
        mKioskButton->raise();
        mEditShortcut->setEnabled(true);
        mKioskButton->setIcon(QIcon(":/static/themes/common/img/fullscreen.svg"));
        mKioskButton->setToolTip(tr("Enter Kiosk Mode (F11)"));
    }
}

DashboardTileWrapper *DashboardPage::wrapTile(const QString &id, QWidget *tile)
{
    auto *wrapper = new DashboardTileWrapper(id, tile, this);

    connect(wrapper, &DashboardTileWrapper::dragStarted,
            this, &DashboardPage::onTileDragStarted);
    connect(wrapper, &DashboardTileWrapper::dragMoved,
            this, &DashboardPage::onTileDragMoved);
    connect(wrapper, &DashboardTileWrapper::dragFinished,
            this, &DashboardPage::onTileDragFinished);
    connect(wrapper, &DashboardTileWrapper::resizeRequested,
            this, &DashboardPage::onTileResizeRequested);
    connect(wrapper, &DashboardTileWrapper::styleChangeRequested,
            this, &DashboardPage::onTileStyleChangeRequested);
    connect(wrapper, &DashboardTileWrapper::removeRequested,
            this, &DashboardPage::onTileRemoveRequested);
    connect(wrapper, &DashboardTileWrapper::colorChangeRequested,
            this, &DashboardPage::onTileColorChangeRequested);
    connect(wrapper, &DashboardTileWrapper::rangeChangeRequested,
            this, &DashboardPage::onTileRangeChangeRequested);

    // Set up style menu for switchable tiles
    QStringList styles = availableStyles(id);
    if (!styles.isEmpty()) {
        QString currentStyle = mTileStyles.value(id, defaultStyle(id));
        wrapper->setStyleMenuItems(styles, currentStyle);
    }

    setupCustomizationMenu(wrapper, mTileStyles.value(id, defaultStyle(id)));

    mTileWrappers.append(wrapper);
    return wrapper;
}

QJsonArray DashboardPage::defaultLayout() const
{
    QJsonArray arr;
    auto addEntry = [&](const QString &id, int row, int col, int rs, int cs) {
        QJsonObject obj;
        obj["id"] = id;
        obj["row"] = row;
        obj["col"] = col;
        obj["rowSpan"] = rs;
        obj["colSpan"] = cs;
        obj["style"] = defaultStyle(id);
        arr.append(obj);
    };

    addEntry("cpu", 0, 0, 1, 1);
    addEntry("memory", 0, 1, 1, 1);
    addEntry("disk", 0, 2, 1, 1);
    addEntry("network", 0, 3, 1, 1);

    int col = 0;
    if (im->hasGpu()) addEntry("gpu", 1, col++, 1, 1);
    if (im->hasThermalSensors()) addEntry("temp", 1, col++, 1, 1);
    if (im->hasBattery()) addEntry("battery", 1, col++, 1, 1);

    return arr;
}

void DashboardPage::deserializeLayout(const QString &json)
{
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    QJsonArray arr = doc.array();

    for (const QJsonValue &val : arr) {
        QJsonObject obj = val.toObject();
        QString id = obj["id"].toString();
        int row = qBound(0, obj["row"].toInt(), GRID_ROWS - 1);
        int col = qBound(0, obj["col"].toInt(), GRID_COLS - 1);
        int rowSpan = qBound(1, obj["rowSpan"].toInt(1), GRID_ROWS - row);
        int colSpan = qBound(1, obj["colSpan"].toInt(1), GRID_COLS - col);

        QString style = obj["style"].toString();
        if (!style.isEmpty())
            mTileStyles[id] = style;

        bool visible = obj.contains("visible") ? obj["visible"].toBool(true) : true;
        if (!visible)
            mHiddenTiles.insert(id);
        else
            mHiddenTiles.remove(id);

        QString color = obj["color"].toString();
        if (color.startsWith("range::")) {
            QString rangeId = color.mid(7);
            if (!rangeId.isEmpty())
                mTileRanges[id] = rangeId;
            else
                mTileRanges.remove(id);
            mTileColors.remove(id);
        } else {
            if (!color.isEmpty())
                mTileColors[id] = color;
            else
                mTileColors.remove(id);
            mTileRanges.remove(id);
        }

        for (DashboardTileWrapper *w : mTileWrappers) {
            if (w->tileId() == id) {
                w->setGridPosition(row, col, rowSpan, colSpan);
                if (!style.isEmpty())
                    w->setCurrentStyle(style);
                if (color.startsWith("range::"))
                    w->setCurrentRange(color.mid(7));
                else
                    w->setCurrentColor(color);
                break;
            }
        }
    }
}

QJsonArray DashboardPage::serializeLayout() const
{
    QJsonArray arr;
    for (const DashboardTileWrapper *w : mTileWrappers) {
        QJsonObject obj;
        obj["id"] = w->tileId();
        obj["row"] = w->gridRow();
        obj["col"] = w->gridCol();
        obj["rowSpan"] = w->gridRowSpan();
        obj["colSpan"] = w->gridColSpan();
        obj["style"] = w->currentStyle();
        if (mHiddenTiles.contains(w->tileId()))
            obj["visible"] = false;
        if (mTileRanges.contains(w->tileId()))
            obj["color"] = QString("range::%1").arg(mTileRanges.value(w->tileId()));
        else if (mTileColors.contains(w->tileId()))
            obj["color"] = mTileColors.value(w->tileId());
        arr.append(obj);
    }
    return arr;
}

void DashboardPage::rebuildOccupancy()
{
    for (int r = 0; r < GRID_ROWS; ++r)
        for (int c = 0; c < GRID_COLS; ++c)
            mOccupancy[r][c].clear();

    for (const DashboardTileWrapper *w : mTileWrappers) {
        if (mHiddenTiles.contains(w->tileId()))
            continue;
        for (int r = w->gridRow(); r < w->gridRow() + w->gridRowSpan(); ++r)
            for (int c = w->gridCol(); c < w->gridCol() + w->gridColSpan(); ++c)
                if (r < GRID_ROWS && c < GRID_COLS)
                    mOccupancy[r][c] = w->tileId();
    }
}

bool DashboardPage::regionIsFree(int row, int col, int rowSpan, int colSpan,
                                  const QString &ignoreTileId) const
{
    if (row + rowSpan > GRID_ROWS || col + colSpan > GRID_COLS)
        return false;
    for (int r = row; r < row + rowSpan; ++r)
        for (int c = col; c < col + colSpan; ++c)
            if (!mOccupancy[r][c].isEmpty() && mOccupancy[r][c] != ignoreTileId)
                return false;
    return true;
}

void DashboardPage::buildGrid()
{
    while (ui->bentoGrid->count() > 0) {
        QLayoutItem *item = ui->bentoGrid->takeAt(0);
        if (item->widget())
            item->widget()->setParent(nullptr);
        delete item;
    }
    qDeleteAll(mPlaceholders);
    mPlaceholders.clear();

    rebuildOccupancy();

    for (DashboardTileWrapper *w : mTileWrappers) {
        if (mHiddenTiles.contains(w->tileId())) {
            w->hide();
            continue;
        }
        w->setParent(this);
        ui->bentoGrid->addWidget(w, w->gridRow(), w->gridCol(),
                                  w->gridRowSpan(), w->gridColSpan());
        applyDisplayModeForSpan(w);
        w->show();
    }

    for (int r = 0; r < GRID_ROWS; ++r) {
        for (int c = 0; c < GRID_COLS; ++c) {
            if (mOccupancy[r][c].isEmpty()) {
                auto *ph = new QWidget(this);
                ph->setObjectName("dashPlaceholder");
                ph->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
                ph->setVisible(mEditMode);
                ui->bentoGrid->addWidget(ph, r, c);
                mPlaceholders.append(ph);
            }
        }
    }

    for (int c = 0; c < GRID_COLS; ++c)
        ui->bentoGrid->setColumnStretch(c, 1);
    for (int r = 0; r < GRID_ROWS; ++r)
        ui->bentoGrid->setRowStretch(r, 1);

    // Re-raise floating buttons above reparented tile wrappers (BUG-63)
    mEditButton->raise();
    mKioskButton->raise();
}

void DashboardPage::applyDisplayModeForSpan(DashboardTileWrapper *wrapper)
{
    auto *metric = qobject_cast<MetricTileBase*>(wrapper->innerWidget());
    if (!metric)
        return;

    int area = wrapper->gridRowSpan() * wrapper->gridColSpan();
    if (area >= 4)
        metric->setDisplayMode(MetricTileBase::Hero);
    else if (area >= 2)
        metric->setDisplayMode(MetricTileBase::Large);
    else
        metric->setDisplayMode(MetricTileBase::Normal);
}

void DashboardPage::rebuildLayout()
{
    QString saved = mSettingManager->getDashboardLayout();
    if (saved.isEmpty())
        deserializeLayout(QString(QJsonDocument(defaultLayout()).toJson()));
    else
        deserializeLayout(saved);
    buildGrid();
}

bool DashboardPage::gridCellAtPos(const QPoint &globalPos, int &outRow, int &outCol) const
{
    QWidget *gridParent = ui->bentoGrid->parentWidget();
    if (!gridParent)
        return false;

    QPoint local = gridParent->mapFromGlobal(globalPos);
    QRect gridRect = ui->bentoGrid->geometry();

    if (!gridRect.contains(local))
        return false;

    int x = local.x() - gridRect.x();
    int y = local.y() - gridRect.y();

    int cellW = gridRect.width() / GRID_COLS;
    int cellH = gridRect.height() / GRID_ROWS;

    if (cellW <= 0 || cellH <= 0)
        return false;

    outCol = qBound(0, x / cellW, GRID_COLS - 1);
    outRow = qBound(0, y / cellH, GRID_ROWS - 1);
    return true;
}

void DashboardPage::onTileDragStarted(DashboardTileWrapper *wrapper, const QPoint &globalPos)
{
    Q_UNUSED(globalPos)
    mDragSource = wrapper;
    wrapper->setWindowOpacity(0.5);
    wrapper->raise();
}

void DashboardPage::onTileDragMoved(DashboardTileWrapper *wrapper, const QPoint &globalPos)
{
    Q_UNUSED(wrapper)

    int targetRow, targetCol;
    if (!gridCellAtPos(globalPos, targetRow, targetCol)) {
        mDragIndicator->hide();
        return;
    }

    if (mDragSource && mDragSource->gridRow() == targetRow && mDragSource->gridCol() == targetCol) {
        mDragIndicator->hide();
        return;
    }

    QRect gridRect = ui->bentoGrid->geometry();
    int cellW = gridRect.width() / GRID_COLS;
    int cellH = gridRect.height() / GRID_ROWS;
    int x = gridRect.x() + targetCol * cellW;
    int y = gridRect.y() + targetRow * cellH;
    mDragIndicator->setGeometry(x, y, cellW, cellH);
    mDragIndicator->show();
    mDragIndicator->raise();
}

void DashboardPage::onTileDragFinished(DashboardTileWrapper *wrapper, const QPoint &globalPos)
{
    wrapper->setWindowOpacity(1.0);
    mDragIndicator->hide();

    if (!mDragSource)
        return;

    int targetRow, targetCol;
    if (!gridCellAtPos(globalPos, targetRow, targetCol)) {
        mDragSource = nullptr;
        return;
    }

    if (mOccupancy[targetRow][targetCol].isEmpty()) {
        int srcRS = mDragSource->gridRowSpan();
        int srcCS = mDragSource->gridColSpan();
        if (regionIsFree(targetRow, targetCol, srcRS, srcCS, mDragSource->tileId())) {
            mDragSource->setGridPosition(targetRow, targetCol, srcRS, srcCS);
            buildGrid();
        }
    } else {
        // Look up the occupant by tileId from the grid (handles multi-cell tiles
        // where the drop lands on a non-top-left cell)
        QString occupantId = mOccupancy[targetRow][targetCol];
        DashboardTileWrapper *target = nullptr;
        for (DashboardTileWrapper *w : mTileWrappers) {
            if (w->tileId() == occupantId && w != mDragSource) {
                target = w;
                break;
            }
        }
        if (target) {
            int srcRow = mDragSource->gridRow(), srcCol = mDragSource->gridCol();
            int srcRS = mDragSource->gridRowSpan(), srcCS = mDragSource->gridColSpan();
            int tgtRow = target->gridRow(), tgtCol = target->gridCol();
            int tgtRS = target->gridRowSpan(), tgtCS = target->gridColSpan();

            // Use regionIsFree for full bounds + collision checking (rows AND cols)
            bool srcFitsAtTarget = regionIsFree(tgtRow, tgtCol, srcRS, srcCS, mDragSource->tileId());
            bool tgtFitsAtSource = regionIsFree(srcRow, srcCol, tgtRS, tgtCS, target->tileId());

            if (srcFitsAtTarget && tgtFitsAtSource) {
                mDragSource->setGridPosition(tgtRow, tgtCol, srcRS, srcCS);
                target->setGridPosition(srcRow, srcCol, tgtRS, tgtCS);
                buildGrid();
            }
        }
    }

    mDragSource = nullptr;
}

void DashboardPage::onTileResizeRequested(DashboardTileWrapper *wrapper, int newColSpan, int newRowSpan)
{
    int row = wrapper->gridRow();
    int col = wrapper->gridCol();

    if (!regionIsFree(row, col, newRowSpan, newColSpan, wrapper->tileId()))
        return;

    wrapper->setGridPosition(row, col, newRowSpan, newColSpan);
    buildGrid();
}

void DashboardPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    mKioskButton->move(width() - mKioskButton->width() - 10, 10);
    mEditButton->move(width() - mKioskButton->width() - mEditButton->width() - 18, 10);
}

// --- Tile Factory & Style Switching ---

void DashboardPage::tileTitle(const QString &id, QString &title, QString &colorToken) const
{
    if (id == "cpu")          { title = tr("CPU");     colorToken = "@cpuColor"; }
    else if (id == "memory")  { title = tr("MEMORY");  colorToken = "@memoryColor"; }
    else if (id == "disk")    { title = tr("DISK");    colorToken = "@diskColor"; }
    else if (id == "temp")    { title = tr("TEMP");    colorToken = "@tempColor"; }
    else if (id == "gpu")     { title = tr("GPU");     colorToken = "@gpuColor"; }
    else if (id == "battery") { title = tr("BATTERY"); colorToken = "@batteryColor"; }
}

MetricTileBase *DashboardPage::createTile(const QString &id, const QString &style)
{
    QString title, colorToken;
    tileTitle(id, title, colorToken);

    if (style == "gauge")
        return new GaugeTile(title, colorToken, this);
    if (style == "ring")
        return new RingTile(title, colorToken, this);
    if (style == "hybrid")
        return new HybridTile(title, colorToken, this);
    if (style == "speedometer")
        return new SpeedometerTile(title, colorToken, this);
    if (style == "vumeter")
        return new VuMeterTile(title, colorToken, this);
    if (style == "donut")
        return new DiskTile(colorToken, "@color02", this);

    return new MetricTile(title, colorToken, this);
}

QStringList DashboardPage::availableStyles(const QString &tileId) const
{
    if (tileId == "network")
        return {};
    if (tileId == "disk")
        return {"donut", "sparkline", "gauge", "hybrid", "ring", "speedometer", "vumeter"};
    return {"sparkline", "gauge", "hybrid", "ring", "speedometer", "vumeter"};
}

QString DashboardPage::defaultStyle(const QString &tileId) const
{
    if (tileId == "disk")
        return "donut";
    if (tileId == "network")
        return "network";
    return "sparkline";
}

void DashboardPage::setupTileGearMenu(const QString &id, MetricTileBase *tile)
{
    if (id == "disk") {
        mDiskMenu->setObjectName("diskSelectorMenu");
        tile->gearButton()->setMenu(mDiskMenu);
        tile->gearButton()->setPopupMode(QToolButton::InstantPopup);
        tile->setGearVisible(mCachedDisks.size() >= 2);
    } else if (id == "temp" && im->hasThermalSensors()) {
        tile->gearButton()->setMenu(mTempSensorMenu);
        tile->gearButton()->setPopupMode(QToolButton::InstantPopup);
        tile->setGearVisible(im->getThermalSensors().size() >= 2);
    }
}

DashboardTileWrapper *DashboardPage::findWrapper(const QString &tileId) const
{
    for (DashboardTileWrapper *w : mTileWrappers)
        if (w->tileId() == tileId)
            return w;
    return nullptr;
}

void DashboardPage::onTileStyleChangeRequested(DashboardTileWrapper *wrapper, const QString &style)
{
    QString id = wrapper->tileId();

    if (wrapper->currentStyle() == style)
        return;

    // Detach GPU combo box before old tile is destroyed
    if (id == "gpu" && mCmbGpuDevice->parentWidget() == mGpuTile) {
        mGpuTile->layout()->removeWidget(mCmbGpuDevice);
        mCmbGpuDevice->setParent(this);
    }

    MetricTileBase *newTile = createTile(id, style);

    // Swap inner widget (old tile scheduled for deletion)
    wrapper->setInnerWidget(newTile);
    wrapper->setCurrentStyle(style);

    // Update member pointer
    if (id == "cpu")          mCpuTile = newTile;
    else if (id == "memory")  mMemTile = newTile;
    else if (id == "disk")    mDiskTile = newTile;
    else if (id == "temp")    mTempTile = newTile;
    else if (id == "gpu")     mGpuTile = newTile;
    else if (id == "battery") mBatteryTile = newTile;

    // Re-attach gear menus
    setupTileGearMenu(id, newTile);

    // Re-add GPU combo box to the new tile
    if (id == "gpu" && im->hasGpu() && im->getGpuDevices().size() > 1)
        newTile->layout()->addWidget(mCmbGpuDevice);

    // Re-apply display mode
    applyDisplayModeForSpan(wrapper);

    // Re-apply disk health badges
    if (id == "disk")
        updateDiskHealthBadge();

    // Rebuild the customization menu (color swatches vs. range presets) for the new style
    wrapper->clearCustomizationSection();
    setupCustomizationMenu(wrapper, style);

    // Re-apply saved customization
    if (tileUsesRangeMenu(style)) {
        if (mTileRanges.contains(id)) {
            newTile->setColorRange(mTileRanges[id]);
            wrapper->setCurrentRange(mTileRanges[id]);
        }
    } else {
        if (mTileColors.contains(id))
            newTile->setColorOverride(mTileColors[id]);
    }

    // Store style
    mTileStyles[id] = style;
}

void DashboardPage::onTileRemoveRequested(DashboardTileWrapper *wrapper)
{
    mHiddenTiles.insert(wrapper->tileId());
    buildGrid();
}

void DashboardPage::onTileColorChangeRequested(DashboardTileWrapper *wrapper, const QString &hexColor)
{
    QString id = wrapper->tileId();

    if (hexColor.isEmpty())
        mTileColors.remove(id);
    else
        mTileColors[id] = hexColor;

    mTileRanges.remove(id);

    if (id == "network") {
        if (mNetworkTile)
            mNetworkTile->setColorOverride(hexColor);
    } else {
        auto *metric = qobject_cast<MetricTileBase*>(wrapper->innerWidget());
        if (metric) {
            metric->setColorRange(QString());
            metric->setColorOverride(hexColor);
        }
    }

    wrapper->setCurrentColor(hexColor);

    mSettingManager->setDashboardLayout(
        QJsonDocument(serializeLayout()).toJson(QJsonDocument::Compact));
}

void DashboardPage::onTileRangeChangeRequested(DashboardTileWrapper *wrapper, const QString &rangeId)
{
    QString id = wrapper->tileId();

    if (rangeId.isEmpty())
        mTileRanges.remove(id);
    else
        mTileRanges[id] = rangeId;

    mTileColors.remove(id);

    auto *metric = qobject_cast<MetricTileBase*>(wrapper->innerWidget());
    if (metric) {
        metric->setColorOverride(QString());
        metric->setColorRange(rangeId);
    }

    wrapper->setCurrentRange(rangeId);

    mSettingManager->setDashboardLayout(
        QJsonDocument(serializeLayout()).toJson(QJsonDocument::Compact));
}

bool DashboardPage::tileUsesRangeMenu(const QString &style) const
{
    return (style == "speedometer" || style == "vumeter");
}

void DashboardPage::setupCustomizationMenu(DashboardTileWrapper *wrapper, const QString &style)
{
    QString id = wrapper->tileId();

    if (tileUsesRangeMenu(style)) {
        QStringList rangeIds = MetricTileBase::availableRangeIds();
        QStringList labels;
        QList<QList<QColor>> swatches;
        for (const QString &rid : rangeIds) {
            labels.append(MetricTileBase::rangeDisplayName(rid));
            swatches.append(MetricTileBase::rangeColors(rid));
        }
        wrapper->setRangeMenuItems(rangeIds, labels, swatches, mTileRanges.value(id));
    } else if (id != "network") {
        static const QStringList colorPalette = {
            "#FF6B1A", "#FFB347", "#E05454", "#26A69A", "#813D9C", "#5B9BD5", "#2EC27E",
            "#E91E63", "#00BCD4", "#8BC34A", "#FF5722", "#607D8B", "#9C27B0", "#FFEB3B", "#795548", "#F48FB1"
        };
        wrapper->setColorMenuItems(colorPalette, mTileColors.value(id));
    } else {
        static const QStringList colorPalette = {
            "#FF6B1A", "#FFB347", "#E05454", "#26A69A", "#813D9C", "#5B9BD5", "#2EC27E",
            "#E91E63", "#00BCD4", "#8BC34A", "#FF5722", "#607D8B", "#9C27B0", "#FFEB3B", "#795548", "#F48FB1"
        };
        wrapper->setColorMenuItems(colorPalette, mTileColors.value(id));
    }
}
