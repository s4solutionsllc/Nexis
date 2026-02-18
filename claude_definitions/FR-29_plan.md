# FR-29 Phase 1 (Battery Health) — Implementation Plan

**Date:** February 2026
**Scope:** Battery health monitoring — data layer, Dashboard, Hardware Info, Settings
**Prerequisites:** Research complete (`FR-29_research.md`)

---

## Overview

Phase 1 adds battery health monitoring across four layers:
1. **Data layer** — `BatteryInfo` class with macOS IOKit and Linux sysfs implementations
2. **Dashboard** — Battery Health CircleBar with graceful degradation and alert
3. **Hardware Info** — Battery detail section with full metrics
4. **Settings** — Battery health alert threshold

Resources page (long-term charts) and disk health are deferred to Phases 2–3.

---

## Task 1: BatteryInfo Data Class

### 1.1 — Create shared header
- [x] Create `shared/nexis-core/Info/battery_info.h`
- [x] Define `BatteryData` struct with fields:
  - `bool hasBattery`
  - `int chargePercent` (0-100)
  - `int healthPercent` (0-100, derived: maxCapacity/designCapacity*100)
  - `int cycleCount` (-1 if unavailable)
  - `int designCycleCount` (-1 if unavailable, usually 1000)
  - `double currentCapacityMah` (-1 if unavailable)
  - `double maxCapacityMah` (-1 if unavailable)
  - `double designCapacityMah` (-1 if unavailable)
  - `double temperatureCelsius` (-1 if unavailable)
  - `int voltageMv` (-1 if unavailable)
  - `double amperageMa` (negative = discharging)
  - `double powerWatts` (-1 if unavailable)
  - `bool isCharging`
  - `bool isPluggedIn`
  - `int timeRemainingMinutes` (-1 if calculating/unavailable)
  - `QString status` ("Charging", "Discharging", "Full", "Not charging")
  - `QString condition` ("Good", "Fair", "Replace")
  - `QString manufacturer`
  - `QString model`
  - `QString technology` ("Li-ion", "Li-poly", etc.)
  - `QDate manufactureDate` (invalid if unavailable)
  - `int chargeStartThreshold` (-1 if no TLP, Linux only)
  - `int chargeStopThreshold` (-1 if no TLP, Linux only)
- [x] Define `BatteryInfo` class:
  - Constructor
  - `BatteryData getBatteryData() const`
  - `bool hasBattery() const`
  - `void updateBatteryInfo()` (re-read all dynamic values)
  - Private: `void discoverBattery()`, `BatteryData mData`

**Acceptance:** Header compiles on both platforms. ✅

### 1.2 — Create shared getters
- [x] Create `shared/nexis-core/Info/battery_info_shared.cpp`
- [x] Implement `getBatteryData()`, `hasBattery()` as simple member returns

**Acceptance:** Shared cpp compiles. ✅

### 1.3 — Create macOS implementation
- [x] Create `macos/nexis-core/Info/battery_info.cpp`
- [x] Include `<IOKit/IOKitLib.h>`, `<CoreFoundation/CoreFoundation.h>`
- [x] `discoverBattery()`:
  - `IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("AppleSmartBattery"))`
  - Check `BatteryInstalled` property
  - Set `mData.hasBattery` based on result
  - Release service handle
- [x] `updateBatteryInfo()`:
  - Get service, call `IORegistryEntryCreateCFProperties()`
  - Read all properties from dictionary:
    - `CycleCount` → cycleCount
    - `DesignCapacity` → designCapacityMah
    - Try `AppleRawMaxCapacity` first, fall back to `MaxCapacity` → maxCapacityMah
    - `CurrentCapacity` → currentCapacityMah
    - `Temperature` ÷ 10.0 → temperatureCelsius
    - `Voltage` → voltageMv
    - `InstantAmperage` (try first), fall back to `Amperage` → amperageMa
    - `IsCharging` → isCharging
    - `ExternalConnected` → isPluggedIn
    - `FullyCharged` → derive status
    - `TimeRemaining` → timeRemainingMinutes (65535 = -1)
    - `DesignCycleCount9C` → designCycleCount
    - `ManufactureDate` → decode packed date (year[15:9]+1980, month[8:5], day[4:0])
  - Derive healthPercent: `(maxCapacityMah / designCapacityMah) * 100`
  - Derive powerWatts: `abs(voltageMv * amperageMa) / 1e6`
  - Derive condition: ≥80% Good, ≥60% Fair, <60% Replace
  - Derive status string from isCharging/isPluggedIn/FullyCharged
  - CFRelease props, IOObjectRelease service
