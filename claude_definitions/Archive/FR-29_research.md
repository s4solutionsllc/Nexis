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

---

# FR-29 Phase 2 (Disk Health / SMART Monitoring) — Research

**Date:** February 2026
**Scope:** Deep research into SMART data access on macOS and Linux for implementing disk health monitoring in Nexis

---

## 13. macOS Disk Health APIs

### 13.1 diskutil info — Native SMART Access (No Root Required)

The simplest and most reliable approach on macOS. The command `diskutil info -plist disk0` returns a property list containing all NVMe SMART health data under the key `SMARTDeviceSpecificKeysMayVaryNotGuaranteed`.

**Verified output on Apple Silicon (M-series) Mac** — actual data from this machine:

```
SMARTDeviceSpecificKeysMayVaryNotGuaranteed:
  AVAILABLE_SPARE              = 100         (% of spare NVM capacity remaining)
  AVAILABLE_SPARE_THRESHOLD    = 99          (manufacturer-set minimum, Apple sets 99%)
  PERCENTAGE_USED              = 0           (% of endurance consumed, 0-255)
  TEMPERATURE                  = 304         (Kelvin — subtract 273 for Celsius = 31°C)
  POWER_ON_HOURS_0             = 75          (total hours powered on)
  POWER_CYCLES_0               = 208         (number of on/off cycles)
  UNSAFE_SHUTDOWNS_0           = 11          (power loss events without clean shutdown)
  MEDIA_ERRORS_0               = 0           (uncorrectable ECC/CRC/LBA errors)
  NUM_ERROR_INFO_LOG_ENTRIES_0 = 0           (count of error log entries)
  DATA_UNITS_READ_0            = 8779426     (in units of 1000 × 512 bytes = ~4.5 TB)
  DATA_UNITS_WRITTEN_0         = 4575923     (in units of 1000 × 512 bytes = ~2.3 TB)
  HOST_READ_COMMANDS_0         = 142409112   (total read commands issued)
  HOST_WRITE_COMMANDS_0        = 125766010   (total write commands issued)
  CONTROLLER_BUSY_TIME_0       = 0           (minutes controller was busy)
```

**Critical observations:**
- Fields with `_0` and `_1` suffixes represent low and high 64-bit halves of 128-bit NVMe counters
- `TEMPERATURE` is in **Kelvin** (not Celsius like smartctl reports) — must subtract 273
- Apple sets `AVAILABLE_SPARE_THRESHOLD` to 99% (extremely conservative vs. typical 10%)
- All NVMe health log fields from the NVMe specification are present
- `SMARTStatus` is a separate top-level key: `"Verified"` or `"Failing"`

**Additional disk classification fields from diskutil plist:**
```
BusProtocol     = "Apple Fabric"   (Apple Silicon NVMe) / "SATA" / "USB"
SolidState      = true/false       (SSD vs HDD)
Internal        = true/false       (internal vs external)
MediaName       = "APPLE SSD AP0512Z"
DeviceIdentifier = "disk0"
IOKitSize       = 500277792768     (size in bytes)
```

**Parsing approach for Nexis:**
- Call `diskutil info -plist diskX` via QProcess
- Parse the output as XML plist using Qt XML or QProcess + plutil
- OR call `diskutil info diskX` (plain text) and parse key-value lines with regex
- The plist approach is more robust (structured data, no localization issues)

**Privilege requirements:** None. `diskutil info` runs without root/sudo.

### 13.2 IOKit APIs for SMART Data

Two IOKit plugin interfaces exist for programmatic SMART access:

**NVMe: `NVMeSMARTLib.plugin`**
- Located at `/System/Library/Extensions/NVMeSMARTLib.plugin/`
- Universal binary (x86_64 + arm64e) — works on both Intel and Apple Silicon
- Plugin type UUID: `AA0FA6F9-C2D6-457F-B10B-59A13253292F`
- Available from IORegistry: `IOCFPlugInTypes` property on `IONVMeBlockStorageDevice` confirms the UUID
- Interface struct `IONVMeSMARTInterface` provides:
  - `SMARTReadData(struct nvme_smart_log *)` — reads NVMe SMART/Health Information log
  - `GetIdentifyData(struct nvme_id_ctrl *, unsigned int nsid)` — reads identify controller data
  - `GetLogPage(void *buf, uint32_t logPageId, uint32_t size)` — reads arbitrary log pages
- Reference implementation: smartmontools `os_darwin.cpp` at https://github.com/smartmontools/smartmontools/blob/master/smartmontools/os_darwin.cpp

**SATA: `SMARTLib.plugin`**
- Located at `/System/Library/Extensions/SMARTLib.plugin/`
- Universal binary (x86_64 + arm64e)
- Interface struct `IOATASMARTInterface` provides:
  - `SMARTReadData(ATASMARTData *)` — reads SMART attribute data
  - `SMARTReadDataThresholds(ATASMARTDataThresholds *)` — reads attribute thresholds
  - `SMARTReturnStatus(Boolean *exceeded)` — checks if any threshold has been exceeded
  - `SMARTEnableDisableOperations(Boolean enable)` — enables/disables SMART on the drive
  - `SMARTExecuteOffLineImmediate(Boolean extended)` — runs self-test

**Why NOT to use IOKit directly for Nexis:**
1. The NVMe SMART interface is **undocumented** — Apple has never published headers
2. Must reverse-engineer UUIDs and struct layouts from smartmontools source
3. The `diskutil info -plist` approach provides identical data with zero complexity
4. IOKit access requires careful CFPlugin lifecycle management (create, query, release)
5. No privilege benefit — both diskutil and IOKit run without root for SMART reads

