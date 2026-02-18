# FR-29 Phase 2 (Disk Health / SMART Monitoring) — Research

**Date:** February 2026
**Scope:** Deep codebase analysis and platform API research for disk health SMART monitoring
**Prerequisites:** Phase 1 (Battery Health) complete — commit `66646a8`

---

## 1. Architecture Overview

### 1.1 Established Pattern: Platform-Split Info Classes

Every hardware info class in Nexis follows a three-file pattern:

```
shared/nexis-core/Info/{name}.h          — Header: struct + class declaration
shared/nexis-core/Info/{name}_shared.cpp — Shared getters (return member data)
macos/nexis-core/Info/{name}.cpp         — macOS platform implementation
linux/nexis-core/Info/{name}.cpp         — Linux platform implementation
```

CMake auto-discovers via `GLOB_RECURSE` (`CMakeLists.txt:37-40`), so no build changes needed when adding new files. Platform headers shadow shared headers when duplicates exist (`CMakeLists.txt:49-58`).

### 1.2 Template: GpuInfo (Multi-Device Platform Abstraction)

`GpuInfo` is the ideal template for `DiskHealthInfo` because both manage **multiple devices** with per-device data.

**Header** (`shared/nexis-core/Info/gpu_info.h`):
```cpp
struct GpuDevice {
    QString name;           // e.g. "NVIDIA GeForce RTX 3080"
    QString vendor;         // "NVIDIA", "AMD", "Intel", "Apple"
    int     utilization;    // 0-100 percent (-1 if unavailable)
    QString sysfsLoadPath;  // Platform-specific internal path
    QString queryCommand;   // Platform-specific command
    int     deviceIndex;    // index within vendor's enumeration
};

class GpuInfo {
public:
    GpuInfo();
    QList<GpuDevice> getGpuDevices() const;
    void updateGpuInfo();
    bool hasGpu() const;
private:
    void discoverGpus();
    QList<GpuDevice> mDevices;
};
```

**Shared getters** (`gpu_info_shared.cpp`):
```cpp
QList<GpuDevice> GpuInfo::getGpuDevices() const { return mDevices; }
bool GpuInfo::hasGpu() const { return !mDevices.isEmpty(); }
```

**macOS** (`macos/nexis-core/Info/gpu_info.cpp`):
- Uses IOKit: `IOServiceGetMatchingServices(kIOMainPortDefault, IOServiceMatching("IOAccelerator"), &iterator)`
- Reads `model` property, derives vendor from `vendor-id`
- Updates utilization from `PerformanceStatistics` dict

**Linux** (`linux/nexis-core/Info/gpu_info.cpp`):
- Enumerates `/sys/class/drm/card*` directories
- Reads PCI vendor ID from sysfs, uses `lspci` for device name
- AMD: reads `gpu_busy_percent` from sysfs
- NVIDIA: uses `nvidia-smi --query-gpu=utilization.gpu`
- Intel: approximates from `gt_cur_freq_mhz / gt_max_freq_mhz`

**Key pattern observations:**
- Constructor calls `discoverGpus()` (one-time enumeration)
- `updateGpuInfo()` re-reads dynamic values (utilization)
- Multiple external tools supported with graceful fallback (`lspci`, `nvidia-smi`)
- `CommandUtil::exec()` for subprocess calls, `FileUtil::readStringFromFile()` for sysfs

### 1.3 Template: BatteryInfo (Single-Device Health Pattern)

`BatteryInfo` (Phase 1) is the template for the **health reporting** aspect:

**Header** (`shared/nexis-core/Info/battery_info.h`):
```cpp
struct BatteryData {
    bool hasBattery = false;
    int  healthPercent = -1;     // derived: maxCapacity/designCapacity*100
    int  chargePercent = -1;
    // ... 23 total fields
    QString status;
    QString condition;           // "Good", "Fair", "Replace"
};

class BatteryInfo {
public:
    BatteryInfo();
    BatteryData getBatteryData() const;
    bool hasBattery() const;
    void updateBatteryInfo();
private:
    void discoverBattery();
    BatteryData mData;
    QString mBatteryPath;        // Linux: sysfs path
};
```

**Key pattern observations:**
- Struct uses `-1` sentinel values for unavailable fields
- `discoverBattery()` runs once in constructor
- `updateBatteryInfo()` refreshes all dynamic values
- Derive `healthPercent` and `condition` from raw data
- `condition` string maps to thresholds: ≥80% Good, ≥60% Fair, <60% Replace

---

## 2. Existing Disk Infrastructure

### 2.1 DiskInfo — Basic Volume Information

