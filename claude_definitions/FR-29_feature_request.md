# FR-29: Battery & SSD/Disk Health Monitoring — Feature Request

**ID:** FR-29
**Date:** February 2026
**Category:** Hardware Health
**Priority:** High (identified as a key competitive differentiator in market research)

---

## 1. Summary

Add battery health monitoring and SMART-based SSD/HDD health monitoring to Nexis. This fills the most significant hardware visibility gap between Nexis and macOS competitors (Sensei, iStat Menus, coconutBattery, DriveDx) and gives Linux users a GUI alternative to CLI tools (`tlp-stat`, `smartctl`, `upower`). The feature integrates into three existing pages — Dashboard (gauges), Hardware Info (detail panels), and Resources (historical charts) — following the same architectural patterns used for GPU and thermal monitoring.

---

## 2. Motivation

### 2.1 Competitive Gap

From the market research (`market_research.md`), battery and SSD health monitoring were identified as **high-priority features** because:

- **Sensei** ($29/yr or $59 one-time) differentiates primarily on hardware health: battery cycle count, capacity degradation, SSD SMART data, SSD lifetime estimates, and fan control. This is the feature set that justifies its price tag.
- **iStat Menus** ($11.99) provides battery health %, cycle count, per-app battery drain, and Bluetooth device battery levels in the menu bar.
- **coconutBattery** (free/$9.95) is a dedicated tool with historical tracking, iOS device battery reading, and CSV export — proving there is enough demand for battery health alone to sustain a standalone product.
- **DriveDx** ($19.99) is a dedicated SMART monitor with a sophisticated multi-tier warning system and per-firmware health algorithms.
- **No Linux GUI tool** combines battery health and SMART monitoring in a single application. Users currently piece together `tlp-stat`, `upower`, `smartctl`, and `gnome-power-statistics` separately.

Nexis already monitors CPU, GPU, memory, disk I/O, network, and temperature. Battery and disk health are the two remaining pillars of comprehensive hardware monitoring.

### 2.2 User Demand

- Laptop users (the majority of macOS and a growing share of Linux users) care deeply about battery longevity. Apple Silicon MacBooks are multi-year investments; users want to track degradation.
- SSD failure without warning causes data loss. SMART monitoring is the only early warning system, and most users don't know how to use `smartctl`.
- The Stacer issue tracker and similar projects frequently request hardware health features.

---

## 3. Competitive Reference

### 3.1 Battery Health — What Competitors Show

| Metric | Sensei | iStat Menus | coconutBattery | CleanMyMac | Nexis (Proposed) |
|--------|--------|-------------|----------------|------------|------------------|
| Health % (max/design capacity) | Yes | Yes | Yes | Yes | **Yes** |
| Cycle count | Yes | Yes | Yes | Yes | **Yes** |
| Design capacity (mAh) | Yes | No | Yes | No | **Yes** |
| Current max capacity (mAh) | Yes | No | Yes | No | **Yes** |
| Current charge (mAh) | Yes | No | Yes | No | **Yes** |
| Temperature | Separate panel | No | Yes | Yes | **Yes** |
| Charge/discharge rate (W) | Yes | No | Yes | No | **Yes** |
| Voltage (mV) | No | No | Yes | No | **Yes** |
| Time remaining | Yes | Yes | Yes | No | **Yes** |
| Condition string (Good/Fair/Poor) | Yes | Yes | Yes | No | **Yes** |
| Manufacture date | No | No | Yes | No | **Yes** (if available) |
| Historical health chart | No | No | Yes (Plus) | No | **Yes** |
| Per-app battery drain | No | Yes | No | No | No (future) |
| iOS device battery | No | No | Yes (Plus) | No | No (out of scope) |
| Alert on health degradation | No | Yes | No | Yes | **Yes** |
| Charge limit recommendation | No | No | No | No | **Yes** (Linux only, via TLP/sysfs) |

**Nexis advantage:** Cross-platform (macOS + Linux) in one tool. No competitor does this. coconutBattery is macOS-only. TLP is Linux CLI-only.

### 3.2 SSD/Disk Health — What Competitors Show

