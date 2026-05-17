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
    const QString StartMinimizedToTray("StartMinimizedToTray");
    const QString UpdateAlertEnabled("UpdateAlertEnabled");
    const QString UpdateCheckIntervalMinutes("UpdateCheckIntervalMinutes");
    const QString UpdateLastCount("UpdateLastCount");
    const QString DashboardFooterVisible("DashboardFooterVisible");
    const QString CleanerExclusions("CleanerExclusions");

    // FR-112
    const QString PreCleanSnapshotEnabled("PreCleanSnapshotEnabled");

    // FR-113
    const QString DownloadsAutoCleanEnabled("DownloadsAutoCleanEnabled");
    const QString DownloadsAutoCleanPath("DownloadsAutoCleanPath");
    const QString DownloadsAutoCleanDays("DownloadsAutoCleanDays");

    // FR-114
    const QString CleanerCategoryTrends("CleanerCategoryTrends");

    // FR-116
    const QString ProcessPinnedNames("ProcessPinnedNames");
    const QString ProcessThresholds("ProcessThresholds");

    // FR-121
    const QString TrustedBinderPrefixes("TrustedBinderPrefixes");

    // FR-117
    const QString CpuTuningPersist("CpuTuningPersist");
    const QString CpuTuningTurboOn("CpuTuningTurboOn");
    const QString CpuTuningMinFreqKHz("CpuTuningMinFreqKHz");
    const QString CpuTuningMaxFreqKHz("CpuTuningMaxFreqKHz");

    // FR-120
    const QString WolHostNames("WolHostNames");

    // FR-119
    const QString NetUsageHistory("NetUsageHistory");
    const QString NetCapGB("NetCapGB");
    const QString NetCapAlertEnabled("NetCapAlertEnabled");
    const QString NetCapResetDay("NetCapResetDay");
    const QString NetCapAlertLastPercent("NetCapAlertLastPercent");

    // GH#55 / SSO-355
    const QString WindowGeometry("WindowGeometry");
    const QString WindowState("WindowState");
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

    void setStartMinimizedToTray(bool value);
    bool getStartMinimizedToTray() const;

    void setUpdateAlertEnabled(bool value);
    bool getUpdateAlertEnabled() const;

    void setUpdateCheckIntervalMinutes(int value);
    int getUpdateCheckIntervalMinutes() const;

    void setUpdateLastCount(int value);
    int getUpdateLastCount() const;

    void setDashboardFooterVisible(bool value);
    bool getDashboardFooterVisible() const;

    void setCleanerExclusions(const QString &json);
    QString getCleanerExclusions() const;

    // FR-112
    void setPreCleanSnapshotEnabled(bool value);
    bool getPreCleanSnapshotEnabled() const;

    // FR-113
    void setDownloadsAutoCleanEnabled(bool value);
    bool getDownloadsAutoCleanEnabled() const;
    void setDownloadsAutoCleanPath(const QString &path);
    QString getDownloadsAutoCleanPath() const;
    void setDownloadsAutoCleanDays(int days);
    int getDownloadsAutoCleanDays() const;

    // FR-114
    void setCleanerCategoryTrends(const QString &json);
    QString getCleanerCategoryTrends() const;

    // FR-116
    void setProcessPinnedNames(const QString &json);
    QString getProcessPinnedNames() const;
    void setProcessThresholds(const QString &json);
    QString getProcessThresholds() const;

    // FR-121
    void setTrustedBinderPrefixes(const QString &json);
    QString getTrustedBinderPrefixes() const;

    // FR-117
    void setCpuTuningPersist(bool value);
    bool getCpuTuningPersist() const;
    void setCpuTuningTurboOn(bool value);
    bool getCpuTuningTurboOn() const;
    void setCpuTuningMinFreqKHz(int value);
    int getCpuTuningMinFreqKHz() const;
    void setCpuTuningMaxFreqKHz(int value);
    int getCpuTuningMaxFreqKHz() const;

    void setWolHostNames(const QString &json);
    QString getWolHostNames() const;

    // FR-119
    void setNetUsageHistory(const QString &json);
    QString getNetUsageHistory() const;
    void setNetCapGB(int gb);
    int getNetCapGB() const;
    void setNetCapAlertEnabled(bool v);
    bool getNetCapAlertEnabled() const;
    void setNetCapResetDay(int day);
    int getNetCapResetDay() const;
    void setNetCapAlertLastPercent(int pct);
    int getNetCapAlertLastPercent() const;

    // GH#55 / SSO-355 — persist main window size/position
    void setWindowGeometry(const QByteArray &value);
    QByteArray getWindowGeometry() const;
    void setWindowState(const QByteArray &value);
    QByteArray getWindowState() const;

private:
    static SettingManager *instance;
    SettingManager();

    QSettings *mSettings;
    QString mConfigPath;
};

#endif // SETTING_MANAGER_H