**Recommendation:** Use `diskutil info -plist` for macOS. Reserve IOKit for edge cases where diskutil is unavailable (highly unlikely).

### 13.3 system_profiler as Alternative

`system_profiler SPNVMeDataType -xml` provides structured XML with per-drive data:
- Model, serial, firmware revision, BSD name, capacity, TRIM support
- `smart_status`: `"Verified"` or `"Failing"` (same as diskutil)
- Does NOT include detailed SMART attributes (no temperature, percentage used, etc.)

`system_profiler SPSerialATADataType -xml` provides the same for SATA drives.

**Use case:** Disk enumeration only (listing all NVMe and SATA devices with basic info). Not sufficient for SMART health data.

### 13.4 smartctl on macOS (via Homebrew)

Available via `brew install smartmontools`. Provides the same data as diskutil but with different formatting.

**Example output for Apple Silicon NVMe:**
```
Model Number:                       APPLE SSD AP1024Z
SMART overall-health self-assessment test result: PASSED
Temperature:                        41 Celsius
Available Spare:                    100%
Percentage Used:                    2%
Data Units Read:                    241 TB
Data Units Written:                 52.7 TB
Power Cycles:                       255
Power On Hours:                     1,514
Unsafe Shutdowns:                   5
Media and Data Integrity Errors:    0
```

**JSON output mode (`smartctl -a --json disk0`):**
```json
{
  "device": { "type": "nvme", "name": "/dev/disk0" },
  "smart_status": { "passed": true },
  "nvme_smart_health_information_log": {
    "critical_warning": 0,
    "temperature": 41,
    "available_spare": 100,
    "available_spare_threshold": 10,
    "percentage_used": 2,
    "data_units_read": 482023671,
    "data_units_written": 105489832,
    "host_reads": 3849187232,
    "host_writes": 2654987123,
    "controller_busy_time": 1234,
    "power_cycles": 255,
    "power_on_hours": 1514,
    "unsafe_shutdowns": 5,
    "media_errors": 0,
    "num_err_log_entries": 0,
    "temperature_sensors": [41, 38]
  },
  "temperature": { "current": 41 },
  "power_cycle_count": 255,
  "power_on_time": { "hours": 1514 }
}
```

**Why NOT to use smartctl as primary on macOS:**
1. Not installed by default — requires Homebrew
2. `diskutil` provides equivalent NVMe data natively
3. Requires `sudo` for some operations (smartctl requires root to open raw device)
4. Additional dependency management (version checks, path detection)

**When smartctl IS useful on macOS:**
- For third-party SATA SSDs that diskutil reports minimal SMART data for
- For detailed SATA attribute tables (attribute IDs, thresholds, raw values)
- As a fallback when diskutil SMART data is unavailable

### 13.5 Apple Silicon vs Third-Party SSDs

**Apple Silicon internal SSDs (M1/M2/M3/M4):**
- Connect via "Apple Fabric" protocol (not standard PCIe NVMe)
- The Apple NVMe controller (`AppleANS3CGv2Controller`) exposes a standard NVMe health log
- All standard NVMe SMART fields are available
- `AVAILABLE_SPARE_THRESHOLD` is set to 99% (vs. typical 10% on consumer NVMe drives)
- `NVMe SMART Capable = Yes` is set in IORegistry
- smartctl reports Apple SSDs correctly as NVMe devices
- No differences observed between M1/M2/M3/M4 in terms of SMART data availability

**Third-party NVMe SSDs (via Thunderbolt/USB enclosure):**
- SMART data availability depends on the enclosure bridge chip
- Most USB-to-NVMe bridges do NOT pass through SMART commands
- As of January 2025, some JMicron and Realtek bridge chips support SMART passthrough
- diskutil may show `SMARTStatus: "Not Supported"` for external drives
- smartctl with specific device types (e.g., `sntasmedia`) can sometimes access SMART data through USB bridges

**Third-party SATA SSDs:**
- Standard `IOATASMARTInterface` works for SATA SSDs
- diskutil reports basic `SMARTStatus: "Verified"/"Failing"` but may NOT expose the detailed `SMARTDeviceSpecificKeysMayVaryNotGuaranteed` dictionary
- smartctl provides full SATA attribute tables for third-party SATA SSDs
- This is the one case where smartctl adds value over diskutil on macOS

### 13.6 Disk Enumeration on macOS

**Method 1: diskutil list (recommended)**
```bash
diskutil list                  # Human-readable
diskutil list -plist           # Structured XML — enumerate all disks and containers
```
Returns all physical disks and synthesized APFS containers.

**Method 2: IOKit IOServiceGetMatchingServices**
```cpp
IOServiceGetMatchingServices(kIOMainPortDefault,
    IOServiceMatching("IOBlockStorageDevice"), &iterator);
```
Enumerates all block storage devices. Check `Device Characteristics` dictionary for model, serial, medium type.

**Method 3: /dev/disk* enumeration**
Already implemented in Nexis `DiskInfo::getDiskNames()` (macOS):
```cpp
QDir devDir("/dev");
QStringList entries = devDir.entryList({"disk*"}, QDir::System);
// Filter: only base disks (no 's' for partitions)
```

**Disk type detection from diskutil plist:**
| Field | NVMe SSD | SATA SSD | SATA HDD |
|-------|----------|----------|----------|
| `BusProtocol` | "Apple Fabric" or "PCI-Express" | "SATA" | "SATA" |
| `SolidState` | true | true | false |
| `Internal` | true | true/false | true/false |

---

## 14. Linux Disk Health APIs