| Metric | Sensei | DriveDx | GSmartControl | CrystalDiskInfo | Nexis (Proposed) |
|--------|--------|---------|---------------|-----------------|------------------|
| Overall health verdict | Yes | Yes (0-100%) | Via smartctl | Color-coded | **Yes** (color-coded) |
| Percentage Used (NVMe) | Unknown | Yes | Yes | Yes | **Yes** |
| Available Spare (NVMe) | Unknown | Yes | Yes | Yes | **Yes** |
| Media Errors (NVMe) | Unknown | Yes | Yes | Yes | **Yes** |
| Power On Hours | Unknown | Yes | Yes | Yes | **Yes** |
| Temperature | Yes | Yes | Yes | Yes | **Yes** |
| SATA SMART attribute table | No | Yes (full) | Yes (full) | Yes (full) | **Yes** (key attrs) |
| Self-test execution | No | Yes | Yes | No | No (future) |
| Multi-tier warnings | No | Yes (4-tier) | Color-coded | 3-tier | **Yes** (3-tier) |
| Background monitoring | No | Yes (daemon) | No | Yes (tray) | **Yes** (via alert thresholds) |
| Historical health chart | No | No | No | No | **Yes** |
| Per-drive details | Yes | Yes | Yes | Yes | **Yes** |

**Nexis advantage:** Integrated into a broader system optimizer (not a standalone SMART tool). Historical charting of drive health over time — something none of the competitors offer.

---

## 4. Detailed Requirements

### 4.1 Battery Health Monitoring

#### 4.1.1 Data Sources

**macOS (IOKit — AppleSmartBattery):**
Access via `IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("AppleSmartBattery"))` and `IORegistryEntryCreateCFProperties`. This follows the exact same pattern already used in `ThermalInfo` and `GpuInfo` for IOKit access.

Key IORegistry properties:
| Property | Type | Description |
|----------|------|-------------|
| `CycleCount` | int | Total charge/discharge cycles |
| `DesignCapacity` | int | Original factory capacity (mAh) |
| `MaxCapacity` / `AppleRawMaxCapacity` | int | Current maximum capacity (mAh) |
| `CurrentCapacity` | int | Current charge level (mAh) |
| `Temperature` | int | Tenths of degrees Celsius |
| `Voltage` | int | Millivolts |
| `Amperage` / `InstantAmperage` | int | Milliamps (negative = discharging) |
| `IsCharging` | bool | Currently charging |
| `FullyCharged` | bool | At maximum charge |
| `ExternalConnected` | bool | Power adapter connected |
| `TimeRemaining` | int | Minutes (65535 = calculating) |
| `BatteryInstalled` | bool | Battery present |
| `ManufactureDate` | int | Packed date (bits: year[15:9] + 1980, month[8:5], day[4:0]) |
| `DesignCycleCount9C` | int | Rated cycle count (usually 1000) |

**Linux (`/sys/class/power_supply/BAT*/`):**
Read from sysfs pseudo-files. Path discovery: enumerate `/sys/class/power_supply/` and filter for `type == "Battery"`.

Key sysfs files:
| File | Unit | Description |
|------|------|-------------|
| `status` | string | "Charging", "Discharging", "Full", "Not charging" |
| `capacity` | % | OS-reported charge percentage |
| `charge_full` | µAh | Current max capacity |
| `charge_full_design` | µAh | Factory design capacity |
| `energy_now` / `energy_full` / `energy_full_design` | µWh | Alternative energy-based reporting |
| `voltage_now` | µV | Current voltage |
| `current_now` | µA | Current draw (sign varies by driver) |
| `power_now` | µW | Power draw/charge rate |
| `temp` | 0.1 °C | Battery temperature (not always available) |
| `cycle_count` | int | Cycle count (hardware-dependent; best on ThinkPads) |
| `manufacturer` | string | Battery manufacturer |
| `model_name` | string | Battery model |

Note: Not all files exist on all hardware. Linux battery reporting uses either charge-based (µAh) or energy-based (µWh) units depending on the driver. The code must handle both.

**Desktop detection:** If no battery is detected (desktop Mac, desktop Linux), the entire battery section is hidden — matching the graceful degradation pattern used for GPU (no GPU = section hidden) and thermal sensors (no sensors = section hidden).

