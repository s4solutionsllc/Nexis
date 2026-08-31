#include "app.h"
#include "Managers/tool_manager.h"
#include "ui_app.h"
#include "Pages/Network/net_usage_tracker.h"
#include "Pages/Resources/disk_treemap_dialog.h"
#include "utilities.h"
#include "signal_mapper.h"
#include "nexis_page.h"
#include "dpi.h"
#include <Managers/cleaner_service.h>
#include <Managers/data_refresh_service.h>
#include "Info/update_info.h"
#include <Utils/format_util.h>
#ifdef Q_OS_LINUX
#include "Pages/Helpers/cpu_tuning_widget.h"
#endif
#ifdef Q_OS_MAC
#include <Tools/mac_tweaks_catalog.h>
#endif
#include <Managers/info_manager.h>
#include <Managers/tray_menu_model.h>
#include <Info/power_profile_info.h>
#include <QStyle>
#include <QDebug>
#include <QScreen>
#include <QGuiApplication>
#include <QIcon>
#include <QEvent>
#include <QWindow>
#include <QThreadPool>
#include <QLabel>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QTimer>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>

#ifdef Q_OS_MAC
#include "macos_dock_helper.h"
#endif

App::~App()
{
    DataRefreshService::ins()->stop();
    disconnect(DataRefreshService::ins(), nullptr, this, nullptr);
    disconnect(CleanerService::ins(), nullptr, this, nullptr);
    disconnect(SignalMapper::ins(), nullptr, this, nullptr);
    QThreadPool::globalInstance()->waitForDone(1000);
    QApplication::processEvents();
    delete ui;
}

App::App(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::App),
    mSlidingStacked(new SlidingStackedWidget(this)),
    mKioskMode(false),
    mSidebarCollapsed(false),
    mPreKioskCollapsed(false),
    mTrayIcon(AppManager::ins()->getTrayIcon()),
    mTrayMenu(new QMenu(this)),
    mSidebarLayout(nullptr),
    mBtnSidebarToggle(nullptr),
    mSidebarBtnGroup(new QButtonGroup(this))
{
    ui->setupUi(this);

    mSidebarBtnGroup->setExclusive(true);

    init();
}

QPushButton *App::createSidebarButton(const QString &tooltip)
{
    auto *btn = new QPushButton(ui->sidebar);
    btn->setToolTip(tooltip);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setCheckable(true);
    btn->setIconSize(Dpi::scale(20, 20));
    mSidebarBtnGroup->addButton(btn);
    return btn;
}

QPushButton *App::createSectionToggle(const QString &text)
{
    auto *btn = new QPushButton(text, ui->sidebar);
    btn->setObjectName("sectionToggle");
    btn->setCursor(Qt::PointingHandCursor);
    btn->setCheckable(false);
    btn->setIconSize(Dpi::scale(12, 12));
    btn->setLayoutDirection(Qt::RightToLeft);
    return btn;
}

void App::buildSidebar()
{
    mSidebarLayout = new QVBoxLayout(ui->sidebar);
    mSidebarLayout->setContentsMargins(0, 8, 0, 8);
    mSidebarLayout->setSpacing(0);

    // Logo + collapse toggle row
    auto *logoRow = new QHBoxLayout();
    logoRow->setContentsMargins(12, 4, 8, 4);
    logoRow->setSpacing(0);

    mLogoLabel = new QLabel(ui->sidebar);
    mLogoLabel->setObjectName("sidebarLogo");
    mLogoLabel->setFixedHeight(Dpi::scale(28));
    mLogoLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    logoRow->addWidget(mLogoLabel);
    logoRow->addStretch();

    mBtnSidebarToggle = new QToolButton(ui->sidebar);
    mBtnSidebarToggle->setObjectName("btnSidebarToggle");
    mBtnSidebarToggle->setCursor(Qt::PointingHandCursor);
    mBtnSidebarToggle->setCheckable(false);
    mBtnSidebarToggle->setAutoRaise(true);
    mBtnSidebarToggle->setIconSize(Dpi::scale(16, 16));
    mBtnSidebarToggle->setFixedSize(Dpi::scale(28, 28));
    connect(mBtnSidebarToggle, &QToolButton::clicked, this, &App::toggleSidebarCollapse);
    logoRow->addWidget(mBtnSidebarToggle);

    mSidebarLayout->addLayout(logoRow);

    // Logo separator line (#6)
    mLogoSeparator = new QFrame(ui->sidebar);
    mLogoSeparator->setObjectName("sidebarDividerLine");
    mLogoSeparator->setFrameShape(QFrame::HLine);
    mLogoSeparator->setFixedHeight(1);
    mSidebarLayout->addWidget(mLogoSeparator);

    // Scrollable nav area — contains all section headers and buttons.
    // Logo row and footer (version + feedback) remain pinned outside the scroll area.
    mNavScrollArea = new QScrollArea(ui->sidebar);
    mNavScrollArea->setObjectName("sidebarScrollArea");
    mNavScrollArea->setFrameShape(QFrame::NoFrame);
    mNavScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mNavScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    mNavScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *navContainer = new QWidget();
    navContainer->setObjectName("sidebarNavContainer");
    auto *navLayout = new QVBoxLayout(navContainer);
    navLayout->setContentsMargins(0, 4, 0, 0);
    navLayout->setSpacing(0);

    mNavScrollArea->setWidget(navContainer);
    mNavScrollArea->setWidgetResizable(true);
    mSidebarLayout->addWidget(mNavScrollArea);

    // Helper lambda to create a section with header, indicator, and container.
    // Pass headerless=true to omit the toggle header and separator (always-visible section).
    auto addSection = [&](const QString &name, bool headerless = false) -> SidebarSection & {
        SidebarSection section;
        section.name = name;
        section.collapsed = false;
        section.headerless = headerless;

        if (!headerless) {
            section.header = createSectionToggle(name);
            navLayout->addWidget(section.header);

            auto *indicator = new QFrame(navContainer);
            indicator->setObjectName("sidebarSectionIndicator");
            indicator->setFrameShape(QFrame::HLine);
            indicator->setFixedHeight(1);
            indicator->hide();
            mSectionIndicators.append(indicator);
            navLayout->addWidget(indicator);
        }

        section.container = new QWidget(navContainer);
        section.containerLayout = new QVBoxLayout(section.container);
        section.containerLayout->setContentsMargins(0, 0, 0, 0);
        section.containerLayout->setSpacing(0);
        navLayout->addWidget(section.container);

        mSections.append(section);
        return mSections.last();
    };

    // ---- MONITOR section (headerless — always visible, no toggle) ----
    {
        auto &sec = addSection(tr("MONITOR"), true);
        btnDash = createSidebarButton(tr("Dashboard"));
        btnDash->setChecked(true);
        sec.containerLayout->addWidget(btnDash);
        sec.buttons.append(btnDash);

        btnHardwareInfo = createSidebarButton(tr("Hardware Info"));
        sec.containerLayout->addWidget(btnHardwareInfo);
        sec.buttons.append(btnHardwareInfo);

        btnResources = createSidebarButton(tr("Resources"));
        sec.containerLayout->addWidget(btnResources);
        sec.buttons.append(btnResources);

        // SSO-23863: top-level entry point for the built-in disk-space
        // treemap (previously only reachable via a launcher widget buried
        // in the Resources page). Not checkable/exclusive and not part of
        // sec.buttons — like btnFeedback, it opens a dialog rather than
        // switching the stacked page, so it must not join mSidebarBtnGroup
        // or the checked-highlight would get stuck on it.
        btnDiskMap = new QPushButton(ui->sidebar);
        btnDiskMap->setToolTip(tr("Disk Map"));
        btnDiskMap->setCursor(Qt::PointingHandCursor);
        btnDiskMap->setCheckable(false);
        btnDiskMap->setIconSize(Dpi::scale(20, 20));
        btnDiskMap->setObjectName("btnDiskMap");
        sec.containerLayout->addWidget(btnDiskMap);

        btnNetworkUsage = createSidebarButton(tr("Network Usage"));
        sec.containerLayout->addWidget(btnNetworkUsage);
        sec.buttons.append(btnNetworkUsage);
    }

    // ---- MANAGE section ----
    {
        auto &sec = addSection(tr("MANAGE"));
        btnSystemCleaner = createSidebarButton(tr("System Cleaner"));
        sec.containerLayout->addWidget(btnSystemCleaner);
        sec.buttons.append(btnSystemCleaner);

        btnDiskTools = createSidebarButton(tr("Disk Tools"));
        sec.containerLayout->addWidget(btnDiskTools);
        sec.buttons.append(btnDiskTools);

        btnSearch = createSidebarButton(tr("Search"));
        sec.containerLayout->addWidget(btnSearch);
        sec.buttons.append(btnSearch);

        btnProcesses = createSidebarButton(tr("Processes"));
        sec.containerLayout->addWidget(btnProcesses);
        sec.buttons.append(btnProcesses);

        btnServices = createSidebarButton(tr("Services"));
        sec.containerLayout->addWidget(btnServices);
        sec.buttons.append(btnServices);

        btnStartupApps = createSidebarButton(tr("Startup Apps"));
        sec.containerLayout->addWidget(btnStartupApps);
        sec.buttons.append(btnStartupApps);

        btnBootAnalysis = createSidebarButton(tr("Boot Analysis"));
        sec.containerLayout->addWidget(btnBootAnalysis);
        sec.buttons.append(btnBootAnalysis);

#ifdef Q_OS_MAC
        btnUninstaller = createSidebarButton(tr("Applications"));
#else
        btnUninstaller = createSidebarButton(tr("Uninstaller"));
#endif
        sec.containerLayout->addWidget(btnUninstaller);
        sec.buttons.append(btnUninstaller);

#ifdef Q_OS_MAC
        btnMailCleanup = createSidebarButton(tr("Mail Cleanup"));
        sec.containerLayout->addWidget(btnMailCleanup);
        sec.buttons.append(btnMailCleanup);
#endif
        btnShredder = createSidebarButton(tr("File Shredder"));
        sec.containerLayout->addWidget(btnShredder);
        sec.buttons.append(btnShredder);
    }

    // ---- SYSTEM section ----
    {
        auto &sec = addSection(tr("SYSTEM"));
        btnDocker = createSidebarButton(tr("Docker"));
        sec.containerLayout->addWidget(btnDocker);
        sec.buttons.append(btnDocker);

        btnHelpers = createSidebarButton(tr("Helpers"));
        sec.containerLayout->addWidget(btnHelpers);
        sec.buttons.append(btnHelpers);

        btnSystemLogs = createSidebarButton(tr("System Logs"));
        sec.containerLayout->addWidget(btnSystemLogs);
        sec.buttons.append(btnSystemLogs);

#ifdef Q_OS_MAC
        btnAptSourceManager = createSidebarButton(tr("Homebrew"));
#else
        btnAptSourceManager = createSidebarButton(tr("APT Repository Manager"));
#endif
        sec.containerLayout->addWidget(btnAptSourceManager);
        sec.buttons.append(btnAptSourceManager);

        btnGnomeSettings = createSidebarButton(tr("GNOME Settings"));
        sec.containerLayout->addWidget(btnGnomeSettings);
        sec.buttons.append(btnGnomeSettings);

        btnSettings = createSidebarButton(tr("Settings"));
        sec.containerLayout->addWidget(btnSettings);
        sec.buttons.append(btnSettings);
    }

    // Connect section header clicks
    for (int i = 0; i < mSections.size(); ++i) {
        connect(mSections[i].header, &QPushButton::clicked, this, [this, i]() {
            toggleSection(i);
        });
    }

    // Set initial chevron icons (will be refreshed on theme change via updateSidebarIcons)
    updateSectionChevrons();

    // Keep sections top-aligned within the scroll container
    navLayout->addStretch();

    // Version label (#3)
    mVersionLabel = new QLabel(QString("v%1").arg(qApp->applicationVersion()), ui->sidebar);
    mVersionLabel->setObjectName("sidebarVersionLabel");
    mVersionLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    mSidebarLayout->addWidget(mVersionLabel);
    mSidebarLayout->addSpacing(4);

    // Feedback button (not a page - opens dialog)
    btnFeedback = new QPushButton(ui->sidebar);
    btnFeedback->setToolTip(tr("Feedback"));
    btnFeedback->setCursor(Qt::PointingHandCursor);
    btnFeedback->setCheckable(false);
    btnFeedback->setIconSize(Dpi::scale(20, 20));
    btnFeedback->setObjectName("btnFeedback");
    mSidebarLayout->addWidget(btnFeedback);

    // System Cleaner badge overlay (#8, #29)
    mCleanerBadge = new QLabel(ui->sidebar);
    mCleanerBadge->setObjectName("sidebarBadge");
    mCleanerBadge->setAlignment(Qt::AlignCenter);
    mCleanerBadge->setFixedSize(32, 16);
    mCleanerBadge->hide();

    mCleanerBadgeDot = new QLabel(ui->sidebar);
    mCleanerBadgeDot->setObjectName("sidebarBadgeDot");
    mCleanerBadgeDot->setFixedSize(8, 8);
    mCleanerBadgeDot->hide();

    // Updates badge overlay on Homebrew/APT button
    mUpdatesBadge = new QLabel(ui->sidebar);
    mUpdatesBadge->setObjectName("updatesBadge");
    mUpdatesBadge->setAlignment(Qt::AlignCenter);
    mUpdatesBadge->setFixedSize(32, 16);
    mUpdatesBadge->hide();

    mUpdatesBadgeDot = new QLabel(ui->sidebar);
    mUpdatesBadgeDot->setObjectName("updatesBadgeDot");
    mUpdatesBadgeDot->setFixedSize(8, 8);
    mUpdatesBadgeDot->hide();
}

