# BUG-44: Settings Page Layout Redesign — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Rebuild the Settings page with grouped QGroupBox sections inside a scroll area, fixing inconsistent column counts, excessive minimum width, and lack of visual grouping.

**Architecture:** Replace the flat 6-column `QGridLayout` in `settings_page.ui` with a `QScrollArea` → `QVBoxLayout` → 5 `QGroupBox` sections (General, Appearance, Alerts, Tools, Scheduled Cleaning). Each group uses a 2-column label/control grid. Move all programmatic Scheduled Cleaning widgets into the `.ui` file. Add QGroupBox QSS rules matching the existing HardwareInfo/GnomeSettings card pattern.

**Tech Stack:** Qt 6 (QWidget, QGroupBox, QGridLayout, QScrollArea), QSS theming, CMake build

---

## Task 1: Rebuild settings_page.ui with QGroupBox sections

**Files:**
- Modify: `shared/nexis/Pages/Settings/settings_page.ui` (full rewrite)

**Step 1: Replace the .ui file with the new grouped layout**

Rewrite `settings_page.ui` to use a `QScrollArea` containing a vertical layout with 5 `QGroupBox` sections. Every widget retains its existing `objectName` so all C++ signal/slot connections continue to work without changes. The Scheduled Cleaning widgets that were previously created programmatically are now declared in the `.ui` file.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<ui version="4.0">
 <class>SettingsPage</class>
 <widget class="QWidget" name="SettingsPage">
  <property name="geometry">
   <rect>
    <x>0</x>
    <y>0</y>
    <width>650</width>
    <height>600</height>
   </rect>
  </property>
  <property name="sizePolicy">
   <sizepolicy hsizetype="Expanding" vsizetype="Expanding">
    <horstretch>0</horstretch>
    <verstretch>0</verstretch>
   </sizepolicy>
  </property>
  <property name="windowTitle">
   <string>Settings</string>
  </property>
  <layout class="QVBoxLayout" name="pageLayout">
   <property name="leftMargin"><number>0</number></property>
   <property name="topMargin"><number>0</number></property>
   <property name="rightMargin"><number>0</number></property>
   <property name="bottomMargin"><number>0</number></property>
   <item>
    <widget class="QScrollArea" name="scrollArea">
     <property name="widgetResizable"><bool>true</bool></property>
     <property name="frameShape"><enum>QFrame::NoFrame</enum></property>
     <widget class="QWidget" name="scrollContent">
      <layout class="QVBoxLayout" name="scrollLayout">
       <property name="leftMargin"><number>12</number></property>
       <property name="topMargin"><number>12</number></property>
       <property name="rightMargin"><number>12</number></property>
       <property name="bottomMargin"><number>12</number></property>
       <property name="spacing"><number>8</number></property>

       <!-- ============ GENERAL ============ -->
       <item>
        <widget class="QGroupBox" name="groupGeneral">
         <property name="title"><string>General</string></property>
         <layout class="QGridLayout" name="gridGeneral">
          <property name="horizontalSpacing"><number>12</number></property>
          <property name="verticalSpacing"><number>8</number></property>

          <item row="0" column="0">
           <widget class="QLabel" name="lblLanguage">
            <property name="text"><string>Language</string></property>
           </widget>
          </item>
          <item row="0" column="1">
           <widget class="QComboBox" name="cmbLanguages">
            <property name="sizePolicy">
             <sizepolicy hsizetype="Expanding" vsizetype="Fixed">
              <horstretch>0</horstretch><verstretch>0</verstretch>
             </sizepolicy>
            </property>
            <property name="maximumSize"><size><width>250</width><height>16777215</height></size></property>
            <property name="cursor"><cursorShape>PointingHandCursor</cursorShape></property>
            <property name="focusPolicy"><enum>Qt::NoFocus</enum></property>
           </widget>
          </item>

          <item row="1" column="0">
           <widget class="QLabel" name="lblHomepage">
            <property name="text"><string>Start Page</string></property>
           </widget>
          </item>
          <item row="1" column="1">
           <widget class="QComboBox" name="cmbStartPage">
            <property name="sizePolicy">
             <sizepolicy hsizetype="Expanding" vsizetype="Fixed">
              <horstretch>0</horstretch><verstretch>0</verstretch>
             </sizepolicy>
            </property>
            <property name="maximumSize"><size><width>250</width><height>16777215</height></size></property>
            <property name="cursor"><cursorShape>PointingHandCursor</cursorShape></property>
            <property name="focusPolicy"><enum>Qt::NoFocus</enum></property>
           </widget>
          </item>

          <item row="2" column="0">
           <widget class="QLabel" name="lblDisks">
            <property name="text"><string>Default Disk</string></property>
           </widget>
          </item>
          <item row="2" column="1">
           <widget class="QComboBox" name="cmbDisks">
            <property name="sizePolicy">
             <sizepolicy hsizetype="Expanding" vsizetype="Fixed">
              <horstretch>0</horstretch><verstretch>0</verstretch>
             </sizepolicy>
            </property>
            <property name="maximumSize"><size><width>250</width><height>16777215</height></size></property>
            <property name="cursor"><cursorShape>PointingHandCursor</cursorShape></property>
            <property name="focusPolicy"><enum>Qt::NoFocus</enum></property>
           </widget>
          </item>

          <item row="3" column="0" colspan="2">
           <widget class="QCheckBox" name="checkAutostart">
            <property name="text"><string>Start Nexis on boot</string></property>
            <property name="cursor"><cursorShape>PointingHandCursor</cursorShape></property>
            <property name="focusPolicy"><enum>Qt::NoFocus</enum></property>
           </widget>
          </item>

          <item row="4" column="0" colspan="2">
           <widget class="QCheckBox" name="checkAppQuitDontAsk">
            <property name="text"><string>Skip quit confirmation dialog</string></property>
            <property name="cursor"><cursorShape>PointingHandCursor</cursorShape></property>
            <property name="focusPolicy"><enum>Qt::NoFocus</enum></property>
           </widget>
          </item>

         </layout>
        </widget>
       </item>

       <!-- ============ APPEARANCE ============ -->
       <item>
        <widget class="QGroupBox" name="groupAppearance">
         <property name="title"><string>Appearance</string></property>
         <layout class="QGridLayout" name="gridAppearance">
          <property name="horizontalSpacing"><number>12</number></property>
          <property name="verticalSpacing"><number>8</number></property>

          <item row="0" column="0">
           <widget class="QLabel" name="lblAppearance">
            <property name="text"><string>Color Scheme</string></property>
           </widget>
          </item>
          <item row="0" column="1">
           <widget class="QComboBox" name="cmbColorScheme">
            <property name="sizePolicy">
             <sizepolicy hsizetype="Expanding" vsizetype="Fixed">
              <horstretch>0</horstretch><verstretch>0</verstretch>
             </sizepolicy>
            </property>
            <property name="maximumSize"><size><width>250</width><height>16777215</height></size></property>
            <property name="cursor"><cursorShape>PointingHandCursor</cursorShape></property>
            <property name="focusPolicy"><enum>Qt::NoFocus</enum></property>
           </widget>
          </item>

          <item row="1" column="0">
           <widget class="QLabel" name="lblFont">
            <property name="text"><string>Font</string></property>
           </widget>
          </item>
          <item row="1" column="1">
           <widget class="QComboBox" name="cmbFont">
            <property name="sizePolicy">
             <sizepolicy hsizetype="Expanding" vsizetype="Fixed">
              <horstretch>0</horstretch><verstretch>0</verstretch>
             </sizepolicy>
            </property>
            <property name="maximumSize"><size><width>250</width><height>16777215</height></size></property>
            <property name="cursor"><cursorShape>PointingHandCursor</cursorShape></property>
            <property name="focusPolicy"><enum>Qt::NoFocus</enum></property>
           </widget>
          </item>

          <item row="2" column="0">
           <widget class="QLabel" name="lblTrayIconStyle">
            <property name="text"><string>Tray Icon Style</string></property>
           </widget>
          </item>
          <item row="2" column="1">
           <widget class="QComboBox" name="cmbTrayIconStyle">
            <property name="sizePolicy">
             <sizepolicy hsizetype="Expanding" vsizetype="Fixed">
              <horstretch>0</horstretch><verstretch>0</verstretch>
             </sizepolicy>
            </property>
            <property name="maximumSize"><size><width>250</width><height>16777215</height></size></property>
            <property name="cursor"><cursorShape>PointingHandCursor</cursorShape></property>
            <property name="focusPolicy"><enum>Qt::NoFocus</enum></property>
           </widget>
          </item>

         </layout>
        </widget>
       </item>

       <!-- ============ ALERTS ============ -->
       <item>
        <widget class="QGroupBox" name="groupAlerts">
         <property name="title"><string>Alerts</string></property>
         <layout class="QGridLayout" name="gridAlerts">
          <property name="horizontalSpacing"><number>12</number></property>
          <property name="verticalSpacing"><number>8</number></property>

          <item row="0" column="0" colspan="2">
           <widget class="QLabel" name="lblAlertMessages">
            <property name="text"><string>Show a tray warning when usage exceeds the threshold</string></property>
            <property name="accessibleName"><string notr="true">dimmed</string></property>
           </widget>
          </item>

          <item row="1" column="0">
           <widget class="QLabel" name="lblCpuPercent">
            <property name="text"><string>CPU Usage</string></property>
           </widget>
          </item>
          <item row="1" column="1">
           <widget class="QSpinBox" name="spinCpuPercent">
            <property name="focusPolicy"><enum>Qt::ClickFocus</enum></property>
            <property name="suffix"><string notr="true"> %</string></property>
            <property name="minimum"><number>0</number></property>
            <property name="maximum"><number>100</number></property>
            <property name="maximumSize"><size><width>120</width><height>16777215</height></size></property>
           </widget>
          </item>

          <item row="2" column="0">
           <widget class="QLabel" name="lblMemoryPercent">
            <property name="text"><string>Memory Usage</string></property>
           </widget>
          </item>
          <item row="2" column="1">
           <widget class="QSpinBox" name="spinMemoryPercent">
            <property name="focusPolicy"><enum>Qt::ClickFocus</enum></property>
            <property name="keyboardTracking"><bool>false</bool></property>
            <property name="suffix"><string notr="true"> %</string></property>
            <property name="minimum"><number>0</number></property>
            <property name="maximum"><number>100</number></property>
            <property name="maximumSize"><size><width>120</width><height>16777215</height></size></property>
           </widget>
          </item>

          <item row="3" column="0">
           <widget class="QLabel" name="lblDiskPercent">
            <property name="text"><string>Disk Usage</string></property>
           </widget>
          </item>
          <item row="3" column="1">
           <widget class="QSpinBox" name="spinDiskPercent">
            <property name="focusPolicy"><enum>Qt::ClickFocus</enum></property>
            <property name="suffix"><string notr="true"> %</string></property>
            <property name="minimum"><number>0</number></property>
            <property name="maximum"><number>100</number></property>
            <property name="maximumSize"><size><width>120</width><height>16777215</height></size></property>
           </widget>
          </item>

          <item row="4" column="0">
           <widget class="QLabel" name="lblBatteryHealthPercent">
            <property name="text"><string>Battery Health</string></property>
           </widget>
          </item>
          <item row="4" column="1">
           <widget class="QSpinBox" name="spinBatteryHealthPercent">
            <property name="focusPolicy"><enum>Qt::ClickFocus</enum></property>
            <property name="keyboardTracking"><bool>false</bool></property>
            <property name="suffix"><string notr="true"> %</string></property>
            <property name="minimum"><number>0</number></property>
            <property name="maximum"><number>100</number></property>
            <property name="maximumSize"><size><width>120</width><height>16777215</height></size></property>
           </widget>
          </item>

          <item row="5" column="0" colspan="2">
           <widget class="QCheckBox" name="checkDiskHealthAlert">
            <property name="text"><string>Enable disk health alerts</string></property>
            <property name="cursor"><cursorShape>PointingHandCursor</cursorShape></property>
            <property name="focusPolicy"><enum>Qt::NoFocus</enum></property>
           </widget>
          </item>

         </layout>
        </widget>
       </item>

       <!-- ============ TOOLS ============ -->
       <item>
        <widget class="QGroupBox" name="groupTools">
         <property name="title"><string>Tools</string></property>
         <layout class="QGridLayout" name="gridTools">
          <property name="horizontalSpacing"><number>12</number></property>
          <property name="verticalSpacing"><number>8</number></property>

          <item row="0" column="0">
           <widget class="QLabel" name="lblDiskAnalyzer">
            <property name="text"><string>Disk Analyzer</string></property>
           </widget>
          </item>
          <item row="0" column="1">
           <widget class="QComboBox" name="cmbDiskAnalyzer">
            <property name="sizePolicy">
             <sizepolicy hsizetype="Expanding" vsizetype="Fixed">
              <horstretch>0</horstretch><verstretch>0</verstretch>
             </sizepolicy>
            </property>
            <property name="maximumSize"><size><width>250</width><height>16777215</height></size></property>
            <property name="cursor"><cursorShape>PointingHandCursor</cursorShape></property>
            <property name="focusPolicy"><enum>Qt::NoFocus</enum></property>
           </widget>
          </item>

          <item row="1" column="0">
           <widget class="QLabel" name="lblDiskAnalyzerCustomPath">
            <property name="text"><string>Custom Executable Path</string></property>
           </widget>
          </item>
          <item row="1" column="1">
           <widget class="QLineEdit" name="txtDiskAnalyzerCustomPath">
            <property name="sizePolicy">
             <sizepolicy hsizetype="Expanding" vsizetype="Fixed">
              <horstretch>0</horstretch><verstretch>0</verstretch>
             </sizepolicy>
            </property>
            <property name="maximumSize"><size><width>250</width><height>16777215</height></size></property>
            <property name="placeholderText"><string>/usr/bin/my-analyzer</string></property>
           </widget>
          </item>

         </layout>
        </widget>
       </item>

       <!-- ============ SCHEDULED CLEANING ============ -->
       <item>
        <widget class="QGroupBox" name="groupScheduledCleaning">
         <property name="title"><string>Scheduled Cleaning</string></property>
         <layout class="QVBoxLayout" name="layoutScheduledCleaning">
          <property name="spacing"><number>8</number></property>

          <item>
           <layout class="QHBoxLayout" name="layoutQuickSetup">
            <item>
             <widget class="QCheckBox" name="chkQuickSetup">
              <property name="text"><string>Enable automatic weekly cleaning</string></property>
              <property name="cursor"><cursorShape>PointingHandCursor</cursorShape></property>
              <property name="focusPolicy"><enum>Qt::NoFocus</enum></property>
             </widget>
            </item>
            <item>
             <widget class="QLabel" name="lblQuickSetupSummary">
              <property name="text"><string/></property>
             </widget>
            </item>
            <item>
             <spacer name="hspacerQuickSetup">
              <property name="orientation"><enum>Qt::Horizontal</enum></property>
              <property name="sizeHint" stdset="0"><size><width>0</width><height>0</height></size></property>
             </spacer>
            </item>
           </layout>
          </item>

          <item>
           <layout class="QHBoxLayout" name="layoutScheduleButtons">
            <item>
             <widget class="QPushButton" name="btnManageSchedules">
              <property name="text"><string>Manage Schedules...</string></property>
              <property name="cursor"><cursorShape>PointingHandCursor</cursorShape></property>
              <property name="focusPolicy"><enum>Qt::NoFocus</enum></property>
              <property name="accessibleName"><string notr="true">primary</string></property>
             </widget>
            </item>
            <item>
             <widget class="QPushButton" name="btnViewHistory">
              <property name="text"><string>View Cleaning History</string></property>
              <property name="cursor"><cursorShape>PointingHandCursor</cursorShape></property>
              <property name="focusPolicy"><enum>Qt::NoFocus</enum></property>
              <property name="accessibleName"><string notr="true">primary</string></property>
             </widget>
            </item>
            <item>
             <spacer name="hspacerButtons">
              <property name="orientation"><enum>Qt::Horizontal</enum></property>
              <property name="sizeHint" stdset="0"><size><width>0</width><height>0</height></size></property>
             </spacer>
            </item>
           </layout>
          </item>

          <item>
           <layout class="QHBoxLayout" name="layoutThreshold">
            <item>
             <widget class="QCheckBox" name="chkThresholdAlert">
              <property name="text"><string>Notify when junk exceeds</string></property>
              <property name="cursor"><cursorShape>PointingHandCursor</cursorShape></property>
              <property name="focusPolicy"><enum>Qt::NoFocus</enum></property>
             </widget>
            </item>
            <item>
             <widget class="QSpinBox" name="spnThresholdGB">
              <property name="focusPolicy"><enum>Qt::ClickFocus</enum></property>
              <property name="suffix"><string> GB</string></property>
              <property name="minimum"><number>1</number></property>
              <property name="maximum"><number>100</number></property>
              <property name="maximumSize"><size><width>120</width><height>16777215</height></size></property>
             </widget>
            </item>
            <item>
             <spacer name="hspacerThreshold">
              <property name="orientation"><enum>Qt::Horizontal</enum></property>
              <property name="sizeHint" stdset="0"><size><width>0</width><height>0</height></size></property>
             </spacer>
            </item>
           </layout>
          </item>

          <item>
           <widget class="QCheckBox" name="chkCleaningNotifications">
            <property name="text"><string>Show notification after scheduled clean</string></property>
            <property name="cursor"><cursorShape>PointingHandCursor</cursorShape></property>
            <property name="focusPolicy"><enum>Qt::NoFocus</enum></property>
           </widget>
          </item>

         </layout>
        </widget>
       </item>

       <!-- ============ FOOTER ============ -->
       <item>
        <spacer name="verticalSpacer">
         <property name="orientation"><enum>Qt::Vertical</enum></property>
         <property name="sizeHint" stdset="0"><size><width>10</width><height>10</height></size></property>
        </spacer>
       </item>

       <item>
        <widget class="QLabel" name="lblCreatedBy">
         <property name="text">
          <string notr="true">&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Nexis &lt;a href=&quot;https://github.com/lsimpsonsfdc&quot;&gt;&lt;span style=&quot; text-decoration: underline; color:#E95420;&quot;&gt;Luke Simpson&lt;/span&gt;&lt;/a&gt;&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</string>
         </property>
         <property name="textFormat"><enum>Qt::RichText</enum></property>
         <property name="alignment"><set>Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter</set></property>
         <property name="openExternalLinks"><bool>true</bool></property>
        </widget>
       </item>

      </layout>
     </widget>
    </widget>
   </item>
  </layout>
 </widget>
 <tabstops>
  <tabstop>spinCpuPercent</tabstop>
  <tabstop>spinMemoryPercent</tabstop>
  <tabstop>spinDiskPercent</tabstop>
 </tabstops>
 <resources>
  <include location="../../static.qrc"/>
 </resources>
 <connections/>
