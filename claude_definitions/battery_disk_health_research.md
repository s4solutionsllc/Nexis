# Battery Health & SSD/Disk Health Monitoring — Market Research

**Purpose:** Competitive feature analysis for Nexis battery and disk health monitoring pages.
**Date:** 2026-02-17

---

## Part 1: Battery Health Monitoring

---

### 1.1 Sensei (macOS) — Battery Health

**Presentation:** Dedicated "Battery" section within the app, with real-time metrics also available as a customizable menu bar widget and dropdown panel.

**Metrics displayed:**
- Cycle count
- Current charge capacity (mAh)
- Design capacity (mAh)
- Health percentage (capacity ratio)
- Real-time charge/discharge rate
- Battery condition status (Good / Replace Soon / Replace Now / Service Battery)
- Time remaining estimate
- Power source status (AC/Battery)

**Additional hardware context:**
- Thermal monitoring is separate (CPU/GPU temperatures, fan speeds) but on the same hardware monitoring tab
- SSD Trim toggle to extend drive lifespan

**Alerting:** Not prominently advertised for battery; primary value is passive display.

**Historical tracking:** Not advertised.

**Pricing:** $29 USD one-time. Free download with limited features. macOS 11+.

**Unique features:**
- Disk Benchmark (read/write speed test) bundled in same tool
- Tight integration with macOS aesthetics and Apple Silicon support
- Trusted by 50,000+ users

---

### 1.2 iStat Menus (macOS) — Battery Health

**Presentation:** Menu bar item that is highly configurable. Can display battery percentage, time remaining, charging indicator, and health rating inline. Clicking shows a dropdown with detailed stats. Supports stacked label/value display mode.

**Metrics displayed:**
- Battery status (charging, discharging, fully charged)
- Health percentage
- Cycle count
- Condition (Normal, Replace Soon, Replace Now, Service Battery — sourced from macOS)
- Time to full charge / time remaining
- Per-app battery usage (identifies power-hungry apps)
- Bluetooth device battery levels (AirPods, Magic Mouse, trackpad, etc.)
- Power draw stats (energy-related items for charging vs discharging)

**Alerting:**
- Configurable notifications for battery-related events (e.g., battery below threshold, charging complete, battery health degraded)
- Alerts for a wide range of events: CPU, GPU, memory, disks, network, sensors, battery, power, and weather

**Historical tracking:** Not prominently featured for battery health history.

**Pricing:**
- Single user: $12 one-time (iStat Menus 7)
- Family (up to 6): $16 one-time
- Upgrade from v6 to v7: $9.99
- Included in Setapp ($10/month subscription)
- 14-day free trial

**Unique features:**
- Most comprehensive menu bar system monitor on macOS
- iStat Menus 7.0 (July 2024) introduced new menu bar modes (stacked labels/values), new items including Wi-Fi network name, GPU FPS
- Fan speed control with custom speed curves

---

### 1.3 coconutBattery (macOS) — Battery Health

**Presentation:** Dedicated standalone battery health app. Primary window shows a card-style layout for Mac and connected iOS devices. Menu bar support in Plus version.

**Metrics displayed (Free):**
- Current charge (mAh)
- Maximum charge capacity (mAh, current full charge)
- Design capacity (mAh, factory original)
- Health percentage (MaxCapacity / DesignCapacity × 100)
- Cycle count
- macOS battery condition status
- Battery temperature (°C)
- Discharge/charge rate
- Power adapter connected status and wattage

**Metrics displayed (Plus, $9.95):**
- All of the above, in the menu bar
- iOS/iPadOS device battery: current charge, max capacity, design capacity, health %, cycle count, temperature
- Wi-Fi connectivity for iOS stats (no USB required)
- **Lifetime Analyzer:** maximum, minimum, average temperature; voltage ranges; charge/discharge rate ranges; battery operating time over lifetime
- Cycle count history graph (how cycle count changes over time)
- SSD statistics viewer (advanced system info)

**Alerting:**
- Notifications when Mac battery drops below a set percentage or remaining time
- Notifications when iPhone/iPad battery percentage drops below a threshold (Plus)

**Historical tracking:**
- Automatic recording each time the app runs: device age, health %, cycle count
- History preserved for 365 days then auto-deleted
- Export to CSV or archive files for long-term preservation
- coconutBattery Online: anonymized data sync to view trends over time

**Pricing:**
- Free version: core Mac battery monitoring
- Plus: $9.95 one-time (v4.x updates); "Lifetime Edition" (all future updates) also available
- 10 free Plus-mode activations as trial

