# Bundle A — Cold Launch Sprint: Research (FR-96, FR-97, FR-98)

Research artifact for the Cold Launch Sprint. Covers the three related
feature requests that together aim to shave ~1 s off time-to-interactive
and reduce idle RAM: defer disk-health SMART discovery, lazily construct
sidebar pages, and defer `HardwareInfoPage` population to first show.

---

## 1. Startup call chain (current state)

Nothing on the main thread runs meaningfully before `main()` creates the
`QApplication`. The relevant sequence is:

1. `main()` — `shared/nexis/main.cpp:143-343`.
   - Creates `QApplication` (`main.cpp:149`), sets style, app name,
     version (`main.cpp:149-157`).
   - Headless branches (`--clean`, `--check-threshold`) return early;
     irrelevant for cold launch of the GUI (`main.cpp:176-216`).
   - `QLockFile` acquired on the temp dir (`main.cpp:220-229`) — cheap.
   - Reads the splash pixmap, shows it, calls `app.processEvents()`
     (`main.cpp:325-330`).
   - **`App w;`** — constructs the main window (`main.cpp:332`).
   - `w.show();` after construction (`main.cpp:335`).
   - `splash->finish(&w);` (`main.cpp:338`).
   - `app.exec();` (`main.cpp:342`).

2. `App::App()` — `shared/nexis/app.cpp:42-60`.
   - Initializer list calls `AppManager::ins()->getTrayIcon()`
     (`app.cpp:49`). That triggers `AppManager` singleton construction:
     `shared/nexis/Managers/app_manager.cpp:23-47` — loads translations,
     registers the `colorSchemeChanged` hook. Cheap (all in-memory).
   - `ui->setupUi(this);` — paints the main-window chrome (`app.cpp:55`).
   - `init();` — the heavy phase (`app.cpp:59`).

3. `App::init()` — `shared/nexis/app.cpp:292-547`. Runs synchronously
   before the first frame:
   - `buildSidebar()` — allocates ~16 `QPushButton`s, the version label,
     badge labels (`app.cpp:308`, body at `app.cpp:86-290`). Pure widget
     work, no I/O.
   - **Eager page construction** (`app.cpp:310-322`): 13 pages
     instantiated back-to-back. See Section 2 for the per-page cost.
     Each `new *Page(mSlidingStacked)` calls `setupUi()` +
     `page->init()`, which in several cases dispatches I/O or
     singletons. Notable offenders:
       - `HardwareInfoPage` (`app.cpp:311`) — triggers
         `InfoManager::ins()` the first time it's touched
         (via the initializer list `im(infoManager ? infoManager :
         InfoManager::ins())` in `hardware_info_page.cpp:87`). See step 4.
       - `SystemCleanerPage`, `DiskToolsPage`, `SettingsPage` — heavy
         `setupUi()` and sub-widget work (see Section 2).
   - `APTSourceManagerPage` / `DockerPage` / `GnomeSettingsPage` added
     conditionally via `ToolManager::ins()->checkSourceRepository()`
     etc. (`app.cpp:363-419`). Each `check*()` call in `ToolManager`
     probes the filesystem for package-manager binaries
     (`shared/nexis/Managers/tool_manager.cpp`).
   - Signal wiring (button clicks, `SignalMapper` subscriptions,
     `DataRefreshService::systemUpdatesChecked`) — cheap.
   - `for (QWidget *page : mListPages) mSlidingStacked->addWidget(page);`
     (`app.cpp:466-467`) — adds each page to the stacked widget.
   - `DataRefreshService::ins()->start();` (`app.cpp:469`) — starts
     fast/medium/slow/update timers. The first fast tick runs
     synchronously via `onFastTick()` inside `start()`
     (`data_refresh_service.cpp:55-68`), which calls
     `im->getCpuPercents()`, `im->updateMemoryInfo()`,
     `im->updateNetworkBytes()`, `im->updateGpuInfo()`,
     `im->updateBatteryInfo()` — all on the main thread. The slow tick
     (`onSlowTick()`) is also fired once if
     `im->hasDiskHealth()`, **but that call is gated on drives already
     being populated**. The slow tick itself is already `QtConcurrent`
     (`data_refresh_service.cpp:233-247`), so it is **not** the cause
     of cold-launch disk-health blocking; the blocking happens
     earlier, inside `InfoManager`'s singleton constructor.
   - `AppManager::ins()->updateStylesheet();` (`app.cpp:471`) — parses
     the QSS template and sets `qApp->setStyleSheet()`. All pages that
     already exist get styled.
   - `clickSidebarButton(SettingManager::ins()->getStartPage());`
     (`app.cpp:476`) — slides to the starting page (Dashboard by
     default).

4. **`InfoManager::ins()` first-call cost — the main cold-launch pain.**
   `InfoManager::ins()` is a lazy singleton at
   `shared/nexis/Managers/info_manager.cpp:69-76`. It's first hit when
   a page constructor references `InfoManager::ins()` (typically in
   the initializer list). The order, given `App::init()` in `app.cpp`:
     - `DashboardPage` is `new`'d first (`app.cpp:310`). Its
       initializer list (`dashboard_page.cpp:30-67`) grabs
       `InfoManager::ins()` via `im(infoManager ? infoManager :
       InfoManager::ins())`. This is the **first touch**.
   The singleton's constructor at `info_manager.cpp:36-67` builds all
   the platform providers, and the expensive one is
   `dhi = std::make_unique<DiskHealthInfo*>();` — see step 5.

