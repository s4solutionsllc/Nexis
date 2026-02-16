# FR-12 & FR-28 Combined Implementation Plan

## Overview

Implement FR-28 (Dashboard fullscreen/kiosk mode) first, then FR-12 (Hardware Info tab). FR-28 is smaller in scope and establishes patterns (settings keys, keyboard shortcuts) that FR-12 will reuse.

---

## Phase A: FR-28 — Dashboard Fullscreen / Kiosk Mode

### Task A1: Add kiosk mode settings infrastructure

- [ ] In `setting_manager.h`: Add `SettingKeys::KioskMode`, `SettingKeys::TempSensorId` key constants.
- [ ] In `setting_manager.h`: Add method declarations: `setKioskMode(bool)`, `getKioskMode()`, `setTempSensorId(const QString &)`, `getTempSensorId()`.
- [ ] In `setting_manager.cpp`: Implement getter/setter pairs. KioskMode defaults to `false`. TempSensorId defaults to empty string.

### Task A2: Implement kiosk mode toggle in App

- [ ] In `app.h`: Add `bool mKioskMode` member, `void toggleKioskMode()` public slot, `void applyKioskMode(bool enable)` private method.
- [ ] In `app.cpp` `init()`: Create `QAction` with `Qt::Key_F11` shortcut, connect to `toggleKioskMode()`.
- [ ] In `app.cpp` `init()`: After page setup, check `SettingManager::ins()->getKioskMode()` — if true, call `applyKioskMode(true)`.
- [ ] In `app.cpp`: Implement `toggleKioskMode()` — flip `mKioskMode`, save to SettingManager, call `applyKioskMode()`.
- [ ] In `app.cpp`: Implement `applyKioskMode(bool enable)`:
  - If enable: `ui->sidebar->hide()`, `ui->pageTitle->hide()`, `showFullScreen()`, force switch to dashboard page via `pageClick(dashboardPage, false)`.
  - If disable: `showNormal()`, `ui->sidebar->show()`, `ui->pageTitle->show()`.

### Task A3: Persist temperature sensor selection

- [ ] In `dashboard_page.cpp` `init()`: After populating `cmbTempSensor`, read `SettingManager::ins()->getTempSensorId()`. Find matching sensor by id and set combo index. Fall back to index 0 if not found.
- [ ] In `dashboard_page.cpp` `onTempSensorChanged()`: Save the selected sensor's id via `SettingManager::ins()->setTempSensorId(...)`.

### Task A4: Add ESC key to exit kiosk mode

- [ ] In `app.cpp` `init()`: Create a second `QAction` with `Qt::Key_Escape` connected to a slot that exits kiosk mode only (no toggle). This provides a safe "exit" for users who may not know F11.

### Task A5: Build and verify FR-28

- [ ] Incremental build.
- [ ] Mark FR-28 as `[x]` in `FEATURE_REQUESTS.md` with resolution note.

---

## Phase B: FR-12 — Hardware Info Tab

### Task B1: Create HardwareInfoPage skeleton

- [ ] Create directory `shared/nexis/Pages/HardwareInfo/`.
- [ ] Create `hardware_info_page.h`: QWidget subclass with Q_OBJECT, constructor, destructor, `init()` slot, `InfoManager *im` member.
- [ ] Create `hardware_info_page.cpp`: Constructor calls `setupUi(this)` + `init()`, sets `windowTitle(tr("Hardware Info"))`.
- [ ] Create `hardware_info_page.ui`: Scrollable layout with grouped sections (see Task B3).
- [ ] In `CMakeLists.txt`: Add `"${GUI_SHARED_DIR}/Pages/HardwareInfo"` to `CMAKE_AUTOUIC_SEARCH_PATHS` and `target_include_directories`.

### Task B2: Register page in App

- [ ] In `app.h`: Add `HardwareInfoPage *hardwareInfoPage` member, `void on_btnHardwareInfo_clicked()` slot.
- [ ] In `app.ui`: Add `btnHardwareInfo` QPushButton to sidebar (between Resources and Helpers, or after Resources).
- [ ] In `app.cpp` `init()`: Instantiate `HardwareInfoPage`, insert into `mListPages` and `mListSidebarButtons` at appropriate index.
- [ ] In `app.cpp`: Implement `on_btnHardwareInfo_clicked()` → `pageClick(hardwareInfoPage)`.
- [ ] In `app.cpp` `updateSidebarIcons()`: Add icon for `btnHardwareInfo` using a bundled SVG.

### Task B3: Design the Hardware Info UI layout

- [ ] In `hardware_info_page.ui`: Create a `QScrollArea` with vertical layout containing QGroupBox sections:
  - **System**: Hostname, OS/Distro, Kernel, Architecture, Desktop Environment, Uptime
  - **Processor**: Model, Physical/Logical Cores, Base Clock, Cache Sizes (if available)
  - **Graphics**: For each GPU — Name, Vendor, VRAM (if available), Driver (if available)
  - **Memory**: Total RAM, Swap Total, Type/Speed (if available)
  - **Storage**: QTableWidget — Device, Mount Point, Filesystem, Total, Used, Free
  - **Network**: QTableWidget — Interface, MAC Address, IPv4, IPv6, Speed
  - **Thermal**: QTableWidget — Sensor, Temperature, Max, Critical

### Task B4: Populate hardware info from existing InfoManager data

- [ ] In `hardware_info_page.cpp` `init()`: Read from `InfoManager::ins()` and populate all UI labels/tables using data already collected by the Info classes.
- [ ] For network: use `QNetworkInterface::allInterfaces()` to get MAC, IP addresses (data available but not yet exposed through NetworkInfo).
- [ ] For thermal: iterate `ThermalInfo::getSensors()` and `getTemperature()`.
- [ ] For storage: iterate `DiskInfo::getDisks()`.

### Task B5: Add extended system info collection (platform-specific)

- [ ] In `system_info.h`: Add method declarations for `getUptime()`, `getDesktopEnvironment()`.
- [ ] In `linux/nexis-core/Info/system_info.cpp`: Implement `getUptime()` (read `/proc/uptime`), `getDesktopEnvironment()` (read `$XDG_CURRENT_DESKTOP`).
- [ ] In `macos/nexis-core/Info/system_info.cpp`: Implement `getUptime()` (sysctl `kern.boottime`), `getDesktopEnvironment()` (return "Aqua").
- [ ] In `cpu_info.h`: Add `getCacheSizes()` method returning L1/L2/L3 sizes.
- [ ] In `linux/nexis-core/Info/cpu_info.cpp`: Implement via sysfs `/sys/devices/system/cpu/cpu0/cache/`.
- [ ] In `macos/nexis-core/Info/cpu_info.cpp`: Implement via `sysctl hw.l1dcachesize`, `hw.l2cachesize`, `hw.l3cachesize`.

### Task B6: Create sidebar icon

- [ ] Create `hardware-info.svg` in both `shared/nexis/static/themes/default/img/sidebar-icons/` and `shared/nexis/static/themes/light/img/sidebar-icons/`. Use 20×20 monochrome style matching existing icons (circuit board or CPU chip motif).
- [ ] Register in `static.qrc`.

### Task B7: Build and verify FR-12

- [ ] Clean rebuild.
- [ ] Mark FR-12 as `[x]` in `FEATURE_REQUESTS.md` with resolution note.

---

## Phase C: Commit and Push

- [ ] Commit FR-28 changes.
- [ ] Commit FR-12 changes (separate commit).
- [ ] Push both.