#### 4.1.2 Derived Metrics

| Metric | Calculation |
|--------|-------------|
| Health % | `(MaxCapacity / DesignCapacity) * 100` |
| Charge % | `(CurrentCapacity / MaxCapacity) * 100` |
| Power (W) | macOS: `abs(Voltage * Amperage) / 1e6`; Linux: `power_now / 1e6` or `voltage_now * current_now / 1e12` |
| Condition | `health >= 80%` → Good; `60% <= health < 80%` → Fair; `health < 60%` → Replace |
| Time remaining | macOS: `TimeRemaining` property (minutes); Linux: estimate from `energy_now / power_now * 60` |

#### 4.1.3 UI Integration

**Dashboard Page — Battery CircleBar:**
- New `CircleBar` widget (same component used for CPU, Memory, Disk, GPU, Temp)
- Displays battery health % as the gauge value (NOT charge % — health is the metric that degrades over time)
- Inner text: health %, cycle count below
- Color coding: green (>80%), yellow (60–80%), red (<60%)
- Tooltip: "Battery Health: 89% | Cycles: 342/1000 | Condition: Good"
- Only shown when battery is detected (graceful degradation)
- Respects kiosk mode layout

**Hardware Info Page — Battery Section:**
New expandable section (same QGroupBox/QFormLayout pattern as existing System, Processor, Graphics, Memory, Storage, Network, Thermal sections):

```
Battery
├── Status:           Charging (72%)
├── Health:           89% (Good)
├── Cycle Count:      342 / 1000
├── Current Charge:   4,821 mAh
├── Max Capacity:     5,103 mAh
├── Design Capacity:  5,731 mAh
├── Temperature:      31.2 °C
├── Voltage:          12,842 mV
├── Power:            28.4 W (charging)
├── Time Remaining:   1h 12m to full
├── Manufacturer:     SMP
├── Model:            DELL 7FHHV85
└── Manufacture Date: 2023-06-15
```

**Resources Page — Battery Health History Chart:**
New chart section (same `QLineSeries` + `QChart` pattern used for CPU, Memory, Disk I/O, Network, GPU history charts):
- X-axis: time (rolling window, same as other resource charts)
- Y-axis: health % (0–100)
- Secondary series: charge % (to show current charge alongside health)
- Optional: temperature overlay
- Data point recorded each time the Resources page is open (matching existing chart behavior — charts accumulate data during the session)

For **long-term health tracking** (across sessions), a lightweight history file:
- Path: `~/.config/nexis/battery_health.json` (Linux) / `~/Library/Application Support/Nexis/battery_health.json` (macOS)
- Schema: `[{ "timestamp": "ISO8601", "health_pct": 89.2, "cycle_count": 342, "max_capacity_mah": 5103 }, ...]`
- One entry per day (deduplicated by date)
- Hardware Info page can render a "Health Over Time" sparkline from this data

**Settings Page — Battery Alert Threshold:**
New threshold setting (same pattern as existing CPU/Memory/Disk alert thresholds):
- "Battery Health Alert: warn below [ 80 ] %"
- Triggers a tray notification: "Battery health has dropped to 78%. Consider service."
- Checked on each Dashboard timer tick when battery data refreshes

### 4.2 SSD/Disk Health Monitoring

#### 4.2.1 Data Sources

**Linux (`smartctl` from smartmontools):**

NVMe drives (`smartctl -j -a /dev/nvme0n1` — JSON output):
| Field (JSON path) | Description |
|--------------------|-------------|
| `nvme_smart_health_information_log.percentage_used` | 0-100+ (>100 = exceeds rated endurance) |
| `nvme_smart_health_information_log.available_spare` | Available spare blocks % |
| `nvme_smart_health_information_log.available_spare_threshold` | Minimum acceptable spare % |
| `nvme_smart_health_information_log.media_errors` | Unrecoverable media errors (any nonzero = investigate) |
| `nvme_smart_health_information_log.unsafe_shutdowns` | Power loss without proper shutdown |
| `nvme_smart_health_information_log.power_on_hours` | Total hours powered on |
| `nvme_smart_health_information_log.temperature` | Celsius |
| `nvme_smart_health_information_log.data_units_written` | Total data written (in 512KB units * 1000) |
| `nvme_smart_health_information_log.data_units_read` | Total data read |
| `nvme_smart_health_information_log.critical_warning` | Bitmask — any bit set = immediate attention |