**Unique features:**
- The most focused, purpose-built Mac battery health tool
- iOS/iPadOS device battery reading via USB and Wi-Fi
- Manufacture date display
- Historical cycle count tracking with graph
- CSV export of battery history — invaluable for long-term monitoring
- coconutBattery Online anonymized data submission for community comparison

---

### 1.4 CleanMyMac X (macOS) — Battery Health

**Presentation:** Part of the broader "Mac Health" monitoring module. Battery health shown as a widget/card in the CleanMyMac menu and dedicated health section.

**Metrics displayed:**
- Battery health percentage (MaxCapacity / DesignCapacity ratio)
- Cycle count
- Charging status (charging, discharging, charged) with time estimate
- Battery temperature (measured hourly; warning issued on overheat)
- Charge remaining percentage with segmented visual chart
- Condition indicator

**Alerting:**
- Temperature warnings when battery overheats
- Critical failure reports (immediate alerts when a severe hardware issue is detected)

**Historical tracking:** Not prominently featured.

**Pricing:** Subscription-based (MacPaw). Approximately $39.95/year or $119.95 one-time (CleanMyMac X). Also available via Setapp.

**Unique features:**
- Integrated as part of an all-in-one Mac cleaner/optimizer suite, not a standalone monitor
- Positions battery health as part of overall "Mac Health Score"

---

### 1.5 TLP (Linux) — Battery Information

**Presentation:** Command-line tool primarily. `tlp-stat -b` outputs battery status to terminal. No GUI.

**Metrics exposed (from `/sys/class/power_supply/BAT0/`):**
- Manufacturer and model
- Cycle count (when supported by hardware, e.g., ThinkPads with tp-smapi)
- Energy now (current charge, Wh)
- Energy full (current max capacity, Wh)
- Energy full design (factory design capacity, Wh)
- Health percentage (energy_full / energy_full_design × 100)
- Energy rate (current power draw/charge rate, W)
- Voltage (V)
- Status (Charging / Discharging / Full)

**Alerting:** None (TLP is a power optimizer, not a monitor/alerter).

**Historical tracking:** None built-in.

**Pricing:** Free, open-source.

**Unique features:**
- Battery charge threshold management: set START_CHARGE_THRESH and STOP_CHARGE_THRESH to limit charging to 80% for longevity
- Power-saving tuning (USB autosuspend, CPU scaling governor, etc.)
- ThinkPad-specific: cycle count and charge threshold control via tp-smapi

---

### 1.6 powertop (Linux) — Battery Information

**Presentation:** Terminal-based ncurses interactive interface with multiple tabs. Also supports HTML/CSV report export.

**Metrics displayed:**
- **Overview tab:** Battery discharge rate (W), estimated remaining time, total system power consumption
- **Device stats tab:** Per-device power consumption breakdown (CPU, GPU, USB devices, network interfaces)
- **Process/timer stats:** Per-process wakeup events per second and power consumption estimate
- **Frequency stats tab:** CPU frequency distribution (how much time spent at each P-state)
- **Idle stats tab:** CPU C-state (power state) distribution
- **Tunables tab:** Power saving settings with Good/Bad status, toggleable from within powertop

**Battery-specific info:**
- Discharge rate in watts (real-time)
- Time remaining estimate
- Does NOT show cycle count, health %, or capacity degradation — those come from `/sys/class/power_supply/` or TLP

**Alerting:** None.

**Historical tracking:** None; designed for real-time analysis.

**Pricing:** Free, open-source (Intel-developed).

**Unique features:**
- Root-level power consumption attribution per process, per device
- Identifies which processes cause the most wakeups (high wakeup rate = more power use)
- HTML report export for sharing/documenting power profile

---

### 1.7 GNOME Power Statistics (Linux)

**Presentation:** Graphical GTK application. Left panel lists power devices (battery, AC adapter, UPS). Right panel shows details and time-series charts.

**Metrics displayed:**
- Current charge percentage
- Charge state (charging, discharging, fully charged)
- Charge history graph (charge % over time — hours to days)
- Capacity graph (current maximum vs design maximum over time)
- Voltage graph (real-time voltage trend)
- Temperature (real-time battery temperature)
- Time to empty / time to full
- Device details: technology (Li-ion, etc.), vendor, model, serial

**Data source:** Reads from `upower` daemon, which reads `/sys/class/power_supply/`.

**Alerting:** Relies on upower/GNOME notification system for low battery warnings, not custom health degradation alerts.

**Historical tracking:**
- Charge history and capacity history graphs built-in
- Data retained while running; not long-term persistent storage

**Pricing:** Free, open-source. Ships by default with GNOME desktop environments.

**Unique features:**
- Best GUI battery tool on Linux for visual capacity degradation tracking
- Shows capacity degradation as a chart over time (great for seeing gradual health decline)
- Reads and displays all UPower-exposed power devices including USB devices

