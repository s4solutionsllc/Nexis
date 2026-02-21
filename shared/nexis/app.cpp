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

App::~App()
{
    delete ui;
}

App::App(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::App),
    mSlidingStacked(new SlidingStackedWidget(this)),
    mKioskMode(false),
    mTrayIcon(AppManager::ins()->getTrayIcon()),
    mTrayMenu(new QMenu(this))
{
    ui->setupUi(this);

    init();
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

    // form settings
    ui->horizontalLayout->setContentsMargins(0,0,0,0);
    ui->horizontalLayout->setSpacing(0);

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

    mListPages = {
        dashboardPage, hardwareInfoPage, startupAppsPage, systemCleanerPage, searchPage, servicesPage,
        processPage, uninstallerPage, resourcesPage, helpersPage, settingsPage
    };

    mListSidebarButtons = {
        ui->btnDash, ui->btnHardwareInfo, ui->btnStartupApps, ui->btnSystemCleaner, ui->btnSearch, ui->btnServices,
        ui->btnProcesses, ui->btnHelpers, ui->btnUninstaller, ui->btnResources, ui->btnSettings
    };

    // APT SOURCE MANAGER
    if (ToolManager::ins()->checkSourceRepository()) {
        aptSourceManagerPage = new APTSourceManagerPage(mSlidingStacked);
        mListPages.insert(7, aptSourceManagerPage);
        mListSidebarButtons.insert(7, ui->btnAptSourceManager);
    } else {
        ui->btnAptSourceManager->hide();
    }

    // DOCKER
    if (ToolManager::ins()->checkDocker()) {
        dockerPage = new DockerPage(mSlidingStacked);
        int dockerIdx = mListSidebarButtons.indexOf(ui->btnHelpers);
        mListPages.insert(dockerIdx, dockerPage);
        mListSidebarButtons.insert(dockerIdx, ui->btnDocker);
    } else {
        ui->btnDocker->hide();
    }

    // GNOME SETTINGS
    if (ToolManager::ins()->checkGnomeSettings()) {
        gnomeSettingsPage = new GnomeSettingsPage(mSlidingStacked);
        // Insert before Settings button (which is always last in the list before spacer)
        int settingsIdx = mListSidebarButtons.indexOf(ui->btnSettings);
        mListPages.insert(settingsIdx, gnomeSettingsPage);
        mListSidebarButtons.insert(settingsIdx, ui->btnGnomeSettings);
    } else {
        ui->btnGnomeSettings->hide();
    }

    // Set sidebar button labels
    ui->btnDash->setText(tr("Dashboard"));
    ui->btnHardwareInfo->setText(tr("Hardware Info"));
    ui->btnStartupApps->setText(tr("Startup Apps"));
    ui->btnSystemCleaner->setText(tr("System Cleaner"));
    ui->btnSearch->setText(tr("Search"));
    ui->btnServices->setText(tr("Services"));
    ui->btnProcesses->setText(tr("Processes"));
    ui->btnHelpers->setText(tr("Helpers"));
#ifdef Q_OS_MAC
    ui->btnUninstaller->setText(tr("Applications"));
    ui->btnUninstaller->setToolTip(tr("Applications"));
#else
    ui->btnUninstaller->setText(tr("Uninstaller"));
#endif
    ui->btnResources->setText(tr("Resources"));
    ui->btnDocker->setText(tr("Docker"));
#ifdef Q_OS_MAC
    ui->btnAptSourceManager->setText(tr("Homebrew"));
#else
    ui->btnAptSourceManager->setText(tr("APT Repository Manager"));
#endif
    ui->btnGnomeSettings->setText(tr("GNOME Settings"));
    ui->btnSettings->setText(tr("Settings"));
    ui->btnFeedback->setText(tr("Feedback"));

    // Refresh sidebar icons when theme changes
    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &App::updateSidebarIcons);

    // add pages
    for (QWidget *page: mListPages) {
        mSlidingStacked->addWidget(page);
    }

    DataRefreshService::ins()->start();

    AppManager::ins()->updateStylesheet();

    Utilities::addDropShadow(ui->sidebar, 60);

    // set start page
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

    // Restore kiosk mode from last session
    if (SettingManager::ins()->getKioskMode())
        applyKioskMode(true);

    // Dashboard kiosk toggle button → App::toggleKioskMode
    connect(SignalMapper::ins(), &SignalMapper::sigKioskToggleRequested,
            this, &App::toggleKioskMode);

    // Relay CleanerService signals through SignalMapper for app-wide access
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

    // Wait for background threads (scans, uninstalls, etc.) to finish
    // so in-progress operations aren't interrupted (BUG-05)
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
    for (QPushButton *button: mListSidebarButtons) {
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
    for (QPushButton *button: mListSidebarButtons) {
        if (button->toolTip() == text) {
            button->setChecked(true);
        }
    }
}