SATA drives (`smartctl -j -a /dev/sda` — JSON output):
| Attribute ID | Name | Significance |
|-------------|------|-------------|
| 5 | Reallocated Sectors Count | Any nonzero = concern (20-60x failure risk per Backblaze) |
| 9 | Power On Hours | Drive age |
| 177 | Wear Leveling Count | SSD wear (starts at 100, decreases) |
| 190/194 | Temperature | Celsius |
| 197 | Current Pending Sector Count | Sectors awaiting reallocation |
| 198 | Offline Uncorrectable Sector Count | Sectors that failed read verification |
| 199 | UDMA CRC Error Count | Interface communication errors |
| 232 | Available Reserved Space | SSD spare blocks remaining |
| 233 | Media Wearout Indicator | SSD life remaining |
| 241 | Total LBAs Written | Total write volume |

`smartctl` requires root/sudo for most drives. Use existing `CommandUtil::sudoExec()` pattern. Consider caching results (SMART data changes slowly — refresh every 5-10 minutes at most, not every second like CPU).

**macOS (IOKit + diskutil):**

macOS approach is more limited than Linux:
- `diskutil info /dev/diskN` provides: SMART Status ("Verified" or "Failing"), device model, serial, protocol (NVMe/SATA), size, partition scheme
- IOKit `IONVMeSMARTInterface` (via `IONVMeFamily.kext`) can read NVMe SMART logs, but requires a C/Objective-C IOKit user client — more complex than a subprocess call
- For initial implementation, use `smartctl` from Homebrew-installed smartmontools (if available) as a fallback, with `diskutil` as the baseline
- Apple Silicon internal SSDs have limited SMART exposure; Apple controls the NVMe firmware. `diskutil` still reports Verified/Failing.

**Detection & enumeration:**
- Linux: enumerate block devices via `/sys/block/` or `lsblk -J -o NAME,TYPE,MODEL,SERIAL,SIZE,ROTA,TRAN`
- macOS: `diskutil list -plist` → parse XML plist for device identifiers
- Filter to physical drives only (exclude partitions, loop devices, RAM disks)

#### 4.2.2 Derived Metrics

| Metric | NVMe | SATA SSD | SATA HDD |
|--------|------|----------|----------|
| Health % | `100 - percentage_used` | Wear Leveling Count (ID 177) raw value | Based on reallocated + pending sectors |
| Life remaining | Health % mapped to estimated time (based on TBW rating if known) | Wear Leveling / Media Wearout raw value | N/A |
| Overall verdict | Green if health > 80%, spare > threshold, media errors = 0, critical warning = 0 | Green if no critical attributes below threshold | Green if ID 5/197/198 all zero |
| Temperature | Direct reading | ID 190 or 194 raw value | ID 190 or 194 raw value |

**Health Verdict (3-tier, inspired by CrystalDiskInfo):**
- **Good** (green): No concerning attributes, health > 80%, no critical warnings
- **Caution** (yellow): Health 20-80%, OR any single concerning attribute (e.g., 1-10 reallocated sectors, available spare nearing threshold)
- **Critical** (red): Health < 20%, OR critical warning bits set, OR multiple failing attributes, OR available spare below threshold

#### 4.2.3 UI Integration

**Dashboard Page — Disk Health Indicator:**
- Enhance the existing Disk CircleBar (currently shows disk usage %) with a health indicator
- Option A: Small colored dot (green/yellow/red) overlay on the existing Disk CircleBar
- Option B: Separate "Disk Health" CircleBar alongside existing "Disk Usage" CircleBar
- Recommendation: **Option A** — avoids Dashboard clutter; clicking the dot navigates to Hardware Info for details

**Hardware Info Page — Storage Section Enhancement:**
Expand the existing Storage section with SMART data. For each detected physical drive:

```
Storage
├── /dev/nvme0n1 — Samsung 970 EVO Plus 1TB (NVMe)
│   ├── Health:              92% (Good)  [████████░░] 🟢
│   ├── Percentage Used:     8%
│   ├── Available Spare:     100% (threshold: 10%)
│   ├── Temperature:         38 °C
│   ├── Power On Hours:      8,342 (347 days)
│   ├── Media Errors:        0
│   ├── Unsafe Shutdowns:    24
│   ├── Data Written:        18.2 TB
│   ├── Data Read:           42.7 TB
│   └── Critical Warnings:   None
│
├── /dev/sda — WDC WD40EFRX-68WT0N0 4TB (SATA HDD)
│   ├── Health:              Good  🟢
│   ├── Reallocated Sectors: 0
│   ├── Pending Sectors:     0
│   ├── Temperature:         34 °C
│   ├── Power On Hours:      22,891 (2.6 years)
│   └── UDMA CRC Errors:     0
```

**Resources Page — Disk Health History Chart:**
- Similar to battery health: long-term tracking of health % per drive across sessions
- History file: `~/.config/nexis/disk_health.json` / `~/Library/Application Support/Nexis/disk_health.json`
- One entry per drive per day
- Chart shows health % trendline — gradual decline is normal; sudden drops indicate problems

**Settings Page — Disk Health Alert Threshold:**
- "Disk Health Alert: warn below [ 80 ] % or when status changes to Caution/Critical"
- Tray notification: "SSD Samsung 970 EVO health has dropped to 75% (Caution). Consider backup and replacement planning."

### 4.3 Alert System

Building on the existing CPU/Memory/Disk alert infrastructure in Dashboard:

| Alert | Trigger | Notification Text |
|-------|---------|-------------------|
| Battery health low | Health % drops below threshold (default 80%) | "Battery health is {X}% ({Condition}). {CycleCount} cycles used." |
| Battery temperature high | Temp > 45°C | "Battery temperature is {X}°C. High temperatures reduce battery lifespan." |
| Disk health caution | Any drive transitions to Caution verdict | "{DriveName} health is {X}% (Caution). Consider backing up important data." |
| Disk health critical | Any drive transitions to Critical verdict | "{DriveName} is in critical condition. Back up immediately and plan replacement." |
| SMART critical warning | NVMe critical_warning bitmask nonzero | "{DriveName} reports a critical hardware warning. Immediate attention required." |

Alerts use the existing `QSystemTrayIcon::showMessage()` pattern from Dashboard.

---

## 5. Architecture

### 5.1 New Classes

Following the established pattern (e.g., `GpuInfo`, `ThermalInfo`, `CpuInfo`):

```
nexis-core/Info/
├── battery_info.h          # BatteryInfo class — cross-platform interface
├── disk_health_info.h      # DiskHealthInfo class — cross-platform interface

macos/nexis-core/Info/
├── battery_info.cpp         # macOS IOKit AppleSmartBattery implementation
├── disk_health_info.cpp     # macOS diskutil + optional smartctl implementation

linux/nexis-core/Info/
├── battery_info.cpp         # Linux /sys/class/power_supply/ implementation
├── disk_health_info.cpp     # Linux smartctl JSON parsing implementation
```

**`BatteryInfo`** (header in `shared/`, platform impls in `macos/` and `linux/`):
```cpp
class BatteryInfo {
public:
    bool hasBattery() const;

    // Current state
    int currentCapacityMah() const;
    int maxCapacityMah() const;
    int designCapacityMah() const;
    double healthPercent() const;        // maxCapacity / designCapacity * 100
    int cycleCount() const;
    int designCycleCount() const;        // rated max cycles (e.g. 1000)
    double temperatureCelsius() const;
    int voltageMv() const;
    int amperageMa() const;
    double powerWatts() const;
    QString status() const;              // "Charging", "Discharging", "Full"
    bool isCharging() const;
    bool isPluggedIn() const;
    int timeRemainingMinutes() const;    // -1 if calculating
    QString condition() const;           // "Good", "Fair", "Replace"
    QString manufacturer() const;
    QString model() const;
    QDate manufactureDate() const;       // QDate() if unavailable

    void updateBatteryInfo();            // refresh all values from OS
};
```