- [x] Constructor calls `discoverBattery()` then `updateBatteryInfo()` if battery found
- [x] Helper: static function to read CFNumber as int from dictionary
- [x] Helper: static function to read CFBoolean from dictionary

**Acceptance:** macOS build succeeds. On a MacBook, `BatteryInfo` reports real values. On a Mac desktop, `hasBattery()` returns false. ✅

### 1.4 — Create Linux implementation
- [x] Create `linux/nexis-core/Info/battery_info.cpp`
- [x] `discoverBattery()`:
  - Enumerate `/sys/class/power_supply/` directories
  - For each, read `type` file; filter for "Battery"
  - Store first matching path (e.g., `/sys/class/power_supply/BAT0`)
  - Set `mData.hasBattery` if found
- [x] `updateBatteryInfo()`:
  - Read sysfs files using `FileUtil::readStringFromFile()`:
    - `status` → status string, derive isCharging/isPluggedIn
    - `capacity` → chargePercent
    - `charge_full` (µAh) OR `energy_full` (µWh) → maxCapacity
    - `charge_full_design` OR `energy_full_design` → designCapacity
    - Convert µAh to mAh (÷ 1000) or µWh to mWh (÷ 1000)
    - `voltage_now` (µV) → voltageMv (÷ 1000)
    - `current_now` (µA) → amperageMa (÷ 1000)
    - `power_now` (µW) → powerWatts (÷ 1e6)
    - `temp` (0.1 °C) → temperatureCelsius (÷ 10)
    - `cycle_count` → cycleCount
    - `manufacturer` → manufacturer
    - `model_name` → model
    - `technology` → technology
  - Handle missing files gracefully (check `QFile::exists()` before reading, set -1 defaults)
  - Handle charge-based vs energy-based: if `charge_full` exists use charge; else use energy
  - For energy-based, convert mWh to mAh using nominal voltage if both energy and voltage are available
  - Derive healthPercent, condition (same formulas as macOS)
  - Time remaining: estimate from `energy_now / power_now * 60` if discharging
  - TLP thresholds (read-only): check `charge_control_start_threshold` and `charge_control_end_threshold` existence
- [x] Constructor calls `discoverBattery()` then `updateBatteryInfo()` if battery found
- [x] Store battery sysfs path as `QString mBatteryPath` private member

**Acceptance:** Linux build succeeds. On a laptop, reports real values. On a desktop, `hasBattery()` returns false. ✅

### 1.5 — Build verification
- [x] Clean rebuild on macOS: `rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) && cmake --build build -j$(sysctl -n hw.ncpu)`
- [x] Verify no warnings related to battery_info files

---

## Task 2: InfoManager Integration

### 2.1 — Update InfoManager header
- [x] Add `#include <Info/battery_info.h>` to `info_manager.h`
- [x] Add `BatteryInfo bi;` to private members
- [x] Add public method declarations:
  - `BatteryData getBatteryData() const;`
  - `void updateBatteryInfo();`
  - `bool hasBattery() const;`

### 2.2 — Update InfoManager implementation
- [x] Add Battery Provider section to `info_manager.cpp`:
  ```cpp
  BatteryData InfoManager::getBatteryData() const { return bi.getBatteryData(); }
  void InfoManager::updateBatteryInfo() { bi.updateBatteryInfo(); }
  bool InfoManager::hasBattery() const { return bi.hasBattery(); }
  ```

### 2.3 — Build verification
- [x] Incremental build succeeds

---

## Task 3: SettingManager — Battery Alert Keys

### 3.1 — Add keys to header
- [x] Add to `SettingKeys` namespace in `setting_manager.h`:
  ```cpp
  const QString BatteryAlertPercent("BatteryAlertPercent");
  const QString BatteryAlertLastHealth("BatteryAlertLastHealth");
  const QString BatteryAlertSnoozedUntil("BatteryAlertSnoozedUntil");
  ```