**Header** (`shared/nexis-core/Info/disk_info.h`):
```cpp
struct Disk {
    QString name;
    QString device;          // e.g. "/dev/disk3s5" or "/dev/sda1"
    QString fileSystemType;
    quint64 size = 0;
    quint64 free = 0;
    quint64 used = 0;
};

class DiskInfo {
public:
    QList<Disk> getDisks() const;        // mounted volumes
    void updateDiskInfo();               // refresh
    QList<quint64> getDiskIO() const;    // [readBytes, writeBytes]
    QStringList getDiskNames() const;    // physical device names
    QList<QString> fileSystemTypes();
    QList<QString> devices();
};
```

**Shared** (`disk_info_shared.cpp`):
- Uses `QStorageInfo::mountedVolumes()` for cross-platform volume enumeration
- Reports **volumes** (partitions), not physical disks

**macOS** (`macos/nexis-core/Info/disk_info_platform.cpp`):
- `getDiskIO()`: Runs `iostat -d -c 1 -w 1` via `CommandUtil::exec()`
- `getDiskNames()`: Enumerates `/dev/disk*`, filters to base disks (no partitions)

**Linux** (`linux/nexis-core/Info/disk_info_platform.cpp`):
- `getDiskIO()`: Reads `/sys/block/{diskname}/stat` (columns 2,6 × 512 bytes)
- `getDiskNames()`: Enumerates `/sys/block/` with `device/` subdirectory check

**Key distinction:** `DiskInfo` is about **volumes and I/O throughput**, not drive health. `DiskHealthInfo` will be a separate class focused on **SMART data and physical drive health**.

### 2.2 InfoManager — Disk Section

**Current disk methods** (`info_manager.h:35-37`, `info_manager.cpp:83-106`):
```cpp
QList<Disk> getDisks() const;    // → di.getDisks()
QList<quint64> getDiskIO();      // → di.getDiskIO()
void updateDiskInfo();           // → di.updateDiskInfo()
QList<QString> getDevices();     // → di.devices()
QList<QString> getFileSystemTypes(); // → di.fileSystemTypes()
```

Will need to add a `DiskHealthInfo dhi;` member and forwarding methods.

### 2.3 Dashboard — Disk Bar (Usage, Not Health)

**Current disk bar** (`dashboard_page.cpp:255-301`):
- Shows **disk usage percent** (used/total), not SMART health
- Uses `mDiskBar` (CircleBar) with red gradient `{"#e01b24", "#c01c28"}`
- Updated via `timerDisk` at 5-second interval
- Selects disk based on `mSettingManager->getDiskName()` or falls back to root volume
- Alert: fires when usage exceeds threshold (`mSettingManager->getDiskAlertPercent()`)

**For Phase 2:** The disk health indicator is a **separate** concern. Per the feature request (OQ-03), we show key attributes in Hardware Info and optionally a health indicator. The existing disk usage bar stays as-is. We may consider adding a disk health indicator row, but the design decision says to focus on Hardware Info.

### 2.4 Hardware Info — No Disk Section Yet

**Current sections** (`hardware_info_page.cpp:28-35`):
```cpp
void HardwareInfoPage::init()
{
    populateSystem();
    populateProcessor();
    populateGraphics();
    populateMemory();
    populateBattery();
}
```

**Pattern per section** (e.g., `populateGraphics()` at line 168):
```cpp
void HardwareInfoPage::populateGraphics()
{
    QTableWidget *t = ui->tblGraphics;
    t->horizontalHeader()->setVisible(false);
    t->verticalHeader()->setVisible(false);
    t->horizontalHeader()->setStretchLastSection(true);

    if (!im->hasGpu()) {
        ui->grpGraphics->hide();
        return;
    }

    QList<GpuDevice> gpus = im->getGpuDevices();
    for (int i = 0; i < gpus.size(); ++i) {
        const GpuDevice &gpu = gpus.at(i);
        if (gpus.size() > 1)
            addRow(t, tr("GPU %1").arg(i + 1), gpu.name);
        else
            addRow(t, tr("Name"), gpu.name);
        addRow(t, tr("Vendor"), gpu.vendor);
    }

    t->resizeColumnsToContents();
    fitTableHeight(t);
}
```

For disk health, we'll add a `populateStorage()` method following this exact pattern, with a `grpStorage` QGroupBox and `tblStorage` QTableWidget in the `.ui` file.

### 2.5 Resources Page — Disk R/W Chart

**Current** (`resources_page.cpp:69-118`):
- `mChartDiskReadWrite` — HistoryChart with 2 series (read/write bytes per second)
- Scrolling 60-second window, updated every 1 second via `mTimer`
- `HistoryChart` widget: `QChartView` with `QSplineSeries`, configurable series count and Y-axis