</ui>
```

**Step 2: Build and verify the .ui compiles**

Run: `cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | head -50`
Expected: Compilation errors related to C++ referencing old widget names (e.g., `mChkQuickSetup` vs `ui->chkQuickSetup`). These are fixed in Task 2.

**Step 3: Commit the .ui file**

```bash
git add shared/nexis/Pages/Settings/settings_page.ui
git commit -m "refactor(settings): rebuild settings_page.ui with QGroupBox sections (BUG-44)

Replace flat 6-column grid with QScrollArea containing 5 QGroupBox
sections: General, Appearance, Alerts, Tools, Scheduled Cleaning.
All widgets use 2-column label/control grids. Scheduled Cleaning
widgets moved from programmatic creation to .ui file."
```

---

## Task 2: Update settings_page.h — remove programmatic widget pointers

**Files:**
- Modify: `shared/nexis/Pages/Settings/settings_page.h`

**Step 1: Remove the member pointers for Scheduled Cleaning widgets**

Since these widgets are now in the `.ui` file and accessed via `ui->`, remove the manual member pointers. Also remove `initScheduledCleaning()` declaration (that logic moves into `init()`).

Replace the entire header content:

```cpp
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
    void cmbColorSchemeChanged(int index);
    void cmbFontChanged(int index);
    void cmbDiskAnalyzerChanged(int index);
    void cmbTrayIconStyleChanged(int index);
    void on_txtDiskAnalyzerCustomPath_editingFinished();
    void on_checkDiskHealthAlert_clicked(bool checked);

    void onQuickSetupToggled(bool checked);
    void onThresholdToggled(bool checked);
    void onThresholdGBChanged(int value);
    void onManageSchedules();
    void onViewCleaningHistory();
    void onCleaningNotificationsToggled(bool checked);
    void updateScheduleSummary();

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
```

Key changes: Removed `QCheckBox`, `QSpinBox`, `QPushButton`, `QLabel` includes (not needed with `.ui` widgets). Removed 7 member pointers (`mChkQuickSetup`, `mLblQuickSetupSummary`, `mBtnManageSchedules`, `mChkThresholdAlert`, `mSpnThresholdGB`, `mBtnViewHistory`, `mChkCleaningNotifications`).

**Step 2: Commit**

```bash
git add shared/nexis/Pages/Settings/settings_page.h
git commit -m "refactor(settings): remove programmatic widget pointers from header (BUG-44)"
```

---

## Task 3: Rewrite settings_page.cpp — wire up .ui widgets, remove initScheduledCleaning grid manipulation

**Files:**
- Modify: `shared/nexis/Pages/Settings/settings_page.cpp`

**Step 1: Rewrite the .cpp to use .ui-defined widgets**

Replace every `mChkQuickSetup` with `ui->chkQuickSetup`, `mLblQuickSetupSummary` with `ui->lblQuickSetupSummary`, `mBtnManageSchedules` with `ui->btnManageSchedules`, `mBtnViewHistory` with `ui->btnViewHistory`, `mChkThresholdAlert` with `ui->chkThresholdAlert`, `mSpnThresholdGB` with `ui->spnThresholdGB`, `mChkCleaningNotifications` with `ui->chkCleaningNotifications`.

The `initScheduledCleaning()` method no longer creates widgets or manipulates the grid — it only restores saved state and connects signals. The grid manipulation code (removing/re-adding spacer and footer, creating QLabels, creating buttons) is deleted entirely.

The `init()` method's drop-shadow list needs to include `ui->btnManageSchedules`, `ui->btnViewHistory`, and `ui->spnThresholdGB`.

The scroll area needs transparent background set (per the QScrollArea viewport gotcha in CLAUDE.md):
```cpp
ui->scrollArea->setStyleSheet("QScrollArea{background-color:transparent;}");
ui->scrollContent->setStyleSheet("background-color:transparent;");
```

Full replacement for `settings_page.cpp`:

```cpp
#include "settings_page.h"
#include "ui_settings_page.h"
#include "Managers/info_manager.h"
#include "Managers/schedule_manager.h"
#include "Managers/cleaner_service.h"
#include "Pages/SystemCleaner/schedule_editor_dialog.h"
#include "utilities.h"
#include <QApplication>
#include <QRegularExpression>
#include <QLineEdit>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QDialog>
#include <QScrollArea>
#include <Utils/format_util.h>
#include <functional>