**`DiskHealthInfo`** (header in `shared/`, platform impls in `macos/` and `linux/`):
```cpp
struct DriveHealth {
    QString devicePath;        // /dev/nvme0n1, /dev/sda
    QString model;
    QString serial;
    QString protocol;          // "NVMe", "SATA"
    bool isRotational;         // true = HDD, false = SSD
    qint64 sizeBytes;

    // Health verdicts
    enum Verdict { Good, Caution, Critical, Unknown };
    Verdict verdict;
    int healthPercent;         // -1 if not determinable

    // NVMe-specific
    int percentageUsed;        // 0-100+
    int availableSpare;        // %
    int availableSpareThreshold; // %
    int mediaErrors;
    int unsafeShutdowns;
    int powerOnHours;
    double temperatureCelsius;
    double dataWrittenTB;
    double dataReadTB;
    int criticalWarning;       // bitmask

    // SATA-specific (key attributes)
    int reallocatedSectors;    // ID 5
    int pendingsectors;        // ID 197
    int uncorrectableSectors;  // ID 198
    int wearLevelingCount;     // ID 177 (SSD)
    int udmaCrcErrors;         // ID 199

    // Full SMART attribute list for advanced view
    QList<SmartAttribute> allAttributes;
};

struct SmartAttribute {
    int id;
    QString name;
    int currentValue;     // normalized 0-100 or 0-253
    int worstValue;
    int threshold;
    qint64 rawValue;
    bool isFailing;       // currentValue <= threshold
};

class DiskHealthInfo {
public:
    bool isAvailable() const;            // smartctl installed / accessible
    QList<DriveHealth> getDriveHealth(); // scan all drives
    DriveHealth getDriveHealth(const QString &devicePath); // single drive

    void refresh();                      // re-scan all drives
};
```

### 5.2 InfoManager Integration

Add to `InfoManager` (existing singleton):
```cpp
BatteryInfo *bi;       // alongside existing ci (CpuInfo), di (DiskInfo), etc.
DiskHealthInfo *dhi;
```

Exposed via:
```cpp
BatteryInfo* getBatteryInfo() const;
DiskHealthInfo* getDiskHealthInfo() const;
```

### 5.3 Dashboard Timer Integration

In `DashboardPage`:
- New `updateBatteryBar()` slot connected to existing `mTimer` (1-second interval)
- Battery data refresh is lightweight (single IOKit call or sysfs file reads) — OK at 1-second granularity
- New `updateDiskHealthIndicator()` — connected to `timerDisk` (5-second interval) but with internal throttling to only actually call `smartctl` every 5-10 minutes (SMART data is near-static; frequent calls waste I/O and CPU)

### 5.4 History Persistence

Lightweight JSON files (not SQLite — avoids adding a dependency):

```json
// battery_health.json
{
  "entries": [
    { "date": "2026-02-17", "health_pct": 89.2, "cycle_count": 342, "max_capacity_mah": 5103 },
    { "date": "2026-02-18", "health_pct": 89.1, "cycle_count": 343, "max_capacity_mah": 5097 }
  ]
}

// disk_health.json
{
  "drives": {
    "/dev/nvme0n1": {
      "model": "Samsung 970 EVO Plus 1TB",
      "entries": [
        { "date": "2026-02-17", "health_pct": 92, "percentage_used": 8, "power_on_hours": 8342 }
      ]
    }
  }
}
```

One entry per device per day. On app launch, if today's entry doesn't exist, record one. On Resources page visit, record another (dedup by date). Cap at 730 entries (2 years) per device, oldest removed.

### 5.5 Dependency Considerations

| Dependency | Required? | Notes |
|------------|-----------|-------|
| `smartmontools` (smartctl) | Recommended (not required) | Linux: available in all major distro repos. macOS: available via Homebrew. If not installed, disk health section shows "Install smartmontools for disk health monitoring" with platform-appropriate install instructions. |
| IOKit framework | Already linked | Used by ThermalInfo and GpuInfo on macOS. No new framework dependency. |
| `diskutil` | Already available | Ships with macOS. Used as baseline for drive enumeration and basic SMART status. |

