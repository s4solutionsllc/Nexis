# FR-12 & FR-28 Combined Implementation Plan

## Overview

Implement FR-28 (Dashboard fullscreen/kiosk mode) first, then FR-12 (Hardware Info tab). FR-28 is smaller in scope and establishes patterns (settings keys, keyboard shortcuts) that FR-12 will reuse. Task A6 removes the Dashboard's system info panel (which FR-12 replaces) and reorganises the remaining widgets.

---

## Phase A: FR-28 — Dashboard Fullscreen / Kiosk Mode

### Task A1: Add kiosk mode and device-selection settings infrastructure

- [x] In `setting_manager.h`: Add `SettingKeys::KioskMode`, `SettingKeys::TempSensorId`, `SettingKeys::GpuDeviceId` key constants.
- [x] In `setting_manager.h`: Add method declarations: `setKioskMode(bool)`, `getKioskMode()`, `setTempSensorId(const QString &)`, `getTempSensorId()`, `setGpuDeviceId(const QString &)`, `getGpuDeviceId()`.
- [x] In `setting_manager.cpp`: Implement getter/setter pairs. KioskMode defaults to `false`. TempSensorId and GpuDeviceId default to empty string.

### Task A2: Implement kiosk mode toggle in App

- [x] In `app.h`: Add `bool mKioskMode` member, `void toggleKioskMode()` public slot, `void applyKioskMode(bool enable)` private method.
- [x] In `app.cpp` `init()`: Create `QAction` with `Qt::Key_F11` shortcut, connect to `toggleKioskMode()`.
- [x] In `app.cpp` `init()`: After page setup, check `SettingManager::ins()->getKioskMode()` — if true, call `applyKioskMode(true)`.
- [x] In `app.cpp`: Implement `toggleKioskMode()` — flip `mKioskMode`, save to SettingManager, call `applyKioskMode()`.
- [x] In `app.cpp`: Implement `applyKioskMode(bool enable)`:
  - If enable: `ui->sidebar->hide()`, `ui->pageTitle->hide()`, `showFullScreen()`, force switch to dashboard page via `pageClick(dashboardPage, false)`.
  - If disable: `showNormal()`, `ui->sidebar->show()`, `ui->pageTitle->show()`.

### Task A3: Persist temperature sensor and GPU device selection

- [x] In `dashboard_page.cpp` `init()`: After populating `cmbTempSensor`, read `SettingManager::ins()->getTempSensorId()`. Find matching sensor by id and set combo index. Fall back to index 0 if not found.
- [x] In `dashboard_page.cpp`: When temperature sensor combo changes, save the selected sensor's id via `SettingManager::ins()->setTempSensorId(...)`.
- [x] In `dashboard_page.cpp` `init()`: After populating `cmbGpuDevice`, read `SettingManager::ins()->getGpuDeviceId()`. Find matching GPU by name and set combo index. Fall back to index 0 if not found.
- [x] In `dashboard_page.cpp`: When GPU device combo changes, save the selected GPU's name via `SettingManager::ins()->setGpuDeviceId(...)`.

### Task A4: Add ESC key to exit kiosk mode

- [x] In `app.cpp` `init()`: Create a second `QAction` with `Qt::Key_Escape` connected to a slot that exits kiosk mode only (no toggle). This provides a safe "exit" for users who may not know F11.

### Task A5: Build and verify FR-28

- [x] Incremental build.
- [x] Mark FR-28 as `[x]` in `FEATURE_REQUESTS.md` with resolution note.

### Task A6: Remove system info panel from Dashboard and reorganise layout

- [x] In `dashboard_page.ui`: Remove the `systemInfo` widget (contains `lblSystemInfoTitle` and `listViewSystemInfo`) from row 1, column 0 of the grid layout.
- [x] In `dashboard_page.ui`: Reorganise row 1 so `tempContainer`, `gpuContainer`, and `lineBars` (network download/upload) expand evenly across the freed space. Adjust column spans so the three modules share the full width.
- [x] In `dashboard_page.cpp`: Remove `systemInformationInit()` method and its call from the constructor, plus remove any related includes or variables.
- [x] In `dashboard_page.h`: Remove `systemInformationInit()` declaration and any related members.
- [x] Incremental build to verify.