SettingsPage::~SettingsPage()
{
    delete ui;
}

SettingsPage::SettingsPage(QWidget *parent, AppManager *appManager,
                           SettingManager *settingManager, InfoManager *infoManager,
                           ScheduleManager *scheduleManager) :
    QWidget(parent),
    ui(new Ui::SettingsPage),
    apm(appManager ? appManager : AppManager::ins()),
    mInfoManager(infoManager ? infoManager : InfoManager::ins()),
    mScheduleManager(scheduleManager ? scheduleManager : ScheduleManager::ins()),
    mSettingManager(settingManager ? settingManager : SettingManager::ins())
{
    ui->setupUi(this);

    // Transparent scroll area (QSS viewport gotcha — see CLAUDE.md)
    ui->scrollArea->setStyleSheet("QScrollArea{background-color:transparent;}");
    ui->scrollContent->setStyleSheet("background-color:transparent;");

    auto updateCreditLink = [this]() {
        QSettings *sv = apm->getStyleValues();
        QString accent = sv ? sv->value("@accentColor").toString() : "#E95420";
        ui->lblCreatedBy->setText(
            QString("<html><head/><body><p>Nexis v%1 "
                    "<a href=\"https://github.com/lsimpsonsfdc\">"
                    "<span style=\" text-decoration: underline; color:%2;\">"
                    "Luke Simpson</span></a></p></body></html>")
                .arg(qApp->applicationVersion(), accent));
    };
    updateCreditLink();

    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme, this, updateCreditLink);

    init();
}