**For Phase 3 (deferred):** Health trend charts would use `QDateTimeAxis` (not session-based rolling window) and read from persistent JSON. Different architecture from current HistoryChart.

### 2.6 Settings — Current Disk Settings

**Current** (`setting_manager.h`):
- `DiskName` — selected disk for dashboard display
- `DiskAlertPercent` — disk usage alert threshold
- `DiskAnalyzerTool` — preferred disk analyzer tool
- `DiskAnalyzerCustomPath` — custom analyzer path

**For Phase 2, will add:**
- `DiskHealthAlertEnabled` — boolean, enable/disable SMART alerts
- No per-attribute threshold — too complex for a first implementation. Instead, alert on overall health verdict change to "Caution" or "Critical".

---

## 3. Platform APIs — macOS

### 3.1 Primary: `diskutil info -plist /dev/diskN`

**Best approach for macOS** — no root required, always available, no Homebrew dependency.

Returns an XML plist with:
- `SMARTStatus`: `"Verified"` or `"Failing"` — Apple's binary health verdict
- `MediaName` / `IORegistryEntryName`: Drive model name
- `DeviceNode`: `/dev/diskN`
- `DiskSize` / `TotalSize`: Capacity in bytes
- `DeviceBlockSize`: Block size (usually 4096 for NVMe)
- `Content`: Partition scheme (GUID_partition_scheme, etc.)
- `IOContent`: Volume type
- `VolumeUUID`: Volume identifier
- `SolidState`: Boolean (Yes for SSD)
- `SMARTDeviceSpecificKeysMayVaryNotGuaranteed`: Dictionary with NVMe SMART data

**NVMe SMART data** (under `SMARTDeviceSpecificKeysMayVaryNotGuaranteed`):
- `AVAILABLE_SPARE`: Available spare percentage
- `AVAILABLE_SPARE_THRESHOLD`: Threshold (Apple sets this to 99% — must handle!)
- `PERCENTAGE_USED`: Endurance consumed (0-100+%)
- `TEMPERATURE`: In **Kelvin** (subtract 273 for Celsius)
- `DATA_UNITS_READ`: Total data read (units of 512KB × 1000)
- `DATA_UNITS_WRITTEN`: Total data written
- `POWER_ON_HOURS`: Total hours powered on
- `POWER_CYCLES`: Power on/off cycles
- `UNSAFE_SHUTDOWNS`: Unexpected power loss events
- `MEDIA_AND_DATA_INTEGRITY_ERRORS`: Uncorrectable errors
- `CRITICAL_WARNING`: Bitmask for critical conditions

**Parsing approach:**
```cpp
QProcess proc;
proc.start("diskutil", {"info", "-plist", deviceNode});
proc.waitForFinished(5000);
QByteArray plistData = proc.readAllStandardOutput();
// Parse XML plist using QXmlStreamReader or QSettings with plist format
```

**Apple Silicon caveat:** `AVAILABLE_SPARE_THRESHOLD` is set to 99% by Apple (industry standard is 10%). A naive check of `availableSpare < threshold` would flag healthy drives. Solution: ignore Apple's threshold and use a fixed threshold of 10%, or skip the available spare check for Apple internal SSDs.

### 3.2 Fallback: `smartctl` (via Homebrew)

Only needed for **third-party SATA/NVMe drives** where `diskutil` may not expose detailed SMART attributes.

- `smartctl -j -a /dev/diskN` — Full SMART dump in JSON format
- Requires `brew install smartmontools`
- Check with `CommandUtil::isExecutable("smartctl")`
- No root required for most drives on macOS

### 3.3 Drive Enumeration on macOS

```cpp
// List all physical disks
QProcess proc;
proc.start("diskutil", {"list", "-plist"});
// Parse plist → AllDisksAndPartitions → extract WholeDisks
// Returns: ["disk0", "disk1", "disk2", ...]
```

Or use the existing `DiskInfo::getDiskNames()` which enumerates `/dev/disk*` (excludes partitions).

### 3.4 Disk Type Detection on macOS

From `diskutil info -plist`:
- `SolidState`: `true` = SSD, `false` = HDD
- `DeviceProtocol`: `"NVMe"`, `"SATA"`, `"USB"`, `"Apple Fabric"` (internal Apple Silicon)
- `Removable`: Whether the drive is removable media

---

## 4. Platform APIs — Linux

### 4.1 Primary: `smartctl -j -a /dev/X`

**Best approach for Linux** — handles both NVMe and SATA with JSON output (available since smartmontools 7.1, Ubuntu 22.04+).