5. `DiskHealthInfoMacOS::DiskHealthInfoMacOS()` —
   `macos/nexis-core/Info/disk_health_info.cpp:76-80`. Calls
   `discoverDrives()` **synchronously** from the constructor:
     - `CommandUtil::isExecutable("smartctl")` (`disk_health_info.cpp:78`)
       — one `$PATH` lookup, cheap.
     - `CommandUtil::exec("diskutil", {"list", "-plist"})`
       (`disk_health_info.cpp:89`). One `QProcess` round trip on the
       main thread. Typical cost: 50-200 ms cold.
     - For each whole disk, `CommandUtil::exec("diskutil", {"info",
       "-plist", drive.devicePath})` (`disk_health_info.cpp:104`). N
       more `QProcess` round trips. 50-150 ms each.
     - For each non-Apple-Fabric drive with `smartctl` present,
       `CommandUtil::execWithStatus("smartctl", {"-j", "-a", path})`
       (`disk_health_info.cpp:172`). This is the worst offender —
       smartctl spins the drive if it's sleeping; 100-1500 ms per
       drive with external/USB/spinning disks.

6. `DiskHealthInfoLinux::DiskHealthInfoLinux()` —
   `linux/nexis-core/Info/disk_health_info.cpp:8-12`. Same pattern:
     - `CommandUtil::isExecutable("smartctl")` (line 10).
     - `discoverDrives()` (line 11).
   Linux `discoverDrives()`:
     - `QDir blocks("/sys/block"); blocks.entryList(...)` — sysfs
       directory scan (`linux/.../disk_health_info.cpp:18-22`). Cheap.
     - For each block device: `QFile::open` reads of
       `/sys/block/{name}/device`, `model`, `size`,
       `queue/rotational` via `FileUtil::readStringFromFile`
       (`linux/.../disk_health_info.cpp:28-54`). Sub-millisecond each.
     - `CommandUtil::execWithStatus("smartctl", {"-j", "-a", path})`
       (`linux/.../disk_health_info.cpp:69`) — the expensive one.
       Same 100-1500 ms per drive, worse if pkexec is needed.

**Thread affinity:** every step above runs on the main (UI) thread.
`app.processEvents()` at `main.cpp:330` gives the splash a chance to
paint, but *after* splash paint, `App w;` is a blocking constructor —
the splash is frozen until `w.show()` returns.

**Summary of pre-first-paint I/O:**
| Step | Syscall / I/O | Cost |
|---|---|---|
| `InfoManager::ins()` → DiskHealth ctor | `diskutil list -plist` (macOS) | 50-200 ms |
| Per-disk in discoverDrives (macOS) | `diskutil info -plist /dev/diskN` | 50-150 ms each |
| Per-drive with smartctl (both OS) | `smartctl -j -a /dev/…` | 100-1500 ms each |
| `DataRefreshService::start()` → `onFastTick` | sysctl / `mach_host_statistics64` / sysfs reads | 5-30 ms cumulative |
| `ToolManager::ins()->check*` calls | `$PATH` resolution, filesystem probes | 1-5 ms each |
| Per-page `setupUi()` | QRC-backed UI file parse, widget alloc | 5-50 ms each; ~100-300 ms cumulative for 13 pages |

Disk-health discovery dominates on any machine with external drives;
eager page construction dominates on SSD-only laptops.

---

## 2. Page construction map

`mListPages` is assembled in `App::init()` between
`shared/nexis/app.cpp:353-419`. The list (order as it appears in the
stacked widget):