- [x] Add getter/setter declarations:
  ```cpp
  void setBatteryAlertPercent(const int value);
  int getBatteryAlertPercent() const;
  void setBatteryAlertLastHealth(const int value);
  int getBatteryAlertLastHealth() const;
  void setBatteryAlertSnoozedUntil(const QString &value);
  QString getBatteryAlertSnoozedUntil() const;
  ```

### 3.2 — Implement getters/setters
- [x] Add to `setting_manager.cpp`:
  - `BatteryAlertPercent` — default `0` (disabled)
  - `BatteryAlertLastHealth` — default `0`
  - `BatteryAlertSnoozedUntil` — default `""` (empty = not snoozed)

### 3.3 — Build verification
- [x] Incremental build succeeds

---

## Task 4: Dashboard — Battery CircleBar

### 4.1 — Add battery container to UI
- [x] Edit `shared/nexis/Pages/Dashboard/dashboard_page.ui`:
  - Add `batteryContainer` QWidget at Row 1, Column 3 (or restructure Row 1 to accommodate 4 columns)
  - Add `batteryContainerLayout` QVBoxLayout inside it
  - Match sizing/margins of `tempContainer` and `gpuContainer` (sizePolicy Expanding/Expanding, spacing 5, margins 10/10/10/5)
  - Update `circleBars` row to `colspan=4`
  - Update `widgetUpdateBar` row to `colspan=4`

### 4.2 — Add members and slots to header
- [x] Edit `dashboard_page.h`:
  - Add slot: `void updateBatteryBar();`
  - Add member: `CircleBar* mBatteryBar;`

### 4.3 — Implement battery CircleBar logic
- [x] Edit `dashboard_page.cpp`:
  - **Constructor initializer list:** `mBatteryBar(new CircleBar(tr("BATTERY"), {"#2ec27e", "#26a269"}, this))` (green gradient — health is the primary metric)
  - **In `init()`, after GPU block:** Graceful degradation:
    ```cpp
    if (im->hasBattery()) {
        ui->batteryContainerLayout->addWidget(mBatteryBar);
        connect(mTimer, &QTimer::timeout, this, &DashboardPage::updateBatteryBar);
    } else {
        ui->batteryContainer->hide();
        mBatteryBar->hide();
    }
    ```
  - **Initial fetch:** `if (im->hasBattery()) updateBatteryBar();`
  - **Drop shadow:** `if (im->hasBattery()) widgets.append(mBatteryBar);`
  - **`updateBatteryBar()` slot:**
    - Call `im->updateBatteryInfo()`
    - Get `BatteryData bat = im->getBatteryData()`
    - Display health % as the CircleBar value (not charge %)
    - Value text: `"89%\n342 cycles"` (health % and cycle count)
    - If cycle count is -1, show just health %
  - **Alert logic** (inverted — warn when BELOW threshold):
    ```cpp
    int alertPercent = mSettingManager->getBatteryAlertPercent();
    if (alertPercent > 0 && bat.healthPercent > 0) {
        int lastHealth = mSettingManager->getBatteryAlertLastHealth();
        QString snoozedUntil = mSettingManager->getBatteryAlertSnoozedUntil();
        bool snoozed = !snoozedUntil.isEmpty() &&
                       QDateTime::currentDateTime() < QDateTime::fromString(snoozedUntil, Qt::ISODate);

        bool shouldFire = bat.healthPercent < alertPercent &&
                          !snoozed &&
                          (lastHealth == 0 || bat.healthPercent <= lastHealth - 5);

        if (shouldFire) {
            AppManager::ins()->getTrayIcon()->showMessage(
                tr("Battery Health Warning"),
                tr("Battery health is %1% (%2). %3 cycles used.")
                    .arg(bat.healthPercent).arg(bat.condition).arg(bat.cycleCount),
                QSystemTrayIcon::Warning);
            mSettingManager->setBatteryAlertLastHealth(bat.healthPercent);
        }
    }
    ```

### 4.4 — Build and visual verification
- [x] Incremental build succeeds
- [x] On laptop: Battery CircleBar appears with correct health %
- [x] On desktop: Battery CircleBar is not visible, no empty space

---

## Task 5: Hardware Info — Battery Section

### 5.1 — Add battery section to UI
- [x] Edit `shared/nexis/Pages/HardwareInfo/hardware_info_page.ui`:
  - Add `grpBattery` QGroupBox (title: "Battery") before vertical spacer
  - Add `tblBattery` QTableWidget inside (2 columns, same properties as existing tables)

