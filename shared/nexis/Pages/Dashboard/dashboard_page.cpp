#include "dashboard_page.h"
#include "ui_dashboard_page.h"
#include <QToolButton>
#include "maintenance_wizard_dialog.h"
#include "add_tile_dialog.h"

#include "utilities.h"
#include "Managers/app_manager.h"
#include "Managers/tool_manager.h"
#include "Managers/data_refresh_service.h"
#include "signal_mapper.h"

#include <QDateTime>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QVersionNumber>
#include <algorithm>
#include <functional>

DashboardPage::~DashboardPage()
{
    delete ui;
}

DashboardPage::DashboardPage(QWidget *parent, InfoManager *infoManager,
                             SettingManager *settingManager, AppManager *appManager,
                             SignalMapper *signalMapper, DataRefreshService *refreshService) :
    NexisPage(parent),
    ui(new Ui::DashboardPage),
    mCpuTile(nullptr),
    mMemTile(nullptr),
    mBatteryTile(nullptr),
    mHealthTile(nullptr),
    mNetworkTile(nullptr),
    im(infoManager ? infoManager : InfoManager::ins()),
    mSettingManager(settingManager ? settingManager : SettingManager::ins()),
    mAppManager(appManager ? appManager : AppManager::ins()),
    mSignalMapper(signalMapper ? signalMapper : SignalMapper::ins()),
    mRefresh(refreshService ? refreshService : DataRefreshService::ins()),
    mKioskButton(new QToolButton(this)),
    mEditButton(new QToolButton(this)),
    mEditToolbar(nullptr),
    mBtnResetLayout(nullptr),
    mBtnDone(nullptr),
    mEditShortcut(nullptr),
    mEditMode(false),
    mKioskMode(false),
    mDragIndicator(nullptr),
    mDragSource(nullptr),
    mActive(true)
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
        QJsonArray arr = doc.isObject()
            ? doc.object().value("tiles").toArray()
            : doc.array();
        for (const QJsonValue &val : arr) {
            QJsonObject obj = val.toObject();
            QString type = obj["id"].toString();
            QString uid = obj.contains("uid") ? obj["uid"].toString() : type;
            QString style = obj["style"].toString();
            if (!style.isEmpty())
                mTileStyles[uid] = style;
        }
    }

    // GH#191: layout-driven tile creation. Parse the layout tiles up front
    // (envelope-aware + migrate) so we create ONE wrapper per PERSISTED tile,
    // including multiple instances of a type (e.g. two temp tiles). For the
    // default/single-instance case this produces exactly one wrapper per type,
    // in the default order — identical to the legacy fixed-create behaviour.
    // deserializeLayout() still re-parses afterwards to apply positions/spans/
    // colors/input by uid.
    QJsonArray layoutTiles;
    {
        QString src = savedLayout.isEmpty()
            ? QString(QJsonDocument(defaultLayout()).toJson())
            : savedLayout;
        QJsonDocument d = QJsonDocument::fromJson(src.toUtf8());
        QJsonArray raw = d.isObject() ? d.object().value("tiles").toArray() : d.array();
        int ver = d.isObject() ? d.object().value("version").toInt(1) : 1;
        layoutTiles = DashboardLayout::migrate(raw, ver);
    }

    // Singleton convenience pointers (mCpuTile/mMemTile/mBatteryTile/
    // mHealthTile/mNetworkTile) are set inside the loop as their type is created.
    auto createForType = [this](const QString &type, const QString &style) -> QWidget* {
        if (type == "network") return new NetworkTile("@networkColor", this);
        if (type == "health")  return new HealthScoreTile("@healthScoreColor", this);
        return createTile(type, style);
    };

    auto assignSingleton = [this](const QString &type, QWidget *tile) {
        if (type == "cpu")          mCpuTile = qobject_cast<MetricTileBase*>(tile);
        else if (type == "memory")  mMemTile = qobject_cast<MetricTileBase*>(tile);
        else if (type == "battery") mBatteryTile = qobject_cast<MetricTileBase*>(tile);
        else if (type == "health")  mHealthTile = qobject_cast<HealthScoreTile*>(tile);
        else if (type == "network") mNetworkTile = qobject_cast<NetworkTile*>(tile);
    };

    for (const QJsonValue &v : layoutTiles) {
        QJsonObject o = v.toObject();
        QString type = o.value("id").toString();
        if (type.isEmpty()) continue;
        QString uid  = o.contains("uid") ? o.value("uid").toString() : type;
        QString input = o.value("input").toString();
        QString style = o.contains("style") ? o.value("style").toString()
                                            : defaultStyle(type);

        // Skip optional types whose hardware is absent.
        if (type == "gpu" && !im->hasGpu()) continue;
        if (type == "temp" && !im->hasThermalSensors()) continue;
        if (type == "battery" && !im->hasBattery()) continue;
        if (type == "fan" && !im->hasFanSensors()) continue;

        QString tileStyle = mTileStyles.value(uid, style);
        mTileStyles[uid] = tileStyle;
        QWidget *tile = createForType(type, tileStyle);
        wrapTile(uid, type, input, tile);
        assignSingleton(type, tile);
    }

    // Singleton safety net: a malformed/old saved layout could omit a required
    // singleton, leaving its member pointer null and crashing the singleton
    // update routines. Guarantee cpu/memory/network/health (and battery when
    // hardware is present) each have at least one wrapper + a non-null pointer.
    // For a normal default/saved layout all are present, so this is a no-op.
    auto ensureSingleton = [&](const QString &type) {
        if (!wrappersOfType(type).isEmpty()) return;
        QString style = mTileStyles.value(type, defaultStyle(type));
        mTileStyles[type] = style;
        QWidget *tile = createForType(type, style);
        wrapTile(type, type, QString(), tile);
        assignSingleton(type, tile);
    };
    ensureSingleton("cpu");
    ensureSingleton("memory");
    ensureSingleton("network");
    ensureSingleton("health");
    if (im->hasBattery())
        ensureSingleton("battery");

    mHealthTile->setQuickAction(tr("System Checkup"), [this]() {
        launchMaintenanceWizard();
    });

    // GH#191: seed default multi-instance tiles' bindings from legacy global
    // selections (or the first detected input). Runs BEFORE deserializeLayout so
    // a saved per-tile input always wins over the legacy seed.
    migrateLegacyBindings();

    // Load saved layout or use default, then build the grid
    if (savedLayout.isEmpty())
        deserializeLayout(QString(QJsonDocument(defaultLayout()).toJson()));
    else
        deserializeLayout(savedLayout);

    for (auto it = mTileColors.constBegin(); it != mTileColors.constEnd(); ++it) {
        DashboardTileWrapper *w = findWrapper(it.key());
        if (!w) continue;
        if (auto *net = qobject_cast<NetworkTile*>(w->innerWidget()))
            net->setColorOverride(it.value());
        else if (auto *metric = qobject_cast<MetricTileBase*>(w->innerWidget()))
            metric->setColorOverride(it.value());
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

    // GH#191: host the bento grid in a vertical scroll area so fixed-size tiles
    // overflow downward instead of compressing.
    mGridContainer = new QWidget;
    mGridContainer->setObjectName("bentoGridContainer");
    mGridContainer->setStyleSheet("#bentoGridContainer{background:transparent;}");
    // The .ui nests bentoGrid as an item inside mainLayout, so the layout
    // already has a parent. Detach it from mainLayout (item + QObject parent)
    // before re-homing it on the container, otherwise setLayout() is rejected.
    ui->mainLayout->removeItem(ui->bentoGrid);
    ui->bentoGrid->setParent(nullptr);
    mGridContainer->setLayout(ui->bentoGrid);          // steals the layout from the .ui parent

    mGridScroll = new QScrollArea(this);
    mGridScroll->setObjectName("bentoScroll");
    mGridScroll->setWidgetResizable(true);
    mGridScroll->setFrameShape(QFrame::NoFrame);
    // GH#191: columns are derived from the available width, so the grid must
    // always fit horizontally — sideways scrolling is never desirable and its
    // appearance is purely a transient-width artifact. Force it off.
    mGridScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mGridScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    mGridScroll->setStyleSheet("QScrollArea{background:transparent;} QScrollArea>QWidget>QWidget{background:transparent;}");
    mGridScroll->setWidget(mGridContainer);

    // bentoGrid was item 0 in mainLayout; restore the scroll area to that slot.
    ui->mainLayout->insertWidget(0, mGridScroll);

    buildGrid();

    // Give the tile grid all available vertical space
    ui->mainLayout->setStretchFactor(mGridScroll, 1);

    // Reflow the freshly-loaded layout to the real viewport width so the first
    // paint uses a responsive column count instead of the kMaxCols placeholder.
    recomputeColumns();

    // GH#191: data-refresh wiring for the multi-instance sensor types. The gear
    // menus and subtitles are now built per-tile in setupTileGearMenu(wrapper)
    // (called from wrapTile) and migrateLegacyBindings(); only the live-data
    // connections live here, still guarded by availability.
    if (im->hasThermalSensors())
        connect(mRefresh, &DataRefreshService::tempUpdated,
                this, &DashboardPage::updateTempTile);

    if (im->hasFanSensors())
        connect(mRefresh, &DataRefreshService::fanUpdated,
                this, &DashboardPage::updateFanTile);

    if (im->hasGpu())
        connect(mRefresh, &DataRefreshService::gpuUpdated,
                this, &DashboardPage::onGpuUpdated);

    // Battery gauge
    if (im->hasBattery()) {
        connect(mRefresh, &DataRefreshService::batteryUpdated,
                this, &DashboardPage::onBatteryUpdated);
    }

    // Disk health data (populates disk tile badges + tray alerts)
    connect(mRefresh, &DataRefreshService::diskHealthUpdated,
            this, &DashboardPage::onDiskHealthUpdated);

    // Health score tile data feeds
    auto *calc = mHealthTile->calculator();
    calc->setComponentAvailable("cpu", true);
    calc->setComponentAvailable("memory", true);
    calc->setComponentAvailable("disk", true);
    calc->setComponentAvailable("temp", im->hasThermalSensors());
    calc->setComponentAvailable("battery", im->hasBattery());
    calc->setComponentAvailable("smart", im->hasDiskHealth());

    connect(mRefresh, &DataRefreshService::cpuUpdated,
            this, &DashboardPage::onHealthCpuUpdated);
    connect(mRefresh, &DataRefreshService::memoryUpdated,
            this, &DashboardPage::onHealthMemoryUpdated);
    connect(mRefresh, &DataRefreshService::diskUsageUpdated,
            this, &DashboardPage::onHealthDiskUpdated);
    if (im->hasThermalSensors())
        connect(mRefresh, &DataRefreshService::tempUpdated,
                this, &DashboardPage::onHealthTempUpdated);
    if (im->hasBattery())
        connect(mRefresh, &DataRefreshService::batteryUpdated,
                this, &DashboardPage::onHealthBatteryUpdated);
    connect(mRefresh, &DataRefreshService::diskHealthUpdated,
            this, &DashboardPage::onHealthDiskHealthUpdated);

    // Set CPU model + core info as subtitle
    {
        QString cpuModel = im->getCpuModel();
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

    // Apply dashboard footer visibility from settings
    applyFooterVisibility();
    connect(mSignalMapper, &SignalMapper::sigDashboardFooterChanged,
            this, &DashboardPage::applyFooterVisibility);

    // Kiosk mode toggle button (floating, top-right)
    mKioskButton->setFixedSize(32, 32);
    mKioskButton->setIcon(QIcon(":/static/themes/common/img/fullscreen.svg"));
    mKioskButton->setIconSize(QSize(16, 16));
    mKioskButton->setToolTip(tr("Enter Kiosk Mode (F11)"));
    mKioskButton->setCursor(Qt::PointingHandCursor);
    mKioskButton->setObjectName("btnKioskToggle");
    mKioskButton->setAutoRaise(true);
    mKioskButton->raise();

    connect(mKioskButton, &QToolButton::clicked, this, [this]() {
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
    mEditButton->setObjectName("btnEditToggle");
    mEditButton->setAutoRaise(true);
    mEditButton->raise();

    connect(mEditButton, &QToolButton::clicked, this, &DashboardPage::toggleEditMode);

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
    toolbarLayout->addWidget(mBtnResetLayout);

    mAddTileButton = new QToolButton(mEditToolbar);
    mAddTileButton->setObjectName("btnAddTile");
    mAddTileButton->setText(tr("Add Tile \u25be"));
    mAddTileButton->setPopupMode(QToolButton::InstantPopup);
    mAddTileButton->setCursor(Qt::PointingHandCursor);
    mAddTileButton->hide();
    toolbarLayout->addWidget(mAddTileButton);
    connect(mAddTileButton, &QToolButton::clicked, this, &DashboardPage::onAddTileClicked);

    mBtnDone = new QPushButton(tr("Done"), mEditToolbar);
    mBtnDone->setObjectName("btnEditDone");
    mBtnDone->setCursor(Qt::PointingHandCursor);
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

    mSummaryHostname = im->getHostname();
    mSummaryOs = im->getDistribution();
    mSummaryCpu = im->getCpuModel();
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
    const QNetworkRequest updateCheckRequest(QUrl("https://api.github.com/repos/s4solutionsllc/Nexis/releases/latest"));
    connect(nam,&QNetworkAccessManager::finished,this,[this,nam](QNetworkReply * reply){
        if(reply->error()==QNetworkReply::NoError)
        {
            const QString requestResult= reply->readAll();
            const QJsonDocument result = QJsonDocument::fromJson(requestResult.toUtf8());
            const QRegularExpression ex("([0-9]+\\.[0-9]+\\.[0-9]+)");
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

        // SSO-3399: free the QNetworkReply and the NAM after the one-shot
        // check completes — both used to leak for the lifetime of the page.
        reply->deleteLater();
        nam->deleteLater();
    });
    nam->get(updateCheckRequest);
}

void DashboardPage::on_btnDownloadUpdate_clicked()
{
    QDesktopServices::openUrl(QUrl("https://github.com/s4solutionsllc/Nexis/releases/latest"));
}

void DashboardPage::onCpuUpdated(const QList<int> &percents, double clockGHz,
                                  const QList<double> &loadAvgs)
{
    Q_UNUSED(loadAvgs)

    // SSO-3380 / WI-18: DataRefreshService already gates the emit on a
    // non-empty list, but keep this bounds check as a belt-and-braces
    // guard for any future direct caller — `.at(0)` is UB on empty.
    if (percents.isEmpty()) return;
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

    if (!mActive) return;

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

    if (!mActive) return;

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

    bool firstFill = mCachedDisks.isEmpty();
    mCachedDisks = disks;

    // GH#191: the disk list became available — (re)build each disk tile's gear
    // menu so it lists the detected disks. On the first fill the wrappers were
    // created before mCachedDisks was populated, so their gears were empty.
    if (firstFill)
        for (DashboardTileWrapper *w : wrappersOfType("disk"))
            setupTileGearMenu(w);

    // alert message — fire once for the worst disk, using the same threshold
    // logic as before but evaluated against the highest-usage disk.
    int diskAlertPercent = mSettingManager->getDiskAlertPercent();
    if (diskAlertPercent > 0) {
        int worst = 0;
        for (const Disk &d : disks)
            if (d.size > 0)
                worst = qMax(worst, static_cast<int>((double)d.used / (double)d.size * 100.0));
        static bool isShow = true;
        if (worst > diskAlertPercent && isShow) {
            mAppManager->getTrayIcon()->showMessage(tr("High Disk Usage"),
                                                          tr("The amount of disk used is over %1%.").arg(diskAlertPercent),
                                                          QSystemTrayIcon::Warning);
            isShow = false;
        } else if (worst < diskAlertPercent) {
            isShow = true;
        }
    }

    if (!mActive) return;

    // Drive each disk tile from its own bound disk (fallback to first if unbound
    // or the binding no longer resolves).
    for (DashboardTileWrapper *w : wrappersOfType("disk"))
        refreshDiskUsageTile(w);

    updateDiskHealthBadge();
}

void DashboardPage::onNetworkUpdated(quint64 rxBytes, quint64 txBytes)
{
    Q_UNUSED(rxBytes) Q_UNUSED(txBytes)
    if (!mActive) return;

    NetInterfaceStatsMap stats = im->getInterfaceStats();
    for (DashboardTileWrapper *w : wrappersOfType("network")) {
        auto *tile = qobject_cast<NetworkTile*>(w->innerWidget());
        if (!tile) continue;
        QString iface = w->inputKey().isEmpty() ? im->getDefaultNetworkInterface()
                                                : w->inputKey();
        if (!stats.contains(iface)) continue;
        quint64 rx = stats.value(iface).rx;
        quint64 tx = stats.value(iface).tx;
        auto last = mNetLastBytes.value(w->tileId(), {rx, tx});
        // GH#191: show the friendly form ("Wi-Fi (en0)") on the tile subtitle.
        // The macOS display-name cache keeps this cheap per-tick.
        QString friendly = im->getNetworkInterfaceDisplayName(iface);
        tile->setInterfaceName(friendly.isEmpty() ? iface
                                                   : QStringLiteral("%1 (%2)").arg(friendly, iface));
        tile->setValues(rx - last.first, tx - last.second, rx, tx);
        mNetLastBytes[w->tileId()] = {rx, tx};
    }
}

void DashboardPage::updateTempTile()
{
    if (!mActive) return;
    QList<ThermalSensor> sensors = im->getThermalSensors();
    for (DashboardTileWrapper *w : wrappersOfType("temp")) {
        int idx = 0;
        for (int i = 0; i < sensors.size(); ++i)
            if (sensors.at(i).id == w->inputKey()) { idx = i; break; }
        double temp = im->getThermalTemperature(idx);
        int percent = qBound(0, static_cast<int>(temp), 100);
        auto *tile = qobject_cast<MetricTileBase*>(w->innerWidget());
        if (!tile) continue;
        if (idx >= 0 && idx < sensors.size())
            tile->setSubtitle(sensors.at(idx).label);
        tile->setValue(percent, QString("%1\u00B0C").arg(temp, 0, 'f', 1));
        tile->addDataPoint(temp);
    }
}

void DashboardPage::updateFanTile()
{
    if (!mActive) return;
    QList<FanSensor> fans = im->getFanSensors();
    for (DashboardTileWrapper *w : wrappersOfType("fan")) {
        int idx = 0;
        for (int i = 0; i < fans.size(); ++i)
            if (fans.at(i).id == w->inputKey()) { idx = i; break; }
        int rpm = im->getFanSpeed(idx);
        int maxRpm = (idx >= 0 && idx < fans.size() && fans.at(idx).maxRpm > 0)
                       ? fans.at(idx).maxRpm : 6000;
        int percent = qBound(0, static_cast<int>(rpm * 100.0 / maxRpm), 100);
        auto *tile = qobject_cast<MetricTileBase*>(w->innerWidget());
        if (!tile) continue;
        if (idx >= 0 && idx < fans.size())
            tile->setSubtitle(fans.at(idx).label);
        tile->setValue(percent, QString("%1 RPM").arg(rpm));
        tile->addDataPoint(rpm);
    }
}

void DashboardPage::onGpuUpdated(const QList<GpuDevice> &gpus)
{
    if (!mActive) return;
    for (DashboardTileWrapper *w : wrappersOfType("gpu")) {
        int idx = 0;
        for (int i = 0; i < gpus.size(); ++i)
            if (gpus.at(i).name == w->inputKey()) { idx = i; break; }
        if (idx < 0 || idx >= gpus.size()) continue;
        const GpuDevice &gpu = gpus.at(idx);
        auto *tile = qobject_cast<MetricTileBase*>(w->innerWidget());
        if (!tile) continue;
        tile->setSubtitle(gpu.name);
        if (gpu.utilization < 0) { tile->setValue(0, tr("N/A")); }
        else {
            int util = qBound(0, gpu.utilization, 100);
            tile->setValue(util, QString("%1%").arg(util));
            tile->addDataPoint(util);
        }
    }
}

void DashboardPage::onTileInputSelected(DashboardTileWrapper *wrapper, const QString &input)
{
    wrapper->setInputKey(input);
    auto *tile = qobject_cast<MetricTileBase*>(wrapper->innerWidget());
    if (tile) {
        // Subtitle = the human label for this input.
        QString label = input;
        if (wrapper->tileType() == "temp")
            for (const ThermalSensor &s : im->getThermalSensors())
                if (s.id == input) { label = s.label; break; }
        else if (wrapper->tileType() == "fan")
            for (const FanSensor &f : im->getFanSensors())
                if (f.id == input) { label = f.label; break; }
        tile->setSubtitle(label);
        tile->clearDataPoints();
        if (QToolButton *g = tile->gearButton()) {
            if (QMenu *m = g->menu())
                for (QAction *a : m->actions())
                    a->setChecked(a->data().toString() == input);
        }
    }
    // Push a fresh value immediately for the changed type.
    if (wrapper->tileType() == "temp") updateTempTile();
    else if (wrapper->tileType() == "fan") updateFanTile();
    persistLayout();
}

void DashboardPage::onBatteryUpdated(const BatteryData &bat)
{
    if (!mActive || !bat.hasBattery)
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

void DashboardPage::refreshDiskUsageTile(DashboardTileWrapper *w)
{
    if (mCachedDisks.isEmpty()) return;

    const Disk *disk = nullptr;
    for (const Disk &d : mCachedDisks)
        if (d.name.trimmed() == w->inputKey().trimmed()) { disk = &d; break; }
    if (!disk)
        disk = &mCachedDisks.at(0);

    int diskPercent = 0;
    if (disk->size > 0)
        diskPercent = ((double) disk->used / (double) disk->size) * 100.0;

    auto *tile = qobject_cast<MetricTileBase*>(w->innerWidget());
    if (!tile) return;
    tile->setInputName(w->inputKey().isEmpty() ? disk->name : w->inputKey());
    tile->setDiskInfo(diskPercent,
                      FormatUtil::formatBytes(disk->used),
                      FormatUtil::formatBytes(disk->size));
}

void DashboardPage::updateDiskHealthBadge()
{
    if (mCachedDriveHealth.isEmpty() || mCachedDisks.isEmpty())
        return;

    // GH#191: each disk tile's health badge reflects ITS bound disk.
    for (DashboardTileWrapper *w : wrappersOfType("disk")) {
        auto *tile = qobject_cast<MetricTileBase*>(w->innerWidget());
        if (!tile) continue;

        // Resolve this tile's bound disk (fallback: root volume, then first).
        const Disk *selectedDisk = nullptr;
        for (const Disk &d : mCachedDisks) {
            if (d.name.trimmed() == w->inputKey().trimmed()) {
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

        if (matched) {
            bool good = (matched->healthVerdict == "Good" || matched->smartPassed);
            tile->setDriveHealthSegment(matched->healthVerdict, good);
            // Req 4: identify the drive by the friendly input name; SMART model → tooltip.
            const QString friendly = w->inputKey().isEmpty() ? selectedDisk->name : w->inputKey();
            tile->setInputName(friendly, matched->model.isEmpty() ? matched->deviceName : matched->model);
        } else {
            tile->setDriveHealthSegment(QString(), true);
        }
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
        updateAddTileButton();
        // GH#191: re-validate the column count now that placeholders are shown,
        // so a stale count can't leave the grid wider/narrower than the
        // viewport. recomputeColumns()'s no-change guard makes this a no-op when
        // the width-derived count is unchanged (the common case), so it is not a
        // rebuild on every toggle and cannot recurse.
        recomputeColumns();
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
    // GH#191: re-validate the column count after hiding placeholders. No-op via
    // recomputeColumns()'s guard when the count is unchanged; recursion-safe.
    recomputeColumns();
    persistLayout();
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
        QString type = w->tileType();
        QString defStyle = defaultStyle(type);
        if (w->currentStyle() != defStyle && !availableStyles(type).isEmpty()) {
            MetricTileBase *newTile = createTile(type, defStyle);
            w->setInnerWidget(newTile);
            w->setCurrentStyle(defStyle);

            if (type == "cpu") mCpuTile = newTile;
            else if (type == "memory") mMemTile = newTile;
            else if (type == "battery") mBatteryTile = newTile;
            else if (type == "health") mHealthTile = qobject_cast<HealthScoreTile*>(newTile);

            setupTileGearMenu(w);

            w->clearCustomizationSection();
            setupCustomizationMenu(w, defStyle);
        }
    }

    mSettingManager->clearDashboardLayout();
    deserializeLayout(QString(QJsonDocument(defaultLayout()).toJson()));
    buildGrid();
    // The default layout is authored at kMaxCols; reflow it to the current
    // viewport width. Invalidate mVisibleCols first so recomputeColumns()'s
    // no-change guard can't skip the reflow when the width-derived count
    // happens to equal the pre-reset value. (GH#191)
    mVisibleCols = -1;
    recomputeColumns();
    updateAddTileButton();
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
        ui->systemSummary->hide();
        ui->statusFooter->hide();
    } else {
        mEditButton->show();
        mEditButton->raise();
        mKioskButton->raise();
        mEditShortcut->setEnabled(true);
        mKioskButton->setIcon(QIcon(":/static/themes/common/img/fullscreen.svg"));
        mKioskButton->setToolTip(tr("Enter Kiosk Mode (F11)"));
        applyFooterVisibility();
    }
}

void DashboardPage::applyFooterVisibility()
{
    bool visible = mSettingManager->getDashboardFooterVisible() && !mKioskMode;
    ui->systemSummary->setVisible(visible);
    ui->statusFooter->setVisible(visible);
}

DashboardTileWrapper *DashboardPage::wrapTile(const QString &uid, const QString &type,
                                              const QString &input, QWidget *tile)
{
    auto *wrapper = new DashboardTileWrapper(uid, type, input, tile, this);

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

    // Set up style menu for switchable tiles. Styles/customization are keyed by
    // the unique instance id (uid) but defaults are derived from the metric type.
    QStringList styles = availableStyles(type);
    if (!styles.isEmpty()) {
        QString currentStyle = mTileStyles.value(uid, defaultStyle(type));
        wrapper->setStyleMenuItems(styles, currentStyle);
    }

    setupCustomizationMenu(wrapper, mTileStyles.value(uid, defaultStyle(type)));

    mTileWrappers.append(wrapper);

    // GH#191: build this tile's per-instance input-selection gear menu.
    setupTileGearMenu(wrapper);

    return wrapper;
}

QJsonArray DashboardPage::defaultLayout() const
{
    QJsonArray arr;
    // GH#191: the grid is now 8x8; default tiles span 2x2 so the out-of-box
    // dashboard matches the legacy 4x4 single-cell layout. Positions are the
    // legacy row/col multiplied by 2.
    auto addEntry = [&](const QString &id, int row, int col) {
        QJsonObject obj;
        obj["id"] = id;
        obj["row"] = row;
        obj["col"] = col;
        obj["rowSpan"] = 2;
        obj["colSpan"] = 2;
        obj["style"] = defaultStyle(id);
        arr.append(obj);
    };

    addEntry("cpu",     0, 0);
    addEntry("memory",  0, 2);
    addEntry("disk",    0, 4);
    addEntry("network", 0, 6);

    addEntry("health",  2, 6);

    // Sensor/optional tiles fill the second visual row left-to-right, wrapping
    // to a third row, mirroring the legacy addSensor() behaviour (now ×2).
    int sRow = 2, sCol = 0;
    auto addSensor = [&](const QString &id) {
        if (sCol >= 6) { sRow = 4; sCol = 0; }
        addEntry(id, sRow, sCol);
        sCol += 2;
    };
    if (im->hasGpu())            addSensor("gpu");
    if (im->hasThermalSensors()) addSensor("temp");
    if (im->hasBattery())        addSensor("battery");
    if (im->hasFanSensors())     addSensor("fan");

    return arr;
}

void DashboardPage::deserializeLayout(const QString &json)
{
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());

    // Layouts persist as a v2 envelope {"version":N,"tiles":[...]}. A bare
    // array is a legacy v1 (4x4) layout with no version field. (GH#191)
    QJsonArray rawTiles;
    int version = 1;
    if (doc.isObject()) {
        QJsonObject env = doc.object();
        version = env.value("version").toInt(1);
        rawTiles = env.value("tiles").toArray();
    } else {
        rawTiles = doc.array();
    }

    QJsonArray arr = DashboardLayout::migrate(rawTiles, version);

    for (const QJsonValue &val : arr) {
        QJsonObject obj = val.toObject();
        QString type = obj["id"].toString();
        QString uid = obj.contains("uid") ? obj["uid"].toString() : type;
        QString input = obj["input"].toString();
        int col = qBound(0, obj["col"].toInt(), mVisibleCols - 1);
        int row = qMax(0, obj["row"].toInt());
        int rowSpan = qMax(1, obj["rowSpan"].toInt(1));
        int colSpan = qBound(1, obj["colSpan"].toInt(1), mVisibleCols - col);

        QString style = obj["style"].toString();
        if (!style.isEmpty())
            mTileStyles[uid] = style;

        bool visible = obj.contains("visible") ? obj["visible"].toBool(true) : true;
        if (!visible)
            mHiddenTiles.insert(uid);
        else
            mHiddenTiles.remove(uid);

        QString color = obj["color"].toString();
        if (color.startsWith("range::")) {
            QString rangeId = color.mid(7);
            if (!rangeId.isEmpty())
                mTileRanges[uid] = rangeId;
            else
                mTileRanges.remove(uid);
            mTileColors.remove(uid);
        } else {
            if (!color.isEmpty())
                mTileColors[uid] = color;
            else
                mTileColors.remove(uid);
            mTileRanges.remove(uid);
        }

        for (DashboardTileWrapper *w : mTileWrappers) {
            if (w->tileId() == uid) {
                w->setGridPosition(row, col, rowSpan, colSpan);
                if (!style.isEmpty())
                    w->setCurrentStyle(style);
                if (!input.isEmpty())
                    w->setInputKey(input);
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
        obj["id"] = w->tileType();
        obj["uid"] = w->tileId();
        if (!w->inputKey().isEmpty())
            obj["input"] = w->inputKey();
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

QJsonObject DashboardPage::layoutEnvelope() const
{
    QJsonObject env;
    env["version"] = DashboardLayout::kSchemaVersion;
    env["tiles"] = serializeLayout();
    return env;
}

void DashboardPage::persistLayout()
{
    mSettingManager->setDashboardLayout(
        QJsonDocument(layoutEnvelope()).toJson(QJsonDocument::Compact));
}

void DashboardPage::rebuildOccupancy()
{
    // Row count grows to fit the lowest-reaching visible tile (min 1 row).
    mRowCount = 0;
    for (const DashboardTileWrapper *w : mTileWrappers) {
        if (mHiddenTiles.contains(w->tileId())) continue;
        mRowCount = std::max(mRowCount, w->gridRow() + w->gridRowSpan());
    }
    mRowCount = std::max(mRowCount, 1);

    mOccupancy.assign(mRowCount, QVector<QString>(mVisibleCols));
    for (const DashboardTileWrapper *w : mTileWrappers) {
        if (mHiddenTiles.contains(w->tileId())) continue;
        for (int r = w->gridRow(); r < w->gridRow() + w->gridRowSpan(); ++r)
            for (int c = w->gridCol(); c < w->gridCol() + w->gridColSpan(); ++c)
                if (r < mRowCount && c < mVisibleCols)
                    mOccupancy[r][c] = w->tileId();
    }
}

bool DashboardPage::regionIsFree(int row, int col, int rowSpan, int colSpan,
                                  const QString &ignoreTileId) const
{
    if (col + colSpan > mVisibleCols || row < 0 || col < 0)
        return false;
    for (int r = row; r < row + rowSpan; ++r)
        for (int c = col; c < col + colSpan; ++c) {
            if (r >= mOccupancy.size() || c >= mVisibleCols) continue; // beyond current rows is free
            if (!mOccupancy[r][c].isEmpty() && mOccupancy[r][c] != ignoreTileId)
                return false;
        }
    return true;
}

void DashboardPage::recomputeColumns()
{
    // Don't reflow before the grid is actually on-screen: during init()/pre-show
    // the scroll area reports a tiny default width, which collapses the column
    // count to kMinCols and would repack the just-restored saved layout into a
    // few columns. Wait for a real shown size; showEvent/resizeEvent then run
    // recomputeColumns() again with the true viewport width. (GH#191)
    if (!mGridScroll || !mGridScroll->isVisible())
        return;

    // GH#191: derive the column count from a STABLE available width.
    //
    // viewport()->width() is unreliable here: during the page's resizeEvent the
    // scroll area's viewport may not have been re-laid-out yet (stale, too wide),
    // and it shrinks by ~15px the moment a vertical scrollbar appears. Either
    // makes the width-derived column count wrong and produces a scrollbar
    // cascade.
    //
    // Instead compute from the scroll area's own width (a direct layout child of
    // this page, so its width IS current in resizeEvent), minus the frame and a
    // RESERVED vertical-scrollbar width. Reserving the v-scrollbar width makes
    // the result identical whether or not the v-scrollbar is currently shown, so
    // the column count never oscillates. The grid min width
    // (mVisibleCols*kCellW + gaps) is then always <= availW <= true viewport
    // width, so with horizontal scrolling off the grid can never clip.
    int availW = width();
    if (mGridScroll) {
        int sbw = mGridScroll->verticalScrollBar()
                    ? mGridScroll->verticalScrollBar()->sizeHint().width()
                    : 0;
        int frame = 2 * mGridScroll->frameWidth();
        availW = mGridScroll->width() - frame - sbw;
    }
    int cols = DashboardLayout::columnsForWidth(qMax(0, availW));
    if (cols == mVisibleCols && !mOccupancy.isEmpty())
        return;
    mVisibleCols = cols;

    // Reflow current tiles into the new column count (pure logic), then apply
    // the repacked positions back to the wrappers.
    // GH#191: build the reflow input from each LIVE visible wrapper, but take
    // its position from the CANONICAL saved layout (not the live wrapper
    // position, which a transient narrow-width pass may have repacked). This
    // makes reflow idempotent w.r.t. width: a wide pass always re-derives the
    // saved arrangement instead of preserving a transient squash.
    QHash<QString, QJsonObject> canonByUid;
    {
        QString canon = mSettingManager->getDashboardLayout();
        if (!canon.isEmpty()) {
            QJsonDocument d = QJsonDocument::fromJson(canon.toUtf8());
            QJsonArray raw = d.isObject() ? d.object().value("tiles").toArray()
                                          : d.array();
            int ver = d.isObject() ? d.object().value("version").toInt(1) : 1;
            const QJsonArray canonTiles = DashboardLayout::migrate(raw, ver);
            for (const QJsonValue &cv : canonTiles) {
                QJsonObject co = cv.toObject();
                QString cuid = co.contains("uid") ? co["uid"].toString()
                                                  : co["id"].toString();
                canonByUid.insert(cuid, co);
            }
        }
    }

    QJsonArray visibleTiles;
    for (DashboardTileWrapper *w : mTileWrappers) {
        if (mHiddenTiles.contains(w->tileId()))
            continue;                                 // hidden tiles reserve no cell
        QJsonObject o;
        if (canonByUid.contains(w->tileId())) {
            o = canonByUid.value(w->tileId());        // canonical saved position
        } else {
            o["uid"] = w->tileId();                   // not in saved layout: use live pos
            o["row"] = w->gridRow();
            o["col"] = w->gridCol();
            o["rowSpan"] = w->gridRowSpan();
            o["colSpan"] = w->gridColSpan();
        }
        visibleTiles.append(o);
    }
    QJsonArray packed = DashboardLayout::reflowPreserve(visibleTiles, mVisibleCols);
    for (const QJsonValue &v : packed) {
        QJsonObject o = v.toObject();
        QString uid = o.contains("uid") ? o["uid"].toString() : o["id"].toString();
        for (DashboardTileWrapper *w : mTileWrappers)
            if (w->tileId() == uid) {
                w->setGridPosition(o["row"].toInt(), o["col"].toInt(),
                                   o["rowSpan"].toInt(1), o["colSpan"].toInt(1));
                break;
            }
    }
    buildGrid();
    // GH#191: do NOT persistLayout() here. A responsive column change is a VIEW
    // concern; reflowPreserve keeps saved positions when the width allows and
    // repacks only overflowing tiles for display. Persisting would overwrite the
    // user's canonical saved arrangement with this auto-packed view on every
    // launch/resize. Saves happen only on explicit edits (drag/resize/Done).
}

void DashboardPage::buildGrid()
{
    while (ui->bentoGrid->count() > 0) {
        QLayoutItem *item = ui->bentoGrid->takeAt(0);
        if (item->widget()) item->widget()->setParent(nullptr);
        delete item;
    }
    qDeleteAll(mPlaceholders);
    mPlaceholders.clear();

    rebuildOccupancy();   // sizes mOccupancy to mRowCount x mVisibleCols

    for (DashboardTileWrapper *w : mTileWrappers) {
        if (mHiddenTiles.contains(w->tileId())) { w->hide(); continue; }
        w->setParent(mGridContainer);
        w->setFixedSize(w->gridColSpan() * DashboardLayout::kCellW + (w->gridColSpan() - 1) * DashboardLayout::kGap,
                        w->gridRowSpan() * DashboardLayout::kCellH + (w->gridRowSpan() - 1) * DashboardLayout::kGap);
        ui->bentoGrid->addWidget(w, w->gridRow(), w->gridCol(), w->gridRowSpan(), w->gridColSpan());
        applyDisplayModeForSpan(w);
        w->show();
    }

    // Edit-mode placeholders for every empty cell (fixed-size too).
    for (int r = 0; r < mRowCount; ++r)
        for (int c = 0; c < mVisibleCols; ++c)
            if (mOccupancy[r][c].isEmpty()) {
                auto *ph = new QWidget(mGridContainer);
                ph->setObjectName("dashPlaceholder");
                ph->setFixedSize(DashboardLayout::kCellW, DashboardLayout::kCellH);
                ph->setVisible(mEditMode);
                ui->bentoGrid->addWidget(ph, r, c);
                mPlaceholders.append(ph);
            }

    // Reset stale row/column constraints from any previous (larger) build —
    // QGridLayout never shrinks its row/column count, and per-index minimum
    // sizes/stretches persist, so old empty rows/cols would otherwise keep
    // reserving space and trigger spurious scrollbars. (GH#191)
    {
        int prevRows = ui->bentoGrid->rowCount();
        int prevCols = ui->bentoGrid->columnCount();
        for (int r = 0; r < prevRows; ++r) {
            ui->bentoGrid->setRowMinimumHeight(r, 0);
            ui->bentoGrid->setRowStretch(r, 0);
        }
        for (int c = 0; c < prevCols; ++c) {
            ui->bentoGrid->setColumnMinimumWidth(c, 0);
            ui->bentoGrid->setColumnStretch(c, 0);
        }
    }

    // Fixed cell pitch: each used column/row gets the exact cell size; a
    // trailing stretch column/row absorbs extra space so tiles pack top-left.
    ui->bentoGrid->setHorizontalSpacing(DashboardLayout::kGap);
    ui->bentoGrid->setVerticalSpacing(DashboardLayout::kGap);
    for (int c = 0; c < mVisibleCols; ++c) {
        ui->bentoGrid->setColumnMinimumWidth(c, DashboardLayout::kCellW);
        ui->bentoGrid->setColumnStretch(c, 0);
    }
    for (int r = 0; r < mRowCount; ++r) {
        ui->bentoGrid->setRowMinimumHeight(r, DashboardLayout::kCellH);
        ui->bentoGrid->setRowStretch(r, 0);
    }
    ui->bentoGrid->setColumnStretch(mVisibleCols, 1);   // trailing spacer col
    ui->bentoGrid->setRowStretch(mRowCount, 1);          // trailing spacer row

    mEditButton->raise();
    mKioskButton->raise();
}

void DashboardPage::applyDisplayModeForSpan(DashboardTileWrapper *wrapper)
{
    int area = wrapper->gridRowSpan() * wrapper->gridColSpan();
    const bool compact = (DashboardLayout::tierForArea(area) == DashboardLayout::Compact);

    // NetworkTile is a plain QWidget (not a MetricTileBase), so it sits
    // outside the DisplayMode system; toggle its sparklines directly. (GH#191)
    if (auto *net = qobject_cast<NetworkTile*>(wrapper->innerWidget())) {
        net->setCompact(compact);
        return;
    }

    auto *metric = qobject_cast<MetricTileBase*>(wrapper->innerWidget());
    if (!metric)
        return;

    switch (DashboardLayout::tierForArea(area)) {
    case DashboardLayout::Hero:
        metric->setDisplayMode(MetricTileBase::Hero);
        break;
    case DashboardLayout::Large:
        metric->setDisplayMode(MetricTileBase::Large);
        break;
    case DashboardLayout::Normal:
        metric->setDisplayMode(MetricTileBase::Normal);
        break;
    case DashboardLayout::Compact:
        metric->setDisplayMode(MetricTileBase::Compact);
        break;
    }
}

void DashboardPage::rebuildLayout()
{
    QString saved = mSettingManager->getDashboardLayout();
    if (saved.isEmpty())
        deserializeLayout(QString(QJsonDocument(defaultLayout()).toJson()));
    else
        deserializeLayout(saved);
    buildGrid();
    // Reflow the rebuilt layout to the current viewport width. Invalidate
    // mVisibleCols first so the no-change guard can't skip the reflow. (GH#191)
    mVisibleCols = -1;
    recomputeColumns();
}

bool DashboardPage::gridCellAtPos(const QPoint &globalPos, int &outRow, int &outCol) const
{
    if (!mGridContainer) return false;
    QPoint local = mGridContainer->mapFromGlobal(globalPos);
    if (local.x() < 0 || local.y() < 0) return false;

    int pitchX = DashboardLayout::kCellW + DashboardLayout::kGap;
    int pitchY = DashboardLayout::kCellH + DashboardLayout::kGap;
    int col = local.x() / pitchX;
    int row = local.y() / pitchY;
    if (col < 0 || col >= mVisibleCols) return false;
    outCol = col;
    outRow = std::max(0, row);
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

    int pitchX = DashboardLayout::kCellW + DashboardLayout::kGap;
    int pitchY = DashboardLayout::kCellH + DashboardLayout::kGap;

    // GH#191: size the drop preview to the dragged tile's FULL footprint
    // (rowSpan x colSpan), anchored at the drop cell, so the operator sees
    // exactly how much space the tile will occupy — not just a single cell.
    // onTileDragFinished() places the tile's top-left at (targetRow,targetCol),
    // so the highlight matches the actual drop.
    int rowSpan = mDragSource ? mDragSource->gridRowSpan() : 1;
    int colSpan = mDragSource ? mDragSource->gridColSpan() : 1;
    int indW = colSpan * DashboardLayout::kCellW + (colSpan - 1) * DashboardLayout::kGap;
    int indH = rowSpan * DashboardLayout::kCellH + (rowSpan - 1) * DashboardLayout::kGap;

    QPoint topLeftInContainer(targetCol * pitchX, targetRow * pitchY);
    QPoint global = mGridContainer->mapToGlobal(topLeftInContainer);
    QPoint inPage = mapFromGlobal(global);
    mDragIndicator->setGeometry(inPage.x(), inPage.y(), indW, indH);
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

    bool occupantEmpty = (targetRow >= mOccupancy.size())
                           ? true
                           : mOccupancy[targetRow][targetCol].isEmpty();
    if (occupantEmpty) {
        int srcRS = mDragSource->gridRowSpan();
        int srcCS = mDragSource->gridColSpan();
        if (regionIsFree(targetRow, targetCol, srcRS, srcCS, mDragSource->tileId())) {
            mDragSource->setGridPosition(targetRow, targetCol, srcRS, srcCS);
            buildGrid();
            persistLayout();   // GH#191: persist the move immediately
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
                persistLayout();   // GH#191: persist the swap immediately
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
    persistLayout();   // GH#191: persist the resize immediately
}

void DashboardPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    mKioskButton->move(width() - mKioskButton->width() - 10, 10);
    mEditButton->move(width() - mKioskButton->width() - mEditButton->width() - 18, 10);
    recomputeColumns();
}

void DashboardPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // GH#191: recompute once the page has real geometry. The init()/resizeEvent
    // calls may run before the scroll area has its settled size; this guarantees
    // the first correct column count is applied as soon as the page is shown.
    recomputeColumns();
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
    else if (id == "fan")     { title = tr("FANS");    colorToken = "@fanColor"; }
    else if (id == "health")  { title = tr("HEALTH");  colorToken = "@healthScoreColor"; }
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
    if (style == "health")
        return new HealthScoreTile(colorToken, this);

    return new MetricTile(title, colorToken, this);
}

QStringList DashboardPage::availableStyles(const QString &tileId) const
{
    if (tileId == "network")
        return {};
    if (tileId == "health")
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
    if (tileId == "health")
        return "health";
    return "sparkline";
}

void DashboardPage::setupTileGearMenu(DashboardTileWrapper *wrapper)
{
    auto *tile = qobject_cast<MetricTileBase*>(wrapper->innerWidget());
    QToolButton *gear = tile ? tile->gearButton() : nullptr;
    QString type = wrapper->tileType();

    if (!gear || !DashboardLayout::isMultiInstanceType(type)) {
        if (tile) tile->setGearVisible(false);
        return;
    }

    QList<QPair<QString,QString>> inputs; // label, key
    if (type == "temp")
        for (const ThermalSensor &s : im->getThermalSensors()) inputs.append({s.label, s.id});
    else if (type == "fan")
        for (const FanSensor &f : im->getFanSensors()) inputs.append({f.label, f.id});
    else if (type == "gpu")
        for (const GpuDevice &g : im->getGpuDevices()) inputs.append({g.name, g.name});
    else if (type == "disk")
        for (const Disk &d : im->getDisks()) inputs.append({d.name, d.name});
    // GH#191: network per-interface enumeration arrives in a later task; its
    // gear stays hidden for now.

    auto *menu = new QMenu(wrapper);
    for (const auto &p : inputs) {
        QAction *a = menu->addAction(p.first);
        a->setCheckable(true);
        a->setData(p.second);
        a->setChecked(p.second == wrapper->inputKey());
    }
    connect(menu, &QMenu::triggered, this, [this, wrapper](QAction *a) {
        onTileInputSelected(wrapper, a->data().toString());
    });
    gear->setMenu(menu);
    gear->setPopupMode(QToolButton::InstantPopup);
    tile->setGearVisible(inputs.size() >= 2);
}

void DashboardPage::migrateLegacyBindings()
{
    auto bindFirst = [&](const QString &type, const QString &savedId,
                         std::function<QStringList()> allIds) {
        for (DashboardTileWrapper *w : wrappersOfType(type)) {
            if (!w->inputKey().isEmpty()) continue;
            QStringList ids = allIds();
            QString chosen = (!savedId.isEmpty() && ids.contains(savedId))
                                ? savedId
                                : (ids.isEmpty() ? QString() : ids.first());
            w->setInputKey(chosen);
        }
    };
    bindFirst("temp", mSettingManager->getTempSensorId(), [&]{
        QStringList v; for (const ThermalSensor &s : im->getThermalSensors()) v << s.id; return v; });
    bindFirst("fan", mSettingManager->getFanSensorId(), [&]{
        QStringList v; for (const FanSensor &f : im->getFanSensors()) v << f.id; return v; });
    bindFirst("gpu", mSettingManager->getGpuDeviceId(), [&]{
        QStringList v; for (const GpuDevice &g : im->getGpuDevices()) v << g.name; return v; });
    bindFirst("disk", mSettingManager->getDiskName(), [&]{
        QStringList v; for (const Disk &d : im->getDisks()) v << d.name; return v; });
    bindFirst("network", QString(), [&]{
        QString def = im->getDefaultNetworkInterface();
        if (!def.isEmpty())
            return QStringList{def};
        return im->getNetworkInterfaceNames(); });
}

DashboardTileWrapper *DashboardPage::findWrapper(const QString &tileId) const
{
    for (DashboardTileWrapper *w : mTileWrappers)
        if (w->tileId() == tileId)
            return w;
    return nullptr;
}

QList<DashboardTileWrapper*> DashboardPage::wrappersOfType(const QString &type) const
{
    QList<DashboardTileWrapper*> out;
    for (DashboardTileWrapper *w : mTileWrappers)
        if (w->tileType() == type)
            out.append(w);
    return out;
}

void DashboardPage::onTileStyleChangeRequested(DashboardTileWrapper *wrapper, const QString &style)
{
    QString uid = wrapper->tileId();
    QString type = wrapper->tileType();

    if (wrapper->currentStyle() == style)
        return;

    MetricTileBase *newTile = createTile(type, style);

    // Swap inner widget (old tile scheduled for deletion)
    wrapper->setInnerWidget(newTile);
    wrapper->setCurrentStyle(style);

    // Update member pointer (singleton tiles only; multi-instance types are
    // driven per-wrapper and have no member). (GH#191)
    if (type == "cpu")          mCpuTile = newTile;
    else if (type == "memory")  mMemTile = newTile;
    else if (type == "battery") mBatteryTile = newTile;
    else if (type == "health")  mHealthTile = qobject_cast<HealthScoreTile*>(newTile);

    // Re-attach gear menu (per-tile input binding)
    setupTileGearMenu(wrapper);

    // Re-apply display mode
    applyDisplayModeForSpan(wrapper);

    // Re-apply disk usage + health badges immediately from cache so the new
    // tile doesn't show stale zeros until the next poll tick.
    if (type == "disk") {
        refreshDiskUsageTile(wrapper);
        updateDiskHealthBadge();
    }

    // Rebuild the customization menu (color swatches vs. range presets) for the new style
    wrapper->clearCustomizationSection();
    setupCustomizationMenu(wrapper, style);

    // Re-apply saved customization
    if (tileUsesRangeMenu(style)) {
        if (mTileRanges.contains(uid)) {
            newTile->setColorRange(mTileRanges[uid]);
            wrapper->setCurrentRange(mTileRanges[uid]);
        }
    } else {
        if (mTileColors.contains(uid))
            newTile->setColorOverride(mTileColors[uid]);
    }

    // Store style
    mTileStyles[uid] = style;

    persistLayout();   // GH#191: persist the style change immediately
}

void DashboardPage::onTileRemoveRequested(DashboardTileWrapper *wrapper)
{
    mHiddenTiles.insert(wrapper->tileId());
    buildGrid();
    updateAddTileButton();
}

void DashboardPage::updateAddTileButton()
{
    if (mAddTileButton)
        mAddTileButton->setVisible(mEditMode);
}

void DashboardPage::onAddTileClicked()
{
    rebuildOccupancy();

    // Build the available-inputs map (detected minus already-placed). GH#191:
    // count only VISIBLE wrappers' inputs as used — a removed (hidden) tile must
    // NOT reserve its input, so its input becomes available to re-add again.
    QHash<QString, QList<QPair<QString, QString>>> avail;
    auto usedInputs = [&](const QString &type) {
        QStringList used;
        for (DashboardTileWrapper *w : wrappersOfType(type))
            if (!mHiddenTiles.contains(w->tileId()) && !w->inputKey().isEmpty())
                used.append(w->inputKey());
        return used;
    };
    auto addAvail = [&](const QString &type, const QList<QPair<QString, QString>> &all) {
        QStringList used = usedInputs(type);
        QList<QPair<QString, QString>> free;
        for (const auto &p : all)
            if (!used.contains(p.first))
                free.append(p);
        avail[type] = free;
    };
    { QList<QPair<QString, QString>> v; for (const ThermalSensor &s : im->getThermalSensors()) v.append({s.id, s.label}); addAvail("temp", v); }
    { QList<QPair<QString, QString>> v; for (const FanSensor &f : im->getFanSensors()) v.append({f.id, f.label}); addAvail("fan", v); }
    { QList<QPair<QString, QString>> v; for (const GpuDevice &g : im->getGpuDevices()) v.append({g.name, g.name}); addAvail("gpu", v); }
    { QList<QPair<QString, QString>> v; for (const Disk &d : im->getDisks()) v.append({d.name, d.name}); addAvail("disk", v); }
    {
        // GH#191: build a friendly LABEL (display name + iface + IP) while
        // keeping the KEY = the raw interface name (the dialog binds on the
        // key and addAvail filters used inputs by key, so both still work).
        QList<QPair<QString, QString>> v;
        for (const QString &n : im->getNetworkInterfaceNames()) {
            QString friendly = im->getNetworkInterfaceDisplayName(n);
            QString ip;
            const QNetworkInterface iface = QNetworkInterface::interfaceFromName(n);
            for (const QNetworkAddressEntry &e : iface.addressEntries()) {
                const QHostAddress a = e.ip();
                if (!a.isNull() && !a.isLinkLocal() && !a.isLoopback()) { ip = a.toString(); break; }
            }
            QString label = friendly.isEmpty() ? n : QStringLiteral("%1 (%2)").arg(friendly, n);
            if (!ip.isEmpty()) label += QStringLiteral(" — %1").arg(ip);
            v.append({n, label});
        }
        addAvail("network", v);
    }

    // GH#191: a multi-instance type is offered in the type list whenever its
    // hardware EXISTS on the system (>=1 detected input), regardless of whether
    // inputs are already placed. The input list passed to the dialog still
    // carries only the UNUSED (avail) inputs, so a fully-placed type shows up
    // but with an empty input list (the dialog disables OK). Singletons are
    // offered only when not currently shown (hidden or no wrapper).
    auto hardwareExists = [&](const QString &type) {
        if (type == "temp")    return !im->getThermalSensors().isEmpty();
        if (type == "fan")     return !im->getFanSensors().isEmpty();
        if (type == "disk")    return !im->getDisks().isEmpty();
        if (type == "gpu")     return !im->getGpuDevices().isEmpty();
        if (type == "network") return !im->getNetworkInterfaceNames().isEmpty();
        return false;
    };
    static const QList<QPair<QString, QString>> kTypes = {
        {"cpu", tr("CPU Usage")}, {"memory", tr("Memory Usage")}, {"disk", tr("Disk Usage")},
        {"network", tr("Network Speed")}, {"gpu", tr("GPU Usage")}, {"temp", tr("Temperature")},
        {"battery", tr("Battery")}, {"fan", tr("Fan Speed")}, {"health", tr("Health Score")},
    };
    QList<QPair<QString, QString>> typeOptions;
    for (const auto &t : kTypes) {
        if (DashboardLayout::isMultiInstanceType(t.first)) {
            if (hardwareExists(t.first))
                typeOptions.append(t);
        } else if (mHiddenTiles.contains(t.first) || wrappersOfType(t.first).isEmpty()) {
            typeOptions.append(t); // singleton not currently shown
        }
    }
    if (typeOptions.isEmpty())
        return;

    AddTileDialog dlg(typeOptions, avail, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    QString type = dlg.chosenType();
    QString input = dlg.chosenInput();
    if (type.isEmpty())
        return;
    // GH#191: defensive guard — input-bound types require a chosen input. OK is
    // already disabled in the dialog when none is available, but never create a
    // tile bound to nothing.
    if (DashboardLayout::isMultiInstanceType(type) && input.isEmpty())
        return;

    // Find the first free 2x2 region in the A2 dynamic grid: columns are
    // mVisibleCols, rows are unbounded (rows beyond the current grid count as
    // free, so an empty row always fits and the scan always terminates).
    int row = -1, col = -1, rs = 2, cs = 2;
    if (mVisibleCols >= 2) {
        for (int r = 0; row == -1; ++r)
            for (int c = 0; c + 2 <= mVisibleCols; ++c)
                if (regionIsFree(r, c, 2, 2)) { row = r; col = c; break; }
    } else {
        rs = cs = 1;
        for (int r = 0; row == -1; ++r)
            for (int c = 0; c < mVisibleCols; ++c)
                if (regionIsFree(r, c, 1, 1)) { row = r; col = c; break; }
    }
    if (row == -1)
        return; // unreachable (empty rows are always free), but stay defensive

    if (!DashboardLayout::isMultiInstanceType(type)) {
        // Singleton re-show: reposition its existing (hidden) wrapper.
        mHiddenTiles.remove(type);
        DashboardTileWrapper *w = findWrapper(type);
        if (w)
            w->setGridPosition(row, col, rs, cs);
    } else {
        // GH#191: re-use a removed (hidden) tile of this type+input rather than
        // creating a duplicate/orphan — re-adding a removed GPU/disk/etc. just
        // brings it back.
        DashboardTileWrapper *reuse = nullptr;
        for (DashboardTileWrapper *w : wrappersOfType(type)) {
            if (mHiddenTiles.contains(w->tileId()) && w->inputKey() == input) { reuse = w; break; }
        }
        if (reuse) {
            mHiddenTiles.remove(reuse->tileId());
            reuse->setGridPosition(row, col, rs, cs);
            reuse->setEditMode(mEditMode);
        } else {
            // New input-bound instance.
            QStringList uids;
            for (DashboardTileWrapper *w : mTileWrappers)
                uids << w->tileId();
            QString uid = DashboardLayout::makeUid(uids, type);
            QString style = defaultStyle(type);
            QWidget *tile = (type == "network")
                                ? static_cast<QWidget *>(new NetworkTile("@networkColor", this))
                                : static_cast<QWidget *>(createTile(type, style));
            mTileStyles[uid] = style;
            DashboardTileWrapper *w = wrapTile(uid, type, input, tile);
            w->setGridPosition(row, col, rs, cs);
            w->setEditMode(mEditMode);
            // Populate subtitle + initial value for metric-based tiles. Network
            // tiles refresh on the next tick.
            if (qobject_cast<MetricTileBase *>(tile))
                onTileInputSelected(w, input);
        }
    }

    buildGrid();
    updateAddTileButton();
    persistLayout();
}

void DashboardPage::onTileColorChangeRequested(DashboardTileWrapper *wrapper, const QString &hexColor)
{
    QString id = wrapper->tileId();

    if (hexColor.isEmpty())
        mTileColors.remove(id);
    else
        mTileColors[id] = hexColor;

    mTileRanges.remove(id);

    if (auto *net = qobject_cast<NetworkTile*>(wrapper->innerWidget())) {
        net->setColorOverride(hexColor);
    } else if (auto *metric = qobject_cast<MetricTileBase*>(wrapper->innerWidget())) {
        metric->setColorRange(QString());
        metric->setColorOverride(hexColor);
    }

    wrapper->setCurrentColor(hexColor);

    persistLayout();
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

    persistLayout();
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
    } else {
        QSettings *sv = mAppManager->getStyleValues();
        QStringList colorPalette;
        if (sv) {
            for (const QString &t : {"@cpuColor", "@memoryColor", "@diskColor", "@networkColor",
                                      "@gpuColor", "@tempColor", "@batteryColor"})
                colorPalette << sv->value(t).toString();
        } else {
            colorPalette << "#FF6B1A" << "#FFB347" << "#E05454" << "#26A69A"
                         << "#813D9C" << "#5B9BD5" << "#2EC27E";
        }
        colorPalette << "#E91E63" << "#00BCD4" << "#8BC34A" << "#FF5722"
                     << "#607D8B" << "#9C27B0" << "#FFEB3B" << "#795548" << "#F48FB1";
        wrapper->setColorMenuItems(colorPalette, mTileColors.value(id));
    }
}

void DashboardPage::onHealthCpuUpdated(const QList<int> &percents, double clockGHz,
                                        const QList<double> &loadAvgs)
{
    if (!mActive) return;
    Q_UNUSED(percents)
    Q_UNUSED(clockGHz)
    int coreCount = im->getCpuCoreCount();
    double load1m = loadAvgs.isEmpty() ? 0.0 : loadAvgs.first();
    int score = 100;
    if (coreCount > 0 && load1m > 0) {
        double ratio = load1m / coreCount;
        score = qBound(0, qRound(100.0 * (1.0 - ratio)), 100);
    }
    mHealthTile->calculator()->setCpuScore(score);
    mHealthTile->recalculate();
}

void DashboardPage::onHealthMemoryUpdated(const MemorySnapshot &snap)
{
    if (!mActive) return;
    int score = 100;
    if (snap.total > 0)
        score = qBound(0, 100 - (int)(100.0 * snap.used / snap.total), 100);
    mHealthTile->calculator()->setMemoryScore(score);
    mHealthTile->recalculate();
}

void DashboardPage::onHealthDiskUpdated(const QList<Disk> &disks)
{
    if (!mActive) return;
    qint64 totalSize = 0;
    double weightedScore = 0.0;
    for (const Disk &d : disks) {
        if (d.size == 0) continue;
        int usedPercent = (int)(100.0 * d.used / d.size);
        int diskScore = qBound(0, 100 - usedPercent, 100);
        weightedScore += (double)diskScore * d.size;
        totalSize += d.size;
    }
    int score = (totalSize > 0) ? qBound(0, (int)qRound(weightedScore / totalSize), 100) : 100;
    mHealthTile->calculator()->setDiskScore(score);
    mHealthTile->recalculate();
}

void DashboardPage::onHealthTempUpdated()
{
    if (!mActive) return;
    // GH#191: the health score tracks the first temp tile's bound sensor (or the
    // first detected sensor when unbound).
    int sensorIdx = 0;
    QList<DashboardTileWrapper*> tempWrappers = wrappersOfType("temp");
    if (!tempWrappers.isEmpty()) {
        QList<ThermalSensor> sensors = im->getThermalSensors();
        const QString key = tempWrappers.first()->inputKey();
        for (int i = 0; i < sensors.size(); ++i)
            if (sensors.at(i).id == key) { sensorIdx = i; break; }
    }
    double tempC = im->getThermalTemperature(sensorIdx);
    int score = 100;
    if (tempC >= 100.0) score = 0;
    else if (tempC > 60.0) score = qRound(100.0 * (100.0 - tempC) / 40.0);
    mHealthTile->calculator()->setTempScore(score);
    mHealthTile->recalculate();
}

void DashboardPage::onHealthBatteryUpdated(const BatteryData &bat)
{
    if (!mActive) return;
    mHealthTile->calculator()->setBatteryScore(qBound(0, bat.healthPercent, 100));
    mHealthTile->recalculate();
}

void DashboardPage::onHealthDiskHealthUpdated(const QList<DriveHealth> &drives)
{
    // FR-96: disk discovery is now async, so hasDiskHealth() is false at
    // construction and smart-component availability was wrong. Re-set here
    // on first data arrival — idempotent on subsequent ticks.
    mHealthTile->calculator()->setComponentAvailable("smart", !drives.isEmpty());

    if (!mActive) return;
    int worstScore = 100;
    for (const DriveHealth &d : drives) {
        if (!d.smartPassed) worstScore = qMin(worstScore, 0);
        else if (d.healthPercent >= 0 && d.healthPercent < 50) worstScore = qMin(worstScore, 50);
        else if (d.healthPercent >= 0) worstScore = qMin(worstScore, d.healthPercent);
    }
    mHealthTile->calculator()->setSmartScore(worstScore);
    mHealthTile->recalculate();
}

void DashboardPage::onPageActivated()
{
    mActive = true;

    // FR-103: subscribe to the signals the dashboard renders. When the
    // dashboard isn't the current page, DataRefreshService stops sampling
    // these (notably nvidia-smi and QStorageInfo walks).
    mRefresh->subscribe(DataRefreshService::Signal::Cpu);
    mRefresh->subscribe(DataRefreshService::Signal::Memory);
    mRefresh->subscribe(DataRefreshService::Signal::Network);
    mRefresh->subscribe(DataRefreshService::Signal::DiskUsage);
    mRefresh->subscribe(DataRefreshService::Signal::Gpu);
    mRefresh->subscribe(DataRefreshService::Signal::Temp);
    mRefresh->subscribe(DataRefreshService::Signal::Fan);
    mRefresh->subscribe(DataRefreshService::Signal::Battery);
}

void DashboardPage::onPageDeactivated()
{
    mActive = false;
    mNetLastBytes.clear();

    mRefresh->unsubscribe(DataRefreshService::Signal::Cpu);
    mRefresh->unsubscribe(DataRefreshService::Signal::Memory);
    mRefresh->unsubscribe(DataRefreshService::Signal::Network);
    mRefresh->unsubscribe(DataRefreshService::Signal::DiskUsage);
    mRefresh->unsubscribe(DataRefreshService::Signal::Gpu);
    mRefresh->unsubscribe(DataRefreshService::Signal::Temp);
    mRefresh->unsubscribe(DataRefreshService::Signal::Fan);
    mRefresh->unsubscribe(DataRefreshService::Signal::Battery);
}

void DashboardPage::launchMaintenanceWizard()
{
    auto *wizard = new MaintenanceWizardDialog(this, mAppManager, im,
                                                ToolManager::ins(), mSignalMapper);
    wizard->setAttribute(Qt::WA_DeleteOnClose);
    wizard->runChecks();
    wizard->exec();
}