void SettingsPage::init()
{
    // load languages
    QMapIterator<QString, QString> lang(apm->getLanguageList());

    while (lang.hasNext()) {
        lang.next();
        ui->cmbLanguages->addItem(lang.value(), lang.key());
    }

    QString lc = mSettingManager->getLanguage();
    ui->cmbLanguages->setCurrentText(apm->getLanguageList().value(lc));

    // load disks
    mInfoManager->updateDiskInfo();
    const QList<Disk> disks = mInfoManager->getDisks();

    for (const Disk &disk : disks) {
        ui->cmbDisks->addItem(QString("%1  (%2)").arg(disk.device).arg(disk.name), disk.name);
    }

    QString dk = mSettingManager->getDiskName().isEmpty() ? QStorageInfo::root().displayName() : mSettingManager->getDiskName();
    if (! dk.isEmpty()) {
        ui->cmbDisks->setCurrentIndex(ui->cmbDisks->findData(dk));
    }

    // start on boot — platform-specific path and check
#ifdef Q_OS_MACOS
    mStartupAppPath = QDir::homePath() + "/Library/LaunchAgents";
    if (! QDir(mStartupAppPath).exists()) {
        QDir().mkdir(mStartupAppPath);
    }
    mStartupAppPath.append("/com.nexis.app.plist");

    QFile startupAppFile(mStartupAppPath);
    if (startupAppFile.exists()) {
        ui->checkAutostart->setChecked(true);
    } else {
        ui->checkAutostart->setChecked(false);
    }
#else
    mStartupAppPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation).append("/autostart");
    if (! QDir(mStartupAppPath).exists()) {
        QDir().mkdir(mStartupAppPath);
    }
    mStartupAppPath.append("/nexis.desktop");

    QFile startupAppFile(mStartupAppPath);
    if (startupAppFile.exists()) {
        QStringList appContent = FileUtil::readListFromFile(mStartupAppPath);
        QString isHidden = Utilities::getDesktopValue(QRegularExpression("^Hidden=.*"), appContent).toLower();
        ui->checkAutostart->setChecked(isHidden == "false");
    } else {
        ui->checkAutostart->setChecked(false);
    }