---

## 6. Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC-01 | On a laptop, the Dashboard shows a Battery Health CircleBar with health %, colored green/yellow/red by condition |
| AC-02 | On a desktop (no battery), the Battery CircleBar is not shown — no error, no empty space |
| AC-03 | Hardware Info page shows a Battery section with all available metrics (cycle count, capacities, temperature, charge rate, condition, etc.) |
| AC-04 | Hardware Info page shows SMART health data for each detected physical drive |
| AC-05 | NVMe drives show: health %, percentage used, available spare, media errors, unsafe shutdowns, power on hours, temperature, data written/read |
| AC-06 | SATA drives show: health verdict, reallocated sectors, pending sectors, temperature, power on hours |
| AC-07 | Drive health verdict is color-coded: Good (green), Caution (yellow), Critical (red) |
| AC-08 | If `smartctl` is not installed, the disk health section shows a helpful install prompt instead of an error |
| AC-09 | Resources page shows a Battery Health chart (health % over time) when battery is present |
| AC-10 | Resources page shows a Disk Health chart (health % per drive over time) |
| AC-11 | Health history is persisted across sessions in JSON files |
| AC-12 | Settings page has configurable alert thresholds for battery health and disk health |
| AC-13 | Tray notifications fire when battery health or disk health drops below threshold |
| AC-14 | All battery/disk health features work on both macOS and Linux |
| AC-15 | Builds cleanly on both platforms with no new required dependencies |
| AC-16 | Graceful degradation: missing sensors, unavailable sysfs files, or inaccessible drives produce no crashes or UI artifacts |

---

## 7. Phased Implementation Plan

### Phase 1 — Battery Health (Lower complexity, no external dependencies)
1. Implement `BatteryInfo` class (macOS IOKit + Linux sysfs)
2. Add Battery CircleBar to Dashboard with graceful degradation
3. Add Battery section to Hardware Info page
4. Add battery alert threshold to Settings
5. Build and test on both platforms

### Phase 2 — Disk Health (Higher complexity, smartctl dependency)
1. Implement `DiskHealthInfo` class (Linux smartctl JSON + macOS diskutil/smartctl)
2. Add disk health indicator to Dashboard
3. Expand Storage section in Hardware Info with SMART data
4. Add disk health alert threshold to Settings
5. Handle `smartctl` not-installed case gracefully
6. Build and test on both platforms

### Phase 3 — Historical Tracking & Charts
1. Implement battery health history JSON persistence
2. Implement disk health history JSON persistence
3. Add Battery Health chart to Resources page
4. Add Disk Health chart to Resources page
5. Test history accumulation across multiple app sessions

---

## 8. Design Decisions (Resolved)

All open questions have been resolved. These decisions are binding for implementation.

| # | Question | Decision | Rationale |
|---|----------|----------|-----------|
| OQ-01 | `smartctl` privilege escalation strategy | **Unprivileged first, prompt if needed.** Try `smartctl` without sudo. If data is incomplete or access denied, show a "Scan with elevated privileges" button. User controls when the auth dialog appears. | Least intrusive UX. Avoids surprise auth dialogs. Many systems provide basic SMART data without root. |
| OQ-02 | macOS Apple Silicon internal SSD SMART access | **Show `diskutil` Verified/Failing status + note about limitations.** Display whatever macOS provides with a clear message: "Detailed SMART data is not available for Apple internal SSDs." Do NOT attempt IOKit NVMe user client access. | Honest UX, avoids fragile IOKit code that would break with macOS updates. Apple deliberately limits this access. |
| OQ-03 | SMART attribute table depth | **Key attributes only (5-8 critical attributes).** Show health %, temperature, power on hours, reallocated sectors, etc. in the same `QFormLayout` style as other Hardware Info sections. No full attribute table. | Clean and approachable. Matches the rest of Hardware Info's design language. Power users who need full tables already use `smartctl` or GSmartControl. |
| OQ-04 | Battery health alert behavior | **Fire once, with a "Remind me later" option.** Alert fires when health first drops below threshold. Dismissed until health drops another 5%+ OR user clicks "Remind me in 30 days." | Balances awareness with notification fatigue. User stays informed as degradation progresses without being nagged. |
| OQ-05 | Resources page chart data scope | **Long-term history from JSON file (days/weeks/months).** Battery and disk health charts will be the first Resources charts with cross-session persistence. Health changes so slowly that session-only data would be a flat line. | This is the primary value of health tracking — seeing degradation trends. Sets a useful precedent that other charts could adopt later. |
| OQ-06 | TLP charge threshold integration | **Read-only display.** If TLP is installed on Linux, show current charge thresholds (e.g., "Charge limit: 80% — set via TLP") as informational text. Do NOT allow configuration changes from within Nexis. | Adds useful context without scope creep. Avoids writing to system config files. Users who want to change thresholds already use TLP directly. |