**NVMe JSON structure:**
```json
{
  "device": {"name": "/dev/nvme0n1", "type": "nvme", "protocol": "NVMe"},
  "model_name": "Samsung 970 EVO Plus 1TB",
  "serial_number": "S4EWNM0R123456",
  "firmware_version": "2B2QEXM7",
  "nvme_smart_health_information_log": {
    "critical_warning": 0,
    "temperature": 38,
    "available_spare": 100,
    "available_spare_threshold": 10,
    "percentage_used": 2,
    "data_units_read": 12345678,
    "data_units_written": 87654321,
    "host_reads": 999999,
    "host_writes": 888888,
    "controller_busy_time": 1234,
    "power_cycles": 567,
    "power_on_hours": 8901,
    "unsafe_shutdowns": 12,
    "media_errors": 0,
    "num_err_log_entries": 0
  },
  "smart_status": {"passed": true}
}
```

**SATA JSON structure:**
```json
{
  "device": {"name": "/dev/sda", "type": "ata", "protocol": "ATA"},
  "model_name": "WDC WD10EZEX-00WN4A0",
  "serial_number": "WD-WMC1T1234567",
  "firmware_version": "01.01A01",
  "rotation_rate": 7200,
  "ata_smart_attributes": {
    "table": [
      {"id": 5, "name": "Reallocated_Sector_Ct", "value": 200, "worst": 200, "thresh": 140, "raw": {"value": 0}},
      {"id": 9, "name": "Power_On_Hours", "value": 97, "worst": 97, "thresh": 0, "raw": {"value": 4567}},
      {"id": 194, "name": "Temperature_Celsius", "value": 111, "worst": 100, "thresh": 0, "raw": {"value": 39}},
      {"id": 197, "name": "Current_Pending_Sector", "value": 200, "worst": 200, "thresh": 0, "raw": {"value": 0}},
      {"id": 198, "name": "Offline_Uncorrectable", "value": 200, "worst": 200, "thresh": 0, "raw": {"value": 0}}
    ]
  },
  "smart_status": {"passed": true}
}
```

### 4.2 Privilege Requirements

`smartctl` requires root on most Linux systems.

**Strategy (per design decision OQ-01: unprivileged first, prompt if needed):**
1. First try: `CommandUtil::exec("smartctl", {"-j", "-a", device})` — no root
2. Check `smartctl.exit_status` in JSON (bit 1 = permission denied)
3. If access denied: set `needsElevation` flag, show available data + "Scan with full access" button
4. Button triggers: `CommandUtil::exec("pkexec", {"smartctl", "-j", "-a", device})` — polkit GUI auth

**Alternative:** Use `CommandUtil::sudoExec()` which already exists in the codebase.

### 4.3 Drive Enumeration on Linux

Existing `DiskInfo::getDiskNames()` (`linux/nexis-core/Info/disk_info_platform.cpp:32-42`):
```cpp
QDir blocks("/sys/block");
for (const QFileInfo &entryInfo : blocks.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
    if (QFile::exists(QString("%1/device").arg(entryInfo.absoluteFilePath())))
        disks.append(entryInfo.baseName());
}
```
Returns: `["sda", "nvme0n1", ...]`

### 4.4 Disk Type Detection on Linux

```cpp
// NVMe vs SATA: name pattern
bool isNvme = deviceName.startsWith("nvme");

// SSD vs HDD: rotational flag
QString rotational = FileUtil::readStringFromFile(
    QString("/sys/block/%1/queue/rotational").arg(deviceName)).trimmed();
bool isSsd = (rotational == "0");

// Model name (without smartctl)
QString model = FileUtil::readStringFromFile(
    QString("/sys/block/%1/device/model").arg(deviceName)).trimmed();
```

### 4.5 Fallback When `smartctl` Not Installed

If `CommandUtil::isExecutable("smartctl")` returns `false`:
- Show basic drive info from sysfs (model, size, type)
- Display message: "Install smartmontools for detailed disk health monitoring"
- Platform-specific install hint: `sudo apt install smartmontools` (Debian/Ubuntu) or `sudo dnf install smartmontools` (Fedora)

---

## 5. Data Structure Design

### 5.1 SmartAttribute Struct

For storing individual SMART attributes (SATA primarily, but also useful for NVMe raw data):

```cpp
struct SmartAttribute {
    int     id = -1;              // SATA attribute ID (e.g., 5, 197, 198)
    QString name;                 // "Reallocated_Sector_Ct"
    int     value = -1;           // Normalized value (0-253, higher is better)
    int     worst = -1;           // Worst recorded value
    int     threshold = -1;       // Failure threshold
    qint64  rawValue = -1;        // Raw count/measurement
    QString status;               // "ok", "warning", "failing"
};
```