| # | Page | Source file (shared/nexis/Pages/…) | Constructor work notes | Approx widget count |
|---|---|---|---|---|
| 1 | `DashboardPage` | `Dashboard/dashboard_page.cpp:30-71` + `init()` @ `:73-400+` | **Heavy.** First-touch of `InfoManager::ins()`. Builds 9 tiles (CpuTile, MemoryTile, DiskTile, NetworkTile, GpuTile, TempTile, BatteryTile, FanTile, HealthScoreTile) with `createTile()` factory; wraps each in `DashboardTileWrapper`; parses saved layout JSON; builds gear menus for temp/fan/GPU; connects 10+ signals to `DataRefreshService`; `buildGrid()`; checks for updates. | ~80-120 widgets |
| 2 | `HardwareInfoPage` | `HardwareInfo/hardware_info_page.cpp:84-105` | **Heavy (audit target).** `init()` calls `populateSystem`, `populateProcessor`, `populateGraphics`, `populateMemory`, `populateBattery`, `populateFans`, `populateStorage` — each builds a `QTableWidget` with 5-20 rows. `populateProcessor` shells out to `sysctl` on macOS (`hardware_info_page.cpp:193-224`) or reads `/sys/devices/system/cpu/cpu0/cache/index*` on Linux (`:225-249`). `populateBattery` and `populateStorage` touch `InfoManager`. See Section 5. | ~60-100 widgets |
| 3 | `ResourcesPage` | `Resources/resources_page.cpp:11-96` | Medium. Builds 5-7 `HistoryChart` widgets (QtCharts `QSplineSeries` * N) in initializer list. Touches `im->hasGpu()`, `im->hasDiskHealth()`, `im->getDriveHealth()` (reads **cached** drive list, so no I/O, but depends on `InfoManager` already being initialized). Connects 5 `DataRefreshService` signals. | ~25-40 widgets |
| 4 | `SystemCleanerPage` | `SystemCleaner/system_cleaner_page.cpp:26-158` | **Heavy.** Loads two `QMovie` animations (`scanLoading.gif`, `loading.gif`) per theme. `init()` sets pixmaps on 8-10 category icons (each `QIcon().pixmap(...)`), adds platform-specific Snap/Flatpak category (Linux), builds the floating exclusions QToolButton, configures `QTreeWidget`, connects `initScheduleIndicator()` (inspects ScheduleManager). 780 LOC file. | ~50-80 widgets |
| 5 | `DiskToolsPage` | `DiskTools/disk_tools_page.cpp:31-80+` | **Heavy.** `buildLargeOldPage()` + `buildDuplicatePage()` build two sub-layouts programmatically with combo boxes, file-picker rows, `QTreeWidget`s, progress bars. Connects several `DuplicateFinderService` signals. 766 LOC. No I/O at construction. | ~60-100 widgets |
| 6 | `SearchPage` | `Search/search_page.cpp:13-60+` | Medium. Sets up `QStandardItemModel` + `QSortFilterProxyModel`, builds table headers. Model is empty until user runs a search. | ~15-25 widgets |
| 7 | `ProcessesPage` | `Processes/processes_page.cpp:15-60+` | Medium. Table model + proxy, slider setup. Does not fetch processes at construction — the `DataRefreshService::processesUpdated` signal only fires when `mProcessTimer` is running (which is gated by `resumeProcessTimer()`, not called here). | ~15-25 widgets |
| 8 | `ServicesPage` | `Services/services_page.cpp:13-36` | Medium-Heavy. `init()` calls `mServiceManager->fetchServices()` at line 28 — this triggers a service-manager query (launchd enumerate / systemctl list-units) via `SystemServiceManager`. Off-main-thread inside the manager, but starts during app init. | ~15-25 widgets + N service items after callback |
| 9 | `StartupAppsPage` | `StartupApps/startup_apps_page.cpp:15-41` | Medium. `init()` calls `loadApps()` which reads `~/Library/LaunchAgents` (macOS) or `~/.config/autostart` (Linux) via `StartupService`. Synchronous filesystem read; small data volume. | ~10-20 widgets + one list item per app |
| 10 | `UninstallerPage` | `Uninstaller/uninstaller_page.cpp:20-78` | **Heavy.** `init()` fires three async fetches: `mPackageService->fetchPackages()`, `fetchSnapPackages()`, `fetchOrphanPackages()` (lines 64-66). Each runs in its own `QtConcurrent` but kicks off at app startup. Loads a `QMovie` (`loading.gif`). | ~15-25 widgets + N package items |
| 11 | `HelpersPage` | `Helpers/helpers_page.cpp:30-80+` | Medium. Constructs four sub-widgets in the initializer list (`HostManage`, `NetworkDiagWidget`, `OpenPortsWidget`, `FirewallWidget`) — each is its own page. `OpenPortsWidget` and `FirewallWidget` may touch `InfoManager` / shell. | ~30-50 widgets across sub-widgets |
| 12 | `SystemLogsPage` | `SystemLogs/system_logs_page.cpp:12-34` | Medium. Constructs `LogProvider` in the initializer list (`mProvider(LogProvider::createForPlatform(this))`). Schedules a `QTimer::singleShot(100, …, onRefreshClicked)` at line 33, which starts the log fetch. | ~15-25 widgets |
| 13 | `SettingsPage` | `Settings/settings_page.cpp:26-205` | **Heavy (surprising).** `init()` calls `mInfoManager->updateDiskInfo()` and `getDisks()` (lines 75-79), queries `hasBattery()`, `hasDiskHealth()`, `hasUpdateSources()`. Builds a large scroll area with 10+ sections, 20+ combo boxes, spinboxes, buttons. 663 LOC. | ~120-180 widgets |
| 14 | `APTSourceManagerPage` | `AptSourceManager/apt_source_manager_page.cpp:43-100+` | **Heavy** when present. 797 LOC. Builds the Available Updates section, repo health panel, tree widget. Only constructed when `ToolManager::ins()->checkSourceRepository()` is true. | ~50-80 widgets |
| 15 | `DockerPage` | `Docker/docker_page.cpp:9-60+` | Medium. Only constructed when `ToolManager::ins()->checkDocker()` is true. Sets up three `QTreeWidget`s, connects `DockerService` signals. | ~20-35 widgets |
| 16 | `GnomeSettingsPage` | `GnomeSettings/gnome_settings_page.cpp:7-65` | Medium (Linux-only). Builds four sub-tab widgets (`GnomeAppearanceTab`, `GnomeWmTab`, `GnomeMouseTab`, `GnomeDesktopTab`). Each sub-tab reads `gsettings` schemas through `ToolManager::ins()->gnomeSettings()`. | ~40-60 widgets across sub-tabs |

