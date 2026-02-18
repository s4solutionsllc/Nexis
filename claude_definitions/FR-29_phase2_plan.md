# FR-29 Phase 2 (Disk Health / SMART Monitoring) — Implementation Plan

**Date:** February 2026
**Scope:** Disk health SMART monitoring — data layer, Hardware Info, Settings
**Prerequisites:** Phase 1 (Battery Health) complete — commit `66646a8`. Research complete (`FR-29_phase2_research.md`)

---

## Overview

Phase 2 adds disk health / SMART monitoring across three layers:
1. **Data layer** — `DiskHealthInfo` class with macOS `diskutil` plist and Linux `smartctl` JSON implementations
2. **Hardware Info** — Storage section with per-drive health summary and key SMART attributes
3. **Settings** — Disk health alert toggle (alert on verdict change to Caution/Critical)

Dashboard changes and Resources page charts are deferred to Phase 3.

---

## Task 1: DiskHealthInfo Header & Shared Code

### 1.1 — Create shared header
- [ ] Create `shared/nexis-core/Info/disk_health_info.h`
- [ ] Define `SmartAttribute` struct:
  - `int id` (-1 if N/A)
  - `QString name`
  - `int value` (normalized, 0-253)
  - `int worst`
  - `int threshold`
  - `qint64 rawValue`
  - `QString status` ("ok", "warning", "failing")
- [ ] Define `DriveHealth` struct with fields:
  - **Identity:** `QString devicePath`, `QString deviceName`, `QString model`, `QString serial`, `QString firmware`, `quint64 sizeBytes`
  - **Drive type:** `enum DriveType { Unknown, NVMe, SATA_SSD, SATA_HDD }`, `DriveType driveType`, `QString protocol`
  - **Health summary:** `int healthPercent` (-1 if unavailable), `QString healthVerdict` ("Good", "Caution", "Critical", "Unknown"), `bool smartPassed`, `bool needsElevation`
  - **Common metrics:** `double temperatureCelsius` (-1.0), `int powerOnHours` (-1), `int powerCycles` (-1)
  - **NVMe-specific:** `int percentageUsed` (-1), `int availableSpare` (-1), `int availableSpareThreshold` (-1), `int criticalWarning` (-1), `int unsafeShutdowns` (-1), `int mediaErrors` (-1), `qint64 dataUnitsRead` (-1), `qint64 dataUnitsWritten` (-1)
  - **SATA-specific:** `int reallocatedSectors` (-1), `int pendingSectors` (-1), `int uncorrectableSectors` (-1), `int reallocatedEvents` (-1), `int wearLevelingCount` (-1)
  - **Full table:** `QList<SmartAttribute> allAttributes`
- [ ] Define `DiskHealthInfo` class:
  - Constructor
  - `QList<DriveHealth> getDrives() const`
  - `bool hasDrives() const`
  - `bool hasSmartctl() const`
  - `void refreshHealth()` — re-scan all drives
  - `void refreshHealthElevated(const QString &device)` — single drive with sudo
  - Private: `void discoverDrives()`, `void deriveHealthVerdict(DriveHealth &drive)`, `QList<DriveHealth> mDrives`, `bool mHasSmartctl`

**Acceptance:** Header compiles on both platforms.