### 14.1 sysfs /sys/block/ for Disk Enumeration

Already implemented in Nexis `DiskInfo::getDiskNames()` (Linux):
```cpp
QDir blocks("/sys/block");
for (const QFileInfo &entryInfo : blocks.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
    if (QFile::exists(QString("%1/device").arg(entryInfo.absoluteFilePath()))) {
        disks.append(entryInfo.baseName());
    }
}
```

**Disk type detection from sysfs:**
| Property Path | NVMe SSD | SATA SSD | SATA HDD |
|--------------|----------|----------|----------|
| Device name pattern | `nvme*` | `sd*` | `sd*` |
| `/sys/block/X/queue/rotational` | 0 | 0 | 1 |
| `/sys/block/X/device/model` | Model name | Model name | Model name |
| `/sys/block/X/device/vendor` | N/A (NVMe) | Vendor name | Vendor name |

```cpp
bool isNVMe = deviceName.startsWith("nvme");
bool isRotational = FileUtil::readStringFromFile(
    QString("/sys/block/%1/queue/rotational").arg(deviceName)).trimmed() == "1";
// NVMe → NVMe SSD; !rotational → SATA SSD; rotational → HDD
```

### 14.2 smartctl on Linux — Primary SMART Data Source

**Installation:** Pre-installed on many distros, or via `apt install smartmontools` / `dnf install smartmontools`.

**Detection:** Check `which smartctl` or attempt `QProcess::execute("smartctl", {"--version"})`.

**NVMe output (`smartctl -a /dev/nvme0n1`):**
```
=== START OF SMART DATA SECTION ===
SMART overall-health self-assessment test result: PASSED

SMART/Health Information (NVMe Log 0x02):
Critical Warning:                   0x00
Temperature:                        38 Celsius
Available Spare:                    100%
Available Spare Threshold:          10%
Percentage Used:                    3%
Data Units Read:                    12,345,678 [6.32 TB]
Data Units Written:                 8,765,432 [4.48 TB]
Host Read Commands:                 234,567,890
Host Write Commands:                123,456,789
Controller Busy Time:               456
Power Cycles:                       312
Power On Hours:                     5,678
Unsafe Shutdowns:                   23
Media and Data Integrity Errors:    0
Error Information Log Entries:      0
Warning Composite Temperature Time: 0
Critical Composite Temperature Time: 0
Temperature Sensor 1:               38 Celsius
Temperature Sensor 2:               42 Celsius
```

**SATA output (`smartctl -a /dev/sda`):**
```
=== START OF READ SMART DATA SECTION ===
SMART overall-health self-assessment test result: PASSED

SMART Attributes Data Structure revision number: 16
Vendor Specific SMART Attributes with Thresholds:
ID# ATTRIBUTE_NAME          FLAG     VALUE WORST THRESH TYPE     UPDATED  WHEN_FAILED RAW_VALUE
  1 Raw_Read_Error_Rate     0x000f   114   099   006    Pre-fail Always        -       77169616
  5 Reallocated_Sector_Ct   0x0033   100   100   036    Pre-fail Always        -       0
  7 Seek_Error_Rate         0x000f   089   060   030    Pre-fail Always        -       886544182
  9 Power_On_Hours          0x0032   085   085   000    Old_age  Always        -       13542
 10 Spin_Retry_Count        0x0013   100   100   097    Pre-fail Always        -       0
 12 Power_Cycle_Count       0x0032   100   100   020    Old_age  Always        -       456
194 Temperature_Celsius     0x0022   037   048   000    Old_age  Always        -       37 (0 18 0 0 0)
196 Reallocated_Event_Count 0x0032   100   100   000    Old_age  Always        -       0
197 Current_Pending_Sector  0x0012   100   100   000    Old_age  Always        -       0
198 Offline_Uncorrectable   0x0010   100   100   000    Old_age  Offline       -       0
199 UDMA_CRC_Error_Count    0x003e   200   200   000    Old_age  Always        -       0
```

### 14.3 smartctl JSON Output (`--json` flag)

Available since smartmontools 7.1 (Debian 11+, Ubuntu 22.04+). This is the recommended parsing approach.

**JSON structure for NVMe (`smartctl -a --json /dev/nvme0n1`):**
```json
{
  "device": {
    "name": "/dev/nvme0n1",
    "info_name": "/dev/nvme0n1",
    "type": "nvme",
    "protocol": "NVMe"
  },
  "model_name": "Samsung SSD 970 EVO Plus 1TB",
  "serial_number": "S4EWNF0M123456",
  "firmware_version": "2B2QEXM7",
  "smart_status": {
    "passed": true
  },
  "nvme_smart_health_information_log": {
    "critical_warning": 0,
    "temperature": 38,
    "available_spare": 100,
    "available_spare_threshold": 10,
    "percentage_used": 3,
    "data_units_read": 12345678,
    "data_units_written": 8765432,
    "host_reads": 234567890,
    "host_writes": 123456789,
    "controller_busy_time": 456,
    "power_cycles": 312,
    "power_on_hours": 5678,
    "unsafe_shutdowns": 23,
    "media_errors": 0,
    "num_err_log_entries": 0,
    "temperature_sensors": [38, 42]
  },
  "temperature": {
    "current": 38
  },
  "power_cycle_count": 312,
  "power_on_time": {
    "hours": 5678
  }
}
```