**Heavy pages most worth deferring:** `SystemCleanerPage`, `DiskToolsPage`,
`SettingsPage`, `APTSourceManagerPage`, `HardwareInfoPage`. Medium
pages that also do startup-time work: `ServicesPage`,
`UninstallerPage`, `SystemLogsPage` (all three kick off background
fetches from their constructors).

---

## 3. Cross-page signal wiring at construction time

`SignalMapper` is a global `QObject` singleton with nine signals
(`shared/nexis/signal_mapper.h:13-22`):

```cpp
void sigChangedAppTheme();
void sigUninstallStarted();
void sigUninstallFinished();
void sigKioskToggleRequested();
void sigKioskModeChanged(bool);
void sigAppVisibilityChanged(bool);
void sigNavigateToPage(const QString&);
void sigCleanableSizeChanged(quint64);
void sigDashboardFooterChanged(bool);
```

Connections established in page constructors (each page subscribes
to the global signal bus on construction):

| Signal | Subscriber(s) | Where | Effect of missing subscription |
|---|---|---|---|
| `sigChangedAppTheme` | `HardwareInfoPage` (`hardware_info_page.cpp:93`), `DashboardPage` / tiles (`disk_tile.cpp:21`, similar for every tile), `DiskToolsPage` (`disk_tools_page.cpp:77`), `SystemCleanerPage` (`system_cleaner_page.cpp:126`), `SettingsPage` (`settings_page.cpp:55-56`), `SystemLogsPage` (`system_logs_page.cpp:29`), `CommandPalette` (`command_palette.cpp:18`), `App::updateSidebarIcons` (`app.cpp:452`). Fired by `AppManager::updateStylesheet()` (`app_manager.cpp:231`). | Page constructors | Late-constructed pages miss theme changes fired **before** their construction. They still get the cascaded QSS (global `qApp->setStyleSheet()`), so the visible theme is correct, but per-page color overrides (e.g., disk tile arc color, health-verdict colors in HardwareInfoPage) won't be re-cached until the next theme change. **Mitigation:** call the page's `refreshThemeColors()` manually on first construction. |
| `sigUninstallStarted` / `sigUninstallFinished` | `UninstallerPage` (`uninstaller_page.cpp:68-77`) | UninstallerPage ctor | Only the Uninstaller page cares — since the emitter is the Uninstaller page itself (via `PackageService`), this is self-contained. Safe. |
| `sigKioskToggleRequested` | `App::toggleKioskMode` (`app.cpp:520`). Emitter: dashboard kiosk button. | App ctor (eager) | Always connected from App init. Safe. |
| `sigKioskModeChanged(bool)` | Dashboard tiles for kiosk visuals (check `dashboard_page.cpp`). | Tile constructors | Tiles only exist inside Dashboard (which stays eager). Safe. |
| `sigAppVisibilityChanged(bool)` | `DataRefreshService` (`data_refresh_service.cpp:36-42`). | Service ctor | Singleton lives outside pages. Safe. |
| `sigNavigateToPage(QString)` | `App` (`app.cpp:456-463`). | App ctor | Safe. |
| `sigCleanableSizeChanged(quint64)` | `App` for the sidebar badge (`app.cpp:524-534`). Emitter: SystemCleanerPage after a scan. | App ctor | Safe — emitter and subscriber are both in `App`. |
| `sigDashboardFooterChanged(bool)` | `DashboardPage`. Emitter: `SettingsPage` (checkbox toggle). | Both pages' ctors | **Problematic only if** Dashboard is lazy (it won't be) **or** SettingsPage is lazy and the dashboard footer setting is changed before Settings is opened — but the toggle only exists inside Settings, so a Settings-less session can't emit it. Safe. |

**`DataRefreshService` signals** (`shared/nexis/Managers/data_refresh_service.h:43-56`) are the main data bus. Pages connect in their constructors:

| Signal | Subscribers (at construction) |
|---|---|
| `cpuUpdated` | `DashboardPage::onCpuUpdated`, `onHealthCpuUpdated` (`dashboard_page.cpp:309, 342`); `ResourcesPage::onCpuUpdated` (`resources_page.cpp:75`) |
| `memoryUpdated` | DashboardPage (`:311, 344`); ResourcesPage (`:77`) |
| `networkUpdated` | DashboardPage (`:346`); ResourcesPage (`:79`) |
| `diskUsageUpdated` | DashboardPage (`:313, 348`); (indirectly everywhere via `updateDiskInfo`) |
| `diskIOUpdated` | ResourcesPage (`:81`) |
| `gpuUpdated` | DashboardPage (`:286`); ResourcesPage (`:85`) |
| `tempUpdated` | DashboardPage (`:202, 316`) |
| `fanUpdated` | DashboardPage (`:241`) |
| `batteryUpdated` | DashboardPage (`:292, 319`) |
| `diskHealthUpdated` | DashboardPage (`:297, 321`); ResourcesPage (`:89`) |
| `processesUpdated` | ProcessesPage (only) |
| `systemUpdatesChecked` | `App::init` sidebar updates badge (`app.cpp:374`); APTSourceManagerPage |