---

## Part 2: SSD / Disk Health Monitoring

---

### 2.1 Sensei (macOS) — Disk/SSD Health

**Presentation:** Dedicated "Drive Health" section with S.M.A.R.T. report card view. Also includes separate Disk Benchmark and SSD Trim sections.

**Metrics displayed:**
- S.M.A.R.T. status summary (overall pass/fail)
- Drive Health Report: SMART variable analysis to gauge drive condition and estimate remaining lifetime
- Read/write speed benchmark (separate Disk Benchmark tool)

**Alerting:** The health report flags drives with SMART anomalies; no continuous background daemon mentioned.

**Historical tracking:** Not prominently advertised.

**Pricing:** $29 USD one-time (same as battery — one unified app).

**Unique features:**
- SSD Trim toggle: enables/disables TRIM support on macOS for third-party SSDs
- Disk Benchmark integrated: single-click read/write speed test
- Failure prediction language: "discover faulty or failing disks well in advance — giving you the chance to backup important data"

---

### 2.2 iStat Menus (macOS) — Disk Health

**Presentation:** Menu bar item showing disk read/write activity. Clicking opens a dropdown with disk stats. SMART status visible per drive.

**Metrics displayed:**
- Disk usage (used/free space per volume)
- Real-time read/write activity (speed and graphs)
- S.M.A.R.T. status (Verified / Failing — mirrors what macOS reports)
- Per-app disk usage (which processes are reading/writing most)

**Alerting:**
- Configurable disk-related notifications (free space below threshold, SMART status change)

**Historical tracking:** Activity graphs show recent read/write history, not long-term SMART history.

**Pricing:** Same as battery above ($12 single, $16 family).

**Unique features:**
- SMART status is a basic pass/fail from macOS — does not do its own SMART attribute parsing
- iStat Menus 6.2 reportedly broke SMART support in older macOS (documented by BinaryFruit)
- Best value if you want all-in-one monitoring; not a dedicated disk health tool

---

### 2.3 DriveDx (macOS) — Dedicated SSD/HDD Health

**Presentation:** Standalone dedicated disk health application. Shows per-drive health dashboard with detailed SMART attribute table.

**Metrics displayed:**
- **Overall Drive Health Rating** (0–100% percentage score)
- **Drive Performance Rating** (0–100%)
- **SSD Lifetime Left** (percentage of endurance remaining, SSD-specific)
- Full SMART attribute table: ID number, name, raw value, normalized value, threshold, health rating per attribute
- Drive temperature (real-time; overheat warnings)
- Drive information: model, firmware version, serial number, capacity, interface type
- Error log and self-test log

**SMART attribute coverage:**
- Reallocated Sectors Count (ID 5) — critical for HDDs
- Reallocated Event Count (ID 196)
- Current Pending Sectors (ID 197)
- Uncorrectable Sector Count (ID 198)
- SSD Wear Leveling Count (ID 177)
- SSD Lifetime Left / Percentage Used
- Power On Hours (ID 9)
- Power Cycle Count (ID 12)
- Temperature (ID 194 / 190)
- Seek Error Rate (ID 7) — HDD-specific
- I/O errors, data retention attributes
- All other drive-reported attributes

**Alerting / Notifications:**
- Multi-tier warning system: Normal → Warning → Failing (Pre-fail) → Failed
- Tracks attribute change dynamics, not just static values (alerts when degradation rate is concerning even if threshold not yet crossed)
- Desktop notification popups
- Email notifications/reports (configurable)
- Free space alerts (below user-defined threshold)
- Background daemon runs continuous monitoring

**Historical tracking:**
- Tracks attribute value changes over time
- Monitors approximation of values toward failure thresholds

**Pricing:** $20 USD (one-time license). Competitors: SMART Utility $25, Drive Scope $50.

**Unique features:**
- Separate algorithms for HDD vs SSD — does not apply HDD rules to SSDs and vice versa
- Per-drive-model and even per-firmware heuristics (specialized algorithms)
- "Pre-fail" state detection before standard SMART failure threshold is crossed
- Email reporting valuable for server/remote monitoring use cases
- Comparison utility page showing DriveDx vs competitors available at binaryfruit.com

---

### 2.4 OnyX (macOS) — Disk Health

**Presentation:** Multifunction Mac maintenance utility. Disk verification is one feature among many (cleaning, rebuilding databases, maintenance scripts).

**Metrics displayed:**
- Disk structure verification (filesystem integrity check, equivalent to `fsck`)
- S.M.A.R.T. status check: reports Verified / Failing (same as Disk Utility)
- Does NOT parse individual SMART attributes or show detailed health data