**JSON structure for SATA (`smartctl -a --json /dev/sda`):**
```json
{
  "device": {
    "name": "/dev/sda",
    "info_name": "/dev/sda",
    "type": "sat",
    "protocol": "ATA"
  },
  "model_name": "ST2000DM008-2FR102",
  "serial_number": "ZFL1234X",
  "firmware_version": "SN06",
  "smart_status": {
    "passed": true
  },
  "ata_smart_attributes": {
    "revision": 16,
    "table": [
      {
        "id": 5,
        "name": "Reallocated_Sector_Ct",
        "value": 100,
        "worst": 100,
        "thresh": 36,
        "when_failed": "",
        "flags": { "string": "PO--CK" },
        "raw": { "value": 0, "string": "0" }
      },
      {
        "id": 9,
        "name": "Power_On_Hours",
        "value": 85,
        "worst": 85,
        "thresh": 0,
        "when_failed": "",
        "flags": { "string": "-O--CK" },
        "raw": { "value": 13542, "string": "13542" }
      },
      {
        "id": 194,
        "name": "Temperature_Celsius",
        "value": 37,
        "worst": 48,
        "thresh": 0,
        "when_failed": "",
        "flags": { "string": "-O---K" },
        "raw": { "value": 37, "string": "37 (Min/Max 18/48)" }
      }
    ]
  },
  "temperature": {
    "current": 37
  },
  "power_cycle_count": 456,
  "power_on_time": {
    "hours": 13542
  }
}
```

**Key JSON paths for unified data extraction:**

| Data | NVMe Path | SATA Path |
|------|-----------|-----------|
| Device type | `device.type` == `"nvme"` | `device.type` == `"sat"` |
| Overall health | `smart_status.passed` | `smart_status.passed` |
| Temperature | `nvme_smart_health_information_log.temperature` | `temperature.current` |
| Power-on hours | `nvme_smart_health_information_log.power_on_hours` | `power_on_time.hours` |
| Power cycles | `nvme_smart_health_information_log.power_cycles` | `power_cycle_count` |
| NVMe Available Spare | `nvme_smart_health_information_log.available_spare` | N/A |
| NVMe Percentage Used | `nvme_smart_health_information_log.percentage_used` | N/A |
| NVMe Unsafe Shutdowns | `nvme_smart_health_information_log.unsafe_shutdowns` | N/A |
| NVMe Media Errors | `nvme_smart_health_information_log.media_errors` | N/A |
| NVMe Critical Warning | `nvme_smart_health_information_log.critical_warning` | N/A |
| SATA Reallocated Sectors | N/A | `ata_smart_attributes.table[id=5].raw.value` |
| SATA Pending Sectors | N/A | `ata_smart_attributes.table[id=197].raw.value` |
| SATA Uncorrectable | N/A | `ata_smart_attributes.table[id=198].raw.value` |

### 14.4 smartctl --scan for Disk Discovery

`smartctl --scan` lists all detected devices with their types:
```
/dev/sda -d scsi # /dev/sda, SCSI device
/dev/nvme0 -d nvme # /dev/nvme0, NVMe device
```

`smartctl --scan --json` provides structured output:
```json
{
  "devices": [
    { "name": "/dev/sda", "info_name": "/dev/sda", "type": "scsi", "protocol": "SCSI" },
    { "name": "/dev/nvme0", "info_name": "/dev/nvme0", "type": "nvme", "protocol": "NVMe" }
  ]
}
```

This can supplement the existing sysfs enumeration in `DiskInfo::getDiskNames()`.

### 14.5 Privilege Requirements on Linux

**smartctl requires elevated privileges:**
- `smartctl -a /dev/sdX` requires root or membership in the `disk` group
- NVMe devices additionally require `CAP_SYS_ADMIN` (for `NVME_IOCTL_ADMIN_CMD`)
- SATA devices additionally require `CAP_SYS_RAWIO`
- Adding the user to the `disk` group is not sufficient — the underlying ioctl calls still require root-level capabilities
- In practice, `sudo smartctl` is the only reliable approach

**Approaches for non-root access:**
1. **Polkit / pkexec:** Launch `smartctl` via `pkexec smartctl -a --json /dev/nvme0n1` — prompts user for password via GUI dialog. Recommended for desktop applications.
2. **setuid helper:** Install a small setuid-root binary that runs smartctl. Used by GSmartControl.
3. **sudo with NOPASSWD:** Configure sudoers for smartctl only. Not user-friendly for desktop apps.
4. **Capabilities on binary:** `setcap cap_sys_rawio,cap_sys_admin+ep /usr/sbin/smartctl` — works but requires root to set initially and may be reset by package updates.
5. **Cache approach:** Run smartctl once with pkexec, cache results for the session. Re-prompt only on manual refresh.

**Recommendation for Nexis:** Use `pkexec` on Linux. Show a "Requires authentication to read disk health data" message in the UI. Cache results to avoid repeated authentication prompts. If pkexec fails (e.g., no polkit agent), fall back to showing "Root access required to read SMART data" with instructions.

### 14.6 libatasmart vs smartctl