### Implementation Implications of Decisions

**OQ-01 (Unprivileged-first smartctl):**
- `DiskHealthInfo::refresh()` first runs `smartctl -j -a /dev/X` without sudo
- Parse JSON result; check for `smartctl.exit_status` error codes indicating permission issues
- If permission denied: store partial data, set a `needsElevation` flag per drive
- UI shows available data + a "🔒 Scan with full access" button per drive (or globally)
- Button triggers `CommandUtil::sudoExec("smartctl", ...)` and re-parses

**OQ-02 (Apple SSD limitations):**
- On macOS, always run `diskutil info -plist /dev/diskN` for all drives
- Parse plist for: `SMARTStatus` ("Verified"/"Failing"), `MediaName`, `IORegistryEntryName`, `TotalSize`
- If `SMARTStatus` is present, display it with color coding (green = Verified, red = Failing)
- Show info label: "Apple internal SSDs provide limited health reporting. Status shows Verified (healthy) or Failing (replace immediately)."
- If `smartctl` is installed (via Homebrew) AND the drive is NOT an Apple internal SSD (e.g., external NVMe/SATA), use smartctl for full SMART data

**OQ-03 (Key attributes only):**
- `DriveHealth` struct retains the `QList<SmartAttribute> allAttributes` field internally (for future use / logging) but the Hardware Info UI only renders the curated subset
- NVMe key display: Health %, Percentage Used, Available Spare, Temperature, Power On Hours, Media Errors, Unsafe Shutdowns, Data Written
- SATA SSD key display: Health verdict, Wear Leveling %, Temperature, Power On Hours, Reallocated Sectors, Pending Sectors
- SATA HDD key display: Health verdict, Temperature, Power On Hours, Reallocated Sectors, Pending Sectors, UDMA CRC Errors

**OQ-04 (Fire-once alert with remind-later):**
- New `SettingManager` keys: `BatteryAlertLastHealth` (int, health % when alert last fired), `BatteryAlertSnoozedUntil` (QDateTime)
- Alert fires when: `currentHealth < threshold` AND (`currentHealth <= lastAlertHealth - 5` OR `now > snoozedUntil`)
- Notification action buttons: "View Details" (opens Hardware Info) and "Remind in 30 days" (sets snooze)
- Same pattern for disk health alerts: `DiskAlertLastHealth_{deviceId}`, `DiskAlertSnoozedUntil_{deviceId}`

**OQ-05 (Long-term history charts):**
- Resources page `BatteryHealthChart` and `DiskHealthChart` load data from JSON history on page init
- X-axis: calendar dates (not seconds like CPU/memory charts)
- Y-axis: health % (0-100, inverted logic — decline over time is expected)
- Chart type: `QLineSeries` with `QDateTimeAxis` for X — different from existing `QValueAxis` time-offset approach
- Refresh: append today's data point on page load if not already present
- Time range selector: Last 30 days / 90 days / 1 year / All time

**OQ-06 (Read-only TLP display):**
- On Linux, check for TLP: `which tlp` or existence of `/etc/tlp.conf`
- If present, read charge thresholds: `tlp-stat -b | grep "charge_control"` or parse `/sys/class/power_supply/BAT0/charge_control_start_threshold` and `charge_control_end_threshold`
- Display in Battery section of Hardware Info: "Charge Limit: 20% – 80% (managed by TLP)"
- If TLP not installed: omit the line entirely (no "install TLP" prompt — out of scope)