void App::init()
{
    setMinimumSize(700, 480);

    // GH#55 / SSO-355: restore saved window geometry & state, otherwise center
    // on the primary screen at the design size.
    const QByteArray savedGeometry = SettingManager::ins()->getWindowGeometry();
    const QByteArray savedState    = SettingManager::ins()->getWindowState();
    bool restored = false;
    if (!savedGeometry.isEmpty())
        restored = restoreGeometry(savedGeometry);
    if (!savedState.isEmpty())
        restoreState(savedState);
    if (!restored) {
        QScreen *screen = qApp->primaryScreen();
        if (screen) {
            setGeometry(
                QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter,
                    size(), screen->availableGeometry())
            );
        }
    }

    ui->horizontalLayout->setContentsMargins(0,0,0,0);
    ui->horizontalLayout->setSpacing(0);

    // Build sidebar programmatically with section headers
    buildSidebar();

    // SSO-15037 (supersedes SSO-14661 / PR #284): that earlier fix aligned
    // every page's top edge with the sidebar divider line by giving
    // pageContentLayout a top margin, which stopped tiles rendering under
    // Dashboard's kiosk/edit buttons but left the buttons themselves as
    // Dashboard-local children — so both ended up below the divider,
    // in the same band as the tile grid. The band above the divider
    // (next to the logo/collapse button) is real, unpopulated pageContent
    // space; insert a persistent header-action-bar row there instead, sized
    // to the divider's actual geometry, and let pageContentLayout itself
    // stay flush with the window top. Sidebar collapse only animates width
    // (see applySidebarCollapse), never this vertical geometry, so the row
    // height holds at every breakpoint including the collapsed sidebar.
    // SSO-15303: at this point in the constructor `ui->sidebar` has only
    // been given its size via the window's saved/default geometry, not
    // via an actual layout pass — the outer layouts that place sidebar
    // within centralwidget (and centralwidget within the window) haven't
    // run yet, so sidebar (and mLogoSeparator inside it) still reports a
    // stale, near-zero height. Activate that chain outside-in before
    // activating sidebar's own layout, so mLogoSeparator's geometry
    // reflects the window's real size.
    layout()->activate();
    ui->horizontalLayout->activate();
    ui->sidebar->layout()->activate();
    const int headerRowHeight = mLogoSeparator->geometry().bottom() + 1;

    mHeaderActionsRow = new QWidget(ui->pageContent);
    mHeaderActionsRow->setObjectName("pageHeaderActionsRow");
    mHeaderActionsRow->setFixedHeight(headerRowHeight);
    mHeaderActionsRowLayout = new QHBoxLayout(mHeaderActionsRow);
    mHeaderActionsRowLayout->setContentsMargins(12, 4, 12, 4);
    mHeaderActionsRowLayout->setSpacing(0);

    ui->pageContentLayout->setContentsMargins(0, 0, 0, 0);
    ui->pageContentLayout->addWidget(mHeaderActionsRow);
    ui->pageContentLayout->addWidget(mSlidingStacked);

    // Set button labels
    btnDash->setText(tr("Dashboard"));
    btnHardwareInfo->setText(tr("Hardware Info"));
    btnResources->setText(tr("Resources"));
    btnDiskMap->setText(tr("Disk Map"));
    btnNetworkUsage->setText(tr("Network Usage"));
    btnSystemCleaner->setText(tr("System Cleaner"));
    btnDiskTools->setText(tr("Disk Tools"));
    btnSearch->setText(tr("Search"));
    btnProcesses->setText(tr("Processes"));
    btnServices->setText(tr("Services"));
    btnStartupApps->setText(tr("Startup Apps"));
    btnBootAnalysis->setText(tr("Boot Analysis"));