#endif

    // app quit dont ask
    ui->checkAppQuitDontAsk->setChecked(mSettingManager->getAppQuitDialogDontAsk());

    // load pages
    ui->cmbStartPage->addItems({
        tr("Dashboard"), tr("Startup Apps"), tr("System Cleaner"), tr("Search"),
        tr("Services"), tr("Processes"), tr("Helpers"), tr("Uninstaller"), tr("Resources")
    });

    ui->cmbStartPage->setCurrentText(mSettingManager->getStartPage());

    // color scheme (appearance)
    ui->cmbColorScheme->addItem(tr("Auto"), "auto");
    ui->cmbColorScheme->addItem(tr("Light"), "light");
    ui->cmbColorScheme->addItem(tr("Dark"), "dark");
    ui->cmbColorScheme->setCurrentIndex(
        ui->cmbColorScheme->findData(mSettingManager->getColorScheme()));

    // font family
    ui->cmbFont->addItem(tr("Inter (Recommended)"), "Inter");
    ui->cmbFont->addItem("Ubuntu", "Ubuntu");
    ui->cmbFont->addItem("JetBrains Mono", "JetBrains Mono");
    ui->cmbFont->addItem(tr("System Default"), "system-ui");
    ui->cmbFont->setCurrentIndex(
        ui->cmbFont->findData(mSettingManager->getAppFont()));

    // tray icon style
    ui->cmbTrayIconStyle->addItem(tr("Color (Default)"), "color");
    ui->cmbTrayIconStyle->addItem(tr("Symbolic"), "symbolic");
    ui->cmbTrayIconStyle->addItem(tr("Outline"), "outline");
    ui->cmbTrayIconStyle->addItem(tr("Accent"), "accent");
    ui->cmbTrayIconStyle->setCurrentIndex(
        ui->cmbTrayIconStyle->findData(mSettingManager->getTrayIconStyle()));

    // load resource percents
    ui->spinCpuPercent->setValue(mSettingManager->getCpuAlertPercent());
    ui->spinMemoryPercent->setValue(mSettingManager->getMemoryAlertPercent());
    ui->spinDiskPercent->setValue(mSettingManager->getDiskAlertPercent());

    // battery health alert (hide row if no battery)
    ui->spinBatteryHealthPercent->setValue(mSettingManager->getBatteryAlertPercent());
    if (!mInfoManager->hasBattery()) {
        ui->lblBatteryHealthPercent->hide();
        ui->spinBatteryHealthPercent->hide();
    }

    // disk health alert (hide if no SMART data)
    ui->checkDiskHealthAlert->setChecked(mSettingManager->getDiskHealthAlertEnabled());
    if (!mInfoManager->hasDiskHealth()) {
        ui->checkDiskHealthAlert->hide();
    }

    // disk analyzer preference
    initDiskAnalyzerCombo();

    // drop shadows
    Utilities::addDropShadow({
        ui->cmbLanguages, ui->cmbDisks, ui->cmbStartPage, ui->cmbColorScheme,
        ui->cmbFont, ui->cmbTrayIconStyle, ui->spinCpuPercent, ui->spinMemoryPercent,
        ui->spinDiskPercent, ui->cmbDiskAnalyzer, ui->btnManageSchedules,
        ui->btnViewHistory, ui->spnThresholdGB
    }, 50);

    // signal connections
    connect(ui->cmbLanguages, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsPage::cmbLanguagesChanged);
    connect(ui->cmbDisks, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsPage::cmbDiskChanged);
    connect(ui->cmbStartPage, &QComboBox::currentTextChanged, this, &SettingsPage::cmbStartPageChanged);
    connect(ui->cmbColorScheme, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsPage::cmbColorSchemeChanged);
    connect(ui->cmbFont, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsPage::cmbFontChanged);
    connect(ui->cmbTrayIconStyle, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsPage::cmbTrayIconStyleChanged);
    connect(ui->cmbDiskAnalyzer, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsPage::cmbDiskAnalyzerChanged);

    // scheduled cleaning
    initScheduledCleaning();
}

void SettingsPage::cmbLanguagesChanged(const int &index)
{
    QString langCode = ui->cmbLanguages->itemData(index).toString();
    mSettingManager->setLanguage(langCode);
}

void SettingsPage::cmbDiskChanged(const int &index)
{
    QString diskName = ui->cmbDisks->itemData(index).toString();
    mSettingManager->setDiskName(diskName);
}

void SettingsPage::on_checkAutostart_clicked(bool checked)
{
    if (checked) {
#ifdef Q_OS_MACOS
        QString appTemplate = QString(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
            "<plist version=\"1.0\">\n"
            "<dict>\n"
            "    <key>Label</key>\n"
            "    <string>com.nexis.app</string>\n"
            "    <key>ProgramArguments</key>\n"
            "    <array>\n"
            "        <string>nexis</string>\n"
            "        <string>--hide</string>\n"
            "    </array>\n"
            "    <key>RunAtLoad</key>\n"
            "    <true/>\n"
            "</dict>\n"
            "</plist>\n");
#else
        QString appTemplate = QString("[Desktop Entry]\n"
                                      "Name=Nexis\n"
                                      "Comment=Linux System Optimizer and Monitoring\n"
                                      "Exec=nexis --hide \n"
                                      "Type=Application\n"
                                      "Terminal=false\n"
                                      "Hidden=false\n");
#endif
        FileUtil::writeFile(mStartupAppPath, appTemplate);
    } else {
        QFile::remove(mStartupAppPath);
    }
}