**Alerting:** None beyond what macOS reports.

**Historical tracking:** None.

**Pricing:** Free. (Titanium Software)

**Unique features:**
- Not a SMART monitoring tool — it runs `diskutil verifyDisk` and checks macOS SMART status
- Best for: cache cleaning, rebuilding Launch Services, rebuilding Spotlight index, running maintenance scripts
- Automated scan scheduling
- A good "do basic verification" tool but not a substitute for DriveDx or smartmontools for SMART data

---

### 2.5 GSmartControl (Linux) — SMART Monitoring GUI

**Presentation:** Graphical Qt-based frontend for `smartctl`. Window shows a list of detected drives. Double-clicking a drive opens a detailed multi-tab view.

**Tabs/Metrics displayed:**
- **Identity tab:** Model, firmware, serial, capacity, interface (SATA/NVMe)
- **Attributes tab:** Full SMART attribute table with ID, name, flag, raw value, normalized value (current), worst value, threshold. Rows highlighted in red/yellow when values are anomalous
- **Capabilities tab:** SMART self-test capabilities, offline test settings
- **Self-tests tab:** Run short/long self-tests, view test history and results
- **Error log tab:** History of SMART error events
- **Temperature tab:** Current temperature and history graph

**Alerting:**
- Visual highlighting of anomalous attributes (color-coded rows)
- No background daemon; requires manual launch

**Historical tracking:**
- Error log and self-test history shown within the tool
- Temperature history graph within a session

**Pricing:** Free, open-source. Available on most Linux distributions via package manager.

**Unique features:**
- Supports SATA, PATA, NVMe drives, and drives behind some USB bridges and RAID controllers
- Can read smartctl output from saved files (virtual drive for offline analysis)
- Tooltip explanations for attribute names built into UI
- Automatic anomaly detection and row highlighting

---

### 2.6 smartmontools / smartctl (Linux/macOS/Windows CLI)

**Presentation:** Command-line tool. The underlying engine that most GUI tools wrap.

**Key commands:**
- `smartctl -a /dev/sdX` — Full SMART data dump (all attributes, health, errors, self-test log)
- `smartctl -H /dev/sdX` — Health status only (PASSED / FAILED)
- `smartctl -t short /dev/sdX` — Run short self-test
- `smartctl -t long /dev/sdX` — Run extended self-test
- `smartctl -x /dev/sdX` — Extended info (includes NVMe equivalent)
- `smartd` daemon — Background monitoring with configurable alerts via email or syslog

**ATA/SATA SMART attributes exposed:**
| ID | Name | Significance |
|----|------|-------------|
| 1  | Raw Read Error Rate | HDD read error rate |
| 5  | Reallocated Sectors Count | Critical: bad sectors replaced |
| 7  | Seek Error Rate | HDD head seek errors |
| 9  | Power On Hours | Total operational time |
| 10 | Spin Retry Count | HDD spindle issues |
| 12 | Power Cycle Count | On/off cycles |
| 177 | Wear Leveling Count (SSD) | Average erase cycles |
| 190 | Airflow Temperature | Drive temperature |
| 194 | Temperature | Drive temperature |
| 196 | Reallocation Event Count | Times reallocation occurred |
| 197 | Current Pending Sectors | Sectors awaiting reallocation |
| 198 | Uncorrectable Sector Count | Sectors that couldn't be read or reallocated |
| 199 | UDMA CRC Error Count | Interface/cable errors |
| 233 | Media Wearout Indicator (Intel SSD) | NAND erase cycle percentage |
| 241 | Total LBAs Written | Total data written (SSD endurance) |
| 242 | Total LBAs Read | Total data read |

**NVMe health log fields (via `smartctl -x` or `nvme smart-log`):**
| Field | Description |
|-------|-------------|
| Critical Warning | Bit flags for: spare capacity low, temp out of range, reliability degraded, media read-only, volatile backup failed |
| Temperature | Drive temperature (°C) |
| Available Spare | % of spare NVM capacity remaining |
| Available Spare Threshold | Alert threshold for spare capacity |
| Percentage Used | Estimated % of drive endurance consumed (0–100+%) |
| Data Units Read | Total 512-byte units read (in thousands) |
| Data Units Written | Total 512-byte units written (in thousands) |
| Host Read Commands | Count of read commands issued |
| Host Write Commands | Count of write commands issued |
| Controller Busy Time | Minutes controller was busy |
| Power Cycles | Number of power on/off cycles |
| Power On Hours | Total hours powered on |
| Unsafe Shutdowns | Power loss events without clean shutdown |
| Media and Data Integrity Errors | Uncorrectable ECC, CRC failures, LBA mismatches |
| Error Information Log Entries | Count of error log entries |