#ifdef Q_OS_MAC
    btnUninstaller->setText(tr("Applications"));
#else
    btnUninstaller->setText(tr("Uninstaller"));
#endif
    btnShredder->setText(tr("File Shredder"));
    btnDocker->setText(tr("Docker"));
#ifdef Q_OS_MAC
    btnMailCleanup->setText(tr("Mail Cleanup"));
#endif
    btnHelpers->setText(tr("Helpers"));
    btnSystemLogs->setText(tr("System Logs"));
#ifdef Q_OS_MAC
    btnAptSourceManager->setText(tr("Homebrew"));
#else
    btnAptSourceManager->setText(tr("APT Repository Manager"));
#endif
    btnGnomeSettings->setText(tr("GNOME Settings"));
    btnSettings->setText(tr("Settings"));
    btnFeedback->setText(tr("Feedback"));

    // Populate page slots with factory lambdas. Construction is driven by
    // ensurePage() — this commit still constructs all slots eagerly below
    // (zero behavior change). Commit B will drop the eager loop for
    // non-Dashboard slots.
    mPageSlots.append({
        "dashboard",
        tr("Dashboard"),
        [this]() -> QWidget* { dashboardPage = new DashboardPage(mSlidingStacked); return dashboardPage; },
        nullptr,
        // SSO-15037: give Dashboard a way to populate/clear the shell's
        // header-action-bar row without App hardcoding what it contains —
        // Dashboard pushes/clears its own widget from onPageActivated()/
        // onPageDeactivated().
        [this](QWidget *w) {
            static_cast<DashboardPage*>(w)->setHeaderActionsCallback(
                [this](QWidget *actions) { setPageHeaderActions(actions); });
        }
    });
    mPageSlots.append({
        "hardwareInfo",
        tr("Hardware Info"),
        [this]() -> QWidget* { hardwareInfoPage = new HardwareInfoPage(mSlidingStacked); return hardwareInfoPage; },
        nullptr, {}
    });
    mPageSlots.append({
        "resources",
        tr("Resources"),
        [this]() -> QWidget* { resourcesPage = new ResourcesPage(mSlidingStacked); return resourcesPage; },
        nullptr, {}
    });
    mPageSlots.append({
        "networkUsage",
        tr("Network Usage"),
        [this]() -> QWidget* { networkUsagePage = new NetworkUsagePage(mSlidingStacked); return networkUsagePage; },
        nullptr, {}
    });
    mPageSlots.append({
        "systemCleaner",
        tr("System Cleaner"),
        [this]() -> QWidget* {
            systemCleanerPage = new SystemCleanerPage(mSlidingStacked);
            connect(systemCleanerPage, &SystemCleanerPage::checkedCategoryCountChanged,
                    this, [this](int count) {
                if (count > 0) {
                    mCleanerBadge->setText(QString::number(count));
                    repositionBadges();
                    mCleanerBadge->show();
                    mCleanerBadgeDot->show();
                } else {
                    mCleanerBadge->clear();
                    mCleanerBadge->hide();
                    mCleanerBadgeDot->hide();
                }
            });
            return systemCleanerPage;
        },
        nullptr, {}
    });
    mPageSlots.append({
        "diskTools",
        tr("Disk Tools"),
        [this]() -> QWidget* { diskToolsPage = new DiskToolsPage(mSlidingStacked); return diskToolsPage; },
        nullptr, {}
    });
    mPageSlots.append({
        "search",
        tr("Search"),
        [this]() -> QWidget* { searchPage = new SearchPage(mSlidingStacked); return searchPage; },
        nullptr, {}
    });
    mPageSlots.append({
        "processes",
        tr("Processes"),
        [this]() -> QWidget* { processPage = new ProcessesPage(mSlidingStacked); return processPage; },
        nullptr, {}
    });
    mPageSlots.append({
        "services",
        tr("Services"),
        [this]() -> QWidget* { servicesPage = new ServicesPage(mSlidingStacked); return servicesPage; },
        nullptr, {}
    });
    mPageSlots.append({
        "startupApps",
        tr("Startup Apps"),
        [this]() -> QWidget* { startupAppsPage = new StartupAppsPage(mSlidingStacked); return startupAppsPage; },
        nullptr, {}
    });
    mPageSlots.append({
        "bootAnalysis",
        tr("Boot Analysis"),
        [this]() -> QWidget* { bootAnalysisPage = new BootAnalysisPage(mSlidingStacked); return bootAnalysisPage; },
        nullptr, {}
    });
    mPageSlots.append({
        "uninstaller",
#ifdef Q_OS_MAC
        tr("Applications"),
#else
        tr("Uninstaller"),
#endif
        [this]() -> QWidget* { uninstallerPage = new UninstallerPage(mSlidingStacked); return uninstallerPage; },
        nullptr, {}
    });
#ifdef Q_OS_MAC
    mPageSlots.append({
        "mailCleanup",
        tr("Mail Cleanup"),
        [this]() -> QWidget* { mailAttachmentCleanupPage = new MailAttachmentCleanupPage(mSlidingStacked); return mailAttachmentCleanupPage; },
        nullptr, {}
    });
#endif
    mPageSlots.append({
        "shredder",
        tr("File Shredder"),
        [this]() -> QWidget* { shredderPage = new ShredderPage(mSlidingStacked); return shredderPage; },
        nullptr, {}
    });
    mPageSlots.append({
        "helpers",
        tr("Helpers"),
        [this]() -> QWidget* { helpersPage = new HelpersPage(mSlidingStacked); return helpersPage; },
        nullptr, {}
    });
    mPageSlots.append({
        "systemLogs",
        tr("System Logs"),
        [this]() -> QWidget* { systemLogsPage = new SystemLogsPage(mSlidingStacked); return systemLogsPage; },
        nullptr, {}
    });
    mPageSlots.append({
        "settings",
        tr("Settings"),
        [this]() -> QWidget* { settingsPage = new SettingsPage(mSlidingStacked); return settingsPage; },
        nullptr, {}
    });

    mListSidebarButtons = {
        btnDash, btnHardwareInfo, btnResources, btnNetworkUsage, btnSystemCleaner, btnDiskTools, btnSearch,
        btnProcesses, btnServices, btnStartupApps, btnBootAnalysis, btnUninstaller,
#ifdef Q_OS_MAC
        btnMailCleanup,
#endif
        btnShredder, btnHelpers, btnSystemLogs, btnSettings
    };

    // Software sources page — Homebrew on macOS, APT on Linux
    if (ToolManager::ins()->checkSourceRepository()) {
        int idx = mListSidebarButtons.indexOf(btnSettings);
        mPageSlots.insert(idx, {
            "aptSourceManager",
#ifdef Q_OS_MAC
            tr("Homebrew"),
            [this]() -> QWidget* { homebrewPage = new HomebrewPage(mSlidingStacked); return homebrewPage; },
#else
            tr("APT Repository Manager"),
            [this]() -> QWidget* { aptSourceManagerPage = new APTSourceManagerPage(mSlidingStacked); return aptSourceManagerPage; },
#endif
            nullptr, {}
        });
        mListSidebarButtons.insert(idx, btnAptSourceManager);
    } else {
        btnAptSourceManager->hide();
    }

    // Updates badge on Homebrew/APT sidebar button
    connect(DataRefreshService::ins(), &DataRefreshService::systemUpdatesChecked,
            this, [this](const UpdateCheckResult &result) {
        int count = result.success ? result.totalCount : 0;
        if (count > 0) {
            mUpdatesBadge->setText(QString::number(count));
            repositionBadges();
        } else {
            mUpdatesBadge->setText(QString());
            mUpdatesBadge->hide();
            mUpdatesBadgeDot->hide();
        }

        // Tray alert when updates go from 0 to >0
        int lastCount = SettingManager::ins()->getUpdateLastCount();
        if (SettingManager::ins()->getUpdateAlertEnabled() && count > 0 && lastCount == 0) {
            mTrayIcon->showMessage(
                tr("System Updates Available"),
                tr("%1 %2 available").arg(count).arg(count == 1 ? tr("update") : tr("updates")),
                QSystemTrayIcon::Information);
        }
        SettingManager::ins()->setUpdateLastCount(count);
    });

    connect(NetUsageTracker::ins(), &NetUsageTracker::thresholdBreached,
            this, [this](int pct) {
        if (!SettingManager::ins()->getNetCapAlertEnabled())
            return;
        mTrayIcon->showMessage(
            tr("Network Cap Alert"),
            tr("You've used %1% of your monthly data cap.").arg(pct),
            QSystemTrayIcon::Warning);
    });

    // DOCKER
    if (ToolManager::ins()->checkDocker()) {
        int dockerIdx = mListSidebarButtons.indexOf(btnHelpers);
        mPageSlots.insert(dockerIdx, {
            "docker",
            tr("Docker"),
            [this]() -> QWidget* { dockerPage = new DockerPage(mSlidingStacked); return dockerPage; },
            nullptr, {}
        });
        mListSidebarButtons.insert(dockerIdx, btnDocker);
    } else {
        btnDocker->hide();
    }

    // GNOME SETTINGS (hidden on macOS — most settings have no valid macOS mapping)