void SettingsPage::cmbStartPageChanged(const QString text)
{
    mSettingManager->setStartPage(text);
}

void SettingsPage::on_spinCpuPercent_valueChanged(int value)
{
    mSettingManager->setCpuAlertPercent(value);
}

void SettingsPage::on_spinMemoryPercent_valueChanged(int value)
{
    mSettingManager->setMemoryAlertPercent(value);
}

void SettingsPage::on_spinDiskPercent_valueChanged(int value)
{
    mSettingManager->setDiskAlertPercent(value);
}

void SettingsPage::on_spinBatteryHealthPercent_valueChanged(int value)
{
    mSettingManager->setBatteryAlertPercent(value);
}

void SettingsPage::on_checkAppQuitDontAsk_clicked(bool checked)
{
    mSettingManager->setAppQuitDialogDontAsk(checked);
}

void SettingsPage::cmbColorSchemeChanged(int index)
{
    QString scheme = ui->cmbColorScheme->itemData(index).toString();
    mSettingManager->setColorScheme(scheme);
    apm->updateStylesheet();
}

void SettingsPage::cmbFontChanged(int index)
{
    QString fontFamily = ui->cmbFont->itemData(index).toString();
    mSettingManager->setAppFont(fontFamily);
    apm->updateStylesheet();
}

void SettingsPage::cmbTrayIconStyleChanged(int index)
{
    QString style = ui->cmbTrayIconStyle->itemData(index).toString();
    mSettingManager->setTrayIconStyle(style);
    apm->updateTrayIcon();
}

void SettingsPage::initDiskAnalyzerCombo()
{
    ui->cmbDiskAnalyzer->addItem(tr("Auto (Detect)"), "auto");
#ifdef Q_OS_MACOS
    ui->cmbDiskAnalyzer->addItem(tr("GrandPerspective"), "grandperspective");
    ui->cmbDiskAnalyzer->addItem(tr("DaisyDisk"), "daisydisk");
    ui->cmbDiskAnalyzer->addItem(tr("OmniDiskSweeper"), "omnidisksweeper");
#else
    ui->cmbDiskAnalyzer->addItem(tr("Baobab (GNOME Disk Usage Analyzer)"), "baobab");
    ui->cmbDiskAnalyzer->addItem(tr("Filelight (KDE)"), "filelight");
    ui->cmbDiskAnalyzer->addItem(tr("QDirStat"), "qdirstat");
    ui->cmbDiskAnalyzer->addItem(tr("ncdu (Terminal)"), "ncdu");
#endif
    ui->cmbDiskAnalyzer->addItem(tr("Custom..."), "custom");

    QString saved = mSettingManager->getDiskAnalyzerTool();
    int idx = ui->cmbDiskAnalyzer->findData(saved);
    if (idx >= 0)
        ui->cmbDiskAnalyzer->setCurrentIndex(idx);
    else
        ui->cmbDiskAnalyzer->setCurrentIndex(0);

    ui->txtDiskAnalyzerCustomPath->setText(mSettingManager->getDiskAnalyzerCustomPath());
    updateCustomPathVisibility();
}

void SettingsPage::updateCustomPathVisibility()
{
    bool isCustom = (ui->cmbDiskAnalyzer->currentData().toString() == "custom");
    ui->lblDiskAnalyzerCustomPath->setVisible(isCustom);
    ui->txtDiskAnalyzerCustomPath->setVisible(isCustom);
}

void SettingsPage::cmbDiskAnalyzerChanged(int index)
{
    QString tool = ui->cmbDiskAnalyzer->itemData(index).toString();
    mSettingManager->setDiskAnalyzerTool(tool);
    updateCustomPathVisibility();
}

void SettingsPage::on_txtDiskAnalyzerCustomPath_editingFinished()
{
    mSettingManager->setDiskAnalyzerCustomPath(ui->txtDiskAnalyzerCustomPath->text().trimmed());
}

void SettingsPage::on_checkDiskHealthAlert_clicked(bool checked)
{
    mSettingManager->setDiskHealthAlertEnabled(checked);
}

void SettingsPage::initScheduledCleaning()
{
    // Restore saved state
    ui->chkThresholdAlert->setChecked(mSettingManager->getThresholdAlertEnabled());
    ui->spnThresholdGB->setValue(mSettingManager->getThresholdGB());
    ui->spnThresholdGB->setEnabled(mSettingManager->getThresholdAlertEnabled());
    ui->chkCleaningNotifications->setChecked(mSettingManager->getCleaningNotificationsEnabled());

    // Check if quick setup schedule exists
    bool hasQuickSetup = false;
    for (const auto &s : mScheduleManager->getAllSchedules()) {
        if (s.name == "Weekly Cleanup" && s.frequency == ScheduleManager::Weekly) {
            hasQuickSetup = true;
            break;
        }
    }
    ui->chkQuickSetup->setChecked(hasQuickSetup);
    updateScheduleSummary();

    // Connections
    connect(ui->chkQuickSetup, &QCheckBox::toggled, this, &SettingsPage::onQuickSetupToggled);
    connect(ui->chkThresholdAlert, &QCheckBox::toggled, this, &SettingsPage::onThresholdToggled);
    connect(ui->spnThresholdGB, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsPage::onThresholdGBChanged);
    connect(ui->btnManageSchedules, &QPushButton::clicked, this, &SettingsPage::onManageSchedules);
    connect(ui->btnViewHistory, &QPushButton::clicked, this, &SettingsPage::onViewCleaningHistory);
    connect(ui->chkCleaningNotifications, &QCheckBox::toggled, this, &SettingsPage::onCleaningNotificationsToggled);
    connect(mScheduleManager, &ScheduleManager::schedulesChanged, this, &SettingsPage::updateScheduleSummary);
}

