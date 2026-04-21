#include "setting_manager.h"

SettingManager::SettingManager()
{
    mConfigPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    mSettings = new QSettings(QString("%1/settings.ini").arg(mConfigPath), QSettings::IniFormat);
}

SettingManager *SettingManager::instance = nullptr;

SettingManager* SettingManager::ins()
{
    if (! instance) {
        instance = new SettingManager;
    }
    return instance;
}

QString SettingManager::getConfigPath() const
{
    return mConfigPath;
}

void SettingManager::setLanguage(const QString &value)
{
    mSettings->setValue(SettingKeys::Language, value);
}

QString SettingManager::getLanguage() const
{
    return mSettings->value(SettingKeys::Language, "en").toString();
}

void SettingManager::setThemeName(const QString &value)
{
    mSettings->setValue(SettingKeys::ThemeName, value);
}

QString SettingManager::getThemeName() const
{
    return mSettings->value(SettingKeys::ThemeName, "default").toString();
}

void SettingManager::setDiskName(const QString &value)
{
    mSettings->setValue(SettingKeys::DiskName, value);
}

QString SettingManager::getDiskName() const
{
    return mSettings->value(SettingKeys::DiskName, "").toString();
}

void SettingManager::setStartPage(const QString &value)
{
    mSettings->setValue(SettingKeys::StartPage, value);
}

QString SettingManager::getStartPage() const
{
    return mSettings->value(SettingKeys::StartPage, QObject::tr("Dashboard")).toString();
}

void SettingManager::setCpuAlertPercent(const int value)
{
    mSettings->setValue(SettingKeys::CPUAlertPercent, value);
}

int SettingManager::getCpuAlertPercent() const
{
    return mSettings->value(SettingKeys::CPUAlertPercent, 0).toInt();
}

void SettingManager::setMemoryAlertPercent(const int value)
{
    mSettings->setValue(SettingKeys::MemoryAlertPercent, value);
}

int SettingManager::getMemoryAlertPercent() const
{
    return mSettings->value(SettingKeys::MemoryAlertPercent, 0).toInt();
}

void SettingManager::setDiskAlertPercent(const int value)
{
    mSettings->setValue(SettingKeys::DiskAlertPercent, value);
}

int SettingManager::getDiskAlertPercent() const
{
    return mSettings->value(SettingKeys::DiskAlertPercent, 0).toInt();
}

void SettingManager::setAppQuitDialogDontAsk(const bool value)
{
    mSettings->setValue(SettingKeys::AppQuitDialogDontAsk, value);
}

bool SettingManager::getAppQuitDialogDontAsk() const
{
    return mSettings->value(SettingKeys::AppQuitDialogDontAsk, false).toBool();
}

void SettingManager::setAppQuitDialogChoice(const QString &value)
{
    mSettings->setValue(SettingKeys::AppQuitDialogChoice, value);
}

QString SettingManager::getAppQuitDialogChoice() const
{
    return mSettings->value(SettingKeys::AppQuitDialogChoice, "close").toString();
}

void SettingManager::setColorScheme(const QString &value)
{
    mSettings->setValue(SettingKeys::ColorScheme, value);
}

QString SettingManager::getColorScheme() const
{
    return mSettings->value(SettingKeys::ColorScheme, "auto").toString();
}

void SettingManager::setDiskAnalyzerTool(const QString &value)
{
    mSettings->setValue(SettingKeys::DiskAnalyzerTool, value);
}

QString SettingManager::getDiskAnalyzerTool() const
{
    return mSettings->value(SettingKeys::DiskAnalyzerTool, "auto").toString();
}

void SettingManager::setDiskAnalyzerCustomPath(const QString &value)
{
    mSettings->setValue(SettingKeys::DiskAnalyzerCustomPath, value);
}

QString SettingManager::getDiskAnalyzerCustomPath() const
{
    return mSettings->value(SettingKeys::DiskAnalyzerCustomPath, "").toString();
}

void SettingManager::setKioskMode(bool value)
{
    mSettings->setValue(SettingKeys::KioskMode, value);
}

bool SettingManager::getKioskMode() const
{
    return mSettings->value(SettingKeys::KioskMode, false).toBool();
}

void SettingManager::setTempSensorId(const QString &value)
{
    mSettings->setValue(SettingKeys::TempSensorId, value);
}

QString SettingManager::getTempSensorId() const
{
    return mSettings->value(SettingKeys::TempSensorId, "").toString();
}

void SettingManager::setFanSensorId(const QString &value)
{
    mSettings->setValue(SettingKeys::FanSensorId, value);
}

QString SettingManager::getFanSensorId() const
{
    return mSettings->value(SettingKeys::FanSensorId).toString();
}

void SettingManager::setGpuDeviceId(const QString &value)
{
    mSettings->setValue(SettingKeys::GpuDeviceId, value);
}

QString SettingManager::getGpuDeviceId() const
{
    return mSettings->value(SettingKeys::GpuDeviceId, "").toString();
}