---

## Phase B: FR-12 — Hardware Info Tab

### Task B1: Create HardwareInfoPage skeleton

- [x] Create directory `shared/nexis/Pages/HardwareInfo/`.
- [x] Create `hardware_info_page.h`: QWidget subclass with Q_OBJECT, constructor, destructor, `init()` slot, `InfoManager *im` member.
- [x] Create `hardware_info_page.cpp`: Constructor calls `setupUi(this)` + `init()`, sets `windowTitle(tr("Hardware Info"))`.
- [x] Create `hardware_info_page.ui`: Scrollable layout with grouped sections (see Task B3).
- [x] In `CMakeLists.txt`: Add `"${GUI_SHARED_DIR}/Pages/HardwareInfo"` to `CMAKE_AUTOUIC_SEARCH_PATHS` and `target_include_directories`.

### Task B2: Register page in App (sidebar position: between Dashboard and Startup Apps)

- [x] In `app.h`: Add `HardwareInfoPage *hardwareInfoPage` member, `void on_btnHardwareInfo_clicked()` slot.
- [x] In `app.ui`: Add `btnHardwareInfo` QPushButton to sidebar **between `btnDash` (Dashboard) and `btnStartupApps` (Startup Apps)**.
- [x] In `app.cpp` `init()`: Instantiate `HardwareInfoPage`, insert into `mListPages` and `mListSidebarButtons` at index 1 (after Dashboard, before Startup Apps).
- [x] In `app.cpp`: Implement `on_btnHardwareInfo_clicked()` → `pageClick(hardwareInfoPage)`.
- [x] In `app.cpp` `updateSidebarIcons()`: Add icon for `btnHardwareInfo` using a bundled SVG.

### Task B3: Design the Hardware Info UI layout

- [x] In `hardware_info_page.ui`: Create a `QScrollArea` with vertical layout containing QGroupBox sections:
  - **System**: Hostname, OS/Distro, Kernel, Architecture, Desktop Environment
  - **Processor**: Model, Physical/Logical Cores, Base Clock, Cache Sizes (L1/L2/L3 via sysctl on macOS, sysfs on Linux)
  - **Graphics**: For each GPU — Name, Vendor
  - **Memory**: Total RAM, Swap Total
  - **Storage**: QTableWidget — Device, Mount Point, Filesystem, Total, Used, Free
  - **Network**: QTableWidget — Interface, MAC Address, IPv4, IPv6
  - **Thermal**: QTableWidget — Sensor, Temperature, Max, Critical

### Task B4: Populate hardware info from existing InfoManager data

- [x] In `hardware_info_page.cpp` `init()`: Read from `InfoManager::ins()` and populate all UI labels/tables using data already collected by the Info classes.
- [x] For network: use `QNetworkInterface::allInterfaces()` to get MAC, IP addresses.
- [x] For thermal: iterate `ThermalInfo::getSensors()` and `getTemperature()`.
- [x] For storage: iterate `DiskInfo::getDisks()`.

### Task B5: Add extended system info collection (platform-specific)

- [x] Desktop environment: inline in `populateSystem()` via `$XDG_CURRENT_DESKTOP` on Linux, "Aqua" on macOS.
- [x] CPU cache sizes: inline in `populateProcessor()` via sysctl on macOS, sysfs `/sys/devices/system/cpu/cpu0/cache/` on Linux.
- [x] Architecture: via `QSysInfo::currentCpuArchitecture()`.

### Task B6: Create sidebar icon (position: between Dashboard and Startup Apps)

- [x] Create `hardware-info.svg` in both `shared/nexis/static/themes/default/img/sidebar-icons/` and `shared/nexis/static/themes/light/img/sidebar-icons/`. CPU chip motif matching existing icon style.
- [x] Register in `static.qrc`.

### Task B7: Build and verify FR-12

- [x] Incremental build successful.
- [x] Mark FR-12 as `[x]` in `FEATURE_REQUESTS.md` with resolution note.

---

## Phase C: Commit and Push

- [x] Commit FR-28 + FR-12 changes.
- [x] Push.