### 5.2 — Add populate method declaration
- [x] Edit `hardware_info_page.h`:
  - Add `void populateBattery();` to private methods

### 5.3 — Implement populateBattery()
- [x] Edit `hardware_info_page.cpp`:
  - Follow exact pattern from `populateGraphics()`:
    ```cpp
    void HardwareInfoPage::populateBattery()
    {
        QTableWidget *t = ui->tblBattery;
        t->horizontalHeader()->setVisible(false);
        t->verticalHeader()->setVisible(false);
        t->horizontalHeader()->setStretchLastSection(true);

        if (!im->hasBattery()) {
            ui->grpBattery->hide();
            return;
        }

        BatteryData bat = im->getBatteryData();

        addRow(t, tr("Status"), bat.status);
        addRow(t, tr("Health"), QString("%1% (%2)").arg(bat.healthPercent).arg(bat.condition));
        addRow(t, tr("Charge"), QString("%1%").arg(bat.chargePercent));
        addRow(t, tr("Cycle Count"), bat.cycleCount >= 0
            ? QString("%1 / %2").arg(bat.cycleCount).arg(bat.designCycleCount > 0 ? QString::number(bat.designCycleCount) : tr("N/A"))
            : tr("N/A"));
        addRow(t, tr("Current Capacity"), bat.currentCapacityMah >= 0
            ? QString("%1 mAh").arg(bat.currentCapacityMah, 0, 'f', 0) : tr("N/A"));
        addRow(t, tr("Maximum Capacity"), bat.maxCapacityMah >= 0
            ? QString("%1 mAh").arg(bat.maxCapacityMah, 0, 'f', 0) : tr("N/A"));
        addRow(t, tr("Design Capacity"), bat.designCapacityMah >= 0
            ? QString("%1 mAh").arg(bat.designCapacityMah, 0, 'f', 0) : tr("N/A"));

        if (bat.temperatureCelsius >= 0) {
            double tempF = bat.temperatureCelsius * 9.0 / 5.0 + 32.0;
            addRow(t, tr("Temperature"), QString("%1 °C / %2 °F")
                .arg(bat.temperatureCelsius, 0, 'f', 1).arg(tempF, 0, 'f', 1));
        }

        if (bat.voltageMv >= 0)
            addRow(t, tr("Voltage"), QString("%1 V").arg(bat.voltageMv / 1000.0, 0, 'f', 3));

        if (bat.powerWatts >= 0)
            addRow(t, tr("Power"), QString("%1 W (%2)")
                .arg(bat.powerWatts, 0, 'f', 1)
                .arg(bat.isCharging ? tr("charging") : tr("discharging")));

        if (bat.timeRemainingMinutes >= 0) {
            int h = bat.timeRemainingMinutes / 60;
            int m = bat.timeRemainingMinutes % 60;
            addRow(t, tr("Time Remaining"), QString("%1h %2m").arg(h).arg(m));
        }

        if (!bat.manufacturer.isEmpty())
            addRow(t, tr("Manufacturer"), bat.manufacturer);

        if (!bat.model.isEmpty())
            addRow(t, tr("Model"), bat.model);

        if (!bat.technology.isEmpty())
            addRow(t, tr("Technology"), bat.technology);

        if (bat.manufactureDate.isValid())
            addRow(t, tr("Manufacture Date"), bat.manufactureDate.toString("yyyy-MM-dd"));

        // Linux TLP charge thresholds (read-only)
        if (bat.chargeStartThreshold >= 0 && bat.chargeStopThreshold >= 0) {
            addRow(t, tr("Charge Limit"),
                QString("%1% – %2% (%3)")
                    .arg(bat.chargeStartThreshold)
                    .arg(bat.chargeStopThreshold)
                    .arg(tr("managed by TLP")));
        }

        t->resizeColumnsToContents();
        fitTableHeight(t);
    }
    ```
  - Wire into `init()`: add `populateBattery();` after `populateMemory();`

### 5.4 — Build and visual verification
- [x] Incremental build succeeds
- [x] On laptop: Battery section appears with all available metrics
- [x] On desktop: Battery section is not visible

---

## Task 6: Settings — Battery Alert Threshold

