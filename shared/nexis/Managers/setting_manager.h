#ifndef SETTING_MANAGER_H
#define SETTING_MANAGER_H

#include <QSettings>
#include <QStandardPaths>

namespace SettingKeys {
    const QString ThemeName("ThemeName");
    const QString Language("Language");
    const QString DiskName("DiskName");
    const QString StartPage("StartPage");
    const QString CPUAlertPercent("CPUAlertPercent");
    const QString MemoryAlertPercent("MemoryAlertPercent");
    const QString DiskAlertPercent("DiskAlertPercent");
    const QString AppQuitDialogDontAsk("AppQuitDialogDontAsk");
    const QString AppQuitDialogChoice("AppQuitDialogChoice");
    const QString ColorScheme("ColorScheme");
    const QString DiskAnalyzerTool("DiskAnalyzerTool");
    const QString DiskAnalyzerCustomPath("DiskAnalyzerCustomPath");
    const QString KioskMode("KioskMode");
    const QString TempSensorId("TempSensorId");
    const QString FanSensorId("FanSensorId");
    const QString GpuDeviceId("GpuDeviceId");
    const QString BatteryAlertPercent("BatteryAlertPercent");
    const QString BatteryAlertLastHealth("BatteryAlertLastHealth");
    const QString BatteryAlertSnoozedUntil("BatteryAlertSnoozedUntil");
    const QString DiskHealthAlertEnabled("DiskHealthAlertEnabled");
    const QString Schedules("Schedules");
    const QString CleaningNotificationsEnabled("CleaningNotificationsEnabled");
    const QString ThresholdAlertEnabled("ThresholdAlertEnabled");
    const QString ThresholdGB("ThresholdGB");
    const QString SidebarCollapsed("SidebarCollapsed");
    const QString SidebarSectionsCollapsed("SidebarSectionsCollapsed");
    const QString AppFont("AppFont");
    const QString TrayIconStyle("TrayIconStyle");
    const QString DashboardLayout("DashboardLayout");
    const QString MinimizeToTray("MinimizeToTray");
    const QString UpdateAlertEnabled("UpdateAlertEnabled");
    const QString UpdateCheckIntervalMinutes("UpdateCheckIntervalMinutes");
    const QString UpdateLastCount("UpdateLastCount");
}

class SettingManager
{
public:
    static SettingManager *ins();

    QString getConfigPath() const;

    void setLanguage(const QString &value);
    QString getLanguage() const;

    void setThemeName(const QString &value);
    QString getThemeName() const;

    void setDiskName(const QString &value);
    QString getDiskName() const;

    void setStartPage(const QString &value);
    QString getStartPage() const;

    void setCpuAlertPercent(const int value);
    int getCpuAlertPercent() const;

    void setMemoryAlertPercent(const int value);
    int getMemoryAlertPercent() const;

    void setDiskAlertPercent(const int value);
    int getDiskAlertPercent() const;

    void setAppQuitDialogDontAsk(const bool value);
    bool getAppQuitDialogDontAsk() const;

    void setAppQuitDialogChoice(const QString &value);
    QString getAppQuitDialogChoice() const;

    void setColorScheme(const QString &value);
    QString getColorScheme() const;

    void setDiskAnalyzerTool(const QString &value);
    QString getDiskAnalyzerTool() const;

    void setDiskAnalyzerCustomPath(const QString &value);
    QString getDiskAnalyzerCustomPath() const;

    void setKioskMode(bool value);
    bool getKioskMode() const;

    void setTempSensorId(const QString &value);
    QString getTempSensorId() const;

    void setFanSensorId(const QString &value);
    QString getFanSensorId() const;

    void setGpuDeviceId(const QString &value);
    QString getGpuDeviceId() const;

    void setBatteryAlertPercent(const int value);
    int getBatteryAlertPercent() const;

    void setBatteryAlertLastHealth(const int value);
    int getBatteryAlertLastHealth() const;

    void setBatteryAlertSnoozedUntil(const QString &value);
    QString getBatteryAlertSnoozedUntil() const;

    void setDiskHealthAlertEnabled(bool value);
    bool getDiskHealthAlertEnabled() const;

    void setSchedules(const QString &json);
    QString getSchedules() const;

    void setCleaningNotificationsEnabled(bool value);
    bool getCleaningNotificationsEnabled() const;

    void setThresholdAlertEnabled(bool value);
    bool getThresholdAlertEnabled() const;

    void setThresholdGB(int value);
    int getThresholdGB() const;

    void setSidebarCollapsed(bool value);
    bool getSidebarCollapsed() const;

    void setSidebarSectionsCollapsed(const QString &json);
    QString getSidebarSectionsCollapsed() const;

    void setAppFont(const QString &value);
    QString getAppFont() const;

    void setTrayIconStyle(const QString &value);
    QString getTrayIconStyle() const;

    void setDashboardLayout(const QString &json);
    QString getDashboardLayout() const;
    void clearDashboardLayout();

    void setMinimizeToTray(bool value);
    bool getMinimizeToTray() const;

    void setUpdateAlertEnabled(bool value);
    bool getUpdateAlertEnabled() const;

    void setUpdateCheckIntervalMinutes(int value);
    int getUpdateCheckIntervalMinutes() const;

    void setUpdateLastCount(int value);
    int getUpdateLastCount() const;

private:
    static SettingManager *instance;
    SettingManager();

    QSettings *mSettings;
    QString mConfigPath;
};

#endif // SETTING_MANAGER_H
