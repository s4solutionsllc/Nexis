# FR-12 & FR-28 Combined Research

## FR-12: Hardware Info Tab | FR-28: Dashboard Fullscreen/Kiosk Mode

---

## Part 1: FR-28 — Dashboard Fullscreen / Kiosk Mode

### Main Window Architecture (`shared/nexis/app.{h,cpp,ui}`)

**Widget hierarchy:**
```
QMainWindow (App)
└── centralwidget (QWidget)
    └── horizontalLayout (QHBoxLayout, 0 margins, 0 spacing)
        ├── sidebar (QWidget, fixed 220px wide)
        │   └── sidebarLayout (QVBoxLayout)
        │       ├── verticalSpacer_2 (40px)
        │       ├── btnDash … btnSettings (QPushButtons, checkable)
        │       ├── verticalSpacer (40px)
        │       └── btnFeedback
        └── pageContent (QWidget, expanding)
            └── pageContentLayout (QVBoxLayout)
                ├── pageTitle (QLabel)
                └── [mSlidingStacked added dynamically in init()]
```

- **Sidebar**: object name `sidebar` (app.ui line 41), fixed width 220px, has drop shadow (app.cpp line 118)
- **Page title**: object name `pageTitle` (app.ui line 518), QLabel in `pageContentLayout`
- **Title bar**: System title bar (default QMainWindow — NOT custom)
- **Default size**: 850×570px (app.ui lines 12–13)

### Dashboard Layout (`shared/nexis/Pages/Dashboard/dashboard_page.ui`)

```
DashboardPage (QWidget)
└── gridLayout (5px margins, 0 spacing)
    ├── Row 0 (min-height 200px): circleBars container (colspan=4)
    │   └── QHBoxLayout: mCpuBar | mMemBar | mDiskBar
    ├── Row 1 (expandable):
    │   ├── Col 0: systemInfo (min-width 200px)
    │   │   └── lblSystemInfoTitle + listViewSystemInfo
    │   ├── Col 1: tempContainer (mTempBar + cmbTempSensor)
    │   ├── Col 2: gpuContainer (mGpuBar + cmbGpuDevice)
    │   └── Col 3: lineBars (min-width 150px)
    │       └── mDownloadBar | mUploadBar
    └── Row 2: widgetUpdateBar (hidden by default)
```

- CircleBars expand vertically with available space (no max-height)
- System info column can be hidden; Qt gridLayout recalculates automatically
- Temperature sensor: `cmbTempSensor` populated in `init()`, selected index stored in `mSelectedSensorIndex` (NOT persisted to disk)

### Settings Persistence (`shared/nexis/Managers/setting_manager.{h,cpp}`)

- Singleton pattern: `SettingManager::ins()`
- QSettings with INI format at `QStandardPaths::AppConfigLocation/settings.ini`
- Keys defined in `SettingKeys` namespace (setting_manager.h lines 7–20)
- Pattern: add key constant, getter/setter methods
- Existing keys: ThemeName, Language, DiskName, StartPage, CPUAlertPercent, MemoryAlertPercent, DiskAlertPercent, ColorScheme, DiskAnalyzerTool, etc.

### Keyboard Shortcuts

- **No existing shortcut infrastructure** — no QShortcut, QAction, or keyPressEvent overrides found
- F11 is the standard fullscreen toggle and is not used
- Best approach: QAction added in `App::init()` for F11 toggle

### Fullscreen Mechanics

- `showFullScreen()` / `showNormal()` — QMainWindow inherited, cross-platform
- `changeEvent()` already overridden (app.cpp line 140) for minimize-to-tray, no fullscreen handling yet
- To enter kiosk: `ui->sidebar->hide()`, `ui->pageTitle->hide()`, `showFullScreen()`, force dashboard page
- To exit kiosk: `showNormal()`, `ui->sidebar->show()`, `ui->pageTitle->show()`, restore previous page

---

## Part 2: FR-12 — Hardware Info Tab

### Currently Collected Data (via Info classes → InfoManager)

