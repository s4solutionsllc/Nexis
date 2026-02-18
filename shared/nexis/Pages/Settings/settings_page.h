#ifndef SETTINGS_PAGE_H
#define SETTINGS_PAGE_H

#include <QWidget>
#include <QMapIterator>

#include "Managers/app_manager.h"
#include "Managers/setting_manager.h"
#include "signal_mapper.h"

namespace Ui {
    class SettingsPage;
}

class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = 0);
    ~SettingsPage();

private slots:
    void init();

//    void cmbThemesChanged(const int &index);
    void cmbLanguagesChanged(const int &index);
    void cmbDiskChanged(const int &index);
    void on_checkAutostart_clicked(bool checked);
void cmbStartPageChanged(const QString text);
    void on_spinCpuPercent_valueChanged(int value);
    void on_spinMemoryPercent_valueChanged(int value);
    void on_spinDiskPercent_valueChanged(int value);
    void on_spinBatteryHealthPercent_valueChanged(int value);
    void on_checkAppQuitDontAsk_clicked(bool checked);
    void cmbColorSchemeChanged(int index);
    void cmbDiskAnalyzerChanged(int index);
    void on_txtDiskAnalyzerCustomPath_editingFinished();

private:
    Ui::SettingsPage *ui;

    /// Populate the disk analyzer combobox with platform-appropriate tools.
    void initDiskAnalyzerCombo();

    /// Show or hide the custom path field based on current selection.
    void updateCustomPathVisibility();

private:
    AppManager *apm;

    QString mStartupAppPath;

    SettingManager *mSettingManager;
};

#endif // SETTINGS_PAGE_H
