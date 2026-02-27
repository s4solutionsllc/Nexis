#include "app.h"
#include "ui_app.h"
#include "utilities.h"
#include "signal_mapper.h"
#include "nexis_page.h"
#include "dpi.h"
#include <Managers/cleaner_service.h>
#include <Managers/data_refresh_service.h>
#include "Info/update_info.h"
#include <Utils/format_util.h>
#include <QStyle>
#include <QDebug>
#include <QScreen>
#include <QIcon>
#include <QEvent>
#include <QWindow>
#include <QThreadPool>
#include <QLabel>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QTimer>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>

#ifdef Q_OS_MAC
#include "macos_dock_helper.h"
#endif

App::~App()
{
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
    btn->setFocusPolicy(Qt::NoFocus);
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
    btn->setFocusPolicy(Qt::NoFocus);
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
    mBtnSidebarToggle->setFocusPolicy(Qt::NoFocus);
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
    mSidebarLayout->addSpacing(4);

    // Helper lambda to create a section with header, indicator, and container
    auto addSection = [&](const QString &name) -> SidebarSection & {
        SidebarSection section;
        section.name = name;
        section.collapsed = false;

        section.header = createSectionToggle(name);
        mSidebarLayout->addWidget(section.header);

        auto *indicator = new QFrame(ui->sidebar);
        indicator->setObjectName("sidebarSectionIndicator");
        indicator->setFrameShape(QFrame::HLine);
        indicator->setFixedHeight(1);
        indicator->hide();
        mSectionIndicators.append(indicator);
        mSidebarLayout->addWidget(indicator);

        section.container = new QWidget(ui->sidebar);
        section.containerLayout = new QVBoxLayout(section.container);
        section.containerLayout->setContentsMargins(0, 0, 0, 0);
        section.containerLayout->setSpacing(0);
        mSidebarLayout->addWidget(section.container);

        mSections.append(section);
        return mSections.last();
    };

    // ---- MONITOR section ----
    {
        auto &sec = addSection(tr("MONITOR"));
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

#ifdef Q_OS_MAC
        btnUninstaller = createSidebarButton(tr("Applications"));
#else
        btnUninstaller = createSidebarButton(tr("Uninstaller"));
#endif
        sec.containerLayout->addWidget(btnUninstaller);
        sec.buttons.append(btnUninstaller);
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

    // Spacer pushes feedback/version to bottom
    mSidebarLayout->addStretch();

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
    btnFeedback->setFocusPolicy(Qt::NoFocus);
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
    QScreen *screen = qApp->primaryScreen();
    if (screen) {
        setGeometry(
            QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter,
                size(), screen->availableGeometry())
        );
    }

    ui->horizontalLayout->setContentsMargins(0,0,0,0);
    ui->horizontalLayout->setSpacing(0);

    // Build sidebar programmatically with section headers
    buildSidebar();

    dashboardPage = new DashboardPage(mSlidingStacked);
    hardwareInfoPage = new HardwareInfoPage(mSlidingStacked);
    startupAppsPage = new StartupAppsPage(mSlidingStacked);
    searchPage = new SearchPage(mSlidingStacked);
    systemCleanerPage = new SystemCleanerPage(mSlidingStacked);
    diskToolsPage = new DiskToolsPage(mSlidingStacked);
    servicesPage = new ServicesPage(mSlidingStacked);
    processPage = new ProcessesPage(mSlidingStacked);
    helpersPage = new HelpersPage(mSlidingStacked);
    systemLogsPage = new SystemLogsPage(mSlidingStacked);
    uninstallerPage = new UninstallerPage(mSlidingStacked);
    resourcesPage = new ResourcesPage(mSlidingStacked);
    settingsPage = new SettingsPage(mSlidingStacked);

    ui->pageContentLayout->addWidget(mSlidingStacked);

    // Set button labels
    btnDash->setText(tr("Dashboard"));
    btnHardwareInfo->setText(tr("Hardware Info"));
    btnResources->setText(tr("Resources"));
    btnSystemCleaner->setText(tr("System Cleaner"));
    btnDiskTools->setText(tr("Disk Tools"));
    btnSearch->setText(tr("Search"));
    btnProcesses->setText(tr("Processes"));
    btnServices->setText(tr("Services"));
    btnStartupApps->setText(tr("Startup Apps"));
#ifdef Q_OS_MAC
    btnUninstaller->setText(tr("Applications"));
#else
    btnUninstaller->setText(tr("Uninstaller"));
#endif
    btnDocker->setText(tr("Docker"));
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

    mListPages = {
        dashboardPage, hardwareInfoPage, resourcesPage, systemCleanerPage, diskToolsPage, searchPage,
        processPage, servicesPage, startupAppsPage, uninstallerPage, helpersPage, systemLogsPage, settingsPage
    };

    mListSidebarButtons = {
        btnDash, btnHardwareInfo, btnResources, btnSystemCleaner, btnDiskTools, btnSearch,
        btnProcesses, btnServices, btnStartupApps, btnUninstaller, btnHelpers, btnSystemLogs, btnSettings
    };

    // APT SOURCE MANAGER
    if (ToolManager::ins()->checkSourceRepository()) {
        aptSourceManagerPage = new APTSourceManagerPage(mSlidingStacked);
        int idx = mListSidebarButtons.indexOf(btnSettings);
        mListPages.insert(idx, aptSourceManagerPage);
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

    // DOCKER
    if (ToolManager::ins()->checkDocker()) {
        dockerPage = new DockerPage(mSlidingStacked);
        int dockerIdx = mListSidebarButtons.indexOf(btnHelpers);
        mListPages.insert(dockerIdx, dockerPage);
        mListSidebarButtons.insert(dockerIdx, btnDocker);
    } else {
        btnDocker->hide();
    }

    // GNOME SETTINGS (hidden on macOS — most settings have no valid macOS mapping)
#ifdef Q_OS_MAC
    btnGnomeSettings->hide();
#else
    if (ToolManager::ins()->checkGnomeSettings()) {
        gnomeSettingsPage = new GnomeSettingsPage(mSlidingStacked);
        int settingsIdx = mListSidebarButtons.indexOf(btnSettings);
        mListPages.insert(settingsIdx, gnomeSettingsPage);
        mListSidebarButtons.insert(settingsIdx, btnGnomeSettings);
    } else {
        btnGnomeSettings->hide();
    }
#endif

    // Connect sidebar button clicks to page navigation
    connect(btnDash, &QPushButton::clicked, this, [this]() { pageClick(dashboardPage); });
    connect(btnHardwareInfo, &QPushButton::clicked, this, [this]() { pageClick(hardwareInfoPage); });
    connect(btnResources, &QPushButton::clicked, this, [this]() { pageClick(resourcesPage); });
    connect(btnSystemCleaner, &QPushButton::clicked, this, [this]() { pageClick(systemCleanerPage); });
    connect(btnDiskTools, &QPushButton::clicked, this, [this]() { pageClick(diskToolsPage); });
    connect(btnSearch, &QPushButton::clicked, this, [this]() { pageClick(searchPage); });
    connect(btnProcesses, &QPushButton::clicked, this, [this]() { pageClick(processPage); });
    connect(btnServices, &QPushButton::clicked, this, [this]() { pageClick(servicesPage); });
    connect(btnStartupApps, &QPushButton::clicked, this, [this]() { pageClick(startupAppsPage); });
    connect(btnUninstaller, &QPushButton::clicked, this, [this]() { pageClick(uninstallerPage); });
    connect(btnHelpers, &QPushButton::clicked, this, [this]() { pageClick(helpersPage); });
    connect(btnSystemLogs, &QPushButton::clicked, this, [this]() { pageClick(systemLogsPage); });
    connect(btnSettings, &QPushButton::clicked, this, [this]() { pageClick(settingsPage); });
    connect(btnFeedback, &QPushButton::clicked, this, [this]() {
        if (feedback.isNull())
            feedback = QSharedPointer<Feedback>(new Feedback(this));
        feedback->show();
    });

    // Conditional page button clicks
    if (ToolManager::ins()->checkDocker())
        connect(btnDocker, &QPushButton::clicked, this, [this]() { pageClick(dockerPage); });
    if (ToolManager::ins()->checkSourceRepository())
        connect(btnAptSourceManager, &QPushButton::clicked, this, [this]() { pageClick(aptSourceManagerPage); });
#ifndef Q_OS_MAC
    if (ToolManager::ins()->checkGnomeSettings())
        connect(btnGnomeSettings, &QPushButton::clicked, this, [this]() { pageClick(gnomeSettingsPage); });
#endif

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

    // Add pages to stacked widget
    for (QWidget *page : mListPages)
        mSlidingStacked->addWidget(page);

    DataRefreshService::ins()->start();

    AppManager::ins()->updateStylesheet();

    Utilities::addDropShadow(ui->sidebar, 60);

    // Set start page
    clickSidebarButton(SettingManager::ins()->getStartPage());

    createTrayActions();

    mTrayIcon->show();

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

    // Restore kiosk mode from last session
    if (SettingManager::ins()->getKioskMode())
        applyKioskMode(true);

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

    // Relay CleanerService signals through SignalMapper
    connect(CleanerService::ins(), &CleanerService::cleaningStarted,
            SignalMapper::ins(), &SignalMapper::sigScheduledCleanStarted);
    connect(CleanerService::ins(), &CleanerService::cleaningFinished,
            this, [this](CleanerService::CleanResult result) {
        emit SignalMapper::ins()->sigScheduledCleanFinished(
            result.totalBytesFreed, result.totalFilesRemoved);

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
    if (SettingManager::ins()->getMinimizeToTray()) {
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
        if (SettingManager::ins()->getMinimizeToTray()) {
            emit SignalMapper::ins()->sigAppVisibilityChanged(false);
            hide();
#ifdef Q_OS_MAC
            nexis_macos_hide_dock_icon();
#endif
            event->ignore();
            return;
        }
    }
    QMainWindow::changeEvent(event);
}

void App::createTrayActions()
{
    for (QPushButton *button : mListSidebarButtons) {
        QString toolTip = button->toolTip();
        QAction *action = new QAction(toolTip, this);
        connect(action, &QAction::triggered, [=] {
            clickSidebarButton(toolTip, true);
        });

        mTrayMenu->addAction(action);
    }

    connect(mTrayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason) {
#ifdef Q_OS_MAC
        nexis_macos_show_dock_icon();
#endif
        setWindowState(windowState() & ~Qt::WindowMinimized);
        show();
        if (windowHandle())
            windowHandle()->requestActivate();
        emit SignalMapper::ins()->sigAppVisibilityChanged(true);
    });

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
        pageClick(mListPages.first());
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
    for (QWidget *page : mListPages) {
        if (page->windowTitle() == title) {
            return page;
        }
    }
    return nullptr;
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
    setIcon(btnSystemCleaner,    "cleaner.svg");
    setIcon(btnDiskTools,        "disk-tools.svg");
    setIcon(btnSearch,           "search.svg");
    setIcon(btnProcesses,        "process.svg");
    setIcon(btnServices,         "services.svg");
    setIcon(btnStartupApps,      "startup-apps.svg");
    setIcon(btnUninstaller,      "uninstaller.svg");
    setIcon(btnDocker,           "docker.svg");
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
    emit SignalMapper::ins()->sigSidebarCollapseToggled(mSidebarCollapsed);
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

        minAnim->start(QAbstractAnimation::DeleteWhenStopped);
        maxAnim->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
        ui->sidebar->setMinimumWidth(targetWidth);
        ui->sidebar->setMaximumWidth(targetWidth);
    }

    // Toggle section headers, containers, and indicators
    for (int i = 0; i < mSections.size(); ++i) {
        mSections[i].header->setVisible(!collapsed);
        mSections[i].container->setVisible(!mSections[i].collapsed);
    }

    for (QFrame *indicator : mSectionIndicators)
        indicator->setVisible(false);

    // Toggle version label
    if (mVersionLabel)
        mVersionLabel->setVisible(!collapsed);

    // Reposition badges after layout settles
    QTimer::singleShot(0, this, &App::repositionBadges);

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
        sec.header->style()->unpolish(sec.header);
        sec.header->style()->polish(sec.header);
    }
    btnFeedback->style()->unpolish(btnFeedback);
    btnFeedback->style()->polish(btnFeedback);
    mBtnSidebarToggle->style()->unpolish(mBtnSidebarToggle);
    mBtnSidebarToggle->style()->polish(mBtnSidebarToggle);
}

