# FR-29 Phase 1 (Battery Health) — Research

**Date:** February 2026
**Scope:** Deep codebase analysis to understand exactly how to implement battery health monitoring

---

## 1. Architecture Summary

Nexis follows a clean three-layer architecture for hardware monitoring:

```
[Data Layer]        Info classes (nexis-core library)
                    ├── shared header:    shared/nexis-core/Info/<name>_info.h
                    ├── shared getters:   shared/nexis-core/Info/<name>_info_shared.cpp
                    ├── macOS impl:       macos/nexis-core/Info/<name>_info.cpp
                    └── linux impl:       linux/nexis-core/Info/<name>_info.cpp

[Manager Layer]     InfoManager singleton (facade)
                    └── shared/nexis/Managers/info_manager.{h,cpp}

[UI Layer]          Pages that consume InfoManager
                    ├── DashboardPage  → CircleBar gauges, alerts
                    ├── HardwareInfoPage → QGroupBox + QTableWidget detail sections
                    ├── ResourcesPage → HistoryChart spline charts
                    └── SettingsPage → QSpinBox alert thresholds
```

**Build system:** Single root `CMakeLists.txt` uses `GLOB_RECURSE` — dropping files into the correct directories is sufficient. No CMake modifications needed. IOKit and CoreFoundation frameworks are already linked on macOS.

---

## 2. Info Class Pattern (Template: GpuInfo)

### Header (`shared/nexis-core/Info/gpu_info.h`)
- Defines a data struct (`GpuDevice`) with all fields
- Defines the Info class with `NEXISCORESHARED_EXPORT` macro
- Public API: constructor, `getGpuDevices()`, `updateGpuInfo()`, `hasGpu()`
- Private: `discoverGpus()`, `QList<GpuDevice> mDevices`

### Shared getters (`shared/nexis-core/Info/gpu_info_shared.cpp`)
- Simple return-the-member methods: `getGpuDevices() const { return mDevices; }`, `hasGpu() const { return !mDevices.isEmpty(); }`

### macOS impl (`macos/nexis-core/Info/gpu_info.cpp`)
- Constructor calls `discoverGpus()`
- `discoverGpus()`: `IOServiceGetMatchingServices(kIOMainPortDefault, IOServiceMatching("IOAccelerator"), &iterator)` then iterates with `IOIteratorNext`
- Properties read via `IORegistryEntryCreateCFProperty()` for individual keys, or `IORegistryEntryCreateCFProperties()` for full dictionary
- CFType handling: `CFGetTypeID()` → `CFDataGetTypeID()` / `CFStringGetTypeID()` / `CFNumberGetTypeID()` branching
- Cleanup: `IOObjectRelease()` for io_objects, `CFRelease()` for CF objects
- `updateGpuInfo()`: Re-enumerates IOAccelerators, reads `PerformanceStatistics` dictionary

### Linux impl (`linux/nexis-core/Info/gpu_info.cpp`)
- Uses sysfs paths (`/sys/class/drm/card*/device/`) and external commands (`nvidia-smi`, `lspci`)
- `FileUtil::readStringFromFile()` for sysfs reads

### Key for BatteryInfo:
- Battery on macOS uses `IOServiceMatching("AppleSmartBattery")` — same IOKit pattern, different service class
- Battery on Linux uses `/sys/class/power_supply/BAT*/` — same sysfs pattern as thermal sensors

---

## 3. InfoManager Integration

**File:** `shared/nexis/Managers/info_manager.{h,cpp}`

Singleton with direct member variables (not pointers):
```cpp
private:
    CpuInfo ci;
    DiskInfo di;
    MemoryInfo mi;
    NetworkInfo ni;
    SystemInfo si;
    ProcessInfo pi;
    ThermalInfo ti;
    GpuInfo gi;
    // NEW: BatteryInfo bi;
```

Forwarding methods delegate to members. GPU example:
```cpp
// Header:
QList<GpuDevice> getGpuDevices() const;
void updateGpuInfo();
bool hasGpu() const;

// Implementation:
QList<GpuDevice> InfoManager::getGpuDevices() const { return gi.getGpuDevices(); }
void InfoManager::updateGpuInfo() { gi.updateGpuInfo(); }
bool InfoManager::hasGpu() const { return gi.hasGpu(); }
```

For BatteryInfo, add the same pattern: `hasBattery()`, `updateBatteryInfo()`, plus getters for individual fields or a struct.

---

## 4. Dashboard Page — CircleBar Architecture