void SettingsPage::onQuickSetupToggled(bool checked)
{
    ScheduleManager *sm = mScheduleManager;

    if (checked) {
        ScheduleManager::CleaningSchedule s;
        s.name = "Weekly Cleanup";
        s.frequency = ScheduleManager::Weekly;
        s.dayOfWeek = 0;
        s.hour = 3;
        s.minute = 0;
        s.categories = {
            CleanerService::PACKAGE_CACHE,
            CleanerService::CRASH_REPORTS,
            CleanerService::APPLICATION_LOGS,
            CleanerService::APPLICATION_CACHES,
            CleanerService::DEV_TOOL_CACHES
        };
        s.minFileAgeSecs = 86400;
        sm->createSchedule(s);
    } else {
        for (const auto &s : sm->getAllSchedules()) {
            if (s.name == "Weekly Cleanup" && s.frequency == ScheduleManager::Weekly) {
                sm->deleteSchedule(s.id);
                break;
            }
        }
    }
}

void SettingsPage::onThresholdToggled(bool checked)
{
    mSettingManager->setThresholdAlertEnabled(checked);
    ui->spnThresholdGB->setEnabled(checked);
}

void SettingsPage::onThresholdGBChanged(int value)
{
    mSettingManager->setThresholdGB(value);
}

void SettingsPage::onCleaningNotificationsToggled(bool checked)
{
    mSettingManager->setCleaningNotificationsEnabled(checked);
}

void SettingsPage::onManageSchedules()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Manage Cleaning Schedules"));
    dialog.setObjectName("manageSchedulesDialog");
    dialog.setMinimumSize(550, 400);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLabel *dlgTitle = new QLabel(tr("Manage Cleaning Schedules"));
    dlgTitle->setProperty("accessibleName", "dialog-title");
    layout->addWidget(dlgTitle);

    QScrollArea *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("QScrollArea{background-color:transparent;}");
    QWidget *scrollWidget = new QWidget;
    scrollWidget->setStyleSheet("background-color:transparent;");
    QVBoxLayout *listLayout = new QVBoxLayout(scrollWidget);

    std::function<void()> refreshList = [&]() {
        QLayoutItem *item;
        while ((item = listLayout->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }

        QList<ScheduleManager::CleaningSchedule> schedules = mScheduleManager->getAllSchedules();

        if (schedules.isEmpty()) {
            listLayout->addWidget(new QLabel(tr("No schedules configured.")));
        }

        for (const auto &s : schedules) {
            QGroupBox *card = new QGroupBox;
            QHBoxLayout *cardLayout = new QHBoxLayout(card);

            QCheckBox *enableCheck = new QCheckBox;
            enableCheck->setChecked(s.enabled);
            cardLayout->addWidget(enableCheck);

            QVBoxLayout *infoLayout = new QVBoxLayout;
            QLabel *nameLabel = new QLabel(QString("<b>%1</b>").arg(s.name));
            QLabel *freqLabel = new QLabel(ScheduleManager::frequencyDisplayText(s));
            freqLabel->setProperty("accessibleName", "dimmed");

            QString lastRunText;
            if (s.lastRun.isValid()) {
                lastRunText = tr("Last: %1 — %2")
                    .arg(s.lastRun.toString("MMM d, h:mm AP"))
                    .arg(FormatUtil::formatBytes(s.lastBytesFreed));
            } else {
                lastRunText = tr("Never run");
            }
            QLabel *lastLabel = new QLabel(lastRunText);
            lastLabel->setProperty("accessibleName", "dimmed-small");

            infoLayout->addWidget(nameLabel);
            infoLayout->addWidget(freqLabel);
            infoLayout->addWidget(lastLabel);
            cardLayout->addLayout(infoLayout, 1);

            QPushButton *editBtn = new QPushButton(tr("Edit"));
            editBtn->setFocusPolicy(Qt::NoFocus);
            QPushButton *deleteBtn = new QPushButton(tr("Delete"));
            deleteBtn->setFocusPolicy(Qt::NoFocus);
            deleteBtn->setProperty("accessibleName", "danger");
            cardLayout->addWidget(editBtn);
            cardLayout->addWidget(deleteBtn);

            QString schedId = s.id;

            connect(enableCheck, &QCheckBox::toggled, [this, schedId](bool checked) {
                ScheduleManager::CleaningSchedule updated = mScheduleManager->getSchedule(schedId);
                updated.enabled = checked;
                mScheduleManager->updateSchedule(updated);
            });

            connect(editBtn, &QPushButton::clicked, [this, schedId, &dialog, &refreshList]() {
                ScheduleManager::CleaningSchedule existing = mScheduleManager->getSchedule(schedId);
                ScheduleEditorDialog editor(existing, &dialog);
                connect(&editor, &ScheduleEditorDialog::scheduleUpdated, this, [this](const ScheduleManager::CleaningSchedule &s) {
                    mScheduleManager->updateSchedule(s);
                });
                editor.exec();
                refreshList();
            });

            connect(deleteBtn, &QPushButton::clicked, [this, schedId, &refreshList]() {
                mScheduleManager->deleteSchedule(schedId);
                refreshList();
            });

            listLayout->addWidget(card);
        }

        listLayout->addStretch();
    };

    refreshList();

    scrollArea->setWidget(scrollWidget);
    layout->addWidget(scrollArea);

    QPushButton *addBtn = new QPushButton(tr("Add Schedule"));
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setProperty("accessibleName", "primary");
    connect(addBtn, &QPushButton::clicked, [this, &dialog, &refreshList]() {
        ScheduleEditorDialog editor(&dialog);
        connect(&editor, &ScheduleEditorDialog::scheduleCreated, this, [this](const ScheduleManager::CleaningSchedule &s) {
            mScheduleManager->createSchedule(s);
        });
        editor.exec();
        refreshList();
    });

    QPushButton *closeBtn = new QPushButton(tr("Close"));
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->addWidget(addBtn);
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    layout->addLayout(btnRow);

    dialog.exec();
}

void SettingsPage::onViewCleaningHistory()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Cleaning History"));
    dialog.setObjectName("cleaningHistoryDialog");
    dialog.setMinimumSize(600, 400);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLabel *dlgTitle = new QLabel(tr("Cleaning History"));
    dlgTitle->setProperty("accessibleName", "dialog-title");
    layout->addWidget(dlgTitle);

    QPlainTextEdit *textEdit = new QPlainTextEdit;
    textEdit->setReadOnly(true);

    QString logPath = mSettingManager->getConfigPath() + "/clean_history.log";
    QFile logFile(logPath);
    if (logFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QStringList lines;
        QTextStream stream(&logFile);
        while (!stream.atEnd()) {
            lines.append(stream.readLine());
        }
        logFile.close();

        int start = qMax(0, lines.size() - 50);
        QStringList recent = lines.mid(start);
        textEdit->setPlainText(recent.join('\n'));
    } else {
        textEdit->setPlainText(tr("No cleaning history available."));
    }

    layout->addWidget(textEdit);

    QHBoxLayout *btnRow = new QHBoxLayout;
    QPushButton *clearBtn = new QPushButton(tr("Clear History"));
    clearBtn->setProperty("accessibleName", "danger");
    connect(clearBtn, &QPushButton::clicked, [logPath, textEdit]() {
        QFile::remove(logPath);
        textEdit->setPlainText("");
    });
    btnRow->addWidget(clearBtn);
    btnRow->addStretch();
    QPushButton *closeBtn = new QPushButton(tr("Close"));
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    btnRow->addWidget(closeBtn);
    layout->addLayout(btnRow);

    dialog.exec();
}