void App::toggleSection(int sectionIndex)
{
    if (sectionIndex < 0 || sectionIndex >= mSections.size())
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

    // Reposition badges after layout settles (handles hide/show + coordinates)
    QTimer::singleShot(0, this, &App::repositionBadges);

    if (mSidebarCollapsed)
        return;

    QWidget *container = sec.container;

    if (!animate) {
        container->setVisible(!collapsed);
        container->setMaximumHeight(collapsed ? 0 : 16777215);
        updateSectionChevrons();
        return;
    }

    if (collapsed) {
        int startHeight = container->height();
        auto *anim = new QPropertyAnimation(container, "maximumHeight", this);
        anim->setDuration(200);
        anim->setStartValue(startHeight);
        anim->setEndValue(0);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(anim, &QPropertyAnimation::finished, this, [container]() {
            container->setVisible(false);
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
        connect(anim, &QPropertyAnimation::finished, this, [container]() {
            container->setMaximumHeight(16777215);
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
    for (const auto &sec : mSections)
        obj[sec.name] = sec.collapsed;
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
        QString chevronName = sec.collapsed ? "section-expand.svg" : "section-collapse.svg";
        QString chevronPath = QString(":/static/themes/%1/img/sidebar-icons/%2").arg(theme, chevronName);
        sec.header->setIcon(QIcon(chevronPath));
    }
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

    // Navigation commands
    for (QWidget *page : mListPages) {
        QString title = page->windowTitle();
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
}