QWidget* App::getPageByTitle(const QString &title)
{
    for (QWidget *page: mListPages) {
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

        ui->pageTitle->setText(widget->windowTitle());
        if (slide) {
            mSlidingStacked->slideInIdx(mSlidingStacked->indexOf(widget));
        } else {
            mSlidingStacked->setCurrentWidget(widget);
        }

        if (auto *page = qobject_cast<NexisPage*>(widget))
            page->onPageActivated();
    }
}

void App::on_btnDash_clicked()
{
    pageClick(dashboardPage);
}

void App::on_btnHardwareInfo_clicked()
{
    pageClick(hardwareInfoPage);
}

void App::on_btnStartupApps_clicked()
{
    pageClick(startupAppsPage);
}

void App::on_btnSystemCleaner_clicked()
{
    pageClick(systemCleanerPage);
}

void App::on_btnSearch_clicked()
{
    pageClick(searchPage);
}

void App::on_btnServices_clicked()
{
    pageClick(servicesPage);
}

void App::on_btnUninstaller_clicked()
{
    pageClick(uninstallerPage);
}

void App::on_btnProcesses_clicked()
{
    pageClick(processPage);
}

void App::on_btnResources_clicked()
{
    pageClick(resourcesPage);
}

void App::on_btnDocker_clicked()
{
    pageClick(dockerPage);
}

void App::on_btnHelpers_clicked()
{
    pageClick(helpersPage);
}

void App::on_btnAptSourceManager_clicked()
{
    pageClick(aptSourceManagerPage);
}

void App::on_btnGnomeSettings_clicked()
{
    pageClick(gnomeSettingsPage);
}

void App::on_btnSettings_clicked()
{
    pageClick(settingsPage);
}

void App::on_btnFeedback_clicked()
{
    if (feedback.isNull()) {
        feedback = QSharedPointer<Feedback>(new Feedback(this));
    }
    feedback->show();
}

void App::updateSidebarIcons()
{
    QString theme = AppManager::ins()->resolveThemeName();

    auto setIcon = [&](QPushButton *btn, const QString &sysTheme, const QString &iconName) {
        QString svgPath = QString(":/static/themes/%1/img/sidebar-icons/%2").arg(theme, iconName);
#ifdef Q_OS_MAC
        // macOS: always use bundled SVGs — Adwaita symbolic icons are greyscale
        // and Qt doesn't recolor them like GNOME does.
        btn->setIcon(QIcon(svgPath));
#else
        btn->setIcon(QIcon::fromTheme(sysTheme, QIcon(svgPath)));
#endif
        btn->setIconSize(Dpi::scale(20, 20));
    };

    setIcon(ui->btnDash,             "utilities-system-monitor",       "dash.svg");
    setIcon(ui->btnHardwareInfo,     "computer",                       "hardware-info.svg");
    setIcon(ui->btnStartupApps,      "media-playback-start",           "startup-apps.svg");
    setIcon(ui->btnSystemCleaner,    "edit-clear-all",                 "cleaner.svg");
    setIcon(ui->btnSearch,           "edit-find",                      "search.svg");
    setIcon(ui->btnServices,         "system-run",                     "services.svg");
    setIcon(ui->btnProcesses,        "view-list",                      "process.svg");
    setIcon(ui->btnHelpers,          "preferences-other",              "helpers.svg");
    setIcon(ui->btnUninstaller,      "edit-delete",                    "uninstaller.svg");
    setIcon(ui->btnResources,        "preferences-system",             "resources.svg");
    setIcon(ui->btnDocker,           "docker",                         "docker.svg");
    setIcon(ui->btnAptSourceManager, "system-software-install",        "ppa-manager.svg");
    setIcon(ui->btnGnomeSettings,    "preferences-desktop-appearance", "gnome-settings.svg");
    setIcon(ui->btnSettings,         "applications-system",            "settings.svg");
    setIcon(ui->btnFeedback,         "mail-message-new",               "feedback.svg");
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
        ui->sidebar->hide();
        ui->pageTitle->hide();
        pageClick(dashboardPage, false);
        showFullScreen();
        showKioskOverlay();
    } else {
        showNormal();
        ui->sidebar->show();
        ui->pageTitle->show();
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
