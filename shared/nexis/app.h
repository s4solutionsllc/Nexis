#ifndef APP_H
#define APP_H

#include <QMainWindow>
#include <QAction>
#include <QButtonGroup>
#include <QPropertyAnimation>
#include <QFrame>
#include <QToolButton>

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
#include "command_palette.h"

class QLabel;
class QVBoxLayout;

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

    void toggleKioskMode();
    void exitKioskMode();
    void toggleSidebarCollapse();

private:
    QWidget *getPageByTitle(const QString &title);
    void checkSidebarButtonByTooltip(const QString &text);
    void createTrayActions();
    void updateSidebarIcons();
    void applyKioskMode(bool enable);
    void showKioskOverlay();

    void buildSidebar();
    QPushButton *createSidebarButton(const QString &tooltip);
    QLabel *createSectionHeader(const QString &text);
    void applySidebarCollapse(bool collapsed, bool animate = true);

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
    bool mSidebarCollapsed;
    bool mPreKioskCollapsed;

    QSystemTrayIcon *mTrayIcon;
    QMenu *mTrayMenu;
    QAction *mKioskAction;

    // Sidebar widgets
    QVBoxLayout *mSidebarLayout;
    QLabel *mLogoLabel;
    QFrame *mLogoSeparator;
    QToolButton *mBtnSidebarToggle;
    QButtonGroup *mSidebarBtnGroup;
    QList<QLabel*> mSectionHeaders;
    QList<QFrame*> mSectionIndicators;
    QLabel *mVersionLabel;
    QLabel *mCleanerBadge;
    QLabel *mCleanerBadgeDot;
    QLabel *mUpdatesBadge;
    QLabel *mUpdatesBadgeDot;

    // Sidebar buttons (programmatically created)
    QPushButton *btnDash;
    QPushButton *btnHardwareInfo;
    QPushButton *btnResources;
    QPushButton *btnSystemCleaner;
    QPushButton *btnSearch;
    QPushButton *btnProcesses;
    QPushButton *btnServices;
    QPushButton *btnStartupApps;
    QPushButton *btnUninstaller;
    QPushButton *btnDocker;
    QPushButton *btnHelpers;
    QPushButton *btnAptSourceManager;
    QPushButton *btnGnomeSettings;
    QPushButton *btnSettings;
    QPushButton *btnFeedback;

    CommandPalette *mCommandPalette;

    void setupCommandPalette();

    static const int SIDEBAR_EXPANDED_WIDTH = 220;
    static const int SIDEBAR_COLLAPSED_WIDTH = 64;
};

#endif // APP_H