### Layout Structure (from `dashboard_page.ui`)
```
QGridLayout (gridLayout), 3 rows x 3 cols:
┌─────────────────────────────────────────────────┐
│ Row 0, Col 0-2 (colspan=3): circleBars          │
│   QHBoxLayout: [CPU] [MEM] [DISK]               │
├────────────────┬────────────────┬────────────────┤
│ Row 1, Col 0:  │ Row 1, Col 1:  │ Row 1, Col 2:  │
│ tempContainer  │ gpuContainer   │ lineBars       │
│  [cmbSensor]   │  [cmbDevice]   │  [Download]    │
│  [mTempBar]    │  [mGpuBar]     │  [Upload]      │
├────────────────┴────────────────┴────────────────┤
│ Row 2, Col 0-2 (colspan=3): widgetUpdateBar      │
└──────────────────────────────────────────────────┘
```

### CircleBar Constructor
```cpp
CircleBar(const QString &title, const QStringList &colors, QWidget *parent = 0);
```
- `title`: label text above the chart
- `colors`: two-element QStringList for conical gradient (e.g., `{"#813d9c", "#613583"}` for GPU purple)
- `setValue(int value, QString valueText)`: sets the pie arc (0-100) and label

### Graceful Degradation Pattern (GPU as template)
```cpp
// In init():
if (im->hasGpu()) {
    // 1. Populate combo box (if multi-device)
    // 2. Restore saved selection from SettingManager
    // 3. Add CircleBar to container layout
    // 4. Connect combo box signal
    // 5. Connect timer -> update slot
} else {
    ui->gpuContainer->hide();    // collapse grid cell
    mGpuBar->hide();             // prevent orphan rendering at (0,0)
}
```

Both temperature AND GPU follow this identical pattern.

### Timer Architecture
- `mTimer` (1s): connected to `updateCpuBar`, `updateMemoryBar`, `updateNetworkBar`, conditionally `updateTempBar`, `updateGpuBar`
- `timerDisk` (5s): connected to `updateDiskBar`

### Alert Pattern
```cpp
int cpuAlerPercent = mSettingManager->getCpuAlertPercent();
if (cpuAlerPercent > 0) {
    static bool isShow = true;
    if (cpuUsedPercent > cpuAlerPercent && isShow) {
        AppManager::ins()->getTrayIcon()->showMessage(
            tr("High CPU Usage"),
            tr("The amount of CPU used is over %1%.").arg(cpuAlerPercent),
            QSystemTrayIcon::Warning);
        isShow = false;
    } else if (cpuUsedPercent < cpuAlerPercent) {
        isShow = true;
    }
}
```
Uses `static bool isShow` as one-shot gate: fires once, suppresses until value recovers. Three alerts exist: CPU, Memory, Disk.

### Drop Shadow
All visible CircleBars are added to a list and passed to `Utilities::addDropShadow(widgets, 60)`. Conditional inclusion:
```cpp
if (im->hasThermalSensors()) widgets.append(mTempBar);
if (im->hasGpu()) widgets.append(mGpuBar);
```

### Battery CircleBar Placement
Two options for where to place the battery CircleBar:

**Option A — Row 1 as a 4th column:**
- Add `batteryContainer` at Row 1, Column 3
- Shift `lineBars` to Column 3 and put battery at Column 2, OR add a 4th column
- Requires updating `circleBars` and `widgetUpdateBar` to `colspan=4`
- Pro: Keeps battery in the "secondary gauges" row with temp/GPU
- Con: Row 1 could get crowded on narrow screens

**Option B — Add to Row 0 `circleBarsLayout`:**
- Add `mBatteryBar` to the existing `QHBoxLayout` alongside CPU/MEM/DISK
- Pro: Simple, no grid restructuring
- Con: Battery health is a different *kind* of metric than CPU/MEM/DISK utilization

**Option C — New Row 1 with battery + network, push temp/GPU/network down to Row 2:**
- Pro: Clean separation
- Con: More grid restructuring

**Recommendation:** Option A is cleanest. The battery bar naturally groups with temperature and GPU as "hardware health" metrics in Row 1, while Row 0 remains "utilization" metrics.

---

## 5. Hardware Info Page — Section Architecture

### Section Widget Pattern
Each section is a `QGroupBox` with a `QVBoxLayout` containing a single `QTableWidget`:
- 2 columns (bold label, plain value)
- No grid, no frame, no selection, no scrollbars
- Headers hidden in code
- Column 1 stretches to fill width
- Fixed height calculated by `fitTableHeight()` at 30px/row

