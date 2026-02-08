#include "app.h"
#include "ui_app.h"
#include "utilities.h"
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

    // Set sidebar icons from system theme with bundled fallbacks
    auto setIcon = [](QPushButton *btn, const QString &themeName, const QString &fallback, const QString &text) {
        btn->setIcon(QIcon::fromTheme(themeName, QIcon(fallback)));
        btn->setText(text);
        btn->setIconSize(QSize(20, 20));
    };

    setIcon(ui->btnDash,             "utilities-system-monitor", ":/static/themes/default/img/sidebar-icons/dash.png",         tr("Dashboard"));
    setIcon(ui->btnStartupApps,      "media-playback-start",     ":/static/themes/default/img/sidebar-icons/startup-apps.png", tr("Startup Apps"));
    setIcon(ui->btnSystemCleaner,    "edit-clear-all",           ":/static/themes/default/img/sidebar-icons/cleaner.png",      tr("System Cleaner"));
    setIcon(ui->btnSearch,           "edit-find",                ":/static/themes/default/img/sidebar-icons/search.png",        tr("Search"));
    setIcon(ui->btnServices,         "system-run",               ":/static/themes/default/img/sidebar-icons/services.png",      tr("Services"));
    setIcon(ui->btnProcesses,        "view-list-details",        ":/static/themes/default/img/sidebar-icons/process.png",       tr("Processes"));
    setIcon(ui->btnHelpers,          "preferences-other",        ":/static/themes/default/img/sidebar-icons/helpers.png",       tr("Helpers"));
    setIcon(ui->btnUninstaller,      "edit-delete",              ":/static/themes/default/img/sidebar-icons/uninstaller.png",   tr("Uninstaller"));
    setIcon(ui->btnResources,        "preferences-system",       ":/static/themes/default/img/sidebar-icons/resources.png",     tr("Resources"));
    setIcon(ui->btnAptSourceManager, "system-software-install",  ":/static/themes/default/img/sidebar-icons/ppa-manager.png",  tr("APT Repository Manager"));
    setIcon(ui->btnGnomeSettings,    "preferences-desktop-appearance", ":/static/themes/default/img/sidebar-icons/gnome-settings.png", tr("GNOME Settings"));
    setIcon(ui->btnSettings,         "preferences-desktop",      ":/static/themes/default/img/sidebar-icons/settings.png",      tr("Settings"));
    setIcon(ui->btnFeedback,         "mail-message-new",         ":/static/themes/default/img/sidebar-icons/feedback.png",      tr("Feedback"));

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