| Category | Class | Data Available | Displayed? |
|----------|-------|----------------|------------|
| **CPU** | CpuInfo | Model, physical/logical cores, clock speeds, per-core usage, load averages | Dashboard (partial) |
| **GPU** | GpuInfo | Device name, vendor, utilization % | Dashboard |
| **Memory** | MemoryInfo | Total/used/free RAM, swap total/used/free | Dashboard |
| **Disk** | DiskInfo | Volumes: name, device path, FS type, size/free/used; disk I/O | Dashboard + Resources |
| **System** | SystemInfo | Hostname, kernel, distro, architecture, CPU model/speed/cores, username | Dashboard |
| **Network** | NetworkInfo | RX/TX bytes, default interface, all interfaces (via QNetworkInterface) | Dashboard |
| **Thermal** | ThermalInfo | Sensor labels, current temps, max/critical thresholds | Dashboard |

### Data NOT Yet Collected (would need new code)

| Data | Linux Source | macOS Source |
|------|-------------|-------------|
| CPU cache sizes (L1/L2/L3) | `lscpu` or sysfs `/sys/devices/system/cpu/cpu0/cache/` | `sysctl hw.l1dcachesize` etc. |
| CPU architecture flags (SSE, AVX) | `/proc/cpuinfo` flags | `sysctl machdep.cpu.features` |
| GPU VRAM | nvidia-smi / sysfs | IOKit |
| GPU driver version | nvidia-smi / modinfo | IOKit |
| RAM type/speed/slots | `dmidecode` (root) | IOKit `SPMemoryDataType` |
| Disk model/serial | `/sys/block/*/device/model` | IOKit |
| Disk SMART data | `smartctl` (root) | IOKit |
| Motherboard/BIOS | `dmidecode` (root) | `sysctl` / IOKit |
| Desktop environment | `$XDG_CURRENT_DESKTOP` | N/A (always Aqua) |
| Uptime | `/proc/uptime` | `sysctl kern.boottime` |
| IP addresses | QNetworkInterface (already available, not exposed) | Same |
| MAC addresses | QNetworkInterface (already available, not exposed) | Same |

### Page Architecture Pattern

Every page follows this structure:
```
shared/nexis/Pages/{PageName}/
├── {page_name}_page.h     — QWidget subclass with Q_OBJECT
├── {page_name}_page.cpp   — Constructor calls setupUi + init()
└── {page_name}_page.ui    — Qt Designer layout
```

**Registration in app.cpp:**
1. Instantiate page (line ~40–49)
2. Add to `mListPages` list (line ~53–61)
3. Add sidebar button to `mListSidebarButtons` (line ~53–61)
4. All pages added to `mSlidingStacked` (line ~112–114)
5. Click handler: `on_btnXxx_clicked()` → `pageClick(xxxPage)`

**CMake registration:**
- Add to `CMAKE_AUTOUIC_SEARCH_PATHS` (CMakeLists.txt line ~88)
- Add to `target_include_directories` (CMakeLists.txt line ~133)

### Sidebar Icon Requirements

Each sidebar button has an icon set in `App::updateSidebarIcons()` (app.cpp lines 290–319). Icons live in:
- `shared/nexis/static/themes/default/img/sidebar-icons/` (dark theme)
- `shared/nexis/static/themes/light/img/sidebar-icons/` (light theme)

Pattern: 20×20 monochrome SVG, same style as existing icons.

---

## Part 3: Implementation Overlap

Both features share:
1. **Settings persistence** — FR-28 needs kiosk mode + temp sensor persistence; FR-12 is display-only but benefits from same infrastructure
2. **Dashboard enhancements** — FR-28 maximizes the dashboard; FR-12 adds a new page with similar data
3. **Sidebar modifications** — FR-28 hides sidebar; FR-12 adds a new sidebar button
4. **System info consolidation** — FR-12's hardware page is a superset of the Dashboard's system info section; could share data sources

### Recommended Implementation Order
1. **FR-28 first** — Smaller scope, modifies existing UI only, no new data collection
2. **FR-12 second** — Builds on familiarity with page architecture, can reuse FR-28's settings patterns