#ifdef Q_OS_MAC
    btnGnomeSettings->hide();
#else
    if (ToolManager::ins()->checkGnomeSettings()) {
        int settingsIdx = mListSidebarButtons.indexOf(btnSettings);
        mPageSlots.insert(settingsIdx, {
            "gnomeSettings",
            tr("GNOME Settings"),
            [this]() -> QWidget* { gnomeSettingsPage = new GnomeSettingsPage(mSlidingStacked); return gnomeSettingsPage; },
            nullptr, {}
        });
        mListSidebarButtons.insert(settingsIdx, btnGnomeSettings);
    } else {
        btnGnomeSettings->hide();
    }
#endif

    // Connect sidebar button clicks to page navigation. Click handlers route
    // through ensurePageByTitle() so they work when the target page has not
    // yet been constructed (FR-97 lazy construction).
    auto navByTitle = [this](const QString &title) {
        if (QWidget *w = ensurePageByTitle(title))
            pageClick(w);
    };
    connect(btnDash,             &QPushButton::clicked, this, [this, navByTitle]() { navByTitle(tr("Dashboard")); });
    connect(btnHardwareInfo,     &QPushButton::clicked, this, [this, navByTitle]() { navByTitle(tr("Hardware Info")); });
    connect(btnResources,        &QPushButton::clicked, this, [this, navByTitle]() { navByTitle(tr("Resources")); });
    connect(btnNetworkUsage,     &QPushButton::clicked, this, [this, navByTitle]() { navByTitle(tr("Network Usage")); });
    connect(btnSystemCleaner,    &QPushButton::clicked, this, [this, navByTitle]() { navByTitle(tr("System Cleaner")); });
    connect(btnDiskTools,        &QPushButton::clicked, this, [this, navByTitle]() { navByTitle(tr("Disk Tools")); });
    connect(btnSearch,           &QPushButton::clicked, this, [this, navByTitle]() { navByTitle(tr("Search")); });
    connect(btnProcesses,        &QPushButton::clicked, this, [this, navByTitle]() { navByTitle(tr("Processes")); });
    connect(btnServices,         &QPushButton::clicked, this, [this, navByTitle]() { navByTitle(tr("Services")); });
    connect(btnStartupApps,      &QPushButton::clicked, this, [this, navByTitle]() { navByTitle(tr("Startup Apps")); });
    connect(btnBootAnalysis,     &QPushButton::clicked, this, [this, navByTitle]() { navByTitle(tr("Boot Analysis")); });
    connect(btnUninstaller,      &QPushButton::clicked, this, [this, navByTitle]() {
#ifdef Q_OS_MAC
        navByTitle(tr("Applications"));
#else
        navByTitle(tr("Uninstaller"));
#endif
    });
#ifdef Q_OS_MAC
    connect(btnMailCleanup,      &QPushButton::clicked, this, [this, navByTitle]() { navByTitle(tr("Mail Cleanup")); });
#endif
    connect(btnShredder,         &QPushButton::clicked, this, [this, navByTitle]() { navByTitle(tr("File Shredder")); });
    connect(btnHelpers,          &QPushButton::clicked, this, [this, navByTitle]() { navByTitle(tr("Helpers")); });
    connect(btnSystemLogs,       &QPushButton::clicked, this, [this, navByTitle]() { navByTitle(tr("System Logs")); });
    connect(btnSettings,         &QPushButton::clicked, this, [this, navByTitle]() { navByTitle(tr("Settings")); });
    connect(btnFeedback,         &QPushButton::clicked, this, [this]() {
        if (feedback.isNull())
            feedback = QSharedPointer<Feedback>(new Feedback(this));
        feedback->show();
    });
    connect(btnDiskMap,          &QPushButton::clicked, this, &App::openDiskTreemapDialog);

    // Conditional page button clicks
    if (ToolManager::ins()->checkDocker())
        connect(btnDocker, &QPushButton::clicked, this, [this, navByTitle]() { navByTitle(tr("Docker")); });
    if (ToolManager::ins()->checkSourceRepository())
        connect(btnAptSourceManager, &QPushButton::clicked, this, [this, navByTitle]() {
#ifdef Q_OS_MAC
            navByTitle(tr("Homebrew"));
#else
            navByTitle(tr("APT Repository Manager"));
#endif
        });
#ifndef Q_OS_MAC
    if (ToolManager::ins()->checkGnomeSettings())
        connect(btnGnomeSettings, &QPushButton::clicked, this, [this, navByTitle]() { navByTitle(tr("GNOME Settings")); });
#endif

    // Reposition badges when the nav scroll position changes
    connect(mNavScrollArea->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &App::repositionBadges);

    // Refresh sidebar icons when theme changes
    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &App::updateSidebarIcons);

    // Navigate-to-page signal from any widget (e.g. dashboard quick actions)
    connect(SignalMapper::ins(), &SignalMapper::sigNavigateToPage,
            this, [this](const QString &title) {
        QWidget *page = getPageByTitle(title);
        if (page) {
            pageClick(page);
            checkSidebarButtonByTooltip(title);
        }
    });

    // Construct Dashboard eagerly (it is the default landing page and owns
    // most of the DataRefreshService signal subscriptions). Other pages are
    // built lazily on first navigation — see ensurePage().
    ensurePage(0);

    AppManager::ins()->updateStylesheet();
    mInitialThemeApplied = true;

    Utilities::addDropShadow(ui->sidebar, 60);

    // Set start page. Must run before DataRefreshService::start() so the
    // landing page's onPageActivated() can register subscribers (FR-103)
    // before the service fires its initial immediate ticks. SSO-3388: the
    // setting is now a stable id; resolve it to the currently-localized
    // title that clickSidebarButton/checkSidebarButtonByTooltip expect.
    clickSidebarButton(pageTitleById(SettingManager::ins()->getStartPage()));

    DataRefreshService::ins()->start();
    NetUsageTracker::ins()->start(DataRefreshService::ins());

#ifdef Q_OS_LINUX
    // FR-117: if the user asked us to persist CPU tuning, re-apply their
    // saved turbo / freq range in the background on launch.
    CpuTuningWidget::applyPersistedSettings();
#endif

    createTrayActions();

    mTrayIcon->show();

#ifdef Q_OS_MAC
    // FW-20 (SSO-3748): optional menu-bar CPU/memory monitor, off by default.
    mMenuBarMonitor = new MenuBarMonitor(this);
    connect(mMenuBarMonitor, &MenuBarMonitor::activationRequested, this, [this]() {
        setWindowState(windowState() & ~Qt::WindowMinimized);
        clickSidebarButton(tr("Dashboard"), true);
        if (windowHandle())
            windowHandle()->requestActivate();
    });
    connect(SignalMapper::ins(), &SignalMapper::sigMenuBarMonitorToggled,
            mMenuBarMonitor, &MenuBarMonitor::setEnabled);
    mMenuBarMonitor->setEnabled(SettingManager::ins()->getMenuBarMonitorEnabled());
