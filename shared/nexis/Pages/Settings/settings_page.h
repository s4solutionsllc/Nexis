#ifndef SETTINGS_PAGE_H
#define SETTINGS_PAGE_H

#include <QWidget>
#include <QMapIterator>

#include "Managers/app_manager.h"
#include "Managers/setting_manager.h"
#include "signal_mapper.h"

class InfoManager;
class ScheduleManager;

namespace Ui {
    class SettingsPage;
}

class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr,
                          AppManager *appManager = nullptr,
                          SettingManager *settingManager = nullptr,
                          InfoManager *infoManager = nullptr,
                          ScheduleManager *scheduleManager = nullptr);
    ~SettingsPage();

private slots:
    void init();

    void cmbLanguagesChanged(const int &index);
    void cmbDiskChanged(const int &index);
    void on_checkAutostart_clicked(bool checked);
    void cmbStartPageChanged(const QString text);
    void on_spinCpuPercent_valueChanged(int value);
    void on_spinMemoryPercent_valueChanged(int value);
    void on_spinDiskPercent_valueChanged(int value);
    void on_spinBatteryHealthPercent_valueChanged(int value);
    void on_checkAppQuitDontAsk_clicked(bool checked);
    void on_checkMinimizeToTray_clicked(bool checked);
    void on_checkDashboardFooter_clicked(bool checked);
    void cmbColorSchemeChanged(int index);
    void cmbFontChanged(int index);
    void cmbDiskAnalyzerChanged(int index);
    void cmbTrayIconStyleChanged(int index);
    void on_txtDiskAnalyzerCustomPath_editingFinished();
    void on_checkDiskHealthAlert_clicked(bool checked);
    void on_checkUpdateAlert_clicked(bool checked);

    void onQuickSetupToggled(bool checked);
    void onThresholdToggled(bool checked);
    void onThresholdGBChanged(int value);
    void onManageSchedules();
    void onViewCleaningHistory();
    void onCleaningNotificationsToggled(bool checked);
    void onPreCleanSnapshotToggled(bool checked);
    void updateScheduleSummary();
    void refreshThemeColors();

private:
    Ui::SettingsPage *ui;

    void initDiskAnalyzerCombo();
    void updateCustomPathVisibility();
    void initScheduledCleaning();

private:
    AppManager *apm;
    InfoManager *mInfoManager;
    ScheduleManager *mScheduleManager;

    QString mStartupAppPath;

    SettingManager *mSettingManager;
};

#endif // SETTINGS_PAGE_H