### 5.2 DriveHealth Struct

```cpp
struct DriveHealth {
    // Identity
    QString devicePath;           // "/dev/disk0" or "/dev/nvme0n1"
    QString deviceName;           // "disk0" or "nvme0n1"
    QString model;                // "APPLE SSD AP1024Q"
    QString serial;               // Serial number
    QString firmware;             // Firmware version
    quint64 sizeBytes = 0;        // Total capacity

    // Drive type
    enum DriveType { Unknown, NVMe, SATA_SSD, SATA_HDD };
    DriveType driveType = Unknown;
    QString protocol;             // "NVMe", "SATA", "Apple Fabric", "USB"

    // Health summary
    int     healthPercent = -1;   // 0-100, derived from SMART data
    QString healthVerdict;        // "Good", "Caution", "Critical", "Unknown"
    bool    smartPassed = true;   // Overall SMART status (passed/failed)
    bool    needsElevation = false; // true if smartctl needs root for full data

    // Common metrics (both NVMe and SATA)
    double  temperatureCelsius = -1.0;
    int     powerOnHours = -1;
    int     powerCycles = -1;

    // NVMe-specific
    int     percentageUsed = -1;       // 0-100+ (NVMe endurance consumed)
    int     availableSpare = -1;       // 0-100 (NVMe spare capacity)
    int     availableSpareThreshold = -1;
    int     criticalWarning = -1;      // Bitmask
    int     unsafeShutdowns = -1;
    int     mediaErrors = -1;
    qint64  dataUnitsRead = -1;        // In units of 512KB × 1000
    qint64  dataUnitsWritten = -1;

    // SATA-specific (key attributes only per OQ-03)
    int     reallocatedSectors = -1;   // ID 5 (raw value)
    int     pendingSectors = -1;       // ID 197 (raw value)
    int     uncorrectableSectors = -1; // ID 198 (raw value)
    int     reallocatedEvents = -1;    // ID 196 (raw value)
    int     wearLevelingCount = -1;    // ID 177 (normalized, for SSDs)

    // Full attribute list (internal — for future detailed view / logging)
    QList<SmartAttribute> allAttributes;
};
```

### 5.3 Health Verdict Derivation

**NVMe:**
```
if criticalWarning != 0           → "Critical"
if mediaErrors > 0                → "Critical"
if percentageUsed >= 100          → "Critical"
if availableSpare <= 10           → "Caution" (use 10%, NOT Apple's 99%)
if percentageUsed >= 80           → "Caution"
if !smartPassed                   → "Critical"
otherwise                         → "Good"
```

**SATA HDD:**
```
if reallocatedSectors > 0 || pendingSectors > 0 || uncorrectableSectors > 0 → "Caution"
if any of above > 100                                                        → "Critical"
if !smartPassed                                                              → "Critical"
otherwise                                                                    → "Good"
```

**SATA SSD:**
```
if wearLevelingCount < 10          → "Critical" (less than 10% life remaining)
if wearLevelingCount < 30          → "Caution"
if reallocatedSectors > 0          → "Caution"
if !smartPassed                    → "Critical"
otherwise                          → "Good"
```

**Health percent derivation:**
- NVMe: `100 - percentageUsed` (clamped 0-100). If percentageUsed unavailable, use `availableSpare`.
- SATA SSD: `wearLevelingCount` (normalized value, typically 0-100 where 100 = new)
- SATA HDD: 100 if all critical attributes are 0; -1 (unavailable) otherwise
- macOS Apple internal (diskutil only): -1 (not available; show "Verified"/"Failing" string only)

---

## 6. DiskHealthInfo Class Design

### 6.1 Class Structure

```cpp
class DiskHealthInfo {
public:
    DiskHealthInfo();

    QList<DriveHealth> getDrives() const;
    bool hasDrives() const;
    bool hasSmartctl() const;
    void refreshHealth();                              // re-scan all drives
    void refreshHealthElevated(const QString &device); // single drive with sudo

private:
    void discoverDrives();
    void parseDiskutilPlist(const QString &device, DriveHealth &drive);  // macOS
    void parseSmartctlJson(const QByteArray &json, DriveHealth &drive);  // both
    void deriveHealthVerdict(DriveHealth &drive);

    QList<DriveHealth> mDrives;
    bool mHasSmartctl = false;
};
```

### 6.2 macOS Implementation Flow