### Population Pattern
```cpp
void HardwareInfoPage::populateXxx()
{
    QTableWidget *t = ui->tblXxx;
    t->horizontalHeader()->setVisible(false);
    t->verticalHeader()->setVisible(false);
    t->horizontalHeader()->setStretchLastSection(true);

    if (!im->hasXxx()) {
        ui->grpXxx->hide();   // graceful degradation
        return;
    }

    // Fetch data
    addRow(t, tr("Label"), value);
    // ...

    t->resizeColumnsToContents();
    fitTableHeight(t);
}
```

### Existing Sections
System, Processor, Graphics, Memory. **No** Storage, Network, Thermal, or Battery sections exist yet.

### Battery Section Additions
1. Add `QGroupBox grpBattery` with `QTableWidget tblBattery` to `.ui` file (before vertical spacer)
2. Add `populateBattery()` declaration to header
3. Implement `populateBattery()` in cpp
4. Call `populateBattery()` from `init()`
5. QSS is automatic — existing `#HardwareInfoPage QGroupBox` selectors catch new groups

### Data is Static (One-Time)
HardwareInfoPage populates data once at construction — no timer, no refresh. Battery health % and cycle count change so slowly that this is acceptable for the initial implementation. A "Refresh" button could be added later.

---

## 6. Resources Page — Chart Architecture

### HistoryChart Widget
Based on Qt Charts (`QChart`, `QChartView`, `QSplineSeries`).

Constructor:
```cpp
HistoryChart(const QString &title, const int &seriesCount,
             QCategoryAxis* categoriAxisY = nullptr, QWidget *parent = 0);
```

- 60-second rolling window, newest at x=0 (reversed X axis)
- Each update: shift all points right by 1, insert new point at x=0, cap at 61 points
- `static int second` counter per update slot

### Existing Charts (6)
| Chart | Series | Y Max | Custom Y Axis |
|-------|--------|-------|--------------|
| CPU | core count | 100 | No |
| CPU Load Avg | 3 | Dynamic | No |
| Disk R/W | 2 | Dynamic | QCategoryAxis (bytes) |
| Memory | 2 | 100 | No |
| Network | 2 | Dynamic | QCategoryAxis (bytes) |
| GPU | device count | 100 | Conditional (hasGpu) |

### GPU Conditional Chart Pattern
```cpp
// Constructor: mChartGpu(nullptr)
// In init():
if (im->hasGpu()) {
    mChartGpu = new HistoryChart(tr("History of GPU"), gpuCount, nullptr, this);
    mChartGpu->setYMax(100);
    connect(mTimer, &QTimer::timeout, this, &ResourcesPage::updateGpuChart);
}
// In layout building:
if (mChartGpu) widgets.insert(2, mChartGpu);
```

### Battery Health Chart Considerations
Battery health is a **long-term metric** (changes over days/months), not a 60-second rolling window. Per the design decisions (OQ-05), battery health charts should show long-term data from a JSON history file.

This requires a **new chart widget class** (e.g., `LongTermChart`):
- X axis: `QDateTimeAxis` (dates, not seconds)
- Y axis: `QValueAxis` (0-100%)
- Data source: JSON file, not real-time timer
- Time range selector: 30d / 90d / 1y / All

The existing `HistoryChart` uses integer X positions and a point-shifting update model that is fundamentally incompatible with `QDateTimeAxis`. A new class is the clean approach.

**For Phase 1:** The battery session chart (charge %, temperature over the last 60s) CAN use the existing `HistoryChart`. The long-term health chart is Phase 3.

---

## 7. Settings Page — Alert Threshold Pattern

### UI Widgets
Grid layout with labels in one row, QSpinBoxes in the next:
```
Row 4: [lblCpuPercent] [lblMemoryPercent] [lblDiskPercent]
Row 5: [spinCpuPercent] [spinMemoryPercent] [spinDiskPercent]
```
Each QSpinBox: range 0-100, suffix " %", focusPolicy ClickFocus.

### Loading / Saving
```cpp
// init():
ui->spinCpuPercent->setValue(mSettingManager->getCpuAlertPercent());

// Slot (auto-connect naming):
void SettingsPage::on_spinCpuPercent_valueChanged(int value) {
    mSettingManager->setCpuAlertPercent(value);
}
```

### Battery Alert Addition
Add `lblBatteryPercent` + `spinBatteryPercent` at Column 3 (or new row). Battery alert is **inverted** (warn when BELOW threshold, not above).

---

## 8. SettingManager — Key Inventory & Pattern