**Alerting:**
- `smartd` daemon supports email alerts on SMART failures, attribute threshold crossings, temperature limits
- Can run self-tests on a schedule (daily/weekly short tests, monthly long tests)

**Historical tracking:** Error log and self-test log stored on drive itself (limited entries).

**Pricing:** Free, open-source.

**Unique features:**
- The ground truth for all SMART tools — all other tools ultimately read the same data
- Available on macOS via Homebrew (`brew install smartmontools`)
- NVMe support via `nvme-cli` (complementary tool)
- Drive database (`drivedb.h`) maps vendor-specific attribute IDs to human-readable names

---

### 2.7 CrystalDiskInfo (Windows — Reference)

**Presentation:** Desktop application with a per-drive tab layout. Clean, simple UI with a prominent overall health status indicator.

**Metrics displayed:**
- **Overall health status:** "Good" (blue), "Caution" (yellow), "Bad" (red) — prominently displayed
- Drive temperature (real-time, with customizable alert threshold)
- Power On Hours
- Power Cycle Count
- Full SMART attribute table: ID, current value, worst value, threshold, raw value — for all 40+ reported attributes
- Drive model, firmware, serial, interface, transfer mode, buffer size, rotation rate (HDD)
- NVMe support: percentage used, available spare, data units read/written, unsafe shutdowns, media errors

**What made it the gold standard:**
1. **Clarity of health summary** — one clear color-coded verdict at top that non-technical users can understand
2. **Free and open-source** — no cost barrier, freely redistributable
3. **Comprehensive attribute display** — shows all attributes, not just "important" ones, with raw values
4. **Accurate SMART parsing** — NIST-compliant parsing without overflow bugs that plague some tools
5. **Long track record** — consistently updated since 2008, trusted by r/DataHoarder and professionals
6. **Caution state** — identifies drives that are degrading but not yet failed (proactive warning)
7. **Theme support, portable version, notification system** — resident monitoring with tray alerts
8. **External drive support** — works with many USB enclosures that macOS Disk Utility ignores

**Alerting:**
- Background monitoring with system tray presence
- Alert when temperature exceeds threshold
- Alert when health status changes to Caution or Bad
- Desktop notification popups

**Historical tracking:** Temperature history graph (session-based). No long-term SMART attribute history.

**Pricing:** Free (open-source).

---

### 2.8 macOS Disk Utility — Built-in SMART Info

**Presentation:** Built-in macOS application. SMART status shown in the drive info panel (right-click drive → Get Info, or select drive and view info pane).

**Metrics displayed:**
- S.M.A.R.T. Status: "Verified" or "Failing" — a single word
- No individual SMART attributes
- No health percentage
- No temperature
- No cycle equivalent for drives

**Critical limitations:**
- Only shows Verified or Failing — binary result with no nuance
- No SMART data for external drives (USB, Thunderbolt) — shows "Not Supported"
- No historical tracking
- Cannot run SMART self-tests
- First Run/Last Run Test data not exposed

**Alerting:** None beyond the passive status display.

**Pricing:** Free (bundled with macOS).

**Unique features:** First Repair (First Aid) tool for filesystem corruption — not a SMART health tool.

---

## Part 3: API Reference

---

### 3.1 macOS IOKit Battery APIs

**Primary API: IOPowerSources (IOKit)**

Access via `IOPSCopyPowerSourcesInfo()` and `IOPSCopyPowerSourcesList()`.

Keys defined in `<IOKit/ps/IOPSKeys.h>`:

| Key | Description |
|-----|-------------|
| `kIOPSCurrentCapacityKey` | Current charge (percentage 0–100, or mAh depending on context) |
| `kIOPSMaxCapacityKey` | Max/full charge capacity (percentage units, usually 100 for Apple batteries) |
| `kIOPSNameKey` | Power source name |
| `kIOPSIsChargingKey` | Boolean: currently charging |
| `kIOPSIsFinishingChargeKey` | Boolean: charge finishing |
| `kIOPSIsPresentKey` | Boolean: power source present |
| `kIOPSPowerSourceStateKey` | "Battery Power" or "AC Power" |
| `kIOPSTimeToEmptyKey` | Minutes to empty (-1 if unknown) |
| `kIOPSTimeToFullChargeKey` | Minutes to full charge |
| `kIOPSBatteryHealthKey` | "Good", "Fair", "Poor" |
| `kIOPSBatteryHealthConditionKey` | Detailed condition string |
| `kIOPSInternalBatteryType` | Battery type identifier |

**Advanced API: AppleSmartBattery IORegistry**

Access via `IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("AppleSmartBattery"))`.