### 6.1 — Add UI widgets
- [x] Edit `shared/nexis/Pages/Settings/settings_page.ui`:
  - Add `lblBatteryHealthPercent` QLabel (text: "Battery Health") at grid Row 4, Column 3
  - Add `spinBatteryHealthPercent` QSpinBox at grid Row 5, Column 3
    - Range 0-100, suffix " %", focusPolicy ClickFocus, special value text "" (0 = disabled)

### 6.2 — Wire loading and saving
- [x] Edit `settings_page.cpp`:
  - In `init()`:
    ```cpp
    ui->spinBatteryHealthPercent->setValue(mSettingManager->getBatteryAlertPercent());
    // Hide battery alert if no battery detected
    if (!InfoManager::ins()->hasBattery()) {
        ui->lblBatteryHealthPercent->hide();
        ui->spinBatteryHealthPercent->hide();
    }
    ```
  - Add slot:
    ```cpp
    void SettingsPage::on_spinBatteryHealthPercent_valueChanged(int value) {
        mSettingManager->setBatteryAlertPercent(value);
    }
    ```

### 6.3 — Build verification
- [x] Incremental build succeeds
- [x] Settings page shows battery health alert spinner on laptops, hidden on desktops

---

## Task 7: Final Verification & Cleanup

### 7.1 — Clean rebuild
- [x] Full clean rebuild on macOS
- [x] Verify zero warnings in battery-related files

### 7.2 — Functional testing
- [x] Dashboard: Battery CircleBar shows correct health % with green gradient
- [x] Dashboard: On desktop, battery bar is hidden without layout artifacts
- [x] Dashboard: Kiosk mode (F11) renders battery bar correctly
- [x] Hardware Info: Battery section shows all available metrics
- [x] Hardware Info: Metrics match `ioreg -brc AppleSmartBattery` output (macOS)
- [x] Settings: Battery health alert threshold spinner works
- [x] Settings: Setting threshold to >0 and letting health be below it triggers a tray notification
- [x] Theme switching: Battery CircleBar updates colors correctly in dark/light mode

### 7.3 — Update tracking files
- [x] Mark FR-29 Phase 1 tasks complete in this plan document
- [x] Commit and push

---

## Summary of All Files

### New (4 files)
| # | File | Lines (est.) |
|---|------|-------------|
| 1 | `shared/nexis-core/Info/battery_info.h` | ~60 |
| 2 | `shared/nexis-core/Info/battery_info_shared.cpp` | ~15 |
| 3 | `macos/nexis-core/Info/battery_info.cpp` | ~150 |
| 4 | `linux/nexis-core/Info/battery_info.cpp` | ~130 |

### Modified (10 files)
| # | File | Changes |
|---|------|---------|
| 5 | `shared/nexis/Managers/info_manager.h` | +5 lines (include, member, 3 declarations) |
| 6 | `shared/nexis/Managers/info_manager.cpp` | +15 lines (Battery Provider section) |
| 7 | `shared/nexis/Managers/setting_manager.h` | +12 lines (3 keys, 6 getter/setter declarations) |
| 8 | `shared/nexis/Managers/setting_manager.cpp` | +25 lines (6 getter/setter implementations) |
| 9 | `shared/nexis/Pages/Dashboard/dashboard_page.ui` | +20 lines (batteryContainer widget) |
| 10 | `shared/nexis/Pages/Dashboard/dashboard_page.h` | +3 lines (member, slot) |
| 11 | `shared/nexis/Pages/Dashboard/dashboard_page.cpp` | +50 lines (init, update, alert) |
| 12 | `shared/nexis/Pages/HardwareInfo/hardware_info_page.ui` | +25 lines (grpBattery group) |
| 13 | `shared/nexis/Pages/HardwareInfo/hardware_info_page.h` | +1 line (populateBattery declaration) |
| 14 | `shared/nexis/Pages/HardwareInfo/hardware_info_page.cpp` | +60 lines (populateBattery implementation) |
| 15 | `shared/nexis/Pages/Settings/settings_page.ui` | +15 lines (label + spinbox) |
| 16 | `shared/nexis/Pages/Settings/settings_page.cpp` | +10 lines (load + save + hide) |

**Total estimated:** ~4 new files, 10 modified files, ~460 new lines of code.