**libatasmart** (https://github.com/Rupan/libatasmart):
- Lean C library for reading ATA SMART data
- Used by GNOME Disks (udisks)
- API: `sk_disk_open()`, `sk_disk_smart_read_data()`, `sk_disk_smart_get_overall()`, `sk_disk_smart_get_temperature()`
- Returns individual attribute data via callback: `sk_disk_smart_parse(disk, callback, user_data)`
- **ATA/SATA only** — no NVMe support whatsoever
- Requires linking against `libatasmart` (additional dependency)
- Not available on macOS
- Last significant update was years ago

**smartctl via QProcess:**
- Supports both NVMe and SATA/ATA
- Cross-platform (Linux, macOS, FreeBSD, Windows)
- JSON output mode since v7.1 (no need to parse fragile text output)
- Actively maintained (latest release 2025)
- Already familiar pattern in Nexis (QProcess used for iostat, lscpu, etc.)
- No additional library dependency — just needs the binary installed

**Verdict:** Use smartctl via QProcess. The JSON output mode eliminates the main historical disadvantage (fragile text parsing). NVMe support alone makes libatasmart unsuitable since NVMe is now the dominant disk type.

---

## 15. Key SMART Attributes Reference

### 15.1 NVMe SMART Fields (NVMe Specification Log Page 02h)

All fields are standardized across vendors — consistent behavior guaranteed.

| Field | Critical Level | Raw Range | Description |
|-------|---------------|-----------|-------------|
| Critical Warning | **Immediate** if nonzero | 0x00-0xFF bitmask | Bit 0: spare below threshold. Bit 1: temp exceeded. Bit 2: reliability degraded. Bit 3: read-only mode. Bit 4: volatile memory backup failed. |
| Temperature | Warning >70C | Kelvin (diskutil) / Celsius (smartctl) | Current composite temperature |
| Available Spare | **Critical** when below threshold | 0-100% | Remaining spare NVM capacity. Drop below threshold = replace soon |
| Available Spare Threshold | Reference | 0-100% | Manufacturer-set minimum. Apple sets 99%, most vendors set 10% |
| Percentage Used | Caution >80%, Critical >100% | 0-255% | Estimated endurance consumed. Can exceed 100%. Drive may still function. |
| Data Units Read | Informational | 128-bit counter | Total 512-byte units read (in units of 1000). Multiply by 512000 for bytes. |
| Data Units Written | Endurance tracking | 128-bit counter | Total 512-byte units written. Compare to TBW rating. |
| Power On Hours | Informational | 128-bit counter | Total hours powered on |
| Power Cycles | Informational | 128-bit counter | Total on/off cycles |
| Unsafe Shutdowns | Monitor | 128-bit counter | Power loss without clean shutdown. High count stresses wear-leveling tables. |
| Media and Data Integrity Errors | **Critical** if nonzero | 128-bit counter | Uncorrectable errors. Any nonzero value warrants investigation. |
| Error Information Log Entries | Monitor | 128-bit counter | Running count of error log entries |
| Controller Busy Time | Informational | 128-bit counter | Minutes controller was processing commands |

**NVMe health score calculation:**
```
if (criticalWarning != 0) → "Failing" (red)
else if (mediaErrors > 0) → "Caution" (yellow)
else if (percentageUsed > 80) → "Caution" (yellow)
else if (availableSpare < availableSpareThreshold + 5) → "Caution" (yellow)
else → "Good" (green/blue)

healthPercent = max(0, 100 - percentageUsed)
// Note: percentageUsed can exceed 100, so clamp to 0
```

### 15.2 SATA SMART Attributes (Vendor-Specific but Widely Standardized)

**Critical failure indicators (nonzero = concern):**

| ID | Name | Type | Description |
|----|------|------|-------------|
| 5 | Reallocated_Sector_Ct | Pre-fail | Bad sectors permanently remapped to spare area. Even 1 reallocated sector correlates with 20-60x higher failure probability within 60 days (Backblaze data). |
| 196 | Reallocated_Event_Count | Old_age | Number of reallocation operations attempted (successful or not). Growing count indicates active degradation. |
| 197 | Current_Pending_Sector | Old_age | Sectors that failed to read and are queued for reallocation on next write. Growing count = active media degradation. |
| 198 | Offline_Uncorrectable | Old_age | Sectors that could not be read or reallocated. Even 1 correlates with 39x higher failure probability (Backblaze). |

**Important monitoring indicators:**

| ID | Name | Type | Description |
|----|------|------|-------------|
| 1 | Raw_Read_Error_Rate | Pre-fail | Rate of hardware read errors. Interpretation is vendor-specific (some encode error rate as fraction, not absolute count). |
| 7 | Seek_Error_Rate | Pre-fail | HDD mechanical head positioning failures. Vendor-specific encoding. |
| 9 | Power_On_Hours | Old_age | Total operational time. Context for other attributes. |
| 10 | Spin_Retry_Count | Pre-fail | HDD spindle couldn't reach operating speed on first attempt. Indicates motor or power issues. |
| 12 | Power_Cycle_Count | Old_age | Total on/off cycles. |
| 190/194 | Temperature_Celsius | Old_age | Drive temperature. Some drives use 190, some use 194, some use both. |
| 199 | UDMA_CRC_Error_Count | Old_age | Interface/cable errors, not drive media failure. High count = check SATA cable. |

**SSD-specific endurance attributes (vendor varies):**

| ID | Name | Description |
|----|------|-------------|
| 177 | Wear_Leveling_Count | Average P/E cycles. Starts high, decreases with wear. |
| 173 | SSD_Wear_Leveling (worst) | Maximum erase count on single block. |
| 233 | Media_Wearout_Indicator | Intel SSDs: NAND erase cycle percentage remaining (100→1). |
| 241 | Total_LBAs_Written | Total data written. Compare to TBW endurance rating. |
| 242 | Total_LBAs_Read | Total data read. |

**SATA health score calculation:**
```
if (attribute[5].raw > 0 || attribute[197].raw > 0 || attribute[198].raw > 0) → "Failing" (red)
else if (attribute[196].raw > 0 || attribute[10].raw > 0) → "Caution" (yellow)
else if (normalized value <= threshold for any Pre-fail attribute) → "Failing" (red)
else → "Good" (green/blue)
```

---

## 16. Cross-Platform Architecture Design

### 16.1 DiskHealthData Struct

```cpp
enum class DiskType {
    NVMe,
    SataSSD,
    SataHDD,
    Unknown
};

enum class DiskHealthStatus {
    Good,       // All values normal
    Caution,    // Degradation detected but not critical
    Failing,    // Critical threshold crossed or critical warning
    Unknown     // SMART data unavailable
};

struct DiskHealthData {
    // Identity
    QString deviceName;       // "disk0" (macOS), "nvme0n1" / "sda" (Linux)
    QString devicePath;       // "/dev/disk0", "/dev/nvme0n1", "/dev/sda"
    QString modelName;        // "APPLE SSD AP0512Z", "Samsung SSD 970 EVO Plus"
    QString serialNumber;
    QString firmwareVersion;
    quint64 capacityBytes = 0;
    DiskType diskType = DiskType::Unknown;
    bool isInternal = true;

    // Overall health
    DiskHealthStatus healthStatus = DiskHealthStatus::Unknown;
    int healthPercent = -1;   // 0-100, -1 if unavailable
    bool smartPassed = false; // Raw SMART overall assessment
    QString smartStatusText;  // "Verified"/"PASSED"/"FAILED"/"Not Supported"

    // Common fields (NVMe and SATA)
    int temperatureCelsius = -1;
    qint64 powerOnHours = -1;
    qint64 powerCycles = -1;

    // NVMe-specific fields
    int availableSpare = -1;            // 0-100%
    int availableSpareThreshold = -1;   // 0-100%
    int percentageUsed = -1;            // 0-255%
    int criticalWarning = -1;           // bitmask
    qint64 unsafeShutdowns = -1;
    qint64 mediaErrors = -1;
    qint64 errorLogEntries = -1;
    qint64 controllerBusyMinutes = -1;
    double dataUnitsReadTB = -1.0;      // converted to TB
    double dataUnitsWrittenTB = -1.0;   // converted to TB

    // SATA-specific fields (raw values from key attribute IDs)
    qint64 reallocatedSectors = -1;     // ID 5
    qint64 reallocatedEvents = -1;      // ID 196
    qint64 pendingSectors = -1;         // ID 197
    qint64 uncorrectableSectors = -1;   // ID 198
    qint64 spinRetryCount = -1;         // ID 10
    qint64 udmaCrcErrors = -1;          // ID 199
    int wearLevelingCount = -1;         // ID 177 (SSD, normalized 0-100)
    double totalBytesWrittenTB = -1.0;  // Derived from ID 241

    // Full attribute table (SATA only — for detailed view)
    struct SmartAttribute {
        int id;
        QString name;
        int currentValue;
        int worstValue;
        int threshold;
        qint64 rawValue;
        QString rawString;
        QString flags;
        QString whenFailed;
    };
    QList<SmartAttribute> allAttributes; // Empty for NVMe (uses named fields instead)
};
```

### 16.2 Data Source Strategy Per Platform

| Platform | Disk Enumeration | SMART Data | Privilege |
|----------|-----------------|------------|-----------|
| macOS | `diskutil list -plist` or existing `/dev/disk*` scan | `diskutil info -plist diskX` (NVMe). `smartctl -a --json diskX` for SATA fallback. | None (diskutil). Homebrew + sudo (smartctl). |
| Linux | Existing `/sys/block/*/device/` scan or `smartctl --scan --json` | `smartctl -a --json /dev/X` via pkexec | pkexec for sudo prompt, or membership in disk group |

### 16.3 Graceful Fallback When smartctl Isn't Installed

**macOS fallback:**
1. Try `diskutil info -plist diskX` — always available, provides full NVMe SMART data
2. If diskutil provides `SMARTDeviceSpecificKeysMayVaryNotGuaranteed` → use it (NVMe)
3. If only `SMARTStatus` is available (SATA or external) → show pass/fail only
4. If user wants detailed SATA attributes → prompt "Install smartmontools: brew install smartmontools"

**Linux fallback:**
1. Check if smartctl exists: `QProcess::execute("which", {"smartctl"})` or `QStandardPaths::findExecutable("smartctl")`
2. If not installed:
   - Still enumerate disks from `/sys/block/`
   - Read basic info from sysfs (model, capacity, rotational flag)
   - Show "SMART data unavailable — install smartmontools for disk health monitoring"
   - Provide install command for the detected package manager: `sudo apt install smartmontools` / `sudo dnf install smartmontools`
3. If installed but permission denied:
   - Show "Authentication required to read disk health data"
   - Offer to retry with pkexec

### 16.4 DiskHealthInfo Class Design

Following the established Info class pattern (template: GpuInfo, BatteryInfo):

```
shared/nexis-core/Info/disk_health_info.h          # Header with DiskHealthData struct + class
shared/nexis-core/Info/disk_health_info_shared.cpp  # Cross-platform getters
macos/nexis-core/Info/disk_health_info.cpp          # macOS: diskutil + optional smartctl
linux/nexis-core/Info/disk_health_info.cpp          # Linux: smartctl via QProcess + pkexec
```

**Key methods:**
```cpp
class NEXISCORESHARED_EXPORT DiskHealthInfo {
public:
    DiskHealthInfo();
    QList<DiskHealthData> getDiskHealthData() const;
    void updateDiskHealth();          // Re-reads SMART data for all disks
    bool hasSmartSupport() const;     // At least one disk has SMART data
    bool isSmartctlAvailable() const; // smartctl binary found in PATH

private:
    void discoverDisks();             // Enumerate and classify disks
    void readSmartData();             // Read SMART data for each disk

    // macOS-specific
    void readDiskutilSmart(DiskHealthData &disk);  // Parse diskutil plist
    void readSmartctlData(DiskHealthData &disk);   // Parse smartctl JSON

    // Linux-specific
    void readSmartctlData(DiskHealthData &disk);   // Parse smartctl JSON

    QList<DiskHealthData> mDisks;
    bool mSmartctlAvailable = false;
};
```

### 16.5 QProcess / JSON Parsing Pattern

```cpp
// Check smartctl availability
mSmartctlAvailable = !QStandardPaths::findExecutable("smartctl").isEmpty();

// Read SMART data via smartctl JSON
QProcess proc;
proc.setProgram("smartctl");
proc.setArguments({"-a", "--json", devicePath});
proc.start();
proc.waitForFinished(10000); // 10 second timeout

if (proc.exitCode() != 0 && proc.exitCode() != 4) {
    // Exit code 4 = SMART command failed but data was returned
    // Other non-zero = error
    return;
}

QJsonDocument doc = QJsonDocument::fromJson(proc.readAllStandardOutput());
QJsonObject root = doc.object();

// Device type detection
QString deviceType = root["device"].toObject()["type"].toString();
bool isNVMe = (deviceType == "nvme");

// Overall health
disk.smartPassed = root["smart_status"].toObject()["passed"].toBool();

// Temperature (unified path)
disk.temperatureCelsius = root["temperature"].toObject()["current"].toInt(-1);

if (isNVMe) {
    QJsonObject nvmeLog = root["nvme_smart_health_information_log"].toObject();
    disk.availableSpare = nvmeLog["available_spare"].toInt(-1);
    disk.percentageUsed = nvmeLog["percentage_used"].toInt(-1);
    disk.unsafeShutdowns = nvmeLog["unsafe_shutdowns"].toVariant().toLongLong();
    disk.mediaErrors = nvmeLog["media_errors"].toVariant().toLongLong();
    // ... etc
} else {
    QJsonArray attrs = root["ata_smart_attributes"].toObject()["table"].toArray();
    for (const QJsonValue &attrVal : attrs) {
        QJsonObject attr = attrVal.toObject();
        int id = attr["id"].toInt();
        qint64 rawValue = attr["raw"].toObject()["value"].toVariant().toLongLong();
        switch (id) {
            case 5:   disk.reallocatedSectors = rawValue; break;
            case 9:   disk.powerOnHours = rawValue; break;
            case 194: disk.temperatureCelsius = rawValue; break;
            case 197: disk.pendingSectors = rawValue; break;
            case 198: disk.uncorrectableSectors = rawValue; break;
            // ... etc
        }
    }
}
```

### 16.6 macOS diskutil Plist Parsing Pattern

```cpp
// On macOS, prefer diskutil for NVMe (no root needed, always available)
QProcess proc;
proc.setProgram("diskutil");
proc.setArguments({"info", "-plist", deviceName}); // e.g., "disk0"
proc.start();
proc.waitForFinished(5000);

QByteArray plistData = proc.readAllStandardOutput();
// Parse with QXmlStreamReader or CFPropertyListCreateFromXMLData

// Classification
disk.isInternal = plistDict["Internal"].toBool();
bool isSolid = plistDict["SolidState"].toBool();
QString protocol = plistDict["BusProtocol"].toString();
if (protocol == "Apple Fabric" || protocol == "PCI-Express") {
    disk.diskType = DiskType::NVMe;
} else if (protocol == "SATA" && isSolid) {
    disk.diskType = DiskType::SataSSD;
} else if (protocol == "SATA" && !isSolid) {
    disk.diskType = DiskType::SataHDD;
}

// SMART data from plist
QVariantMap smartKeys = plistDict["SMARTDeviceSpecificKeysMayVaryNotGuaranteed"].toMap();
if (!smartKeys.isEmpty()) {
    disk.availableSpare = smartKeys["AVAILABLE_SPARE"].toInt();
    disk.availableSpareThreshold = smartKeys["AVAILABLE_SPARE_THRESHOLD"].toInt();
    disk.percentageUsed = smartKeys["PERCENTAGE_USED"].toInt();
    // Temperature is in Kelvin in diskutil plist!
    int tempKelvin = smartKeys["TEMPERATURE"].toInt();
    disk.temperatureCelsius = tempKelvin - 273;
    disk.powerOnHours = smartKeys["POWER_ON_HOURS_0"].toLongLong();
    disk.powerCycles = smartKeys["POWER_CYCLES_0"].toLongLong();
    disk.unsafeShutdowns = smartKeys["UNSAFE_SHUTDOWNS_0"].toLongLong();
    disk.mediaErrors = smartKeys["MEDIA_ERRORS_0"].toLongLong();
    // 128-bit values: combine _0 (low) and _1 (high) parts
    quint64 unitsReadLow = smartKeys["DATA_UNITS_READ_0"].toULongLong();
    quint64 unitsReadHigh = smartKeys["DATA_UNITS_READ_1"].toULongLong();
    // Each unit = 1000 * 512 bytes = 512000 bytes
    disk.dataUnitsReadTB = (double)(unitsReadLow) * 512000.0 / (1024.0*1024*1024*1024);
    disk.dataUnitsWrittenTB = (double)smartKeys["DATA_UNITS_WRITTEN_0"].toULongLong()
                              * 512000.0 / (1024.0*1024*1024*1024);
}

// SMART pass/fail
disk.smartStatusText = plistDict["SMARTStatus"].toString(); // "Verified" or "Failing"
disk.smartPassed = (disk.smartStatusText == "Verified");
```

---

## 17. smartctl Version Compatibility

| Feature | Min Version | Debian/Ubuntu | Notes |
|---------|------------|---------------|-------|
| Basic NVMe support | 6.5 | Debian 9 / Ubuntu 18.04 | |
| `--json` output | 7.1 | Debian 11 / Ubuntu 22.04 | Recommended minimum |
| `--json=c` (compact) | 7.2 | Debian 12 / Ubuntu 24.04 | |
| `--scan --json` | 7.1 | Debian 11 / Ubuntu 22.04 | |

**Fallback for pre-7.1:** Parse text output with regex. The text format has been stable for years, but JSON is strongly preferred for robustness.

**Version detection:**
```cpp
QProcess proc;
proc.start("smartctl", {"--version"});
proc.waitForFinished(3000);
QString versionOutput = proc.readAllStandardOutput();
// Parse: "smartctl 7.4 2023-08-01 ..."
QRegularExpression re("smartctl (\\d+)\\.(\\d+)");
QRegularExpressionMatch match = re.match(versionOutput);
int major = match.captured(1).toInt();
int minor = match.captured(2).toInt();
bool hasJson = (major > 7 || (major == 7 && minor >= 1));
```

---

## 18. Integration with Existing DiskInfo

The existing `DiskInfo` class handles disk **usage** (space, I/O throughput). The new `DiskHealthInfo` class handles disk **health** (SMART data, wear, errors). These are separate concerns:

| Concern | Class | Update Frequency | Data Type |
|---------|-------|------------------|-----------|
| Disk usage (free/used space) | `DiskInfo` | Every 5 seconds (timerDisk) | Real-time utilization |
| Disk I/O (read/write speed) | `DiskInfo` | Every 1 second (mTimer) | Real-time throughput |
| Disk health (SMART data) | `DiskHealthInfo` (new) | On page load, manual refresh | Slowly changing hardware state |

The two classes share disk enumeration logic but differ in everything else. Keeping them separate follows the existing pattern (e.g., `GpuInfo` for GPU utilization is separate from `ThermalInfo` for temperatures).

---

## 19. Risk Analysis — Disk Health

| Risk | Severity | Mitigation |
|------|----------|-----------|
| smartctl not installed on Linux | Medium | Graceful fallback: show disk list without SMART data, prompt user to install smartmontools |
| smartctl requires root on Linux | Medium | Use `pkexec` for polkit GUI prompt. Cache results to avoid repeated prompts. |
| diskutil plist format changes in future macOS | Low | Key names have been stable across macOS 12-15. Fall back to smartctl if parsing fails. |
| Temperature in Kelvin from diskutil vs Celsius from smartctl | Medium | Explicitly handle unit conversion per data source. Document in code comments. |
| Apple's high AVAILABLE_SPARE_THRESHOLD (99%) triggers false alarms | Medium | When determining health status, use actual spare value vs. a reasonable threshold (e.g., 20%), not Apple's 99%. |
| 128-bit NVMe counters overflow 64-bit integers | Very Low | Data Units Read/Written are the only practical concern. At 512000 bytes/unit, 2^64 units = ~9.4 zettabytes. Not a concern. |
| SATA attribute meanings vary by vendor | Medium | Focus on universally-meaningful attributes (IDs 5, 9, 194, 197, 198). Use smartmontools' drivedb.h knowledge via smartctl rather than interpreting raw values ourselves. |
| External/USB drives may not support SMART | Low | Check `SMARTStatus` for "Not Supported". Show "SMART Not Available" in UI. |
| NVMe Percentage Used can exceed 100% | Low | Clamp health percentage to 0 minimum. Display raw value in detailed view. |
| Third-party SATA drives on macOS need smartctl for full attributes | Medium | Use diskutil for basic status. If smartctl is available, use it for full attribute table. Show "Install smartmontools for detailed SMART data" if not available. |
| Multiple physical disks | Low | Already handled by returning `QList<DiskHealthData>`. Dashboard shows primary (internal) disk. Hardware Info page shows all disks. |

---

## 20. Reference Implementations

### 20.1 QDiskInfo (Qt + smartctl frontend)
- GitHub: https://github.com/edisionnano/QDiskInfo
- Qt-based, CrystalDiskInfo-style UI
- Uses QProcess to call smartctl
- Parses smartctl text output (not JSON — older approach)
- Linux-only

### 20.2 smartmontools os_darwin.cpp (IOKit NVMe/SATA)
- GitHub: https://github.com/smartmontools/smartmontools/blob/master/smartmontools/os_darwin.cpp
- C++ implementation of IOKit SMART access for both NVMe (IONVMeSMARTInterface) and SATA (IOATASMARTInterface)
- The authoritative reference for how to use the undocumented Apple NVMe SMART plugin
- UUID for NVMe plugin: `AA0FA6F9-C2D6-457F-B10B-59A13253292F` (matches IORegistry `IOCFPlugInTypes`)

### 20.3 macos-ssd-monitor (Shell-based Apple Silicon monitor)
- GitHub: https://github.com/4ndymcfly/macos-ssd-monitor
- Shell script using smartctl + diskutil + system_profiler
- Apple Silicon native (M1/M2/M3/M4 tested)
- Monitors: SMART health, temperature, usage stats, TRIM support
- Shows how to combine multiple data sources on macOS

### 20.4 prometheus-community/smartctl_exporter (smartctl JSON)
- GitHub: https://github.com/prometheus-community/smartctl_exporter
- Go-based, uses `smartctl --json` for structured parsing
- Handles both NVMe and SATA JSON output formats
- Good reference for JSON field paths and edge cases
