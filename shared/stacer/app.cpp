#include "app.h"
#include "ui_app.h"
#include "utilities.h"
#include "signal_mapper.h"
#include <QStyle>
#include <QDebug>
#include <QScreen>
#include <QIcon>
#include <QEvent>

App::~App()
{
    delete ui;
}

App::App(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::App),
    mSlidingStacked(new SlidingStackedWidget(this)),
    mTrayIcon(AppManager::ins()->getTrayIcon()),
    mTrayMenu(new QMenu(this))
{
    ui->setupUi(this);

    init();
}

void App::init()
{
    setGeometry(
        QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter,
            size(), qApp->primaryScreen()->availableGeometry())
    );

    // form settings
    ui->horizontalLayout->setContentsMargins(0,0,0,0);
    ui->horizontalLayout->setSpacing(0);

    dashboardPage = new DashboardPage(mSlidingStacked);
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
        dashboardPage, startupAppsPage, systemCleanerPage, searchPage, servicesPage,
        processPage, uninstallerPage, resourcesPage, helpersPage, settingsPage
    };

    mListSidebarButtons = {
        ui->btnDash, ui->btnStartupApps, ui->btnSystemCleaner, ui->btnSearch, ui->btnServices,
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
    ui->btnStartupApps->setText(tr("Startup Apps"));
    ui->btnSystemCleaner->setText(tr("System Cleaner"));
    ui->btnSearch->setText(tr("Search"));
    ui->btnServices->setText(tr("Services"));
    ui->btnProcesses->setText(tr("Processes"));
    ui->btnHelpers->setText(tr("Helpers"));
    ui->btnUninstaller->setText(tr("Uninstaller"));
    ui->btnResources->setText(tr("Resources"));
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

    AppManager::ins()->updateStylesheet();

    Utilities::addDropShadow(ui->sidebar, 60);

    // set start page
    clickSidebarButton(SettingManager::ins()->getStartPage());

    createTrayActions();

    mTrayIcon->show();
}

void App::closeEvent(QCloseEvent *event)
{
    mTrayIcon->hide();
    event->accept();
    qApp->quit();
}

void App::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange && windowState().testFlag(Qt::WindowMinimized)) {
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
        raise();
        activateWindow();
    });

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
    if (isShow) activateWindow();
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
        ui->pageTitle->setText(widget->windowTitle());
        if (slide) {
            mSlidingStacked->slideInIdx(mSlidingStacked->indexOf(widget));
        } else {
            mSlidingStacked->setCurrentWidget(widget);
        }
    }
}

void App::on_btnDash_clicked()
{
    pageClick(dashboardPage);
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
        btn->setIconSize(QSize(20, 20));
    };

    setIcon(ui->btnDash,             "utilities-system-monitor",       "dash.svg");
    setIcon(ui->btnStartupApps,      "media-playback-start",           "startup-apps.svg");
    setIcon(ui->btnSystemCleaner,    "edit-clear-all",                 "cleaner.svg");
    setIcon(ui->btnSearch,           "edit-find",                      "search.svg");
    setIcon(ui->btnServices,         "system-run",                     "services.svg");
    setIcon(ui->btnProcesses,        "view-list",                      "process.svg");
    setIcon(ui->btnHelpers,          "preferences-other",              "helpers.svg");
    setIcon(ui->btnUninstaller,      "edit-delete",                    "uninstaller.svg");
    setIcon(ui->btnResources,        "preferences-system",             "resources.svg");
    setIcon(ui->btnAptSourceManager, "system-software-install",        "ppa-manager.svg");
    setIcon(ui->btnGnomeSettings,    "preferences-desktop-appearance", "gnome-settings.svg");
    setIcon(ui->btnSettings,         "applications-system",            "settings.svg");
    setIcon(ui->btnFeedback,         "mail-message-new",               "feedback.svg");
}