```
DiskHealthInfo()
├── discoverDrives()
│   ├── Run: diskutil list -plist  →  parse plist for WholeDisks
│   ├── For each disk (disk0, disk1, ...):
│   │   ├── Run: diskutil info -plist /dev/diskN
│   │   ├── Parse: model, serial, size, SolidState, DeviceProtocol
│   │   ├── Parse: SMARTStatus → smartPassed
│   │   ├── Parse: SMARTDeviceSpecificKeysMayVaryNotGuaranteed
│   │   │   ├── TEMPERATURE → temperatureCelsius (subtract 273 from Kelvin)
│   │   │   ├── PERCENTAGE_USED → percentageUsed
│   │   │   ├── AVAILABLE_SPARE → availableSpare
│   │   │   ├── AVAILABLE_SPARE_THRESHOLD → (ignore Apple's 99%, use 10%)
│   │   │   ├── POWER_ON_HOURS → powerOnHours
│   │   │   ├── POWER_CYCLES → powerCycles
│   │   │   ├── UNSAFE_SHUTDOWNS → unsafeShutdowns
│   │   │   ├── MEDIA_AND_DATA_INTEGRITY_ERRORS → mediaErrors
│   │   │   ├── DATA_UNITS_READ/WRITTEN → dataUnitsRead/Written
│   │   │   └── CRITICAL_WARNING → criticalWarning
│   │   ├── If smartctl available AND drive is NOT Apple internal:
│   │   │   └── Run: smartctl -j -a /dev/diskN → parseSmartctlJson()
│   │   └── deriveHealthVerdict()
│   └── Store in mDrives
└── Check CommandUtil::isExecutable("smartctl") → mHasSmartctl

refreshHealth()
├── For each known drive:
│   ├── Re-run diskutil info -plist
│   ├── Re-parse SMART data
│   └── Re-derive health verdict
```

### 6.3 Linux Implementation Flow

```
DiskHealthInfo()
├── discoverDrives()
│   ├── Enumerate /sys/block/ (same as DiskInfo::getDiskNames())
│   ├── For each disk:
│   │   ├── Read model from /sys/block/{name}/device/model
│   │   ├── Read rotational from /sys/block/{name}/queue/rotational → isSsd
│   │   ├── Detect NVMe from name pattern (starts with "nvme")
│   │   ├── Read size from /sys/block/{name}/size × 512
│   │   ├── If smartctl available:
│   │   │   ├── Run: smartctl -j -a /dev/{name} (unprivileged first)
│   │   │   ├── Check exit_status for permission errors
│   │   │   ├── parseSmartctlJson() → populate fields
│   │   │   └── Set needsElevation if permission denied
│   │   └── deriveHealthVerdict()
│   └── Store in mDrives
└── Check CommandUtil::isExecutable("smartctl") → mHasSmartctl

refreshHealthElevated(device)
├── Run: pkexec smartctl -j -a {device}
├── parseSmartctlJson()
└── deriveHealthVerdict()
```

---

## 7. Integration Points

### 7.1 InfoManager

Add to `info_manager.h`:
```cpp
#include <Info/disk_health_info.h>

// In class declaration:
QList<DriveHealth> getDriveHealth() const;
void refreshDiskHealth();
void refreshDiskHealthElevated(const QString &device);
bool hasDiskHealth() const;
bool hasSmartctl() const;

// In private members:
DiskHealthInfo dhi;
```

### 7.2 Hardware Info — Storage Section

Add `populateStorage()` after `populateBattery()`:
- UI: `grpStorage` QGroupBox with `tblStorage` QTableWidget (2 columns)
- Per drive, show:
  - Model name (or "Drive N" fallback)
  - Health verdict (color-coded: green/yellow/red via QTableWidgetItem foreground color)
  - Temperature
  - Power On Hours (formatted: "1234 hours (51 days)")
  - Power Cycles
  - **NVMe only:** Percentage Used, Available Spare, Media Errors, Unsafe Shutdowns, Data Written
  - **SATA only:** Reallocated Sectors, Pending Sectors
  - If `needsElevation`: show "🔒 Limited data — run with elevated privileges for full report"
  - If no smartctl: show "Install smartmontools for detailed health data"
- Separator row between multiple drives

### 7.3 SettingManager — Disk Health Keys

```cpp
// New keys:
const QString DiskHealthAlertEnabled("DiskHealthAlertEnabled");

// Getter/setter:
void setDiskHealthAlertEnabled(bool value);
bool getDiskHealthAlertEnabled() const;  // default: true
```

### 7.4 Settings Page — Disk Health Alert Toggle

Per OQ-03, we show key attributes only. Alert is simple: on/off toggle for overall health verdict changes. No per-attribute thresholds.