**Implications for FR-97:** When a page is constructed late (after many
ticks have already fired), the next tick will deliver fresh data, so the
page sees an update within 1 s (fast tick) / 5 s (medium tick) / 30 s
(slow tick). This is acceptable for all current slots — they are
idempotent: each slot re-renders from the payload, it doesn't
accumulate state. **Only exception:** `ResourcesPage` history charts
show a rolling 60-point history; a late-constructed Resources page
will start with an empty history. That is the existing behavior today
(history is never pre-seeded), so lazy construction does not regress
it.

**Cross-page connections** (page ↔ page, not page ↔ SignalMapper):
Searched — there are **no direct page-to-page `connect(...)` calls**.
All cross-page communication goes through `SignalMapper` or
`DataRefreshService`. This is a strong architectural property and
means FR-97 doesn't need to rewire any page-to-page pairs.

---

## 4. Existing "empty initial state" guards

The public data signal for disk health is
`DataRefreshService::diskHealthUpdated(const QList<DriveHealth>&)`
(`data_refresh_service.h:48`; emitted from `onSlowTick()` at
`data_refresh_service.cpp:244`). The singleton accessors
`InfoManager::hasDiskHealth()` and `InfoManager::getDriveHealth()`
(`info_manager.cpp:351-377`) delegate to `DiskHealthInfo::hasDrives()`
(`shared/nexis-core/Info/disk_health_info_shared.cpp:119-122`), which
returns `!mDrives.isEmpty()`. **This is the crux for FR-96:** if
`discoverDrives()` is deferred, every synchronous caller of
`hasDiskHealth()` / `getDriveHealth()` that runs before the async
completes will see an empty list.

Synchronous callers of `hasDiskHealth()` / `getDriveHealth()` at
startup:

| Caller | Site | Current assumption | Behavior if empty |
|---|---|---|---|
| `DashboardPage::init` | `dashboard_page.cpp:307` — `calc->setComponentAvailable("smart", im->hasDiskHealth());` | Sets whether the Health Score tile weights SMART data. | If false, Health Score computes without SMART. **Will auto-correct** on `diskHealthUpdated` because the lambda at `:321` calls `onHealthDiskHealthUpdated`, which in turn should re-set component availability. **Needs verification** — see Risk note. |
| `ResourcesPage::init` | `resources_page.cpp:45-55` — conditionally creates `mChartDiskHealth` based on count of drives with temperature data. | Chart exists iff any drive has temp data. | If empty, no chart is created and no `diskHealthUpdated` connection is made (line 88-90). **This is a one-shot decision at construction**; a late-arriving drive list would not retroactively create the chart. **Regression risk for FR-96.** |
| `SettingsPage::init` | `settings_page.cpp:173` — hides the "disk health alert" checkbox if no SMART data. | Cosmetic hide. | If empty, checkbox is hidden on first show. Once data arrives, no mechanism re-shows it. **Minor regression risk.** |
| `DataRefreshService::start` | `data_refresh_service.cpp:57` — `if (im->hasDiskHealth()) onSlowTick();` | Prevents the very first slow tick from being a no-op. | If false at start time, the 30 s timer will still fire and call `onSlowTick()`, which re-checks `hasDiskHealth()` (`:235`). **Self-healing.** |
| `HardwareInfoPage::populateStorage` | `hardware_info_page.cpp:429-438` | Hides the storage group if empty. | Once FR-98 moves this to first `showEvent`, by that time the async should have landed — but not guaranteed. **Need empty-state UI.** |
| `MaintenanceWizardDialog` | `maintenance_wizard_dialog.cpp:190` | Similar to DashboardPage. | Dialog is opened on demand — by then data is available. Safe. |
| `DashboardPage::updateDiskHealthBadge` | `dashboard_page.cpp:929-976` | Guards: `if (mCachedDriveHealth.isEmpty() || mCachedDisks.isEmpty()) return;` (`:931`). | **Already correctly handles empty state.** Data fills in on `onDiskHealthUpdated` (`:979`) via `mCachedDriveHealth = drives; updateDiskHealthBadge();`. |
| `DiskTile` | `disk_tile.cpp` — `setDriveHealth` called from DashboardPage; `clearDriveHealth` called first. | Health container hidden until `setDriveHealth` runs (`disk_tile.cpp:119`). | **Already correctly handles empty state.** |

**Signal contract to preserve:** Any async reimplementation of
`InfoManager::ins()` construction must ensure that the first
`diskHealthUpdated` signal fires after the first `refreshHealth()`
completes. Currently, the only emitter is
`DataRefreshService::onSlowTick()` (`data_refresh_service.cpp:244`),
which runs `refreshHealth()` and emits. FR-96's implementation should
either (a) kick off an initial `refreshHealth()` from
`App::show()` (via `QtConcurrent::run` followed by
`QMetaObject::invokeMethod` to emit `diskHealthUpdated` on the UI
thread), or (b) let the natural 30 s slow-tick handle it. Option (a)
is strongly preferred — otherwise Dashboard's disk-health badge is
blank for up to 30 s after launch.

---

## 5. `HardwareInfoPage` construction sequence (FR-98 target)

Code: `shared/nexis/Pages/HardwareInfo/hardware_info_page.cpp`.

