#include "app.h"
#include "ui_app.h"
#include "utilities.h"
#include "signal_mapper.h"
#include "nexis_page.h"
#include "dpi.h"
#include <Managers/cleaner_service.h>
#include <Managers/data_refresh_service.h>
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
#include <QHBoxLayout>
#include <QVBoxLayout>

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

QLabel *App::createSectionHeader(const QString &text)
{
    auto *label = new QLabel(text, ui->sidebar);
    label->setObjectName("sectionHeader");
    mSectionHeaders.append(label);
    return label;
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

    mBtnSidebarToggle = new QPushButton(ui->sidebar);
    mBtnSidebarToggle->setObjectName("btnSidebarToggle");
    mBtnSidebarToggle->setCursor(Qt::PointingHandCursor);
    mBtnSidebarToggle->setFocusPolicy(Qt::NoFocus);
    mBtnSidebarToggle->setCheckable(false);
    mBtnSidebarToggle->setIconSize(Dpi::scale(16, 16));
    mBtnSidebarToggle->setFixedSize(Dpi::scale(28, 28));
    connect(mBtnSidebarToggle, &QPushButton::clicked, this, &App::toggleSidebarCollapse);
    logoRow->addWidget(mBtnSidebarToggle);

    mSidebarLayout->addLayout(logoRow);

    // Logo separator line (#6)
    mLogoSeparator = new QFrame(ui->sidebar);
    mLogoSeparator->setObjectName("sidebarDividerLine");
    mLogoSeparator->setFrameShape(QFrame::HLine);
    mLogoSeparator->setFixedHeight(1);
    mSidebarLayout->addWidget(mLogoSeparator);
    mSidebarLayout->addSpacing(4);

    // ---- MONITOR section ----
    mSidebarLayout->addWidget(createSectionHeader(tr("MONITOR")));
    {
        auto *indicator = new QFrame(ui->sidebar);
        indicator->setObjectName("sidebarSectionIndicator");
        indicator->setFrameShape(QFrame::HLine);
        indicator->setFixedHeight(1);
        indicator->hide();
        mSectionIndicators.append(indicator);
        mSidebarLayout->addWidget(indicator);
    }

    btnDash = createSidebarButton(tr("Dashboard"));
    btnDash->setChecked(true);
    mSidebarLayout->addWidget(btnDash);

    btnHardwareInfo = createSidebarButton(tr("Hardware Info"));
    mSidebarLayout->addWidget(btnHardwareInfo);

    btnResources = createSidebarButton(tr("Resources"));
    mSidebarLayout->addWidget(btnResources);

    // ---- MANAGE section ----
    mSidebarLayout->addWidget(createSectionHeader(tr("MANAGE")));
    {
        auto *indicator = new QFrame(ui->sidebar);
        indicator->setObjectName("sidebarSectionIndicator");
        indicator->setFrameShape(QFrame::HLine);
        indicator->setFixedHeight(1);
        indicator->hide();
        mSectionIndicators.append(indicator);
        mSidebarLayout->addWidget(indicator);
    }

    btnSystemCleaner = createSidebarButton(tr("System Cleaner"));
    mSidebarLayout->addWidget(btnSystemCleaner);

    btnSearch = createSidebarButton(tr("Search"));
    mSidebarLayout->addWidget(btnSearch);

    btnProcesses = createSidebarButton(tr("Processes"));
    mSidebarLayout->addWidget(btnProcesses);

    btnServices = createSidebarButton(tr("Services"));
    mSidebarLayout->addWidget(btnServices);

    btnStartupApps = createSidebarButton(tr("Startup Apps"));
    mSidebarLayout->addWidget(btnStartupApps);

#ifdef Q_OS_MAC
    btnUninstaller = createSidebarButton(tr("Applications"));
#else
    btnUninstaller = createSidebarButton(tr("Uninstaller"));
#endif
    mSidebarLayout->addWidget(btnUninstaller);

    // ---- SYSTEM section ----
    mSidebarLayout->addWidget(createSectionHeader(tr("SYSTEM")));
    {
        auto *indicator = new QFrame(ui->sidebar);
        indicator->setObjectName("sidebarSectionIndicator");
        indicator->setFrameShape(QFrame::HLine);
        indicator->setFixedHeight(1);
        indicator->hide();
        mSectionIndicators.append(indicator);
        mSidebarLayout->addWidget(indicator);
    }

    btnDocker = createSidebarButton(tr("Docker"));
    mSidebarLayout->addWidget(btnDocker);

    btnHelpers = createSidebarButton(tr("Helpers"));
    mSidebarLayout->addWidget(btnHelpers);

#ifdef Q_OS_MAC
    btnAptSourceManager = createSidebarButton(tr("Homebrew"));
#else
    btnAptSourceManager = createSidebarButton(tr("APT Repository Manager"));
#endif
    mSidebarLayout->addWidget(btnAptSourceManager);

    btnGnomeSettings = createSidebarButton(tr("GNOME Settings"));
    mSidebarLayout->addWidget(btnGnomeSettings);

    btnSettings = createSidebarButton(tr("Settings"));
    mSidebarLayout->addWidget(btnSettings);

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
    servicesPage = new ServicesPage(mSlidingStacked);
    processPage = new ProcessesPage(mSlidingStacked);
    helpersPage = new HelpersPage(mSlidingStacked);
    uninstallerPage = new UninstallerPage(mSlidingStacked);
    resourcesPage = new ResourcesPage(mSlidingStacked);
    settingsPage = new SettingsPage(mSlidingStacked);

    ui->pageContentLayout->addWidget(mSlidingStacked);

    // Set button labels
    btnDash->setText(tr("Dashboard"));
    btnHardwareInfo->setText(tr("Hardware Info"));
    btnResources->setText(tr("Resources"));
    btnSystemCleaner->setText(tr("System Cleaner"));
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
#ifdef Q_OS_MAC
    btnAptSourceManager->setText(tr("Homebrew"));
#else
    btnAptSourceManager->setText(tr("APT Repository Manager"));
#endif
    btnGnomeSettings->setText(tr("GNOME Settings"));
    btnSettings->setText(tr("Settings"));
    btnFeedback->setText(tr("Feedback"));

    mListPages = {
        dashboardPage, hardwareInfoPage, resourcesPage, systemCleanerPage, searchPage,
        processPage, servicesPage, startupAppsPage, uninstallerPage, helpersPage, settingsPage
    };

    mListSidebarButtons = {
        btnDash, btnHardwareInfo, btnResources, btnSystemCleaner, btnSearch,
        btnProcesses, btnServices, btnStartupApps, btnUninstaller, btnHelpers, btnSettings
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

    // DOCKER
    if (ToolManager::ins()->checkDocker()) {
        dockerPage = new DockerPage(mSlidingStacked);
        int dockerIdx = mListSidebarButtons.indexOf(btnHelpers);
        mListPages.insert(dockerIdx, dockerPage);
        mListSidebarButtons.insert(dockerIdx, btnDocker);
    } else {
        btnDocker->hide();
    }

    // GNOME SETTINGS
    if (ToolManager::ins()->checkGnomeSettings()) {
        gnomeSettingsPage = new GnomeSettingsPage(mSlidingStacked);
        int settingsIdx = mListSidebarButtons.indexOf(btnSettings);
        mListPages.insert(settingsIdx, gnomeSettingsPage);
        mListSidebarButtons.insert(settingsIdx, btnGnomeSettings);
    } else {
        btnGnomeSettings->hide();
    }

    // Connect sidebar button clicks to page navigation
    connect(btnDash, &QPushButton::clicked, this, [this]() { pageClick(dashboardPage); });
    connect(btnHardwareInfo, &QPushButton::clicked, this, [this]() { pageClick(hardwareInfoPage); });
    connect(btnResources, &QPushButton::clicked, this, [this]() { pageClick(resourcesPage); });
    connect(btnSystemCleaner, &QPushButton::clicked, this, [this]() { pageClick(systemCleanerPage); });
    connect(btnSearch, &QPushButton::clicked, this, [this]() { pageClick(searchPage); });
    connect(btnProcesses, &QPushButton::clicked, this, [this]() { pageClick(processPage); });
    connect(btnServices, &QPushButton::clicked, this, [this]() { pageClick(servicesPage); });
    connect(btnStartupApps, &QPushButton::clicked, this, [this]() { pageClick(startupAppsPage); });
    connect(btnUninstaller, &QPushButton::clicked, this, [this]() { pageClick(uninstallerPage); });
    connect(btnHelpers, &QPushButton::clicked, this, [this]() { pageClick(helpersPage); });
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
    if (ToolManager::ins()->checkGnomeSettings())
        connect(btnGnomeSettings, &QPushButton::clicked, this, [this]() { pageClick(gnomeSettingsPage); });

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

    // Dashboard kiosk toggle button -> App::toggleKioskMode
    connect(SignalMapper::ins(), &SignalMapper::sigKioskToggleRequested,
            this, &App::toggleKioskMode);

    // Update System Cleaner badge when cleanable size changes
    connect(SignalMapper::ins(), &SignalMapper::sigCleanableSizeChanged,
            this, [this](quint64 bytes) {
        if (bytes > 0) {
            QString text = FormatUtil::formatBytes(bytes);
            mCleanerBadge->setText(text);
            if (!mSidebarCollapsed) {
                mCleanerBadge->show();
                mCleanerBadgeDot->hide();
            } else {
                mCleanerBadge->hide();
                mCleanerBadgeDot->show();
            }
            // Position badge relative to btnSystemCleaner
            QPoint btnPos = btnSystemCleaner->pos();
            int btnW = btnSystemCleaner->width();
            mCleanerBadge->move(btnPos.x() + btnW - mCleanerBadge->width() - 8,
                                btnPos.y() + 2);
            mCleanerBadgeDot->move(btnPos.x() + btnW - 16,
                                   btnPos.y() + 4);
        } else {
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
    mTrayIcon->hide();
    event->accept();

    QThreadPool::globalInstance()->waitForDone();

    qApp->quit();
}

void App::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange && windowState().testFlag(Qt::WindowMinimized)) {
        emit SignalMapper::ins()->sigAppVisibilityChanged(false);
        hide();
        event->ignore();
        return;
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

    auto setIcon = [&](QPushButton *btn, const QString &sysTheme, const QString &iconName) {
        QString svgPath = QString(":/static/themes/%1/img/sidebar-icons/%2").arg(theme, iconName);
#ifdef Q_OS_MAC
        btn->setIcon(QIcon(svgPath));
#else
        btn->setIcon(QIcon::fromTheme(sysTheme, QIcon(svgPath)));
#endif
        btn->setIconSize(Dpi::scale(20, 20));
    };

    setIcon(btnDash,             "utilities-system-monitor",       "dash.svg");
    setIcon(btnHardwareInfo,     "computer",                       "hardware-info.svg");
    setIcon(btnResources,        "preferences-system",             "resources.svg");
    setIcon(btnSystemCleaner,    "edit-clear-all",                 "cleaner.svg");
    setIcon(btnSearch,           "edit-find",                      "search.svg");
    setIcon(btnProcesses,        "view-list",                      "process.svg");
    setIcon(btnServices,         "system-run",                     "services.svg");
    setIcon(btnStartupApps,      "media-playback-start",           "startup-apps.svg");
    setIcon(btnUninstaller,      "edit-delete",                    "uninstaller.svg");
    setIcon(btnDocker,           "docker",                         "docker.svg");
    setIcon(btnHelpers,          "preferences-other",              "helpers.svg");
    setIcon(btnAptSourceManager, "system-software-install",        "ppa-manager.svg");
    setIcon(btnGnomeSettings,    "preferences-desktop-appearance", "gnome-settings.svg");
    setIcon(btnSettings,         "applications-system",            "settings.svg");
    setIcon(btnFeedback,         "mail-message-new",               "feedback.svg");

    // Sidebar toggle icon
    if (mSidebarCollapsed)
        mBtnSidebarToggle->setIcon(QIcon(QString(":/static/themes/%1/img/sidebar-icons/sidebar-expand.svg").arg(theme)));
    else
        mBtnSidebarToggle->setIcon(QIcon(QString(":/static/themes/%1/img/sidebar-icons/sidebar-collapse.svg").arg(theme)));
    mBtnSidebarToggle->setIconSize(Dpi::scale(16, 16));

    // Sidebar logo
    QString logoFile = mSidebarCollapsed ? "sidebar-logo-collapsed.svg" : "sidebar-logo.svg";
    QString logoPath = QString(":/static/themes/%1/img/sidebar-icons/%2").arg(theme, logoFile);
    QPixmap logoPix(logoPath);
    if (!logoPix.isNull())
        mLogoLabel->setPixmap(logoPix.scaledToHeight(Dpi::scale(20), Qt::SmoothTransformation));
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

    // Toggle section headers visibility
    for (QLabel *header : mSectionHeaders)
        header->setVisible(!collapsed);

    // Toggle section indicators (visible only when collapsed)
    for (QFrame *indicator : mSectionIndicators)
        indicator->setVisible(collapsed);

    // Toggle version label
    if (mVersionLabel)
        mVersionLabel->setVisible(!collapsed);

    // Toggle badge vs dot
    if (mCleanerBadge && mCleanerBadge->text().length() > 0) {
        mCleanerBadge->setVisible(!collapsed);
        mCleanerBadgeDot->setVisible(collapsed);
    }

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
    if (collapsed)
        mBtnSidebarToggle->setIcon(QIcon(QString(":/static/themes/%1/img/sidebar-icons/sidebar-expand.svg").arg(theme)));
    else
        mBtnSidebarToggle->setIcon(QIcon(QString(":/static/themes/%1/img/sidebar-icons/sidebar-collapse.svg").arg(theme)));

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
    overlay->setStyleSheet(
        "background-color: rgba(0, 0, 0, 160);"
        "color: white;"
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
        QString current = SettingManager::ins()->getThemeName();
        QString next = (current == "default") ? "light" : "default";
        SettingManager::ins()->setThemeName(next);
        AppManager::ins()->updateStylesheet();
        emit SignalMapper::ins()->sigChangedAppTheme();
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