Command: `ioreg -brc AppleSmartBattery -w 0`

Key properties available:

| Property Key | Description |
|-------------|-------------|
| `CycleCount` | Number of full charge cycles |
| `DesignCapacity` | Factory design capacity (mAh) |
| `MaxCapacity` / `AppleRawMaxCapacity` | Current full charge capacity (mAh) |
| `CurrentCapacity` | Current charge (mAh) |
| `Temperature` | Battery temperature (tenths of °C, divide by 10) |
| `Voltage` | Current voltage (mV) |
| `Amperage` / `InstantAmperage` | Current flow (mA, negative = discharging) |
| `IsCharging` | Boolean |
| `FullyCharged` | Boolean |
| `ExternalConnected` | Boolean: charger plugged in |
| `ExternalChargeCapable` | Boolean |
| `TimeRemaining` | Minutes remaining |
| `DesignCycleCount9C` | Design cycle count (max rated cycles) |
| `ManufactureDate` | Battery manufacture date (IANA packed format) |
| `BatteryInstalled` | Boolean |
| `PermanentFailureStatus` | Bitmask of permanent failure conditions |
| `VirtualTemperature` | Apple Silicon virtual temperature |
| `BatteryData` | Dictionary with nested fields including LifetimeData |

**Notes on Apple Silicon (M-series):**
- Some keys (e.g., `AppleRawMaxCapacity`, certain amperage fields) may be absent or have different names on M-series Macs
- The `BatteryData` sub-dictionary contains `CycleCount`, `Voltage`, `DesignCapacity`, `Serial`, `ManufactureDate`, and `LifetimeData`

---

### 3.2 Linux /sys/class/power_supply/ Attributes

Path: `/sys/class/power_supply/BAT0/` (or BAT1, etc.)

All values in µV, µA, µAh, µWh, seconds, or tenths of °C unless noted.

| File | Description | Units |
|------|-------------|-------|
| `status` | Charging / Discharging / Full / Not charging | String |
| `health` | Good / Overheat / Dead / Over voltage / Unspec failure | String |
| `technology` | Li-ion / NiMH / Li-poly / etc. | String |
| `present` | 1 if battery present | Boolean |
| `capacity` | Current charge percentage (0–100) | % |
| `capacity_level` | Normal / Low / Critical | String |
| `voltage_now` | Current voltage | µV |
| `voltage_min_design` | Minimum design voltage | µV |
| `current_now` | Instantaneous current (negative = discharging) | µA |
| `charge_now` | Current charge | µAh |
| `charge_full` | Current maximum charge (degrades over time) | µAh |
| `charge_full_design` | Factory design charge capacity | µAh |
| `energy_now` | Current energy | µWh |
| `energy_full` | Current maximum energy | µWh |
| `energy_full_design` | Factory design energy capacity | µWh |
| `power_now` | Instantaneous power draw | µW |
| `time_to_empty_now` | Seconds until empty | Seconds |
| `time_to_full_now` | Seconds until full | Seconds |
| `cycle_count` | Charge cycle count (hardware support varies) | Count |
| `temp` | Battery temperature | Tenths of °C |
| `manufacturer` | Battery manufacturer name | String |
| `model_name` | Battery model | String |
| `serial_number` | Battery serial number | String |

**Derived metrics:**
- Health %: `(charge_full / charge_full_design) × 100` or `(energy_full / energy_full_design) × 100`
- Power draw (W): `power_now / 1,000,000` (convert from µW)
- Temperature (°C): `temp / 10`

**Notes:**
- Not all attributes are available on all hardware — drivers expose what the hardware supports
- `cycle_count` is notably absent on many laptops; best supported on ThinkPads and some newer hardware
- `upower` abstracts these values and provides them via D-Bus (used by GNOME Power Statistics)

---

## Part 4: SMART Attributes — Key Reference

---

### 4.1 Most Important SMART Attributes for HDDs

**Critical (any nonzero value is cause for concern):**

| ID | Name | Why Important |
|----|------|---------------|
| 5  | Reallocated Sectors Count | Bad sectors permanently remapped. Even 1 sector → 20–60× higher failure probability in next 60 days |
| 197 | Current Pending Sectors | Sectors that failed to read, awaiting reallocation. Growing count = imminent failure |
| 198 | Uncorrectable Sector Count | Sectors that couldn't be read or reallocated. Even 1 → 39× higher failure probability |
| 196 | Reallocation Event Count | Times a sector reallocation was attempted (successful or not) |

**Monitor but not immediately critical:**