#else
    // SSO-23854: optional Linux tray health score + Clean Now, off by default.
    mTrayHealthMonitor = new TrayHealthMonitor(this);
    connect(mTrayHealthMonitor, &TrayHealthMonitor::scoreTextChanged, this, [this](const QString &text) {
        mTrayHealthAction->setText(text);
        mTrayIcon->setToolTip(text);
    });
    connect(mTrayHealthMonitor, &TrayHealthMonitor::cleanStateChanged, this,
            [this](const QString &label, bool enabled) {
        mTrayCleanAction->setText(label);
        mTrayCleanAction->setEnabled(enabled);
    });
    connect(mTrayCleanAction, &QAction::triggered, mTrayHealthMonitor, &TrayHealthMonitor::startClean);

    auto applyTrayHealthScoreEnabled = [this](bool enabled) {
        mTrayHealthAction->setVisible(enabled);
        mTrayCleanAction->setVisible(enabled);
        mTrayHealthMonitor->setEnabled(enabled);
        if (!enabled)
            mTrayIcon->setToolTip(QString());
    };
    connect(SignalMapper::ins(), &SignalMapper::sigTrayHealthScoreToggled, this, applyTrayHealthScoreEnabled);
    applyTrayHealthScoreEnabled(SettingManager::ins()->getTrayHealthScoreEnabled());
#endif

    // SSO-23855: compact always-on-top mini-monitor window — cross-platform
    // (shared/nexis QWidget, not a native NSPanel), unlike mMenuBarMonitor
    // above. Off by default; restores its last open/closed state below.
    mMiniMonitorWindow = new MiniMonitorWindow(this);
    connect(SignalMapper::ins(), &SignalMapper::sigMiniMonitorToggled,
            mMiniMonitorWindow, &QWidget::setVisible);
    connect(mMiniMonitorWindow, &MiniMonitorWindow::visibilityToggled,
            this, [this](bool visible) {
        if (mMiniMonitorAction)
            mMiniMonitorAction->setChecked(visible);
    });
    mMiniMonitorWindow->setVisible(SettingManager::ins()->getMiniMonitorVisible());

    // Kiosk mode shortcuts
    QAction *kioskToggle = new QAction(this);
    kioskToggle->setShortcut(Qt::Key_F11);
    addAction(kioskToggle);
    connect(kioskToggle, &QAction::triggered, this, &App::toggleKioskMode);

    QAction *kioskExit = new QAction(this);
    kioskExit->setShortcut(Qt::Key_Escape);
    addAction(kioskExit);
    connect(kioskExit, &QAction::triggered, this, &App::exitKioskMode);

    // Sidebar collapse shortcut (Ctrl+B)
    QAction *sidebarToggle = new QAction(this);
    sidebarToggle->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));
    addAction(sidebarToggle);
    connect(sidebarToggle, &QAction::triggered, this, &App::toggleSidebarCollapse);

    // Command palette shortcut (Ctrl+K)
    setupCommandPalette();
    QAction *cmdPaletteAction = new QAction(this);
    cmdPaletteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
    addAction(cmdPaletteAction);
    connect(cmdPaletteAction, &QAction::triggered, this, [this]() {
        mCommandPalette->show();
    });

    // Restore kiosk mode from last session, or force it on if the user has
    // configured Nexis to always launch straight into kiosk mode (GH#207).
    if (SettingManager::ins()->getKioskMode() || SettingManager::ins()->getLaunchInKioskMode()) {
        mKioskMode = true;
        SettingManager::ins()->setKioskMode(true);
        applyKioskMode(true);
    }

    // Restore sidebar collapsed state
    if (SettingManager::ins()->getSidebarCollapsed())
        applySidebarCollapse(true, false);

    // Restore section collapsed states
    restoreSectionStates();

    // Dashboard kiosk toggle button -> App::toggleKioskMode
    connect(SignalMapper::ins(), &SignalMapper::sigKioskToggleRequested,
            this, &App::toggleKioskMode);

    // Update System Cleaner badge when cleanable size changes
    connect(SignalMapper::ins(), &SignalMapper::sigCleanableSizeChanged,
            this, [this](quint64 bytes) {
        if (bytes > 0) {
            mCleanerBadge->setText(FormatUtil::formatBytes(bytes));
            repositionBadges();
        } else {
            mCleanerBadge->setText(QString());
            mCleanerBadge->hide();
            mCleanerBadgeDot->hide();
        }
    });

    connect(CleanerService::ins(), &CleanerService::cleaningFinished,
            this, [this](CleanerService::CleanResult result) {
        if (SettingManager::ins()->getCleaningNotificationsEnabled()) {
            mTrayIcon->showMessage(
                tr("Scheduled Clean Complete"),
                tr("Cleaned %1 — %2")
                    .arg(FormatUtil::formatBytes(result.totalBytesFreed),
                         result.scheduleName),
                QSystemTrayIcon::Information, 5000);
        }
    });
}

void App::closeEvent(QCloseEvent *event)
{
    // GH#55 / SSO-355: persist window size & position on every close path,
    // including the minimize-to-tray "ignored close" — the user can quit from
    // the tray later, after which Qt does not deliver another closeEvent.
    // saveGeometry() captures maximized/fullscreen state alongside the
    // underlying normal geometry, so always call it.
    SettingManager::ins()->setWindowGeometry(saveGeometry());
    SettingManager::ins()->setWindowState(saveState());

    if (SettingManager::ins()->getMinimizeToTray() && QSystemTrayIcon::isSystemTrayAvailable()) {
        emit SignalMapper::ins()->sigAppVisibilityChanged(false);
        hide();
#ifdef Q_OS_MAC
        nexis_macos_hide_dock_icon();
#endif
        event->ignore();
        return;
    }

    mTrayIcon->hide();
    event->accept();

    QThreadPool::globalInstance()->waitForDone();

    qApp->quit();
}

void App::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange && windowState().testFlag(Qt::WindowMinimized)) {
        if (SettingManager::ins()->getMinimizeToTray() && QSystemTrayIcon::isSystemTrayAvailable()) {
            emit SignalMapper::ins()->sigAppVisibilityChanged(false);
            hide();
#ifdef Q_OS_MAC
            nexis_macos_hide_dock_icon();
#endif
            event->ignore();
            return;
        }
    }
    // FR-105: surface focus transitions so DataRefreshService can downshift
    // cadence when the user switches to another app but leaves Nexis visible.
    if (event->type() == QEvent::WindowActivate)
        emit SignalMapper::ins()->sigAppFocusChanged(true);
    else if (event->type() == QEvent::WindowDeactivate)
        emit SignalMapper::ins()->sigAppFocusChanged(false);

    QMainWindow::changeEvent(event);
}