void SettingManager::setBatteryAlertPercent(const int value)
{
    mSettings->setValue(SettingKeys::BatteryAlertPercent, value);
}

int SettingManager::getBatteryAlertPercent() const
{
    return mSettings->value(SettingKeys::BatteryAlertPercent, 0).toInt();
}

void SettingManager::setBatteryAlertLastHealth(const int value)
{
    mSettings->setValue(SettingKeys::BatteryAlertLastHealth, value);
}

int SettingManager::getBatteryAlertLastHealth() const
{
    return mSettings->value(SettingKeys::BatteryAlertLastHealth, 0).toInt();
}

void SettingManager::setBatteryAlertSnoozedUntil(const QString &value)
{
    mSettings->setValue(SettingKeys::BatteryAlertSnoozedUntil, value);
}

QString SettingManager::getBatteryAlertSnoozedUntil() const
{
    return mSettings->value(SettingKeys::BatteryAlertSnoozedUntil, "").toString();
}

void SettingManager::setDiskHealthAlertEnabled(bool value)
{
    mSettings->setValue(SettingKeys::DiskHealthAlertEnabled, value);
}

bool SettingManager::getDiskHealthAlertEnabled() const
{
    return mSettings->value(SettingKeys::DiskHealthAlertEnabled, true).toBool();
}

void SettingManager::setSchedules(const QString &json)
{
    mSettings->setValue(SettingKeys::Schedules, json);
}

QString SettingManager::getSchedules() const
{
    return mSettings->value(SettingKeys::Schedules, "[]").toString();
}

void SettingManager::setCleaningNotificationsEnabled(bool value)
{
    mSettings->setValue(SettingKeys::CleaningNotificationsEnabled, value);
}

bool SettingManager::getCleaningNotificationsEnabled() const
{
    return mSettings->value(SettingKeys::CleaningNotificationsEnabled, true).toBool();
}

void SettingManager::setThresholdAlertEnabled(bool value)
{
    mSettings->setValue(SettingKeys::ThresholdAlertEnabled, value);
}

bool SettingManager::getThresholdAlertEnabled() const
{
    return mSettings->value(SettingKeys::ThresholdAlertEnabled, false).toBool();
}

void SettingManager::setThresholdGB(int value)
{
    mSettings->setValue(SettingKeys::ThresholdGB, value);
}

int SettingManager::getThresholdGB() const
{
    return mSettings->value(SettingKeys::ThresholdGB, 5).toInt();
}

void SettingManager::setSidebarCollapsed(bool value)
{
    mSettings->setValue(SettingKeys::SidebarCollapsed, value);
}

bool SettingManager::getSidebarCollapsed() const
{
    return mSettings->value(SettingKeys::SidebarCollapsed, false).toBool();
}

void SettingManager::setSidebarSectionsCollapsed(const QString &json)
{
    mSettings->setValue(SettingKeys::SidebarSectionsCollapsed, json);
}

QString SettingManager::getSidebarSectionsCollapsed() const
{
    return mSettings->value(SettingKeys::SidebarSectionsCollapsed, "").toString();
}

void SettingManager::setAppFont(const QString &value)
{
    mSettings->setValue(SettingKeys::AppFont, value);
}

QString SettingManager::getAppFont() const
{
    return mSettings->value(SettingKeys::AppFont, "Inter").toString();
}

void SettingManager::setTrayIconStyle(const QString &value)
{
    mSettings->setValue(SettingKeys::TrayIconStyle, value);
}

QString SettingManager::getTrayIconStyle() const
{
    return mSettings->value(SettingKeys::TrayIconStyle, "color").toString();
}

void SettingManager::setDashboardLayout(const QString &json)
{
    mSettings->setValue(SettingKeys::DashboardLayout, json);
}

QString SettingManager::getDashboardLayout() const
{
    return mSettings->value(SettingKeys::DashboardLayout, "").toString();
}

void SettingManager::clearDashboardLayout()
{
    mSettings->remove(SettingKeys::DashboardLayout);
}

void SettingManager::setMinimizeToTray(bool value)
{
    mSettings->setValue(SettingKeys::MinimizeToTray, value);
}

bool SettingManager::getMinimizeToTray() const
{
    return mSettings->value(SettingKeys::MinimizeToTray, false).toBool();
}

void SettingManager::setUpdateAlertEnabled(bool value)
{
    mSettings->setValue(SettingKeys::UpdateAlertEnabled, value);
}

bool SettingManager::getUpdateAlertEnabled() const
{
    return mSettings->value(SettingKeys::UpdateAlertEnabled, true).toBool();
}

void SettingManager::setUpdateCheckIntervalMinutes(int value)
{
    mSettings->setValue(SettingKeys::UpdateCheckIntervalMinutes, value);
}

int SettingManager::getUpdateCheckIntervalMinutes() const
{
    return mSettings->value(SettingKeys::UpdateCheckIntervalMinutes, 60).toInt();
}