- Add checkbox: "Alert on disk health changes" (default: checked)
- When a drive's healthVerdict changes from "Good" to "Caution" or "Critical", fire a tray notification

### 7.5 Dashboard — Disk Health Consideration

The existing `mDiskBar` shows **usage**. The design decision (OQ-03) focuses on Hardware Info for SMART data, not Dashboard.

**Recommendation:** Do NOT add a separate Dashboard disk health CircleBar in Phase 2. Instead, consider:
- Changing the disk bar's color dynamically if a drive health is "Caution" (yellow) or "Critical" (red)
- Or: adding a small health indicator icon next to the existing disk bar
- This is a future enhancement, not Phase 2 scope

---

## 8. CommandUtil Usage

### 8.1 Available Methods

From `command_util.h`:
```cpp
static QString exec(const QString &cmd, QStringList args, QByteArray data, int timeoutMs = 30000);
static QString sudoExec(const QString &cmd, QStringList args, QByteArray data);
static ExecResult execWithStatus(const QString &cmd, QStringList args, int timeoutMs = 30000);
static bool isExecutable(const QString &cmd);
```

### 8.2 Usage for Disk Health

```cpp
// Check smartctl availability
bool hasSmartctl = CommandUtil::isExecutable("smartctl");

// macOS: diskutil (always available, no root)
QString plistOutput = CommandUtil::exec("diskutil", {"info", "-plist", "/dev/disk0"});

// macOS: diskutil list (enumerate drives)
QString listOutput = CommandUtil::exec("diskutil", {"list", "-plist"});

// Both: smartctl JSON (unprivileged attempt)
ExecResult result = CommandUtil::execWithStatus("smartctl", {"-j", "-a", "/dev/sda"});
if (result.exitCode != 0) {
    // Check if permission error (exit code bit 1)
    // ...
}

// Linux: elevated smartctl via pkexec
QString elevated = CommandUtil::exec("pkexec", {"smartctl", "-j", "-a", "/dev/sda"});
```

### 8.3 Performance Considerations

- SMART data changes very slowly (hours/days, not seconds)
- `smartctl` takes ~100-500ms per drive
- `diskutil info` takes ~50-200ms per drive
- **Refresh strategy:** On page load only (Hardware Info), NOT on a timer
- For future Dashboard integration: cache results, refresh every 5-10 minutes at most

---

## 9. JSON Parsing

Qt6 provides `QJsonDocument` for parsing smartctl's JSON output:

```cpp
QByteArray output = proc.readAllStandardOutput();
QJsonDocument doc = QJsonDocument::fromJson(output);
QJsonObject root = doc.object();

// NVMe health log
QJsonObject nvmeLog = root["nvme_smart_health_information_log"].toObject();
int percentUsed = nvmeLog["percentage_used"].toInt(-1);
int temperature = nvmeLog["temperature"].toInt(-1);
int availableSpare = nvmeLog["available_spare"].toInt(-1);

// SATA attributes
QJsonObject ataAttrs = root["ata_smart_attributes"].toObject();
QJsonArray table = ataAttrs["table"].toArray();
for (const QJsonValue &val : table) {
    QJsonObject attr = val.toObject();
    int id = attr["id"].toInt();
    int rawValue = attr["raw"].toObject()["value"].toInt();
    // ...
}

// Overall SMART status
bool passed = root["smart_status"].toObject()["passed"].toBool(true);
```

For macOS plist parsing, use QProcess output piped to a QXmlStreamReader, or convert with `plutil -convert json` first.

---

## 10. File Inventory

### New Files (4)

| # | File | Purpose | Est. Lines |
|---|------|---------|------------|
| 1 | `shared/nexis-core/Info/disk_health_info.h` | DriveHealth struct, SmartAttribute struct, DiskHealthInfo class | ~100 |
| 2 | `shared/nexis-core/Info/disk_health_info_shared.cpp` | Shared getters, health verdict derivation | ~80 |
| 3 | `macos/nexis-core/Info/disk_health_info.cpp` | macOS diskutil plist parsing + optional smartctl | ~200 |
| 4 | `linux/nexis-core/Info/disk_health_info.cpp` | Linux smartctl JSON parsing + sysfs fallback | ~180 |

### Modified Files (8)