| ID | Name | Why Important |
|----|------|---------------|
| 1  | Raw Read Error Rate | High rate of read errors |
| 7  | Seek Error Rate | Mechanical head positioning failures |
| 10 | Spin Retry Count | Drive couldn't spin up on first attempt |
| 9  | Power On Hours | Age of drive (combined with other attributes for context) |
| 12 | Power Cycle Count | On/off cycle count |
| 194/190 | Temperature | Extreme temps shorten HDD lifespan; but temperature alone is weakly predictive |
| 199 | UDMA CRC Error Count | Cable/interface errors, not drive failure itself |

**Research note (Backblaze data):** Temperature showed little correlation to failure. Reallocated sectors, pending sectors, and uncorrectable sectors were the strongest predictors.

---

### 4.2 Most Important SMART Attributes for SATA SSDs

**SSD-specific endurance attributes (vendor varies):**

| ID | Name | Description |
|----|------|-------------|
| 177 | Wear Leveling Count | Average P/E cycles of all blocks. Starts high (e.g., 100%), decreases as drive wears. When it reaches threshold → replace |
| 173 | SSD Wear Leveling (worst block) | Maximum erase count on single block |
| 233 | Media Wearout Indicator (Intel) | NAND erase cycle percentage remaining. Starts at 100, decreases to 1 |
| 241 | Total LBAs Written | Total data written. Compare to TBW rating to estimate remaining life |
| 242 | Total LBAs Read | Total data read |
| 232 | Available Reserved Space | Remaining spare/reserved block capacity |
| 9   | Power On Hours | Operational time |
| 12  | Power Cycle Count | On/off cycles (excessive can indicate instability) |
| 194/190 | Temperature | SSDs degrade faster at high temps; data retention affected |
| 5   | Reallocated NAND Blocks | Bad blocks remapped to spare area |
| 187 | Reported Uncorrectable Errors | Errors ECC could not fix |
| 199 | UDMA CRC Error Count | Interface errors |

**SSD Health % calculation:** Many tools compute: `current Wear Leveling value / initial value × 100` or use `Percentage Used` equivalent.

---

### 4.3 Most Important NVMe SMART Fields

The NVMe specification standardizes the health log page (Log ID 02h), so these are consistent across vendors:

| Field | Critical Level | Description |
|-------|---------------|-------------|
| **Percentage Used** | Critical >100% | Estimated % of endurance consumed. Can exceed 100%. Drive still may function but endurance exhausted. >80% = caution |
| **Available Spare** | Critical when below threshold | % of spare NVM capacity. Must stay above Available Spare Threshold. When it drops below → hardware alert |
| **Available Spare Threshold** | Reference | Manufacturer-set minimum spare capacity level |
| **Media and Data Integrity Errors** | Critical if nonzero | Uncorrectable ECC errors, CRC failures, LBA mismatches |
| **Unsafe Shutdowns** | Monitor | Power loss events without clean shutdown. High count stresses wear-leveling tables |
| **Power On Hours** | Informational | Operational age |
| **Temperature** | Warning >70°C | Drive temp; affects data retention and longevity |
| **Critical Warning** | Immediate action | Bit flags: spare below threshold, temp exceeded, reliability degraded, read-only mode, volatile memory backup failed |
| **Data Units Written** | Endurance tracking | Total 512-byte units written (×1000). Compare to TBW rating |
| **Data Units Read** | Informational | Total data read |
| **Error Information Log Entries** | Monitor | Running count of error log entries |
| **Controller Busy Time** | Informational | Minutes controller was processing commands |

**Key NVMe health indicators summary:**
1. `Percentage Used` > 80% → Caution, > 100% → Replace
2. `Available Spare` approaching `Available Spare Threshold` → Replace soon
3. Any nonzero `Media and Data Integrity Errors` → Investigate immediately
4. `Critical Warning` any bit set → Immediate attention required

---

## Part 5: Feature Matrix Summary

### Battery Health Feature Comparison

| Feature | Sensei | iStat Menus | coconutBattery | CleanMyMac X | TLP | GNOME Power Stats |
|---------|--------|-------------|----------------|--------------|-----|-------------------|
| Cycle count | Yes | Yes | Yes | Yes | Yes | No |
| Max capacity (mAh) | Yes | No | Yes | No | Yes (Wh) | No |
| Design capacity | Yes | No | Yes | Yes (%) | Yes (Wh) | No |
| Health % | Yes | Yes | Yes | Yes | Derived | Yes (graph) |
| Temperature | Yes | No | Yes | Yes (hourly) | Via sysfs | Yes |
| Voltage | No | No | Yes (Plus) | No | Yes | Yes |
| Amperage/Current | No | No | Yes (Plus) | No | Yes | No |
| Wattage/Power rate | No | No | Yes (Plus) | No | Yes | Discharge rate |
| Time remaining | Yes | Yes | No | Yes | Yes | Yes |
| Power source status | Yes | Yes | Yes | Yes | Yes | Yes |
| Condition status | Yes | Yes | Yes | Yes | No | No |
| Bluetooth device levels | No | Yes | No | No | No | Yes (via UPower) |
| iOS/iPadOS battery | No | No | Yes (Plus) | No | No | No |
| Alerting | Basic | Configurable | Yes (Plus) | Temp/critical | None | System only |
| Historical tracking | No | No | Yes (Plus) | No | No | Yes (chart) |
| Export | No | No | CSV (Plus) | No | No | No |
| Background monitoring | Menu bar | Menu bar | Plus only | Yes | N/A | Yes |
| Price | $29 | $12 | Free/$9.95 | ~$40/yr | Free | Free |