void App::createTrayActions()
{
    auto showAndRaise = [this] {
#ifdef Q_OS_MAC
        nexis_macos_show_dock_icon();
#endif
        setWindowState(windowState() & ~Qt::WindowMinimized);
        show();
        if (windowHandle())
            windowHandle()->requestActivate();
        emit SignalMapper::ins()->sigAppVisibilityChanged(true);
    };

    connect(mTrayIcon, &QSystemTrayIcon::activated, this, [showAndRaise](QSystemTrayIcon::ActivationReason) {
        showAndRaise();
    });

    // SSO-23896: groups derived from mSections (same model the sidebar
    // builds from) so a page added to a sidebar section lands in the
    // matching tray group with no tray-side edit.
    QAction *openAction = new QAction(tr("Open Nexis"), this);
    connect(openAction, &QAction::triggered, this, showAndRaise);
    mTrayMenu->addAction(openAction);
    mTrayMenu->addSeparator();

#ifndef Q_OS_MAC
    // SSO-23854: Linux tray counterpart of the macOS menu-bar health score +
    // Clean Now surface (SSO-23853). Actions exist unconditionally so
    // TrayHealthMonitor's signals always have somewhere to write; visibility
    // follows the off-by-default TrayHealthScoreEnabled setting (see init()).
    mTrayHealthAction = new QAction(this);
    mTrayHealthAction->setEnabled(false);
    mTrayMenu->addAction(mTrayHealthAction);

    mTrayCleanAction = new QAction(tr("Clean Now"), this);
    mTrayMenu->addAction(mTrayCleanAction);

    mTrayMenu->addSeparator();
#endif

    auto addNavAction = [this](QMenu *menu, QPushButton *button) {
        const QString toolTip = button->toolTip();
        QAction *action = menu->addAction(toolTip);
        connect(action, &QAction::triggered, this, [this, toolTip] {
            clickSidebarButton(toolTip, true);
        });
    };

    const QList<TrayMenuGroup> groups = buildTrayMenuGroups(mSections);
    bool previousHeaderless = true;
    bool anyGroupEmitted = false;
    for (const TrayMenuGroup &group : groups) {
        if (anyGroupEmitted && group.headerless != previousHeaderless)
            mTrayMenu->addSeparator();

        if (group.headerless) {
            for (QPushButton *button : group.items)
                addNavAction(mTrayMenu, button);
        } else {
            QMenu *submenu = mTrayMenu->addMenu(trayMenuGroupTitle(group.name));
            for (QPushButton *button : group.items)
                addNavAction(submenu, button);
        }

        previousHeaderless = group.headerless;
        anyGroupEmitted = true;
    }
    mTrayMenu->addSeparator();

    // FR-125: Quick Actions submenu
    QMenu *quickMenu = mTrayMenu->addMenu(tr("Quick Actions"));

    QAction *paletteAction = quickMenu->addAction(tr("Open Command Palette"));
    connect(paletteAction, &QAction::triggered, this, [this] {
        show();
        mCommandPalette->show();
    });

    QAction *scanAction = quickMenu->addAction(tr("Run System Cleaner Scan"));
    connect(scanAction, &QAction::triggered, this, [this] {
        clickSidebarButton(btnSystemCleaner->toolTip(), true);
        if (systemCleanerPage)
            systemCleanerPage->quickScan();
    });

    // SSO-23855: toggles the compact mini-monitor window from the tray, the
    // same surface used to open/navigate the main window.
    mMiniMonitorAction = quickMenu->addAction(tr("Mini Monitor"));
    mMiniMonitorAction->setCheckable(true);
    mMiniMonitorAction->setChecked(SettingManager::ins()->getMiniMonitorVisible());
    connect(mMiniMonitorAction, &QAction::triggered, this, [](bool checked) {
        emit SignalMapper::ins()->sigMiniMonitorToggled(checked);
    });

#ifndef Q_OS_MACOS
    if (InfoManager::ins()->hasPowerProfiles()) {
        quickMenu->addSeparator();
        QMenu *profileMenu = quickMenu->addMenu(tr("Power Profile"));
        auto *profileGroup = new QActionGroup(profileMenu);
        profileGroup->setExclusive(true);

        auto refreshProfiles = [this, profileMenu, profileGroup]() {
            const PowerProfileData data = InfoManager::ins()->getPowerProfileData();
            const QString activeLabel   = PowerProfileInfo::backendValueToUserLabel(data.activeProfile, data.backend);
            for (QAction *a : profileGroup->actions()) {
                a->setChecked(a->text() == activeLabel);
            }
            Q_UNUSED(profileMenu)
        };

        const PowerProfileData data = InfoManager::ins()->getPowerProfileData();
        for (const QString &backendVal : data.availableProfiles) {
            const QString label  = PowerProfileInfo::backendValueToUserLabel(backendVal, data.backend);
            QAction *profileAction = profileMenu->addAction(label);
            profileAction->setCheckable(true);
            profileAction->setChecked(backendVal == data.activeProfile);
            profileGroup->addAction(profileAction);
            connect(profileAction, &QAction::triggered, this, [this, backendVal] {
                InfoManager::ins()->setPowerProfile(backendVal);
                InfoManager::ins()->refreshPowerProfile();
            });
        }

        connect(profileMenu, &QMenu::aboutToShow, this, refreshProfiles);
    }
#endif

    mTrayMenu->addSeparator();

    mKioskAction = new QAction(tr("Kiosk Mode (F11)"), this);
    mKioskAction->setCheckable(true);
    mKioskAction->setChecked(mKioskMode);
    connect(mKioskAction, &QAction::triggered, this, &App::toggleKioskMode);
    mTrayMenu->addAction(mKioskAction);

    mTrayMenu->addSeparator();

    QAction *quitAction = new QAction(tr("Quit"), this);
    connect(quitAction, &QAction::triggered, [=] {qApp->quit();});
    mTrayMenu->addAction(quitAction);

    mTrayIcon->setContextMenu(mTrayMenu);
}

void App::clickSidebarButton(QString pageTitle, bool isShow)
{
    QWidget *selectedWidget = getPageByTitle(pageTitle);
    if (selectedWidget) {
        pageClick(selectedWidget, !isShow);
        checkSidebarButtonByTooltip(pageTitle);
    } else {
        pageClick(ensurePage(0));
    }
#ifdef Q_OS_MAC
    if (isShow)
        nexis_macos_show_dock_icon();
#endif
    setVisible(isShow);
    if (isShow && windowHandle())
        windowHandle()->requestActivate();
    if (isShow)
        emit SignalMapper::ins()->sigAppVisibilityChanged(true);
}

void App::checkSidebarButtonByTooltip(const QString &text)
{
    for (QPushButton *button : mListSidebarButtons) {
        if (button->toolTip() == text) {
            expandSectionForButton(button);
            button->setChecked(true);
        }
    }
}

QWidget* App::getPageByTitle(const QString &title)
{
    return ensurePageByTitle(title);
}

QWidget* App::ensurePage(int index)
{
    if (index < 0 || index >= mPageSlots.size())
        return nullptr;
    PageSlot &slot = mPageSlots[index];
    if (slot.widget)
        return slot.widget;
    if (!slot.factory)
        return nullptr;
    QWidget *w = slot.factory();
    if (!w)
        return nullptr;
    slot.widget = w;
    mSlidingStacked->addWidget(w);
    if (slot.onConstructed)
        slot.onConstructed(w);
    // If the initial stylesheet was already applied before this page was
    // constructed, the page missed the sigChangedAppTheme emission fired
    // from updateStylesheet(). Emit once so the new page runs its
    // refreshThemeColors() slot and picks up any cached color tokens
    // (chart series colors, etc.). Global QSS already cascades to new
    // widgets automatically; this signal is for per-page color caches.
    if (mInitialThemeApplied)
        emit SignalMapper::ins()->sigChangedAppTheme();
    return w;
}

QWidget* App::ensurePageByTitle(const QString &title)
{
    for (int i = 0; i < mPageSlots.size(); ++i) {
        if (mPageSlots[i].title == title)
            return ensurePage(i);
    }
    return nullptr;
}

QString App::pageTitleById(const QString &id) const
{
    for (const PageSlot &slot : mPageSlots) {
        if (slot.id == id)
            return slot.title;
    }
    return QString();
}

void App::ensureAllPages()
{
    for (int i = 0; i < mPageSlots.size(); ++i) {
        if (!mPageSlots[i].widget)
            ensurePage(i);
    }
}

void App::setPageHeaderActions(QWidget *widget)
{
    QLayoutItem *item;
    while ((item = mHeaderActionsRowLayout->takeAt(0)) != nullptr) {
        if (item->widget())
            item->widget()->setParent(nullptr);
        delete item;
    }
    if (widget) {
        mHeaderActionsRowLayout->addWidget(widget);
        widget->show();
    }
}

void App::pageClick(QWidget *widget, bool slide)
{
    if (widget) {
        QWidget *current = mSlidingStacked->currentWidget();
        if (current != widget) {
            if (auto *page = qobject_cast<NexisPage*>(current))
                page->onPageDeactivated();
        }

        if (slide) {
            mSlidingStacked->slideInIdx(mSlidingStacked->indexOf(widget));
        } else {
            mSlidingStacked->setCurrentWidget(widget);
        }

        if (auto *page = qobject_cast<NexisPage*>(widget))
            page->onPageActivated();
    }
}

void App::updateSidebarIcons()
{
    QString theme = AppManager::ins()->resolveThemeName();

    auto setIcon = [&](QPushButton *btn, const QString &iconName) {
        QString svgPath = QString(":/static/themes/%1/img/sidebar-icons/%2").arg(theme, iconName);
        btn->setIcon(QIcon(svgPath));
        btn->setIconSize(Dpi::scale(20, 20));
    };

    setIcon(btnDash,             "dash.svg");
    setIcon(btnHardwareInfo,     "hardware-info.svg");
    setIcon(btnResources,        "resources.svg");
    setIcon(btnDiskMap,          "disk-map.svg");
    setIcon(btnNetworkUsage,     "network-usage.svg");
    setIcon(btnSystemCleaner,    "cleaner.svg");
    setIcon(btnDiskTools,        "disk-tools.svg");
    setIcon(btnSearch,           "search.svg");
    setIcon(btnProcesses,        "process.svg");
    setIcon(btnServices,         "services.svg");
    setIcon(btnStartupApps,      "startup-apps.svg");
    setIcon(btnBootAnalysis,     "boot-analysis.svg");
    setIcon(btnUninstaller,      "uninstaller.svg");
    setIcon(btnShredder,         "shredder.svg");
    setIcon(btnDocker,           "docker.svg");
#ifdef Q_OS_MAC
    setIcon(btnMailCleanup,      "mail.svg");
#endif
    setIcon(btnHelpers,          "helpers.svg");
    setIcon(btnSystemLogs,       "system-logs.svg");
    setIcon(btnAptSourceManager, "ppa-manager.svg");
    setIcon(btnGnomeSettings,    "gnome-settings.svg");
    setIcon(btnSettings,         "settings.svg");
    setIcon(btnFeedback,         "feedback.svg");

    // Sidebar toggle icon
    {
        QString toggleName = mSidebarCollapsed ? "sidebar-expand.svg" : "sidebar-collapse.svg";
        QString togglePath = QString(":/static/themes/%1/img/sidebar-icons/%2").arg(theme, toggleName);
        mBtnSidebarToggle->setIcon(QIcon(togglePath));
        mBtnSidebarToggle->setIconSize(Dpi::scale(16, 16));
    }

    // Sidebar logo
    QString logoFile = mSidebarCollapsed ? "sidebar-logo-collapsed.svg" : "sidebar-logo.svg";
    QString logoPath = QString(":/static/themes/%1/img/sidebar-icons/%2").arg(theme, logoFile);
    QPixmap logoPix(logoPath);
    if (!logoPix.isNull())
        mLogoLabel->setPixmap(logoPix.scaledToHeight(Dpi::scale(20), Qt::SmoothTransformation));

    // Section chevron icons
    updateSectionChevrons();
}