| # | File | Changes |
|---|------|---------|
| 5 | `shared/nexis/Managers/info_manager.h` | +include, +member, +5 method declarations |
| 6 | `shared/nexis/Managers/info_manager.cpp` | +Disk Health Provider section (~15 lines) |
| 7 | `shared/nexis/Managers/setting_manager.h` | +1 key, +2 getter/setter declarations |
| 8 | `shared/nexis/Managers/setting_manager.cpp` | +2 getter/setter implementations |
| 9 | `shared/nexis/Pages/HardwareInfo/hardware_info_page.ui` | +grpStorage QGroupBox with tblStorage |
| 10 | `shared/nexis/Pages/HardwareInfo/hardware_info_page.h` | +populateStorage() declaration |
| 11 | `shared/nexis/Pages/HardwareInfo/hardware_info_page.cpp` | +populateStorage() implementation (~100 lines) |
| 12 | `shared/nexis/Pages/Settings/settings_page.ui` | +checkbox for disk health alerts |

**Estimated total:** ~4 new files, 8 modified files, ~700 new lines of code.

---

## 11. Edge Cases and Risks

### 11.1 Apple Internal SSD Limitations
- Apple Fabric protocol SSDs on Apple Silicon expose NVMe SMART data via `diskutil` but with a non-standard `AVAILABLE_SPARE_THRESHOLD` of 99%.
- Solution: Use our own threshold (10%), not Apple's. Display a note explaining limitations.

### 11.2 smartctl Not Installed
- On macOS: `diskutil` provides enough for basic health display. Show install hint for detailed data.
- On Linux: Without smartctl, we can only show basic sysfs info (model, size, type). Show install hint.
- Use `CommandUtil::isExecutable("smartctl")` to check availability.

### 11.3 Permission Denied on Linux
- Per OQ-01, try unprivileged first. If denied, show partial data + elevation button.
- Some systems allow unprivileged SMART access via udev rules or SCSI generic device permissions.
- `pkexec` provides a polkit GUI auth dialog (better UX than terminal sudo).

### 11.4 USB External Drives
- Some USB enclosures don't pass through SMART data.
- `smartctl` may report "Unknown USB bridge" or fail entirely.
- Solution: Show drive info from sysfs/diskutil, mark SMART as "Not supported" for these drives.

### 11.5 Virtual Drives / Loop Devices
- Docker overlay, snap loop mounts, LVM thin volumes should be filtered out.
- Linux: Only enumerate devices with `/sys/block/{name}/device/` subdirectory.
- macOS: Filter by physical disk protocol (exclude disk images, virtual volumes).

### 11.6 Multiple Drives
- System may have 1-10+ drives. UI must handle multiple drives gracefully.
- Hardware Info: Add separator rows between drives, or use nested QGroupBox per drive.
- Health verdict should reflect the **worst** drive for any global indicator.

### 11.7 smartctl JSON Format Changes
- Parse defensively — every field should check `.isUndefined()` before `.toInt()`.
- Use `-1` sentinel values for missing fields (same pattern as BatteryInfo).

### 11.8 Performance
- `diskutil info` runs ~200ms per drive; `smartctl` ~500ms per drive.
- With 4+ drives, initial scan could take 2+ seconds.
- Solution: Run `discoverDrives()` in constructor (blocking, one-time), then `refreshHealth()` only on explicit user action (switching to Hardware Info page) or on a very slow timer.

---

## 12. Dependencies

| Dependency | Required? | Notes |
|------------|-----------|-------|
| `smartmontools` | Recommended (not required) | Linux: `sudo apt install smartmontools`. macOS: `brew install smartmontools`. If absent, show install hint. |
| `diskutil` | Already available | Ships with macOS. Used as primary source on macOS. |
| IOKit framework | Already linked | `CMakeLists.txt:64-68` — used by GpuInfo, BatteryInfo, ThermalInfo. |
| Qt6::Core JSON | Already linked | `QJsonDocument`, `QJsonObject`, `QJsonArray` for smartctl parsing. |

No new build dependencies or CMakeLists.txt changes needed.

---

## 13. Summary of Design Decisions (from FR-29 Feature Request)

| Decision | Summary | Implementation Impact |
|----------|---------|----------------------|
| OQ-01: Privilege escalation | Unprivileged first, prompt if needed | `needsElevation` flag per drive, "Scan with full access" button |
| OQ-02: Apple internal SSD | Show diskutil Verified/Failing + limitations note | Special handling in macOS impl, no IOKit NVMe user client |
| OQ-03: SMART table depth | Key attributes only (5-8 critical) | No full attribute table; curated subset in QTableWidget |
| OQ-04: Alert behavior | Fire once, with remind later | Same fire-once pattern as battery, using SettingManager |
| OQ-05: Chart scope | Long-term history from JSON | Deferred to Phase 3 |
| OQ-06: TLP integration | Read-only display | Already done in Phase 1 (battery) |