### Disk Health Feature Comparison

| Feature | Sensei | iStat Menus | DriveDx | OnyX | GSmartControl | smartctl | CrystalDiskInfo | Disk Utility |
|---------|--------|-------------|---------|------|----------------|----------|-----------------|--------------|
| SMART status (pass/fail) | Yes | Yes | Yes | Yes (basic) | Yes | Yes | Yes | Yes |
| SMART attribute table | Basic | No | Full | No | Full | Full | Full | No |
| Health % score | Yes | No | Yes (0–100%) | No | Highlighted | No | Color status | No |
| SSD lifetime left | Yes | No | Yes | No | Via attributes | Via attributes | Via attributes | No |
| Drive temperature | No | No | Yes | No | Yes | Yes | Yes | No |
| Self-test execution | No | No | Yes | No | Yes | Yes | No | No |
| Error log | No | No | Yes | No | Yes | Yes | No | No |
| Per-attribute health rating | No | No | Yes | No | Color only | No | Color only | No |
| HDD-specific algorithms | No | No | Yes | No | No | No | No | No |
| SSD-specific algorithms | No | No | Yes | No | No | No | No | No |
| Alerting/notifications | Basic | Basic | Multi-tier + email | No | Visual only | Email (smartd) | Tray alerts | No |
| Historical attribute tracking | No | No | Yes (trend) | No | Session | Error log | Session temp | No |
| Background monitoring | No | Yes | Yes | No | No | Yes (smartd) | Yes | No |
| External drive support | Limited | Limited | Yes | Limited | Yes | Yes | Yes | No |
| NVMe support | Limited | No | Yes | No | Yes | Yes | Yes | No |
| Disk benchmark | Yes | No | No | No | No | No | No | Yes (First Aid) |
| Price | $29 | $12 | $20 | Free | Free | Free | Free | Free |

---

## Part 6: Key Design Recommendations for Nexis

Based on this research, a best-in-class implementation for Nexis should include:

### Battery Health Page:
1. **Headline metrics (large, prominent):** Health %, Cycle Count, Condition Status
2. **Capacity card:** Current mAh / Max mAh / Design mAh with visual bar
3. **Live metrics:** Voltage (V), Current/Amperage (mA, with charge/discharge indication), Power Rate (W), Temperature (°C)
4. **Status row:** Power source (AC/Battery), Is Charging, Time Remaining
5. **Device info:** Manufacturer, Model, Manufacture Date, Design Cycle Count (rated max)
6. **Historical chart:** Health % over time (requires persistent recording)
7. **Alerting:** Warn when health drops below threshold (e.g., <80%), warn when temperature is high, warn when condition changes

**macOS data sources:** `AppleSmartBattery` IORegistry keys
**Linux data sources:** `/sys/class/power_supply/BAT0/` sysfs files

### Disk Health Page:
1. **Health score:** Translated percentage (like DriveDx's 0–100%)
2. **Status summary:** Good / Caution / Failing with color coding (like CrystalDiskInfo's blue/yellow/red)
3. **Key metrics row:** Temperature, Power On Hours, Power Cycles
4. **NVMe-specific section:** Percentage Used, Available Spare vs Threshold, Unsafe Shutdowns, Media Errors
5. **SATA SSD-specific section:** Wear Leveling Count, Total Data Written vs TBW rating, Reallocated Blocks
6. **HDD-specific section:** Reallocated Sectors (ID 5), Pending Sectors (ID 197), Uncorrectable Sectors (ID 198)
7. **Full attribute table:** All SMART attributes with ID, name, raw value, normalized value, threshold, status
8. **Self-test launcher:** Short and long self-test with progress
9. **Alerting:** Warn on any nonzero critical attributes, warn when health score drops, warn on temperature

**macOS data sources:** `smartctl` (via `brew install smartmontools`) or `diskutil`, IOKit
**Linux data sources:** `smartctl` / `libatasmart` / NVMe CLI

---