**File:** `shared/nexis/Managers/setting_manager.{h,cpp}`
**Storage:** `QSettings` INI file at `QStandardPaths::AppConfigLocation/settings.ini`

### Existing Keys (15)
ThemeName, Language, DiskName, StartPage, CPUAlertPercent, MemoryAlertPercent, DiskAlertPercent, AppQuitDialogDontAsk, AppQuitDialogChoice, ColorScheme, DiskAnalyzerTool, DiskAnalyzerCustomPath, KioskMode, TempSensorId, GpuDeviceId

### Key Addition Pattern
1. Add `const QString KeyName("KeyName");` to `SettingKeys` namespace in header
2. Add getter/setter declarations to class
3. Implement getter with default: `return mSettings->value(SettingKeys::KeyName, default).toType();`
4. Implement setter: `mSettings->setValue(SettingKeys::KeyName, value);`

### New Keys Needed for Battery
| Key | Type | Default | Purpose |
|-----|------|---------|---------|
| `BatteryAlertPercent` | int | 0 (disabled) | Threshold for health % alert |
| `BatteryAlertLastHealth` | int | 0 | Health % when alert last fired (for fire-once-then-5%-drop logic) |
| `BatteryAlertSnoozedUntil` | QString | "" | ISO 8601 datetime for snooze (for "Remind in 30 days") |

---

## 9. macOS Battery IOKit API

### Service Matching
```cpp
io_service_t service = IOServiceGetMatchingService(
    kIOMainPortDefault,
    IOServiceMatching("AppleSmartBattery"));
```
Note: Uses `IOServiceGetMatchingService` (singular, returns first match) not `IOServiceGetMatchingServices` (plural, returns iterator). There is only one battery service.

### Property Reading
```cpp
CFMutableDictionaryRef props = nullptr;
IORegistryEntryCreateCFProperties(service, &props, kCFAllocatorDefault, 0);

// Then read individual keys:
CFNumberRef cycleRef = (CFNumberRef)CFDictionaryGetValue(props, CFSTR("CycleCount"));
int cycleCount = 0;
if (cycleRef && CFGetTypeID(cycleRef) == CFNumberGetTypeID())
    CFNumberGetValue(cycleRef, kCFNumberIntType, &cycleCount);
```

### Available Properties
| IORegistry Key | Type | Unit | Notes |
|---------------|------|------|-------|
| `BatteryInstalled` | bool | — | Must check before reading other properties |
| `CycleCount` | int | cycles | Total charge/discharge cycles |
| `DesignCapacity` | int | mAh | Factory capacity |
| `MaxCapacity` or `AppleRawMaxCapacity` | int | mAh | Current max (degrades over time) |
| `CurrentCapacity` | int | mAh | Current charge level |
| `Temperature` | int | 0.1 °C | Divide by 10 for Celsius |
| `Voltage` | int | mV | Current voltage |
| `Amperage` or `InstantAmperage` | int | mA | Negative = discharging |
| `IsCharging` | bool | — | |
| `FullyCharged` | bool | — | |
| `ExternalConnected` | bool | — | Power adapter connected |
| `TimeRemaining` | int | minutes | 65535 = calculating |
| `ManufactureDate` | int | packed | year[15:9]+1980, month[8:5], day[4:0] |
| `DesignCycleCount9C` | int | cycles | Rated max (usually 1000) |

### Health Calculation
```
healthPercent = (MaxCapacity / DesignCapacity) * 100.0
```

### Condition Derivation
```
health >= 80% → "Good"
60% <= health < 80% → "Fair"
health < 60% → "Replace"
```

---

## 10. Linux Battery sysfs API

### Discovery
```cpp
QDir powerSupplyDir("/sys/class/power_supply");
QStringList entries = powerSupplyDir.entryList({"BAT*"}, QDir::Dirs);
// Typically: BAT0, BAT1 (rare)
```

Verify type: read `/sys/class/power_supply/BAT0/type` → should be "Battery"

### Available sysfs Files
| File | Type | Unit | Notes |
|------|------|------|-------|
| `status` | string | — | "Charging", "Discharging", "Full", "Not charging" |
| `capacity` | int | % | OS-reported charge percentage |
| `charge_full` | int | µAh | Current max capacity |
| `charge_full_design` | int | µAh | Factory design capacity |
| `energy_now` | int | µWh | Alternative energy-based (some drivers) |
| `energy_full` | int | µWh | Alternative to charge_full |
| `energy_full_design` | int | µWh | Alternative to charge_full_design |
| `voltage_now` | int | µV | Current voltage |
| `current_now` | int | µA | Current draw (sign varies by driver) |
| `power_now` | int | µW | Power draw/charge rate |
| `temp` | int | 0.1 °C | Battery temperature (not always available) |
| `cycle_count` | int | cycles | (Hardware-dependent; best on ThinkPads) |
| `manufacturer` | string | — | Battery manufacturer |
| `model_name` | string | — | Battery model |
| `technology` | string | — | "Li-ion", "Li-poly", etc. |

