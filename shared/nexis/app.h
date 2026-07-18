#ifndef APP_H
#define APP_H

#include <QMainWindow>
#include <QAction>
#include <QButtonGroup>
#include <QPropertyAnimation>
#include <QFrame>
#include <QToolButton>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QPushButton>
#include <functional>

#include "sliding_stacked_widget.h"
#include "Managers/app_manager.h"
#include "Managers/setting_manager.h"

struct SidebarSection {
    QString name;
    QPushButton *header = nullptr;
    QWidget *container = nullptr;
    QVBoxLayout *containerLayout = nullptr;
    QList<QPushButton*> buttons;
    bool collapsed = false;
    bool headerless = false;
};

// Page registry entry. factory constructs the page lazily; widget caches the
// result once built. onConstructed runs after the widget is created and
// attached to the stacked widget. In the FR-97 scaffold (Commit A) every slot
// is still eagerly constructed; Commit B flips non-Dashboard slots to lazy.
struct PageSlot {
    // Stable, untranslated identifier. Used by SettingManager::getStartPage
    // (SSO-3388) so the persisted start page survives a language change and
    // by command-palette / navigation callers that need a language-stable
    // handle on a page.
    QString id;
    QString title;
    std::function<QWidget*()> factory;
    QWidget *widget = nullptr;
    std::function<void(QWidget*)> onConstructed;
};

// Pages
#include "Pages/Dashboard/dashboard_page.h"
#include "Pages/BootAnalysis/boot_analysis_page.h"
#include "Pages/StartupApps/startup_apps_page.h"
#include "Pages/SystemCleaner/system_cleaner_page.h"
#include "Pages/Services/services_page.h"
#include "Pages/Processes/processes_page.h"
#include "Pages/Uninstaller/uninstaller_page.h"
#include "Pages/Resources/resources_page.h"
#include "Pages/Network/network_usage_page.h"
#include "Pages/Settings/settings_page.h"
#ifdef Q_OS_MAC
#include "Pages/Homebrew/homebrew_page.h"
#include "MenuBar/menu_bar_monitor.h"
#else
#include "Pages/AptSourceManager/apt_source_manager_page.h"
#endif
#include "Pages/GnomeSettings/gnome_settings_page.h"
#include "Pages/Search/search_page.h"
#include "Pages/DiskTools/disk_tools_page.h"
#include "Pages/Helpers/helpers_page.h"
#include "Pages/HardwareInfo/hardware_info_page.h"
#include "Pages/SystemLogs/system_logs_page.h"
#include "Pages/Docker/docker_page.h"
#include "feedback.h"
#include "command_palette.h"

class QLabel;

namespace Ui {
    class App;
}

class App : public QMainWindow
{
    Q_OBJECT

public:
    explicit App(QWidget *parent = 0);
    ~App();

    // Force-construct every registered page slot. Primarily used by test
    // harnesses (ScreenshotTests) that need all pages present in the
    // stacked widget regardless of user navigation. Production code should
    // prefer lazy construction via sidebar navigation.
    void ensureAllPages();

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void init();
    void pageClick(QWidget *widget, bool slide = true);
    void clickSidebarButton(QString pageTitle, bool isShow = false);

    void toggleKioskMode();
    void exitKioskMode();
    void toggleSidebarCollapse();

private:
    QWidget *getPageByTitle(const QString &title);
    QWidget *ensurePage(int index);
    QWidget *ensurePageByTitle(const QString &title);
    // SSO-3388: resolve a stable page id (e.g. "dashboard") to its
    // currently-localized sidebar title, or an empty string if no page
    // with that id is registered.
    QString pageTitleById(const QString &id) const;
    void checkSidebarButtonByTooltip(const QString &text);
    void createTrayActions();
    void updateSidebarIcons();
    void applyKioskMode(bool enable);
    void showKioskOverlay();

    void buildSidebar();
    QPushButton *createSidebarButton(const QString &tooltip);
    QPushButton *createSectionToggle(const QString &text);
    void applySidebarCollapse(bool collapsed, bool animate = true);
    void toggleSection(int sectionIndex);
    void applySectionCollapse(int sectionIndex, bool collapsed, bool animate = true);
    void expandSectionForButton(QPushButton *btn);
    void saveSectionStates();
    void restoreSectionStates();
    void updateSectionChevrons();
    void repositionBadges();

private:
    Ui::App *ui;

    // Pages
    QList<PageSlot> mPageSlots;
    QList<QPushButton*> mListSidebarButtons;

    SlidingStackedWidget *mSlidingStacked;

    DashboardPage *dashboardPage;
    HardwareInfoPage *hardwareInfoPage;
    BootAnalysisPage *bootAnalysisPage;
    StartupAppsPage *startupAppsPage;
    SystemCleanerPage *systemCleanerPage;
    DiskToolsPage *diskToolsPage;
    SearchPage *searchPage;
    ServicesPage *servicesPage;
    ProcessesPage *processPage;
    UninstallerPage *uninstallerPage;
    ResourcesPage *resourcesPage;
    NetworkUsagePage *networkUsagePage;
#ifdef Q_OS_MAC
    HomebrewPage *homebrewPage = nullptr;
    MenuBarMonitor *mMenuBarMonitor = nullptr;
#else
    APTSourceManagerPage *aptSourceManagerPage = nullptr;
#endif
    DockerPage *dockerPage;
    GnomeSettingsPage *gnomeSettingsPage;
    SettingsPage *settingsPage;
    HelpersPage *helpersPage;
    SystemLogsPage *systemLogsPage;

    QSharedPointer<Feedback> feedback;

    bool mKioskMode;
    bool mSidebarCollapsed;
    bool mPreKioskCollapsed;
    // True once AppManager::updateStylesheet() has been called during init().
    // After this flag flips, any lazily-constructed page missed the initial
    // sigChangedAppTheme emission, so ensurePage() re-emits to catch it up.
    bool mInitialThemeApplied = false;

    QSystemTrayIcon *mTrayIcon;
    QMenu *mTrayMenu;
    QAction *mKioskAction;

    // Sidebar widgets
    QVBoxLayout *mSidebarLayout;
    QScrollArea *mNavScrollArea = nullptr;
    QLabel *mLogoLabel;
    QFrame *mLogoSeparator;
    QToolButton *mBtnSidebarToggle;
    QButtonGroup *mSidebarBtnGroup;
    QList<SidebarSection> mSections;
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
    QPushButton *btnNetworkUsage;
    QPushButton *btnSystemCleaner;
    QPushButton *btnDiskTools;
    QPushButton *btnSearch;
    QPushButton *btnProcesses;
    QPushButton *btnServices;
    QPushButton *btnStartupApps;
    QPushButton *btnBootAnalysis;
    QPushButton *btnUninstaller;
    QPushButton *btnDocker;
    QPushButton *btnHelpers;
    QPushButton *btnSystemLogs;
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