void App::toggleSidebarCollapse()
{
    mSidebarCollapsed = !mSidebarCollapsed;
    SettingManager::ins()->setSidebarCollapsed(mSidebarCollapsed);
    applySidebarCollapse(mSidebarCollapsed, true);
}

// SSO-23863: shared by the sidebar button and the command palette entry.
// Same call disk_usage_launcher_widget.cpp's "Built-in Treemap" button
// makes — this is a second door to the same dialog, not a fork of it.
void App::openDiskTreemapDialog()
{
    auto *dlg = new DiskTreemapDialog(this, AppManager::ins(), SignalMapper::ins());
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void App::applySidebarCollapse(bool collapsed, bool animate)
{
    mSidebarCollapsed = collapsed;

    int targetWidth = collapsed ? SIDEBAR_COLLAPSED_WIDTH : SIDEBAR_EXPANDED_WIDTH;

    if (animate) {
        auto *minAnim = new QPropertyAnimation(ui->sidebar, "minimumWidth", this);
        minAnim->setDuration(250);
        minAnim->setStartValue(ui->sidebar->minimumWidth());
        minAnim->setEndValue(targetWidth);
        minAnim->setEasingCurve(QEasingCurve::OutCubic);

        auto *maxAnim = new QPropertyAnimation(ui->sidebar, "maximumWidth", this);
        maxAnim->setDuration(250);
        maxAnim->setStartValue(ui->sidebar->maximumWidth());
        maxAnim->setEndValue(targetWidth);
        maxAnim->setEasingCurve(QEasingCurve::OutCubic);

        connect(maxAnim, &QPropertyAnimation::finished, this, &App::repositionBadges);
        minAnim->start(QAbstractAnimation::DeleteWhenStopped);
        maxAnim->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
        ui->sidebar->setMinimumWidth(targetWidth);
        ui->sidebar->setMaximumWidth(targetWidth);
        repositionBadges();
    }

    // Toggle section headers, containers, and indicators
    for (int i = 0; i < mSections.size(); ++i) {
        if (mSections[i].header)
            mSections[i].header->setVisible(!collapsed);
        mSections[i].container->setVisible(!mSections[i].collapsed);
    }

    for (QFrame *indicator : mSectionIndicators)
        indicator->setVisible(false);

    // Toggle version label
    if (mVersionLabel)
        mVersionLabel->setVisible(!collapsed);

    // Toggle button text visibility
    for (QPushButton *btn : mListSidebarButtons) {
        if (collapsed) {
            btn->setProperty("sidebarText", btn->text());
            btn->setText(QString());
        } else {
            QString savedText = btn->property("sidebarText").toString();
            if (!savedText.isEmpty())
                btn->setText(savedText);
        }
    }

    // Toggle feedback button text
    if (collapsed) {
        btnFeedback->setProperty("sidebarText", btnFeedback->text());
        btnFeedback->setText(QString());
    } else {
        QString savedFeedback = btnFeedback->property("sidebarText").toString();
        if (!savedFeedback.isEmpty())
            btnFeedback->setText(savedFeedback);
    }

    // Toggle Disk Map button text — not in mListSidebarButtons (same reason
    // as btnFeedback: it opens a dialog, not a page), so it needs the same
    // manual handling here.
    if (collapsed) {
        btnDiskMap->setProperty("sidebarText", btnDiskMap->text());
        btnDiskMap->setText(QString());
    } else {
        QString savedDiskMap = btnDiskMap->property("sidebarText").toString();
        if (!savedDiskMap.isEmpty())
            btnDiskMap->setText(savedDiskMap);
    }

    // Update toggle icon and logo
    QString theme = AppManager::ins()->resolveThemeName();
    {
        QString toggleName = collapsed ? "sidebar-expand.svg" : "sidebar-collapse.svg";
        QString togglePath = QString(":/static/themes/%1/img/sidebar-icons/%2").arg(theme, toggleName);
        mBtnSidebarToggle->setIcon(QIcon(togglePath));
        mBtnSidebarToggle->setIconSize(Dpi::scale(16, 16));
    }

    // Swap logo variant
    QString logoFile = collapsed ? "sidebar-logo-collapsed.svg" : "sidebar-logo.svg";
    QString logoPath = QString(":/static/themes/%1/img/sidebar-icons/%2").arg(theme, logoFile);
    QPixmap logoPix(logoPath);
    if (!logoPix.isNull())
        mLogoLabel->setPixmap(logoPix.scaledToHeight(Dpi::scale(20), Qt::SmoothTransformation));

    // Set dynamic property for QSS targeting
    ui->sidebar->setProperty("collapsed", collapsed);
    ui->sidebar->style()->unpolish(ui->sidebar);
    ui->sidebar->style()->polish(ui->sidebar);

    // Re-polish child buttons so QSS selectors depending on the parent's
    // collapsed property (e.g. #sidebar[collapsed="true"] QPushButton)
    // are re-evaluated — Qt does not do this recursively.
    for (QPushButton *btn : mListSidebarButtons) {
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
    }
    for (auto &sec : mSections) {
        if (sec.header) {
            sec.header->style()->unpolish(sec.header);
            sec.header->style()->polish(sec.header);
        }
    }
    btnFeedback->style()->unpolish(btnFeedback);
    btnFeedback->style()->polish(btnFeedback);
    btnDiskMap->style()->unpolish(btnDiskMap);
    btnDiskMap->style()->polish(btnDiskMap);
    mBtnSidebarToggle->style()->unpolish(mBtnSidebarToggle);
    mBtnSidebarToggle->style()->polish(mBtnSidebarToggle);
}

void App::toggleSection(int sectionIndex)
{
    if (sectionIndex < 0 || sectionIndex >= mSections.size())
        return;
    if (mSections[sectionIndex].headerless)
        return;
    mSections[sectionIndex].collapsed = !mSections[sectionIndex].collapsed;
    applySectionCollapse(sectionIndex, mSections[sectionIndex].collapsed, true);
    saveSectionStates();
}

void App::applySectionCollapse(int sectionIndex, bool collapsed, bool animate)
{
    if (sectionIndex < 0 || sectionIndex >= mSections.size())
        return;

    auto &sec = mSections[sectionIndex];
    sec.collapsed = collapsed;

    if (mSidebarCollapsed) {
        repositionBadges();
        return;
    }

    QWidget *container = sec.container;

    if (!animate) {
        container->setVisible(!collapsed);
        container->setMaximumHeight(collapsed ? 0 : 16777215);
        updateSectionChevrons();
        repositionBadges();
        return;
    }

    if (collapsed) {
        int startHeight = container->height();
        auto *anim = new QPropertyAnimation(container, "maximumHeight", this);
        anim->setDuration(200);
        anim->setStartValue(startHeight);
        anim->setEndValue(0);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(anim, &QPropertyAnimation::finished, this, [this, container]() {
            container->setVisible(false);
            repositionBadges();
        });
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
        container->setMaximumHeight(0);
        container->setVisible(true);
        container->adjustSize();
        int targetHeight = container->sizeHint().height();
        auto *anim = new QPropertyAnimation(container, "maximumHeight", this);
        anim->setDuration(200);
        anim->setStartValue(0);
        anim->setEndValue(targetHeight);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(anim, &QPropertyAnimation::finished, this, [this, container]() {
            container->setMaximumHeight(16777215);
            repositionBadges();
        });
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }

    updateSectionChevrons();
}

void App::expandSectionForButton(QPushButton *btn)
{
    for (int i = 0; i < mSections.size(); ++i) {
        if (mSections[i].buttons.contains(btn) && mSections[i].collapsed) {
            applySectionCollapse(i, false, false);
            saveSectionStates();
            return;
        }
    }
}

void App::saveSectionStates()
{
    QJsonObject obj;
    for (const auto &sec : mSections) {
        if (!sec.headerless)
            obj[sec.name] = sec.collapsed;
    }
    SettingManager::ins()->setSidebarSectionsCollapsed(
        QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void App::restoreSectionStates()
{
    QString json = SettingManager::ins()->getSidebarSectionsCollapsed();
    if (json.isEmpty())
        return;
    QJsonObject obj = QJsonDocument::fromJson(json.toUtf8()).object();
    for (int i = 0; i < mSections.size(); ++i) {
        if (obj.contains(mSections[i].name)) {
            applySectionCollapse(i, obj[mSections[i].name].toBool(), false);
        }
    }
}

void App::updateSectionChevrons()
{
    QString theme = AppManager::ins()->resolveThemeName();
    for (auto &sec : mSections) {
        if (!sec.header)
            continue;
        QString chevronName = sec.collapsed ? "section-expand.svg" : "section-collapse.svg";
        QString chevronPath = QString(":/static/themes/%1/img/sidebar-icons/%2").arg(theme, chevronName);
        sec.header->setIcon(QIcon(chevronPath));
    }
}

void App::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    repositionBadges();
}

void App::repositionBadges()
{
    auto positionBadge = [this](QPushButton *btn, QLabel *badge, QLabel *dot) {
        bool hasBadge = badge && badge->text().length() > 0;
        if (!hasBadge)
            return;

        if (!btn->isVisible() || btn->height() == 0) {
            badge->hide();
            dot->hide();
            return;
        }

        // Hide badge when the button has been scrolled out of the nav viewport
        if (mNavScrollArea) {
            QRect viewportRect = mNavScrollArea->viewport()->rect();
            QPoint btnInViewport = btn->mapTo(mNavScrollArea->viewport(), QPoint(0, 0));
            if (!viewportRect.intersects(QRect(btnInViewport, btn->size()))) {
                badge->hide();
                dot->hide();
                return;
            }
        }

        QPoint btnPos = btn->mapTo(ui->sidebar, QPoint(0, 0));
        int btnW = btn->width();
        badge->move(btnPos.x() + btnW - badge->width() - 8,
                    btnPos.y() + 2);
        dot->move(btnPos.x() + btnW - 16,
                  btnPos.y() + 4);

        badge->setVisible(!mSidebarCollapsed);
        dot->setVisible(mSidebarCollapsed);
    };

    positionBadge(btnSystemCleaner, mCleanerBadge, mCleanerBadgeDot);
    positionBadge(btnAptSourceManager, mUpdatesBadge, mUpdatesBadgeDot);
}

void App::toggleKioskMode()
{
    mKioskMode = !mKioskMode;
    SettingManager::ins()->setKioskMode(mKioskMode);
    applyKioskMode(mKioskMode);
}

void App::exitKioskMode()
{
    if (!mKioskMode)
        return;
    mKioskMode = false;
    SettingManager::ins()->setKioskMode(false);
    applyKioskMode(false);
}

void App::applyKioskMode(bool enable)
{
    mKioskAction->blockSignals(true);
    mKioskAction->setChecked(enable);
    mKioskAction->blockSignals(false);

    if (enable) {
        mPreKioskCollapsed = mSidebarCollapsed;
        ui->sidebar->hide();
        pageClick(dashboardPage, false);

        // GH#207: place the kiosk window on the configured monitor, if any.
        if (QScreen *targetScreen = resolveKioskScreen()) {
            winId(); // force native window handle creation so setScreen() takes effect
            if (QWindow *handle = windowHandle())
                handle->setScreen(targetScreen);
            setGeometry(targetScreen->geometry());
        }

        showFullScreen();
        showKioskOverlay();
    } else {
        showNormal();
        ui->sidebar->show();
        if (mPreKioskCollapsed != mSidebarCollapsed)
            applySidebarCollapse(mPreKioskCollapsed, false);
    }

    emit SignalMapper::ins()->sigKioskModeChanged(enable);
}

QScreen *App::resolveKioskScreen() const
{
    const QString name = SettingManager::ins()->getKioskMonitorName();
    if (name.isEmpty())
        return nullptr;

    for (QScreen *screen : QGuiApplication::screens()) {
        if (screen->name() == name)
            return screen;
    }
    return nullptr;
}

void App::showKioskOverlay()
{
    QLabel *overlay = new QLabel(this);
    overlay->setText(tr("Press ESC to exit kiosk mode"));
    overlay->setAlignment(Qt::AlignCenter);
    overlay->setObjectName("kioskOverlay");
    QSettings *sv = AppManager::ins()->getStyleValues();
    QString overlayBg = sv ? sv->value("@overlayBackground").toString() : "#A0000000";
    QString overlayText = sv ? sv->value("@overlayText").toString() : "#ffffff";
    overlay->setStyleSheet(
        "background-color: " + overlayBg + ";"
        "color: " + overlayText + ";"
        "font-size: 14pt;"
        "padding: 16px 32px;"
        "border-radius: 8px;"
    );
    overlay->adjustSize();

    QScreen *screen = windowHandle() ? windowHandle()->screen() : qApp->primaryScreen();
    if (screen) {
        int x = (screen->geometry().width() - overlay->width()) / 2;
        int y = (screen->geometry().height() - overlay->height()) / 2;
        overlay->move(x, y);
    }
    overlay->raise();
    overlay->show();

    auto *effect = new QGraphicsOpacityEffect(overlay);
    overlay->setGraphicsEffect(effect);

    auto *fadeOut = new QPropertyAnimation(effect, "opacity");
    fadeOut->setDuration(2000);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);
    fadeOut->setEasingCurve(QEasingCurve::OutCubic);

    QTimer::singleShot(1500, fadeOut, [fadeOut]() {
        fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
    });
    connect(fadeOut, &QPropertyAnimation::finished, overlay, &QLabel::deleteLater);
}

void App::setupCommandPalette()
{
    mCommandPalette = new CommandPalette(this);

    // Navigation commands — iterate slots so titles are available even if
    // pages are not yet constructed (Commit B).
    for (const PageSlot &slot : mPageSlots) {
        QString title = slot.title;
        mCommandPalette->addCommand(title, tr("Navigate"), [this, title]() {
            clickSidebarButton(title, true);
        });
    }

    // Actions
    mCommandPalette->addCommand(tr("Toggle Theme"), tr("Action"), [this]() {
        QString current = SettingManager::ins()->getColorScheme();
        QString next = (current == "light") ? "dark" : "light";
        SettingManager::ins()->setColorScheme(next);
        AppManager::ins()->updateStylesheet();
    });

    mCommandPalette->addCommand(tr("Toggle Sidebar"), tr("Action"), [this]() {
        toggleSidebarCollapse();
    });

    mCommandPalette->addCommand(tr("Kiosk Mode"), tr("Action"), [this]() {
        toggleKioskMode();
    });

    mCommandPalette->addCommand(tr("Quick Clean"), tr("Action"), [this]() {
        clickSidebarButton(tr("System Cleaner"), true);
    });

    mCommandPalette->addCommand(tr("Disk Map"), tr("Action"), [this]() {
        openDiskTreemapDialog();
    });

#ifdef Q_OS_MAC
    // SSO-23857: expose individual Tweaks-pane items so they're invocable
    // without navigating to the Helpers page first. Bool tweaks toggle;
    // everything else resets to its documented default (palette has no
    // input control for picking an arbitrary enum/string/int value).
    for (const MacTweakDef &tweak : MacTweaksCatalog::all()) {
        if (tweak.type == MacDefaultsValueType::Bool) {
            mCommandPalette->addCommand(tr("Tweaks: Toggle %1").arg(tweak.name), tr("Tweaks"),
                [tweak]() { MacTweaksCatalog::toggleBoolTweak(tweak); });
        } else {
            mCommandPalette->addCommand(tr("Tweaks: Reset %1 to Default").arg(tweak.name), tr("Tweaks"),
                [tweak]() { MacTweaksCatalog::resetToDefault(tweak); });
        }
    }
#endif
}