### Unit Handling
Linux batteries report in either **charge-based** (µAh) or **energy-based** (µWh) units depending on the driver. The code must:
1. Check for `charge_full` first
2. If missing, fall back to `energy_full`
3. Health calculation works with either: `health = (full / full_design) * 100`

### File Reading Pattern
Use existing `FileUtil::readStringFromFile()` from `Utils/file_util.h` (already used by thermal_info and gpu_info on Linux).

### TLP Integration (Read-Only)
Per design decision OQ-06, check for TLP charge thresholds:
```
/sys/class/power_supply/BAT0/charge_control_start_threshold  (start charging below this %)
/sys/class/power_supply/BAT0/charge_control_end_threshold    (stop charging above this %)
```
These files only exist when TLP (or a compatible power management tool) is active. Display as informational text if present.

---

## 11. File Inventory — What Needs to Be Created/Modified

### New Files (4)
| File | Purpose |
|------|---------|
| `shared/nexis-core/Info/battery_info.h` | BatteryInfo class header |
| `shared/nexis-core/Info/battery_info_shared.cpp` | Cross-platform getters |
| `macos/nexis-core/Info/battery_info.cpp` | macOS IOKit AppleSmartBattery impl |
| `linux/nexis-core/Info/battery_info.cpp` | Linux sysfs /sys/class/power_supply impl |

### Modified Files (8)
| File | Changes |
|------|---------|
| `shared/nexis/Managers/info_manager.h` | Add `#include`, `BatteryInfo bi;`, forwarding declarations |
| `shared/nexis/Managers/info_manager.cpp` | Add Battery Provider forwarding methods |
| `shared/nexis/Managers/setting_manager.h` | Add `BatteryAlertPercent` (+ snooze keys) to SettingKeys, getter/setter declarations |
| `shared/nexis/Managers/setting_manager.cpp` | Add getter/setter implementations |
| `shared/nexis/Pages/Dashboard/dashboard_page.ui` | Add `batteryContainer` widget in grid |
| `shared/nexis/Pages/Dashboard/dashboard_page.h` | Add `CircleBar* mBatteryBar`, `updateBatteryBar()` slot |
| `shared/nexis/Pages/Dashboard/dashboard_page.cpp` | Add battery init, timer connection, update logic, alert |
| `shared/nexis/Pages/HardwareInfo/hardware_info_page.ui` | Add `grpBattery` QGroupBox with `tblBattery` |
| `shared/nexis/Pages/HardwareInfo/hardware_info_page.h` | Add `populateBattery()` declaration |
| `shared/nexis/Pages/HardwareInfo/hardware_info_page.cpp` | Implement `populateBattery()`, wire into `init()` |
| `shared/nexis/Pages/Settings/settings_page.ui` | Add `lblBatteryPercent` + `spinBatteryPercent` |
| `shared/nexis/Pages/Settings/settings_page.cpp` | Load/save battery alert threshold |

### NOT Modified
| File | Reason |
|------|--------|
| `CMakeLists.txt` | GLOB_RECURSE auto-discovers new files |
| Any QSS files | Generic selectors auto-style new widgets |
| Resources page | Long-term charts are Phase 3 |

---

## 12. Risk Analysis

| Risk | Mitigation |
|------|-----------|
| `AppleSmartBattery` properties differ on Apple Silicon vs Intel Macs | Test both key names (`MaxCapacity` vs `AppleRawMaxCapacity`); fall back gracefully |
| Some Linux laptops don't expose `cycle_count` | Return -1, display "N/A" in UI |
| Linux uses charge-based or energy-based units inconsistently | Check for charge_full first, fall back to energy_full |
| `/sys/class/power_supply/BAT0/temp` not always available | Check file existence before reading; skip temperature row if unavailable |
| Desktop machines have no battery | `hasBattery()` returns false → all battery UI hidden cleanly (proven pattern from GPU/Thermal) |
| Multi-battery systems (rare ThinkPads, etc.) | Support by discovering all BAT* entries; for V1, show first battery only on Dashboard, all in HardwareInfo |
| IOKit API changes in future macOS versions | IORegistry key names have been stable for 10+ years; low risk |