void SettingManager::setUpdateLastCount(int value)
{
    mSettings->setValue(SettingKeys::UpdateLastCount, value);
}

int SettingManager::getUpdateLastCount() const
{
    return mSettings->value(SettingKeys::UpdateLastCount, 0).toInt();
}

void SettingManager::setDashboardFooterVisible(bool value)
{
    mSettings->setValue(SettingKeys::DashboardFooterVisible, value);
}

bool SettingManager::getDashboardFooterVisible() const
{
    return mSettings->value(SettingKeys::DashboardFooterVisible, true).toBool();
}

void SettingManager::setCleanerExclusions(const QString &json)
{
    mSettings->setValue(SettingKeys::CleanerExclusions, json);
}

QString SettingManager::getCleanerExclusions() const
{
    return mSettings->value(SettingKeys::CleanerExclusions, "[]").toString();
}

// FR-112
void SettingManager::setPreCleanSnapshotEnabled(bool value)
{
    mSettings->setValue(SettingKeys::PreCleanSnapshotEnabled, value);
}

bool SettingManager::getPreCleanSnapshotEnabled() const
{
    return mSettings->value(SettingKeys::PreCleanSnapshotEnabled, false).toBool();
}

// FR-113
void SettingManager::setDownloadsAutoCleanEnabled(bool value)
{
    mSettings->setValue(SettingKeys::DownloadsAutoCleanEnabled, value);
}

bool SettingManager::getDownloadsAutoCleanEnabled() const
{
    return mSettings->value(SettingKeys::DownloadsAutoCleanEnabled, false).toBool();
}

void SettingManager::setDownloadsAutoCleanPath(const QString &path)
{
    mSettings->setValue(SettingKeys::DownloadsAutoCleanPath, path);
}

QString SettingManager::getDownloadsAutoCleanPath() const
{
    const QString fallback = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    return mSettings->value(SettingKeys::DownloadsAutoCleanPath, fallback).toString();
}

void SettingManager::setDownloadsAutoCleanDays(int days)
{
    mSettings->setValue(SettingKeys::DownloadsAutoCleanDays, days);
}

int SettingManager::getDownloadsAutoCleanDays() const
{
    return mSettings->value(SettingKeys::DownloadsAutoCleanDays, 30).toInt();
}

// FR-114
void SettingManager::setCleanerCategoryTrends(const QString &json)
{
    mSettings->setValue(SettingKeys::CleanerCategoryTrends, json);
}

QString SettingManager::getCleanerCategoryTrends() const
{
    return mSettings->value(SettingKeys::CleanerCategoryTrends, "{}").toString();
}

// FR-116
void SettingManager::setProcessPinnedNames(const QString &json)
{
    mSettings->setValue(SettingKeys::ProcessPinnedNames, json);
}

QString SettingManager::getProcessPinnedNames() const
{
    return mSettings->value(SettingKeys::ProcessPinnedNames, "[]").toString();
}

void SettingManager::setProcessThresholds(const QString &json)
{
    mSettings->setValue(SettingKeys::ProcessThresholds, json);
}

QString SettingManager::getProcessThresholds() const
{
    return mSettings->value(SettingKeys::ProcessThresholds, "[]").toString();
}

// FR-121
void SettingManager::setTrustedBinderPrefixes(const QString &json)
{
    mSettings->setValue(SettingKeys::TrustedBinderPrefixes, json);
}

QString SettingManager::getTrustedBinderPrefixes() const
{
    return mSettings->value(SettingKeys::TrustedBinderPrefixes, "[]").toString();
}

// FR-117
void SettingManager::setCpuTuningPersist(bool value)
{
    mSettings->setValue(SettingKeys::CpuTuningPersist, value);
}

bool SettingManager::getCpuTuningPersist() const
{
    return mSettings->value(SettingKeys::CpuTuningPersist, false).toBool();
}

void SettingManager::setCpuTuningTurboOn(bool value)
{
    mSettings->setValue(SettingKeys::CpuTuningTurboOn, value);
}

bool SettingManager::getCpuTuningTurboOn() const
{
    return mSettings->value(SettingKeys::CpuTuningTurboOn, true).toBool();
}

void SettingManager::setCpuTuningMinFreqKHz(int value)
{
    mSettings->setValue(SettingKeys::CpuTuningMinFreqKHz, value);
}

int SettingManager::getCpuTuningMinFreqKHz() const
{
    return mSettings->value(SettingKeys::CpuTuningMinFreqKHz, 0).toInt();
}

void SettingManager::setCpuTuningMaxFreqKHz(int value)
{
    mSettings->setValue(SettingKeys::CpuTuningMaxFreqKHz, value);
}

int SettingManager::getCpuTuningMaxFreqKHz() const
{
    return mSettings->value(SettingKeys::CpuTuningMaxFreqKHz, 0).toInt();
}