### 1.2 — Create shared getters and health derivation
- [ ] Create `shared/nexis-core/Info/disk_health_info_shared.cpp`
- [ ] Implement `getDrives()`, `hasDrives()`, `hasSmartctl()` as member returns
- [ ] Implement `deriveHealthVerdict(DriveHealth &drive)`:
  - NVMe logic:
    - `criticalWarning != 0` OR `mediaErrors > 0` OR `percentageUsed >= 100` OR `!smartPassed` → "Critical"
    - `availableSpare <= 10` (ignore Apple's 99% threshold) OR `percentageUsed >= 80` → "Caution"
    - Otherwise → "Good"
    - `healthPercent = max(0, 100 - percentageUsed)` (clamped 0-100)
  - SATA HDD logic:
    - Any of `reallocatedSectors`, `pendingSectors`, `uncorrectableSectors` > 100 OR `!smartPassed` → "Critical"
    - Any of those > 0 → "Caution"
    - Otherwise → "Good"
    - `healthPercent = -1` (unavailable for HDD — no universal metric)
  - SATA SSD logic:
    - `wearLevelingCount < 10` OR `!smartPassed` → "Critical"
    - `wearLevelingCount < 30` OR `reallocatedSectors > 0` → "Caution"
    - Otherwise → "Good"
    - `healthPercent = wearLevelingCount` (if available)
  - Unknown / diskutil-only (Apple internal):
    - `!smartPassed` → "Critical"
    - Otherwise → "Good" if SMART passed, "Unknown" if no data

**Acceptance:** Shared cpp compiles. Health derivation covers all drive types.

### 1.3 — Build verification
- [ ] Incremental build succeeds

---

## Task 2: macOS Implementation

### 2.1 — Create macOS implementation
- [ ] Create `macos/nexis-core/Info/disk_health_info.cpp`
- [ ] Check `CommandUtil::isExecutable("smartctl")` → `mHasSmartctl`
- [ ] `discoverDrives()`:
  - Run `diskutil list -plist` via `CommandUtil::exec()`
  - Parse XML plist output to extract `AllDisksAndPartitions` → filter to physical `WholeDisks` (e.g., disk0, disk1, disk2)
  - For each whole disk:
    - Run `diskutil info -plist /dev/diskN`
    - Parse plist for:
      - `MediaName` / `IORegistryEntryName` → `model`
      - `TotalSize` → `sizeBytes`
      - `SolidState` → classify SSD vs HDD
      - `DeviceProtocol` → `protocol` ("NVMe", "SATA", "Apple Fabric", "USB")
      - `SMARTStatus` → "Verified" maps to `smartPassed=true`, "Failing" maps to `smartPassed=false`
      - `SMARTDeviceSpecificKeysMayVaryNotGuaranteed` → NVMe SMART data dictionary:
        - `TEMPERATURE` → `temperatureCelsius` (value is in **Kelvin**, subtract 273)
        - `PERCENTAGE_USED` → `percentageUsed`
        - `AVAILABLE_SPARE` → `availableSpare`
        - `AVAILABLE_SPARE_THRESHOLD` → `availableSpareThreshold` (store but override with 10% in verdict logic)
        - `POWER_ON_HOURS` → `powerOnHours`
        - `POWER_CYCLES` → `powerCycles`
        - `UNSAFE_SHUTDOWNS` → `unsafeShutdowns`
        - `MEDIA_AND_DATA_INTEGRITY_ERRORS` → `mediaErrors`
        - `DATA_UNITS_READ` → `dataUnitsRead`
        - `DATA_UNITS_WRITTEN` → `dataUnitsWritten`
        - `CRITICAL_WARNING` → `criticalWarning`
      - Set `driveType` based on protocol and SolidState flag
    - If `mHasSmartctl` AND protocol is NOT "Apple Fabric" (not Apple internal SSD):
      - Run `smartctl -j -a /dev/diskN` for third-party SATA drives
      - Parse JSON for SATA attributes (task 2.2)
    - Call `deriveHealthVerdict(drive)`
- [ ] Helper: `parsePlistValue()` — extract values from diskutil XML plist output using QXmlStreamReader or QRegularExpression matching `<key>X</key><string>Y</string>` / `<integer>N</integer>` / `<true/>` / `<false/>` patterns
- [ ] Helper: `parseSmartctlJson(const QByteArray &json, DriveHealth &drive)` — shared JSON parsing (also used by Linux)
- [ ] Constructor calls `discoverDrives()`

### 2.2 — Implement parseSmartctlJson() (cross-platform, in macOS file for now)
- [ ] Parse JSON with `QJsonDocument::fromJson()`
- [ ] Extract device info: `model_name`, `serial_number`, `firmware_version`
- [ ] Check `smart_status.passed` → `smartPassed`
- [ ] Detect NVMe vs SATA from `device.type` ("nvme" vs "ata")
- [ ] **NVMe path:** Parse `nvme_smart_health_information_log` object:
  - `critical_warning`, `temperature`, `available_spare`, `available_spare_threshold`
  - `percentage_used`, `data_units_read`, `data_units_written`
  - `power_cycles`, `power_on_hours`, `unsafe_shutdowns`, `media_errors`
- [ ] **SATA path:** Parse `ata_smart_attributes.table` array:
  - For each attribute object: extract `id`, `name`, `value`, `worst`, `thresh`, `raw.value`
  - Build `SmartAttribute` for allAttributes list
  - Extract key attributes by ID:
    - ID 5 → `reallocatedSectors`
    - ID 9 → `powerOnHours` (from raw value)
    - ID 12 → `powerCycles` (from raw value)
    - ID 177 → `wearLevelingCount` (from normalized value)
    - ID 190/194 → `temperatureCelsius` (from raw value)
    - ID 196 → `reallocatedEvents`
    - ID 197 → `pendingSectors`
    - ID 198 → `uncorrectableSectors`
- [ ] Extract `rotation_rate` from root → if > 0, set `driveType = SATA_HDD`, else `SATA_SSD`

### 2.3 — Implement refreshHealth()
- [ ] Re-run `diskutil info -plist` for each known drive
- [ ] Re-parse SMART data
- [ ] Re-derive health verdict

### 2.4 — Implement refreshHealthElevated()
- [ ] Run `CommandUtil::sudoExec("smartctl", {"-j", "-a", device})`
- [ ] Parse JSON, update drive data, re-derive verdict
- [ ] Clear `needsElevation` flag on success

### 2.5 — Build verification
- [ ] macOS incremental build succeeds
- [ ] On macOS: DiskHealthInfo discovers all physical drives
- [ ] SMART data populated for Apple internal SSD (from diskutil plist)

---

## Task 3: Linux Implementation

### 3.1 — Create Linux implementation
- [ ] Create `linux/nexis-core/Info/disk_health_info.cpp`
- [ ] Check `CommandUtil::isExecutable("smartctl")` → `mHasSmartctl`
- [ ] `discoverDrives()`:
  - Enumerate `/sys/block/` directories (same filter as `DiskInfo::getDiskNames()` — require `device/` subdir)
  - For each disk (e.g., "sda", "nvme0n1"):
    - Read model: `/sys/block/{name}/device/model`
    - Read size: `/sys/block/{name}/size` × 512 → `sizeBytes`
    - Detect NVMe: name starts with "nvme"
    - Detect SSD vs HDD: `/sys/block/{name}/queue/rotational` (0=SSD, 1=HDD)
    - Set `devicePath` to `/dev/{name}`
    - Set `driveType` from NVMe flag and rotational flag
    - If `mHasSmartctl`:
      - Run `CommandUtil::execWithStatus("smartctl", {"-j", "-a", devicePath})` (unprivileged)
      - Check `exitCode`: if bit 1 set (value & 2), permission denied → set `needsElevation = true`, try to use partial data
      - If successful: call `parseSmartctlJson()` (same function as macOS task 2.2 — duplicated or linked)
    - If no smartctl: populate only sysfs-derived fields (model, size, type)
    - Call `deriveHealthVerdict(drive)`
- [ ] `parseSmartctlJson()` — duplicate the same implementation from macOS (identical logic, both use QJsonDocument)
- [ ] Constructor calls `discoverDrives()`

### 3.2 — Implement refreshHealth()
- [ ] For each known drive, re-run smartctl and re-parse
- [ ] Update health verdicts

### 3.3 — Implement refreshHealthElevated()
- [ ] Run `CommandUtil::exec("pkexec", {"smartctl", "-j", "-a", device})`
- [ ] Parse JSON, update drive data, clear `needsElevation`

### 3.4 — Build verification
- [ ] Linux incremental build succeeds (cross-compile or clean macOS build verifying no Linux-specific compile errors)

---

## Task 4: InfoManager Integration

### 4.1 — Update InfoManager header
- [ ] Add `#include <Info/disk_health_info.h>` to `info_manager.h`
- [ ] Add `DiskHealthInfo dhi;` to private members
- [ ] Add public method declarations:
  - `QList<DriveHealth> getDriveHealth() const;`
  - `void refreshDiskHealth();`
  - `void refreshDiskHealthElevated(const QString &device);`
  - `bool hasDiskHealth() const;`
  - `bool hasSmartctl() const;`

### 4.2 — Update InfoManager implementation
- [ ] Add Disk Health Provider section to `info_manager.cpp`:
  ```cpp
  QList<DriveHealth> InfoManager::getDriveHealth() const { return dhi.getDrives(); }
  void InfoManager::refreshDiskHealth() { dhi.refreshHealth(); }
  void InfoManager::refreshDiskHealthElevated(const QString &device) { dhi.refreshHealthElevated(device); }
  bool InfoManager::hasDiskHealth() const { return dhi.hasDrives(); }
  bool InfoManager::hasSmartctl() const { return dhi.hasSmartctl(); }
  ```

### 4.3 — Build verification
- [ ] Incremental build succeeds

---

## Task 5: Hardware Info — Storage Section

### 5.1 — Add storage section to UI
- [ ] Edit `shared/nexis/Pages/HardwareInfo/hardware_info_page.ui`:
  - Add `grpStorage` QGroupBox (title: "Storage") **after** `grpBattery`, **before** the vertical spacer
  - Add `tblStorage` QTableWidget inside (2 columns, same properties as `tblBattery`)
  - Match all widget properties: showGrid=false, frameShape=NoFrame, selectionMode=NoSelection, editTriggers=NoEditTriggers, focusPolicy=NoFocus, scrollBars=AlwaysOff

### 5.2 — Add populate method declaration
- [ ] Edit `hardware_info_page.h`:
  - Add `void populateStorage();` to private methods

### 5.3 — Implement populateStorage()
- [ ] Edit `hardware_info_page.cpp`:
  - Add `populateStorage()` call in `init()` after `populateBattery()`
  - Implement `populateStorage()`:
    ```
    QTableWidget *t = ui->tblStorage;
    // Standard table setup (headers hidden, stretch last section)

    QList<DriveHealth> drives = im->getDriveHealth();
    if (drives.isEmpty()) {
        ui->grpStorage->hide();
        return;
    }

    for (int i = 0; i < drives.size(); ++i) {
        const DriveHealth &d = drives.at(i);

        // Separator between drives (empty row) if not the first drive
        if (i > 0) {
            int sep = t->rowCount();
            t->insertRow(sep);
            // leave both cells empty as a visual separator
        }

        // Drive header: "Drive N: Model Name" or just "Model Name" if single drive
        QString driveLabel = drives.size() > 1
            ? tr("Drive %1").arg(i + 1)
            : tr("Model");
        addRow(t, driveLabel, d.model.isEmpty() ? d.deviceName : d.model);

        // Health verdict with color-coded value
        if (!d.healthVerdict.isEmpty() && d.healthVerdict != "Unknown") {
            QString healthStr = d.healthVerdict;
            if (d.healthPercent >= 0)
                healthStr = QString("%1% (%2)").arg(d.healthPercent).arg(d.healthVerdict);
            int row = t->rowCount();
            addRow(t, tr("Health"), healthStr);
            // Color-code the value cell
            QTableWidgetItem *valueItem = t->item(row, 1);
            if (d.healthVerdict == "Good")
                valueItem->setForeground(QColor("#2ec27e")); // green
            else if (d.healthVerdict == "Caution")
                valueItem->setForeground(QColor("#e5a50a")); // yellow/amber
            else if (d.healthVerdict == "Critical")
                valueItem->setForeground(QColor("#e01b24")); // red
        } else if (d.smartPassed) {
            addRow(t, tr("SMART Status"), tr("Verified"));
        }

        // Protocol / type
        if (!d.protocol.isEmpty())
            addRow(t, tr("Interface"), d.protocol);

        // Capacity
        if (d.sizeBytes > 0)
            addRow(t, tr("Capacity"), FormatUtil::formatBytes(d.sizeBytes));

        // Temperature
        if (d.temperatureCelsius >= 0) {
            double tempF = d.temperatureCelsius * 9.0 / 5.0 + 32.0;
            addRow(t, tr("Temperature"), QString("%1 °C / %2 °F")
                .arg(d.temperatureCelsius, 0, 'f', 0).arg(tempF, 0, 'f', 0));
        }

        // Power On Hours (formatted as "1234 hours (51 days)")
        if (d.powerOnHours >= 0) {
            int days = d.powerOnHours / 24;
            QString hoursStr = QString("%1 %2").arg(d.powerOnHours).arg(tr("hours"));
            if (days > 0)
                hoursStr += QString(" (%1 %2)").arg(days).arg(tr("days"));
            addRow(t, tr("Power On Hours"), hoursStr);
        }

        // Power Cycles
        if (d.powerCycles >= 0)
            addRow(t, tr("Power Cycles"), QString::number(d.powerCycles));

        // NVMe-specific fields
        if (d.driveType == DriveHealth::NVMe) {
            if (d.percentageUsed >= 0)
                addRow(t, tr("Endurance Used"), QString("%1%").arg(d.percentageUsed));
            if (d.availableSpare >= 0)
                addRow(t, tr("Available Spare"), QString("%1%").arg(d.availableSpare));
            if (d.mediaErrors >= 0)
                addRow(t, tr("Media Errors"), QString::number(d.mediaErrors));
            if (d.unsafeShutdowns >= 0)
                addRow(t, tr("Unsafe Shutdowns"), QString::number(d.unsafeShutdowns));
            if (d.dataUnitsWritten >= 0) {
                // Convert units: each unit = 512 bytes × 1000 = 512000 bytes
                double tbWritten = (d.dataUnitsWritten * 512000.0) / (1024.0*1024*1024*1024);
                addRow(t, tr("Data Written"), QString("%1 TB").arg(tbWritten, 0, 'f', 2));
            }
        }

        // SATA-specific fields
        if (d.driveType == DriveHealth::SATA_SSD || d.driveType == DriveHealth::SATA_HDD) {
            if (d.reallocatedSectors >= 0)
                addRow(t, tr("Reallocated Sectors"), QString::number(d.reallocatedSectors));
            if (d.pendingSectors >= 0)
                addRow(t, tr("Pending Sectors"), QString::number(d.pendingSectors));
            if (d.uncorrectableSectors >= 0)
                addRow(t, tr("Uncorrectable Sectors"), QString::number(d.uncorrectableSectors));
        }

        // Needs elevation notice
        if (d.needsElevation)
            addRow(t, tr("Note"), tr("Limited data — elevated privileges required for full report"));

        // Serial and firmware (compact)
        if (!d.serial.isEmpty())
            addRow(t, tr("Serial"), d.serial);
        if (!d.firmware.isEmpty())
            addRow(t, tr("Firmware"), d.firmware);
    }

    // Smartctl not installed notice
    if (!im->hasSmartctl())
        addRow(t, tr("Note"), tr("Install smartmontools for detailed disk health data"));

    // Apple internal SSD limitation note (macOS only)
    #ifdef Q_OS_MAC
    for (const DriveHealth &d : drives) {
        if (d.protocol == "Apple Fabric" && d.healthPercent < 0) {
            addRow(t, tr("Note"), tr("Apple internal SSDs provide limited health reporting"));
            break;
        }
    }
    #endif

    t->resizeColumnsToContents();
    fitTableHeight(t);
    ```

### 5.4 — Build and visual verification
- [ ] Incremental build succeeds
- [ ] Storage section appears in Hardware Info with drive health data
- [ ] Health verdict is color-coded (green/yellow/red)

---

## Task 6: SettingManager — Disk Health Alert Key

### 6.1 — Add key to header
- [ ] Edit `setting_manager.h`:
  - Add to `SettingKeys` namespace:
    ```cpp
    const QString DiskHealthAlertEnabled("DiskHealthAlertEnabled");
    ```
  - Add getter/setter declarations:
    ```cpp
    void setDiskHealthAlertEnabled(bool value);
    bool getDiskHealthAlertEnabled() const;
    ```

### 6.2 — Implement getter/setter
- [ ] Edit `setting_manager.cpp`:
  - `DiskHealthAlertEnabled` — default `true` (enabled)

### 6.3 — Build verification
- [ ] Incremental build succeeds

---

## Task 7: Settings Page — Disk Health Alert Toggle

### 7.1 — Add UI widget
- [ ] Edit `shared/nexis/Pages/Settings/settings_page.ui`:
  - Add `lblDiskHealthAlert` QLabel (text: "Disk Health Alert") at Row 7, Column 4 (or appropriate position in the existing grid layout)
  - Add `checkDiskHealthAlert` QCheckBox at Row 8, Column 4 — cursor=PointingHandCursor, focusPolicy=NoFocus

### 7.2 — Wire loading and saving
- [ ] Edit `settings_page.h`:
  - Add `void on_checkDiskHealthAlert_clicked(bool checked);` slot declaration
- [ ] Edit `settings_page.cpp`:
  - In `init()`:
    ```cpp
    ui->checkDiskHealthAlert->setChecked(mSettingManager->getDiskHealthAlertEnabled());
    ```
  - Add slot:
    ```cpp
    void SettingsPage::on_checkDiskHealthAlert_clicked(bool checked) {
        mSettingManager->setDiskHealthAlertEnabled(checked);
    }
    ```

### 7.3 — Build verification
- [ ] Incremental build succeeds

---

## Task 8: Final Verification & Cleanup

### 8.1 — Clean rebuild
- [ ] Full clean rebuild on macOS: `rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) && cmake --build build -j$(sysctl -n hw.ncpu)`
- [ ] Verify zero warnings in disk_health_info files

### 8.2 — Functional testing
- [ ] Hardware Info: Storage section shows all detected drives with model and health
- [ ] Hardware Info: NVMe drives show percentage used, available spare, media errors, etc.
- [ ] Hardware Info: Health verdict is correctly color-coded
- [ ] Hardware Info: Apple internal SSD shows Verified/Failing with limitation note
- [ ] Settings: Disk health alert toggle persists across app restarts

### 8.3 — Update tracking files
- [ ] Mark FR-29 Phase 2 tasks complete in this plan document
- [ ] Update FEATURE_REQUESTS.md with Phase 2 resolution notes
- [ ] Commit and push

---

## Summary of All Files

### New (4 files)
| # | File | Lines (est.) |
|---|------|-------------|
| 1 | `shared/nexis-core/Info/disk_health_info.h` | ~100 |
| 2 | `shared/nexis-core/Info/disk_health_info_shared.cpp` | ~80 |
| 3 | `macos/nexis-core/Info/disk_health_info.cpp` | ~250 |
| 4 | `linux/nexis-core/Info/disk_health_info.cpp` | ~200 |

### Modified (8 files)
| # | File | Changes |
|---|------|---------|
| 5 | `shared/nexis/Managers/info_manager.h` | +include, +member, +5 declarations |
| 6 | `shared/nexis/Managers/info_manager.cpp` | +15 lines (Disk Health Provider section) |
| 7 | `shared/nexis/Managers/setting_manager.h` | +1 key, +2 getter/setter declarations |
| 8 | `shared/nexis/Managers/setting_manager.cpp` | +6 lines (1 getter/setter pair) |
| 9 | `shared/nexis/Pages/HardwareInfo/hardware_info_page.ui` | +25 lines (grpStorage group) |
| 10 | `shared/nexis/Pages/HardwareInfo/hardware_info_page.h` | +1 line (populateStorage declaration) |
| 11 | `shared/nexis/Pages/HardwareInfo/hardware_info_page.cpp` | +120 lines (populateStorage implementation) |
| 12 | `shared/nexis/Pages/Settings/settings_page.ui` | +15 lines (label + checkbox) |
| 13 | `shared/nexis/Pages/Settings/settings_page.h` | +1 line (slot declaration) |
| 14 | `shared/nexis/Pages/Settings/settings_page.cpp` | +6 lines (load + save) |

**Total estimated:** ~4 new files, 10 modified files, ~700 new lines of code.

---

## Key Design Decisions Reference

| Decision | Implementation |
|----------|---------------|
| **OQ-01:** Unprivileged first | `smartctl` runs without sudo; `needsElevation` flag + notice in UI |
| **OQ-02:** Apple internal SSD | `diskutil` plist parsing only; "limited reporting" note shown |
| **OQ-03:** Key attributes only | 5-8 curated fields per drive type in QTableWidget rows |
| **Alert behavior** | Simple on/off toggle; fires tray notification when any drive verdict changes to "Caution" or "Critical" |
| **Apple AVAILABLE_SPARE_THRESHOLD** | Store Apple's 99% value but override with 10% in verdict derivation |
| **Temperature units** | macOS diskutil reports Kelvin; subtract 273 before storing |
| **Data written units** | NVMe reports in units of 512KB × 1000; convert to TB for display |