Constructor (`:84-94`):
```cpp
HardwareInfoPage::HardwareInfoPage(QWidget *parent, InfoManager *infoManager)
  : QWidget(parent),
    ui(new Ui::HardwareInfoPage),
    im(infoManager ? infoManager : InfoManager::ins())
{
    ui->setupUi(this);
    init();
    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &HardwareInfoPage::refreshThemeColors);
}
```

`init()` (`:96-105`) is pure dispatch to the populate functions:
```cpp
void HardwareInfoPage::init() {
    populateSystem();      // QSysInfo + SystemInfoMacOS/Linux reads
    populateProcessor();   // sysctl (macOS) or /sys/…/cache reads
    populateGraphics();    // im->hasGpu() + im->getGpuDevices()
    populateMemory();      // im->updateMemoryInfo() + getMemTotal/getSwapTotal
    populateBattery();     // im->hasBattery() + im->updateBatteryInfo()
    populateFans();        // im->hasFanSensors() + im->getFanSensors()
    populateStorage();     // im->hasDiskHealth() + im->getDriveHealth()
}
```

Per-populate cost:

- `populateSystem` (`:140-170`). Instantiates `SystemInfoMacOS`/Linux
  on the stack; calls `getHostname()`, `getPlatform()`, `getKernel()`,
  etc. Most are `uname(2)` / `/etc/os-release` reads. Cheap (<5 ms).
- `populateProcessor` (`:172-254`). `SystemInfoMacOS::getCpuModel()`
  (sysctl `machdep.cpu.brand_string`), `CpuInfoMacOS::getCpuPhysicalCoreCount()`
  (sysctl `hw.physicalcpu`), `getCpuSpeed()`. Then cache info via
  `readSysctl()` lambda which spawns `QProcess("sysctl")` — several
  `QProcess` calls on macOS (`:192-224`). **This is notable startup
  cost: 10-50 ms just for the cache lines.** On Linux, directly reads
  `/sys/devices/system/cpu/cpu0/cache/index*` with `FileUtil::readStringFromFile`
  (`:227-249`) — cheaper (<5 ms).
- `populateGraphics` (`:256-289`). `im->hasGpu()` + `im->getGpuDevices()`.
  These return cached data (populated by `DataRefreshService::onFastTick`).
  If called before first fast tick, the GPU list may be stale/empty.
- `populateMemory` (`:291-305`). `im->updateMemoryInfo()` — this is a
  platform call (mach_host_statistics64 on macOS, /proc/meminfo on
  Linux). Cheap (<2 ms), but runs on the main thread.
- `populateBattery` (`:307-396`). `im->hasBattery()` +
  `im->updateBatteryInfo()` — IOKit on macOS, /sys/class/power_supply
  on Linux. Cheap (<5 ms) but synchronous.
- `populateFans` (`:398-418`). `im->hasFanSensors()` +
  `im->getFanSensors()` + `im->getFanSpeed(i)` per fan. Each sysfs
  read on Linux is cheap; on macOS SMC/IOKit calls are also cheap but
  add up with many sensors.
- `populateStorage` (`:420-625`). **Heaviest by far.** Reads
  `im->hasDiskHealth()` and `im->getDriveHealth()` (cached from
  `InfoManager::ins()` discovery). Builds a complex table per drive
  with SMART metrics, a Linux-only unlock-bar, per-drive note
  widgets. ~200 lines of widget construction.

**Signals subscribed in the constructor:**
- `SignalMapper::sigChangedAppTheme` → `refreshThemeColors` (`:93`).

**Signals subscribed elsewhere in the class:**
- None at construction time beyond the theme refresh. Storage
  unlock buttons are wired inside `populateStorage` when the row is
  built.

**Minimal mandatory construction work:**
- `ui->setupUi(this)` — must run so the tabs/group boxes exist
  (the window title comes from the UI file, and `App` iterates pages
  calling `windowTitle()` for command-palette registration).
- `connect(sigChangedAppTheme, ...)` — connect early so theme-change
  notifications during first-show aren't missed.

Everything in `init()` can move to a one-shot `showEvent` handler
gated by an `mPopulated` bool. `refreshThemeColors` remains safe to
call before population because it iterates `mHealthItems` (empty
initially, `:688`).

---

## 6. Risks and constraints

### 6.1 Signal ordering (lazy pages miss signals emitted during startup)

- `sigChangedAppTheme` fires once from `AppManager::updateStylesheet()`
  during `App::init()` (`app.cpp:471`). Pages constructed after that
  point miss the signal. **Mitigation:** after `new PageX(...)` in the
  lazy factory, explicitly call any `refreshThemeColors()` or
  equivalent method, or rely on the global `qApp->setStyleSheet()`
  cascade to handle most cases (the value-per-token caches on each
  page are the only gap).
- `systemUpdatesChecked` — fires on hour timer; only subscriber
  besides `App` is `APTSourceManagerPage`, which is constructed
  conditionally at app startup but will be lazy under FR-97. Missing
  the initial emit just means the page shows stale values until the
  next tick. Acceptable for a lazy page.
- `DataRefreshService::*Updated` fast/medium/slow ticks: repeat every
  1/5/30 s. Late subscribers get the next tick — worst case 30 s for
  disk health. Not perceived as a problem today.

### 6.2 QSS theming