void SettingsPage::updateScheduleSummary()
{
    QList<ScheduleManager::CleaningSchedule> schedules = mScheduleManager->getAllSchedules();

    if (schedules.isEmpty()) {
        ui->lblQuickSetupSummary->setText(tr("No schedules active"));
        return;
    }

    QDateTime earliest;
    QString nextName;
    for (const auto &s : schedules) {
        if (!s.enabled) continue;
        QDateTime next = mScheduleManager->getNextRunTime(s);
        if (!earliest.isValid() || next < earliest) {
            earliest = next;
            nextName = s.name;
        }
    }

    if (earliest.isValid()) {
        ui->lblQuickSetupSummary->setText(
            tr("Next: %1 — %2").arg(nextName, earliest.toString("ddd, MMM d h:mm AP")));
    } else {
        ui->lblQuickSetupSummary->setText(tr("%1 schedule(s) configured").arg(schedules.size()));
    }
}
```

**Step 2: Build and verify compilation**

Run: `cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -20`
Expected: Clean build, 0 errors, 0 warnings

**Step 3: Commit**

```bash
git add shared/nexis/Pages/Settings/settings_page.cpp
git commit -m "refactor(settings): rewrite settings_page.cpp for .ui-defined widgets (BUG-44)

Wire up Scheduled Cleaning widgets via ui-> instead of member pointers.
Remove all grid manipulation code from initScheduledCleaning().
Add transparent scroll area backgrounds. Update drop shadow list."
```

---

## Task 4: Add QGroupBox QSS rules for Settings page

**Files:**
- Modify: `shared/nexis/static/themes/default/style/style.qss` (insert at line ~1356, in SETTINGS section)

**Step 1: Replace the SETTINGS QSS section**

Replace the existing Settings section (lines 1353-1377) with updated rules that include QGroupBox styling, scroll area transparency, and updated label rules that work inside groups.

Find and replace from `/****** SETTINGS ******/` through `#lblQuickSetupSummary { ... }`:

```qss
/***************
    SETTINGS
****************/

#SettingsPage #scrollArea,
#SettingsPage #scrollContent {
    background-color: transparent;
}

#SettingsPage QGroupBox {
    border: 1px solid @borderColor;
    border-radius: 12;
    margin-top: 12;
    padding: 12 12 6 12;
    background-color: @cardBg;
}

#SettingsPage QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    padding: 0 6;
    color: @color05;
    font-size: 11pt;
    font-weight: bold;
}

#SettingsPage QLabel {
    font-size: 10pt;
    color: @color12;
}

#lblCreatedBy {
    font-size: 9pt;
    color: @color06;
}

#lblQuickSetupSummary {
    font-size: 9pt;
    color: @color06;
}
```

Note: The `QLabel[accessibleName="title"]` rule is removed — the QGroupBox `::title` pseudo-element now handles section titles.

**Step 2: Build and verify**

Run: `cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: Clean build

**Step 3: Commit**

```bash
git add shared/nexis/static/themes/default/style/style.qss
git commit -m "style(settings): add QGroupBox card rules matching HardwareInfo/GnomeSettings (BUG-44)"
```

---

## Task 5: Build, run app, and visually verify

**Step 1: Clean rebuild**

Run: `rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) && cmake --build build -j$(sysctl -n hw.ncpu)`
Expected: Clean build, 0 errors

**Step 2: Run tests**

Run: `ctest --test-dir build --output-on-failure`
Expected: All tests pass (the Settings page has no unit tests, but this verifies no regressions)

**Step 3: Run the app and navigate to Settings**

Run: `./build/nexis/nexis`

Verify:
- [ ] Settings page shows 5 titled card sections (General, Appearance, Alerts, Tools, Scheduled Cleaning)
- [ ] Each card has a border, rounded corners, and dark card background
- [ ] Labels are left-aligned, controls are to the right
- [ ] Checkboxes have inline text labels (no separate label above)
- [ ] Battery Health row is hidden if no battery present
- [ ] Disk Health Alert checkbox is hidden if no SMART data
- [ ] Disk Analyzer custom path row appears only when "Custom..." is selected
- [ ] Scheduled Cleaning section shows all controls (quick setup, buttons, threshold, notifications)
- [ ] Footer credit label appears at the bottom, right-aligned
- [ ] Page scrolls when the window is made shorter
- [ ] Window can be resized narrower than before (test ~500px width)
- [ ] Light/Dark theme switching works correctly on all sections
- [ ] All combobox selections persist when switching pages and returning

**Step 4: Commit final verification**

```bash
git add -A
git commit -m "fix(settings): complete settings page layout redesign (BUG-44)

Fixes: inconsistent column count, excessive minimum width, no visual
grouping, no scroll area. Settings now use 5 QGroupBox card sections
inside a QScrollArea, matching the HardwareInfo and GnomeSettings
page patterns."
```

---

## Task 6: Update tracking files and documentation

**Files:**
- Modify: `BUGS.md` — Mark BUG-44 `[x]` with resolution note
- Modify: `docs/APPLICATION_OVERVIEW.md` — Update Settings page description if applicable
- Modify: `docs/ARCHITECTURE_REVIEW.md` — Update if relevant (reduced QSS complexity, pattern consistency)

**Step 1: Update BUGS.md**

Change `[~]` to `[x]` and add resolution note.

**Step 2: Archive research/plan files**

```bash
mv claude_definitions/BUG-44_research.md claude_definitions/Archive/
mv claude_definitions/BUG-44_plan.md claude_definitions/Archive/
```

**Step 3: Commit tracking updates**

```bash
git add BUGS.md docs/ claude_definitions/
git commit -m "docs: mark BUG-44 resolved, archive research/plan files"
```
