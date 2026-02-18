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