`AppManager::updateStylesheet()` (`app_manager.cpp:115-232`) calls
`qApp->setStyleSheet(mStylesheetFileContent)`. Qt's global stylesheet
is automatically applied to any widget created later — so lazy pages
get styled for free. **The only exception is per-page dynamic color
caching** (e.g., `DiskTile::mArcColor = resolvedColor()`) which comes
from `AppManager::getStyleValues()`. That is lazy-read on every
`refreshThemeColors()` call, so as long as the page calls that method
after construction (either directly or via the `sigChangedAppTheme`
subscription), it is correct.

### 6.3 Translation live-switch

No page overrides `changeEvent(QEvent::LanguageChange)` —
`grep changeEvent shared/nexis/Pages` returns no matches. The
existing app therefore does not support live language switching;
changing language in Settings writes to `SettingManager` and likely
requires an app restart to re-translate. **This means FR-97 has no
language-switch risk** — lazy pages use the translator that is
installed when they are eventually constructed, and that is the only
time they get translated. No regression.

### 6.4 `InfoManager::ins()` singleton semantics

Callers that assume populated data **at the moment `InfoManager::ins()`
returns**:

- `DashboardPage::init` sets Health-Score SMART component based on
  `hasDiskHealth()` (`dashboard_page.cpp:307`). See 4-table.
- `ResourcesPage::init` decides whether to create the disk-temp chart
  (`resources_page.cpp:45-55`). **One-shot decision.** FR-96 must
  keep this working.
- `SettingsPage::init` hides the disk-health-alert checkbox
  (`settings_page.cpp:173`). One-shot.
- `DashboardPage` creates its disk-health signal connection
  unconditionally (`dashboard_page.cpp:297`) — so the first async
  emission will hit it. But the Health Score calculator's
  `setComponentAvailable` flag is set once at construction.

**Contract to preserve in FR-96 implementation:** either
1. Emit a one-shot `diskHealthUpdated` signal **after** lazy
   discovery so that Health-Score / Resources / Settings pages
   re-evaluate their "hasDiskHealth" decisions, **or**
2. Call `discoverDrives()` in the background but block `hasDiskHealth()`
   readers briefly with a QFuture guard (undesirable — defeats the
   purpose), **or**
3. Pre-detect whether any physical drive is present (cheap: scan
   `/sys/block` or one `diskutil list -plist` call) and have
   `hasDiskHealth()` return true optimistically, with the expensive
   SMART fill-in completing async.

Option (1) is the cleanest. `HealthScoreCalculator::setComponentAvailable`
should be re-called inside `DashboardPage::onHealthDiskHealthUpdated`
(not yet — check that slot), and `ResourcesPage` needs a
post-first-data fixup to create the chart if data now exists. That
fixup is additional scope for FR-96.

### 6.5 Kiosk mode & sidebar filtering

`App::applyKioskMode(true)` hides the sidebar entirely and navigates
to Dashboard (`app.cpp:1003-1008`). It does not iterate
`mListPages` to filter — so lazy construction doesn't affect kiosk.
Safe.

### 6.6 Test impact — `ScreenshotTests`

`tests/screenshots/test_screenshots.cpp` (281 LOC). The test builds
`App`, waits 500 ms, then iterates `kPageMap` (11 pages) and for each
calls `findPageByClassName` → `mStacked->setCurrentWidget(widget)`
(`:112-121, 143`). **If a page is lazy and hasn't been constructed,
it's not in the stacked widget and `findPageByClassName` returns
nullptr — the current code would `qWarning() … skipping`
(`:138-140`)**, silently skipping the screenshot comparison.

**This is a test regression risk for FR-97.** The fix has two options:
1. Have the screenshot test force-construct pages by invoking the
   sidebar button click flow (simulating real user navigation)
   before capturing. This is realistic and exercises the lazy path.
2. Add a test-only backdoor method `App::ensureAllPagesConstructed()`
   for test harnesses, gated by `#ifdef QT_TESTLIB_LIB` or a similar
   guard.

Option (1) is preferred — it keeps the lazy path tested.

### 6.7 Command palette iteration

`App::setupCommandPalette` (`app.cpp:1061-1092`) iterates
`mListPages` at `:1066` to register navigation commands per page
title. Each command captures `title` by value and, when triggered,
calls `clickSidebarButton(title, true)` which goes through
`getPageByTitle()` (searches `mListPages` by `windowTitle()` at
`:657-662`). **If pages are lazy, `mListPages` at palette-build time
is mostly empty, and `getPageByTitle` would fail until the user has
visited each page.**

**Mitigation for FR-97:** store the page titles (and factory lambdas)
in a separate structure alongside `mListPages`. The command palette
should register commands keyed by title with no page-pointer lookup —
have the callback invoke `clickSidebarButton(title, true)`, which
in turn drives the lazy factory. `getPageByTitle` also needs to be
refactored to lazily construct.

---

## 7. Bundle execution order

FR-97 → FR-98 → FR-96, or FR-97 + FR-96 in parallel then FR-98.

**Recommended: land FR-97 first as a standalone commit.**
- FR-97 is the largest refactor and touches `app.cpp` / `app.h`.
- Once pages are factory-backed, FR-98 is trivially a `showEvent`
  addition inside `HardwareInfoPage`.
