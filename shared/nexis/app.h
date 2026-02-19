#ifndef APP_H
#define APP_H

#include <QMainWindow>
#include <QAction>

#include "sliding_stacked_widget.h"
#include "Managers/app_manager.h"
#include "Managers/setting_manager.h"

// Pages
#include "Pages/Dashboard/dashboard_page.h"
#include "Pages/StartupApps/startup_apps_page.h"
#include "Pages/SystemCleaner/system_cleaner_page.h"
#include "Pages/Services/services_page.h"
#include "Pages/Processes/processes_page.h"
#include "Pages/Uninstaller/uninstaller_page.h"
#include "Pages/Resources/resources_page.h"
#include "Pages/Settings/settings_page.h"
#include "Pages/AptSourceManager/apt_source_manager_page.h"
#include "Pages/GnomeSettings/gnome_settings_page.h"
#include "Pages/Search/search_page.h"
#include "Pages/Helpers/helpers_page.h"
#include "Pages/HardwareInfo/hardware_info_page.h"
#include "Pages/Docker/docker_page.h"
#include "feedback.h"

namespace Ui {
    class App;
}

class App : public QMainWindow
{
    Q_OBJECT

public:
    explicit App(QWidget *parent = 0);
    ~App();

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;

private slots:
    void init();
    void pageClick(QWidget *widget, bool slide = true);
    void clickSidebarButton(QString pageTitle, bool isShow = false);

    void on_btnDash_clicked();
    void on_btnHardwareInfo_clicked();
    void on_btnSystemCleaner_clicked();
    void on_btnStartupApps_clicked();
    void on_btnServices_clicked();
    void on_btnSearch_clicked();
    void on_btnUninstaller_clicked();
    void on_btnHelpers_clicked();
    void on_btnResources_clicked();
    void on_btnProcesses_clicked();
    void on_btnSettings_clicked();
    void on_btnAptSourceManager_clicked();
    void on_btnDocker_clicked();
    void on_btnGnomeSettings_clicked();

    void on_btnFeedback_clicked();

    void toggleKioskMode();
    void exitKioskMode();

private:
    QWidget *getPageByTitle(const QString &title);
    void checkSidebarButtonByTooltip(const QString &text);
    void createTrayActions();
    void updateSidebarIcons();
    void applyKioskMode(bool enable);
    void showKioskOverlay();

private:
    Ui::App *ui;

    // Pages
    QList<QWidget*> mListPages;
    QList<QPushButton*> mListSidebarButtons;

    SlidingStackedWidget *mSlidingStacked;

    DashboardPage *dashboardPage;
    HardwareInfoPage *hardwareInfoPage;
    StartupAppsPage *startupAppsPage;
    SystemCleanerPage *systemCleanerPage;
    SearchPage *searchPage;
    ServicesPage *servicesPage;
    ProcessesPage *processPage;
    UninstallerPage *uninstallerPage;
    ResourcesPage *resourcesPage;
    APTSourceManagerPage *aptSourceManagerPage;
    DockerPage *dockerPage;
    GnomeSettingsPage *gnomeSettingsPage;
    SettingsPage *settingsPage;
    HelpersPage *helpersPage;

    QSharedPointer<Feedback> feedback;

    bool mKioskMode;

    QSystemTrayIcon *mTrayIcon;

    QMenu *mTrayMenu;

    QAction *mKioskAction;

};

#endif // APP_H