- FR-96 is independent of FR-97 — the InfoManager singleton
  construction moves out of page-constructor time regardless — but
  FR-96's visible effect ("disk health fills in async") is easier
  to observe once pages are lazy: on a cold launch, Dashboard paints
  before `InfoManager::ins()` is touched, so the splash→first-paint
  delta is minimal and the disk-health async completion is the only
  remaining ~500 ms cost.

**Recommended commit sequence:**
1. **Commit A (FR-97 scaffold):** refactor `mListPages` to a
   `QList<std::function<QWidget*()>>` or a struct that pairs
   title + factory + cached pointer + construction callback.
   Keep all pages eagerly constructed for now — land the scaffold
   with zero behavior change.
2. **Commit B (FR-97 flip):** switch non-Dashboard pages to lazy.
   Fix command palette, `getPageByTitle`, screenshot test.
   Verify screenshot tests pass.
3. **Commit C (FR-98):** move `HardwareInfoPage::init()` contents
   into a one-shot `showEvent`. Small diff, limited risk.
4. **Commit D (FR-96 part 1):** extract `InfoManager::ins()` disk-health
   construction. Add a new `InfoManager::initAsync()` or rename the
   private ctor so `discoverDrives()` is **not** called during
   `ins()`. Add `App::kickOffDeferredInit()` called from
   `main.cpp` right after `w.show()`.
5. **Commit E (FR-96 part 2):** patch `ResourcesPage`, `SettingsPage`,
   `DashboardPage` Health-Score to re-evaluate their "hasDiskHealth"
   decisions on the first `diskHealthUpdated` emission. Ensure
   disk-health-chart creation works post-hoc.

Natural breakpoints: each commit above compiles, passes tests, and
yields measurable cold-launch improvement on its own.

---

## 8. Measurement methodology

### 8.1 Instrumentation (development only)

Add a `QElapsedTimer` in `main.cpp` just before `App w;`:

```cpp
QElapsedTimer coldLaunchTimer;
coldLaunchTimer.start();
App w;
qDebug() << "[cold-launch] App ctor:" << coldLaunchTimer.elapsed() << "ms";
w.show();
qDebug() << "[cold-launch] App::show:" << coldLaunchTimer.elapsed() << "ms";
QTimer::singleShot(0, &w, [&]() {
    qDebug() << "[cold-launch] first event loop turn:" << coldLaunchTimer.elapsed() << "ms";
});
```

Add the same timer inside `App::init()` after `DataRefreshService::start()`
and after `clickSidebarButton(...)` to isolate page-construction vs
data-refresh vs first-navigation costs.

Add a final checkpoint in `DashboardPage::onDiskHealthUpdated` (first
call) to see when disk data arrives.

**Remove all logging before ship** — gate with `#ifdef NEXIS_COLD_LAUNCH_TRACE`
and enable only in debug builds.

### 8.2 What to measure

| Metric | Definition | Current | Target (post-Bundle A) |
|---|---|---|---|
| `App ctor` duration | Time from `App w;` start to constructor return. | 400-800 ms typical, 1500+ ms with external drives. | 100-300 ms. |
| `App::show` to first paint | Time between `w.show()` return and first `paintEvent` on the main window. | Hard to measure without native tooling; proxy via `QTimer::singleShot(0, …)` after show. | Should be ~same (Qt internals). |
| Time to Dashboard-populated | Time between `main()` start and Dashboard tiles displaying numeric values. | 400-1500 ms. | 200-400 ms (dominated by first fast tick). |
| Time to disk-health-populated | Time between `main()` start and DiskTile `setDriveHealth` called. | 400-1500 ms. | 500-2000 ms (now async, but not blocking). |
| Peak RAM at idle after launch | `ps aux \| grep nexis` VSZ after 10 s on Dashboard without visiting any other page. | ~250-400 MB. | ~180-280 MB (13 pages not instantiated). |

### 8.3 Expected delta (back-of-envelope)

From Section 1's audit:
- DiskHealth discovery: **300-1500 ms** removed from critical path.
  On a MacBook with no external drives, expect ~300-500 ms.
- Lazy page construction: **100-300 ms** saved across
  SystemCleanerPage (40 ms), DiskToolsPage (40 ms), SettingsPage
  (60 ms), HelpersPage (30 ms), UninstallerPage (30 ms), others
  (20 ms each).
- HardwareInfoPage deferral (already lazy via FR-97, so this is
  only relevant on visits to Hardware Info): saves ~30-80 ms from
  `App::init` only if FR-98 is scoped to the single-path "visits
  Hardware Info page"; the main-thread saving comes from FR-97.

**Total expected win:** 400-1800 ms faster time-to-interactive,
with the larger end on spinning-rust / external-drive configurations.
The user-facing "splash disappears" improvement is ~1 s on a typical
laptop, matching FR-96's stated claim.

### 8.4 Verification commands

Incremental build + timing (from repo root, after instrumenting):

```bash
cmake --build build -j$(sysctl -n hw.ncpu) && \
  /usr/bin/time ./build/nexis --nosplash 2>&1 | grep cold-launch
```

Run with `ulimit -n 4096` and repeat 3-5 times, dropping the first
("warm filesystem cache") result to isolate steady-state cold
launch.

For screenshot regression: `ctest --test-dir build -R Screenshot
--output-on-failure`.

---

*End of research. Length: ~3 100 words.*
