# FR-35 Research: Dependency Injection for Page Constructors

## Overview

FR-35 aims to add constructor parameters with default values to all 14 page classes so that managers (singletons) can be injected for testing, while production code remains unchanged. This is Phase 6 of the Architecture Roadmap and depends on FR-34 (abstract base classes for platform code).

Currently, all 14 page classes access manager singletons via the `::ins()` static method — either in the member initializer list, in-body during `init()`, or scattered across methods. This tight coupling makes unit testing impossible without instantiating the real singletons.

---

## Manager Singleton Inventory

All managers follow the same pattern: a static `ins()` method returning a raw pointer to a lazily-created singleton.

| Manager | Header | `ins()` Signature | QObject? |
|---------|--------|-------------------|----------|
| `InfoManager` | `shared/nexis/Managers/info_manager.h` | `static InfoManager *ins()` | No |
| `SettingManager` | `shared/nexis/Managers/setting_manager.h` | `static SettingManager *ins()` | No |
| `ToolManager` | `shared/nexis/Managers/tool_manager.h` | `static ToolManager *ins()` | No |
| `AppManager` | `shared/nexis/Managers/app_manager.h` | `static AppManager *ins()` | No |
| `SignalMapper` | `shared/nexis/signal_mapper.h` | `static SignalMapper *ins()` | **Yes** (QObject) |
| `CleanerService` | `shared/nexis/Managers/cleaner_service.h` | `static CleanerService *ins()` | **Yes** (QObject) |
| `ScheduleManager` | `shared/nexis/Managers/schedule_manager.h` | `static ScheduleManager *ins()` | **Yes** (QObject) |

**Note:** `GnomeSettingsTool` and `DockerTool` use **static methods only** (no `::ins()` instance). They are stateless utility classes, not singletons. These are not injectable in the same way — they would need an abstract interface wrapper if we wanted to mock them.

---

## Page-by-Page Analysis

### 1. DashboardPage

- **File:** `shared/nexis/Pages/Dashboard/dashboard_page.h` (line 29) / `.cpp` (line 20)
- **Current constructor:** `explicit DashboardPage(QWidget *parent = 0);`
- **Member initializer list (line 30–31):**
  - `im(InfoManager::ins())`
  - `mSettingManager(SettingManager::ins())`
- **In-body / method `::ins()` calls:**
  - `AppManager::ins()->getTrayIcon()->showMessage(...)` — lines 247, 282, 327, 467, 541 (alert notifications)
  - `SignalMapper::ins()->sigKioskToggleRequested` — line 197 (emit)
  - `SignalMapper::ins()->sigKioskModeChanged` — line 199 (connect)
- **Manager dependencies:**
  - `InfoManager` — stored as member `im`, used extensively
  - `SettingManager` — stored as member `mSettingManager`, used extensively
  - `AppManager` — accessed via `::ins()` in methods (tray alerts)
  - `SignalMapper` — accessed via `::ins()` in methods (kiosk signals)
- **Other singletons:** None

### 2. HardwareInfoPage

- **File:** `shared/nexis/Pages/HardwareInfo/hardware_info_page.h` (line 18) / `.cpp` (line 19)
- **Current constructor:** `explicit HardwareInfoPage(QWidget *parent = 0);`
- **Member initializer list (line 22):**
  - `im(InfoManager::ins())`
- **In-body / method `::ins()` calls:** None
- **Manager dependencies:**
  - `InfoManager` — stored as member `im`, used throughout populate methods
- **Other singletons:** None
- **Note:** Also uses `SystemInfo` and `CpuInfo` as local stack objects (not singletons).

### 3. StartupAppsPage

- **File:** `shared/nexis/Pages/StartupApps/startup_apps_page.h` (line 24) / `.cpp` (line 14)
- **Current constructor:** `explicit StartupAppsPage(QWidget *parent = 0);`
- **Member initializer list:** No manager references
- **In-body / method `::ins()` calls:** None
- **Manager dependencies:** **None**
- **Other singletons:** None
- **Note:** This page is entirely self-contained. It reads filesystem paths directly using `QDir`, `QFileInfo`, and `FileUtil`. No manager singletons.

### 4. SystemCleanerPage

- **File:** `shared/nexis/Pages/SystemCleaner/system_cleaner_page.h` (line 38) / `.cpp` (line 18)
- **Current constructor:** `explicit SystemCleanerPage(QWidget *parent = nullptr);`
- **Member initializer list:** No manager references in initializer
- **In-body `::ins()` calls (constructor + init):**
  - `AppManager::ins()->resolveThemeName()` — line 31 (constructor body, for loading GIF)
  - `SignalMapper::ins()` — line 90 (connect theme change signal, in `init()`)
  - `AppManager::ins()->resolveThemeName()` — line 91 (inside theme change lambda)
- **Method `::ins()` calls:**
  - `CleanerService::ins()->scan(...)` — line 192 (worker thread)
  - `CleanerService::ins()->cleanTrash()` — line 306 (worker thread)
  - `CleanerService::ins()->cleanFiles(...)` — line 310 (worker thread)
  - `ScheduleManager::ins()` — lines 529, 537, 549 (schedule indicator)
- **Manager dependencies:**
  - `AppManager` — theme name resolution (constructor + theme change)
  - `SignalMapper` — theme change signal
  - `CleanerService` — scan and clean operations
  - `ScheduleManager` — schedule indicator display
- **Other singletons:** None

### 5. SearchPage

- **File:** `shared/nexis/Pages/Search/search_page.h` (line 28) / `.cpp` (line 8)
- **Current constructor:** `explicit SearchPage(QWidget *parent = 0);`
- **Member initializer list:** No manager references
- **In-body `::ins()` calls (init):**
  - `SettingManager::ins()->getThemeName()` — line 64 (loading GIF path)
  - `InfoManager::ins()->getUserList()` — line 127
  - `InfoManager::ins()->getGroupList()` — line 130
- **Method `::ins()` calls:**
  - `InfoManager::ins()->getUserName()` — lines 425, 474 (trash/delete operations)
- **Manager dependencies:**
  - `SettingManager` — theme name (init only)
  - `InfoManager` — user/group lists (init), username check (methods)
- **Other singletons:** None
- **Note:** Only platform-specific `.cpp` file is `shared/nexis/Pages/Search/search_page.cpp` — there is no separate macOS/Linux variant for the page itself. The `#ifdef Q_OS_MACOS` blocks are inline.

### 6. ServicesPage

- **File:** `shared/nexis/Pages/Services/services_page.h` (line 16) / `.cpp` (line 13)
- **Current constructor:** `explicit ServicesPage(QWidget *parent = 0);`
- **Member initializer list:** No manager references
- **In-body `::ins()` calls:**
  - `ToolManager::ins()->getServices()` — line 36 (in `getServices()`, called from worker thread)
- **Manager dependencies:**
  - `ToolManager` — service list (via method call on worker thread)
- **Other singletons:** None
- **Note:** Does NOT store ToolManager as a member. Uses `::ins()` directly in a method.

### 7. ProcessesPage

- **File:** `shared/nexis/Pages/Processes/processes_page.h` (line 24) / `.cpp` (line 13)
- **Current constructor:** `explicit ProcessesPage(QWidget *parent = 0);`
- **Member initializer list (line 18):**
  - `im(InfoManager::ins())`
- **In-body / method `::ins()` calls:** None (uses member `im` throughout)
- **Manager dependencies:**
  - `InfoManager` — stored as member `im`, used for process list and user info
- **Other singletons:** None

### 8. UninstallerPage

- **File:** `shared/nexis/Pages/Uninstaller/uninstaller_page.h` (line 22) / `.cpp` (line 15)
- **Current constructor:** `explicit UninstallerPage(QWidget *parent = 0);`
- **Member initializer list (line 18):**
  - `tm(ToolManager::ins())`
- **In-body `::ins()` calls (init):**
  - `AppManager::ins()->resolveThemeName()` — line 32 (loading GIF)
  - `SignalMapper::ins()` — lines 57, 59, 62 (connect uninstall signals)
- **Method `::ins()` calls:**
  - `SignalMapper::ins()->sigUninstallStarted` — lines 269, 317 (emit in worker)
  - `ToolManager::ins()->trashApps(...)` — line 270 (macOS worker)
  - `ToolManager::ins()->uninstallPackages(...)` — line 319 (Linux worker)
  - `ToolManager::ins()->uninstallSnapPackages(...)` — line 320 (Linux worker)
  - `SignalMapper::ins()->sigUninstallFinished` — lines 271, 322 (emit in worker)
- **Manager dependencies:**
  - `ToolManager` — stored as member `tm`, also accessed via `::ins()` in worker lambdas
  - `AppManager` — theme name (init)
  - `SignalMapper` — uninstall signals (connect + emit)
- **Other singletons:** None

### 9. ResourcesPage

- **File:** `shared/nexis/Pages/Resources/resources_page.h` (line 22) / `.cpp` (line 10)
- **Current constructor:** `explicit ResourcesPage(QWidget *parent = 0);`
- **Member initializer list (line 13):**
  - `im(InfoManager::ins())`
- **In-body / method `::ins()` calls:** None (uses member `im` throughout)
- **Manager dependencies:**
  - `InfoManager` — stored as member `im`, used extensively
- **Other singletons:** None
- **Note:** Child widgets `HistoryChart` and `DiskUsageLauncherWidget` access `SignalMapper::ins()`, `AppManager::ins()`, and `SettingManager::ins()` internally, but those are their own dependencies, not the page's.

### 10. HelpersPage

- **File:** `shared/nexis/Pages/Helpers/helpers_page.h` (line 17) / `.cpp` (line 9)
- **Current constructor:** `explicit HelpersPage(QWidget *parent = 0);`
- **Member initializer list:** No manager references
- **In-body / method `::ins()` calls:** None
- **Manager dependencies:** **None**
- **Other singletons:** None
- **Note:** Completely self-contained. Uses only `Utilities` and its child widget `HostManage`.

### 11. APTSourceManagerPage

- **File:** `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.h` (line 28) / `.cpp` (line 23)
- **Current constructor:** `explicit APTSourceManagerPage(QWidget *parent = 0);`
- **Member initializer list:** No manager references
- **In-body `::ins()` calls (init):**
  - `SignalMapper::ins()` — line 78 (connect, macOS only)
- **Method `::ins()` calls:**
  - `ToolManager::ins()->getSourceList()` — line 110
  - `ToolManager::ins()->getPackages()` — line 143 (macOS worker)
  - `ToolManager::ins()->addAPTRepository(...)` — line 248
  - `ToolManager::ins()->dryRunRemovePackages(...)` — line 324 (macOS)
  - `SignalMapper::ins()->sigUninstallStarted` — line 347 (macOS emit)
  - `ToolManager::ins()->uninstallPackages(...)` — line 348 (macOS)
  - `SignalMapper::ins()->sigUninstallFinished` — line 349 (macOS emit)
  - `ToolManager::ins()->removeAPTSource(...)` — line 353 (Linux)
- **Manager dependencies:**
  - `ToolManager` — APT/Homebrew operations (methods only)
  - `SignalMapper` — uninstall signals (macOS only)
- **Other singletons:** None

### 12. GnomeSettingsPage

- **File:** `shared/nexis/Pages/GnomeSettings/gnome_settings_page.h` (line 20) / `.cpp` (line 7)
- **Current constructor:** `explicit GnomeSettingsPage(QWidget *parent = nullptr);`
- **Member initializer list:** No manager references
- **In-body / method `::ins()` calls:** None
- **Manager dependencies:** **None**
- **Other singletons:** None
- **Note:** Sub-tabs (`GnomeAppearanceTab`, `GnomeWmTab`, `GnomeMouseTab`, `GnomeDesktopTab`) use `GnomeSettingsTool` static methods extensively, but the page itself has zero singleton dependencies. `GnomeSettingsTool` is a static utility class, not an injectable singleton.

### 13. DockerPage

- **File:** `shared/nexis/Pages/Docker/docker_page.h` (line 19) / `.cpp` (line 10)
- **Current constructor:** `explicit DockerPage(QWidget *parent = nullptr);`
- **Member initializer list:** No manager references
- **In-body / method `::ins()` calls:** None directly on managers
- **Manager dependencies:** **None** (of the injectable singleton managers)
- **Other singletons:** None
- **Note:** Uses `DockerTool::isDaemonRunning()`, `DockerTool::getImages()`, etc. — all **static methods** on a utility class. Also uses `AppManager::ins()` only in `docker_page.cpp` line 5 (included header) but not actually called. The page has zero injectable singleton dependencies.

### 14. SettingsPage

- **File:** `shared/nexis/Pages/Settings/settings_page.h` (line 24) / `.cpp` (line 26)
- **Location:** `shared/nexis/Pages/Settings/` (single implementation for both platforms; `#ifdef Q_OS_MACOS` blocks inline)
- **Current constructor:** `explicit SettingsPage(QWidget *parent = 0);`
- **Member initializer list (lines 29–30):**
  - `apm(AppManager::ins())`
  - `mSettingManager(SettingManager::ins())`
- **In-body `::ins()` calls (init):**
  - `InfoManager::ins()->updateDiskInfo()` — line 59
  - `InfoManager::ins()->getDisks()` — line 60
  - `InfoManager::ins()->hasBattery()` — line 129
  - `InfoManager::ins()->hasDiskHealth()` — line 136
  - `ScheduleManager::ins()->getAllSchedules()` — line 374
  - `ScheduleManager::ins()` — line 390 (connect)
- **Method `::ins()` calls:**
  - `ScheduleManager::ins()` — lines 395, 467, 513, 515, 519, 522, 529, 550, 623, 635 (extensive schedule management)
- **Manager dependencies:**
  - `AppManager` — stored as member `apm`, used for theme/stylesheet
  - `SettingManager` — stored as member `mSettingManager`, used extensively
  - `InfoManager` — disk list, battery check, disk health check (init)
  - `ScheduleManager` — schedule CRUD, summaries (init + methods)
- **Other singletons:** None

---

## Page Instantiation in app.cpp

All pages are created in `App::init()` (file: `shared/nexis/app.cpp`).

| Line | Instantiation | Conditional |
|------|--------------|-------------|
| 51 | `dashboardPage = new DashboardPage(mSlidingStacked);` | Always |
| 52 | `hardwareInfoPage = new HardwareInfoPage(mSlidingStacked);` | Always |
| 53 | `startupAppsPage = new StartupAppsPage(mSlidingStacked);` | Always |
| 54 | `searchPage = new SearchPage(mSlidingStacked);` | Always |
| 55 | `systemCleanerPage = new SystemCleanerPage(mSlidingStacked);` | Always |
| 56 | `servicesPage = new ServicesPage(mSlidingStacked);` | Always |
| 57 | `processPage = new ProcessesPage(mSlidingStacked);` | Always |
| 58 | `helpersPage = new HelpersPage(mSlidingStacked);` | Always |
| 59 | `uninstallerPage = new UninstallerPage(mSlidingStacked);` | Always |
| 60 | `resourcesPage = new ResourcesPage(mSlidingStacked);` | Always |
| 61 | `settingsPage = new SettingsPage(mSlidingStacked);` | Always |
| 77 | `aptSourceManagerPage = new APTSourceManagerPage(mSlidingStacked);` | `ToolManager::ins()->checkSourceRepository()` |
| 86 | `dockerPage = new DockerPage(mSlidingStacked);` | `ToolManager::ins()->checkDocker()` |
| 96 | `gnomeSettingsPage = new GnomeSettingsPage(mSlidingStacked);` | `ToolManager::ins()->checkGnomeSettings()` |

All constructors receive only a `QWidget *parent` argument. No managers are passed between pages. No shared state exists between pages except through singletons.

---

## SignalMapper Usage Pattern Analysis

`SignalMapper::ins()` is used in two distinct patterns:

1. **Signal connections (in `init()` or constructor body):**
   ```cpp
   connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme, this, ...);
   connect(SignalMapper::ins(), &SignalMapper::sigUninstallFinished, this, ...);
   connect(SignalMapper::ins(), &SignalMapper::sigKioskModeChanged, this, ...);
   ```
   Used by: DashboardPage, SystemCleanerPage, UninstallerPage, APTSourceManagerPage

2. **Signal emission (in lambdas / worker threads):**
   ```cpp
   emit SignalMapper::ins()->sigKioskToggleRequested();
   emit SignalMapper::ins()->sigUninstallStarted();
   emit SignalMapper::ins()->sigUninstallFinished();
   ```
   Used by: DashboardPage, UninstallerPage, APTSourceManagerPage

SignalMapper is **never** stored as a member variable. It is always accessed inline via `::ins()`.

---

## Summary Table: Dependencies by Page

| # | Page | File | Constructor Line | Manager Dependencies | Access Pattern |
|---|------|------|-----------------|---------------------|----------------|
| 1 | DashboardPage | `shared/nexis/Pages/Dashboard/dashboard_page.{h,cpp}` | `.cpp:20` | InfoManager, SettingManager, AppManager, SignalMapper | Members (IM, SM); methods (AM, SigMap) |
| 2 | HardwareInfoPage | `shared/nexis/Pages/HardwareInfo/hardware_info_page.{h,cpp}` | `.cpp:19` | InfoManager | Member (IM) |
| 3 | StartupAppsPage | `shared/nexis/Pages/StartupApps/startup_apps_page.{h,cpp}` | `.cpp:14` | **None** | N/A |
| 4 | SystemCleanerPage | `shared/nexis/Pages/SystemCleaner/system_cleaner_page.{h,cpp}` | `.cpp:18` | AppManager, SignalMapper, CleanerService, ScheduleManager | All via `::ins()` in methods |
| 5 | SearchPage | `shared/nexis/Pages/Search/search_page.{h,cpp}` | `.cpp:8` | SettingManager, InfoManager | `::ins()` in init + methods |
| 6 | ServicesPage | `shared/nexis/Pages/Services/services_page.{h,cpp}` | `.cpp:13` | ToolManager | `::ins()` in method |
| 7 | ProcessesPage | `shared/nexis/Pages/Processes/processes_page.{h,cpp}` | `.cpp:13` | InfoManager | Member (IM) |
| 8 | UninstallerPage | `shared/nexis/Pages/Uninstaller/uninstaller_page.{h,cpp}` | `.cpp:15` | ToolManager, AppManager, SignalMapper | Member (TM); `::ins()` (AM, SigMap) |
| 9 | ResourcesPage | `shared/nexis/Pages/Resources/resources_page.{h,cpp}` | `.cpp:10` | InfoManager | Member (IM) |
| 10 | HelpersPage | `shared/nexis/Pages/Helpers/helpers_page.{h,cpp}` | `.cpp:9` | **None** | N/A |
| 11 | APTSourceManagerPage | `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.{h,cpp}` | `.cpp:23` | ToolManager, SignalMapper | `::ins()` in methods |
| 12 | GnomeSettingsPage | `shared/nexis/Pages/GnomeSettings/gnome_settings_page.{h,cpp}` | `.cpp:7` | **None** | N/A |
| 13 | DockerPage | `shared/nexis/Pages/Docker/docker_page.{h,cpp}` | `.cpp:10` | **None** | N/A (DockerTool is static util) |
| 14 | SettingsPage | `shared/nexis/Pages/Settings/settings_page.{h,cpp}` | `.cpp:26` | AppManager, SettingManager, InfoManager, ScheduleManager | Members (AM, SM); `::ins()` (IM, SchedM) |

---

## Proposed DI Constructor Signatures

The strategy: add optional manager pointer parameters with `nullptr` defaults. When `nullptr`, the page falls back to the existing `::ins()` singleton. This ensures zero change to production call sites while enabling test injection.

Pages with **no manager dependencies** (StartupAppsPage, HelpersPage, GnomeSettingsPage, DockerPage) need no changes for DI purposes, but for consistency all 14 pages could adopt the same pattern with a no-op default.

### Convention

```cpp
// Pattern: default nullptr = use singleton
explicit PageName(QWidget *parent = nullptr,
                  InfoManager *infoManager = nullptr,
                  SettingManager *settingManager = nullptr);

// In constructor body or member initializer:
im(infoManager ? infoManager : InfoManager::ins())
```

### Proposed Signatures

| # | Page | Proposed Constructor Signature |
|---|------|-------------------------------|
| 1 | DashboardPage | `DashboardPage(QWidget *parent = nullptr, InfoManager *im = nullptr, SettingManager *sm = nullptr, AppManager *am = nullptr, SignalMapper *sig = nullptr)` |
| 2 | HardwareInfoPage | `HardwareInfoPage(QWidget *parent = nullptr, InfoManager *im = nullptr)` |
| 3 | StartupAppsPage | `StartupAppsPage(QWidget *parent = nullptr)` — **no change needed** |
| 4 | SystemCleanerPage | `SystemCleanerPage(QWidget *parent = nullptr, AppManager *am = nullptr, SignalMapper *sig = nullptr, CleanerService *cs = nullptr, ScheduleManager *schm = nullptr)` |
| 5 | SearchPage | `SearchPage(QWidget *parent = nullptr, SettingManager *sm = nullptr, InfoManager *im = nullptr)` |
| 6 | ServicesPage | `ServicesPage(QWidget *parent = nullptr, ToolManager *tm = nullptr)` |
| 7 | ProcessesPage | `ProcessesPage(QWidget *parent = nullptr, InfoManager *im = nullptr)` |
| 8 | UninstallerPage | `UninstallerPage(QWidget *parent = nullptr, ToolManager *tm = nullptr, AppManager *am = nullptr, SignalMapper *sig = nullptr)` |
| 9 | ResourcesPage | `ResourcesPage(QWidget *parent = nullptr, InfoManager *im = nullptr)` |
| 10 | HelpersPage | `HelpersPage(QWidget *parent = nullptr)` — **no change needed** |
| 11 | APTSourceManagerPage | `APTSourceManagerPage(QWidget *parent = nullptr, ToolManager *tm = nullptr, SignalMapper *sig = nullptr)` |
| 12 | GnomeSettingsPage | `GnomeSettingsPage(QWidget *parent = nullptr)` — **no change needed** |
| 13 | DockerPage | `DockerPage(QWidget *parent = nullptr)` — **no change needed** |
| 14 | SettingsPage | `SettingsPage(QWidget *parent = nullptr, AppManager *am = nullptr, SettingManager *sm = nullptr, InfoManager *im = nullptr, ScheduleManager *schm = nullptr)` |

---

## Implementation Considerations

### 1. Member Storage vs. Method-Only Access

Some pages already store manager pointers as members (`im`, `tm`, `apm`, `mSettingManager`), making them straightforward to inject. Others only call `::ins()` in scattered methods.

**Pages that already store members:**
- DashboardPage: `im` (InfoManager), `mSettingManager` (SettingManager)
- HardwareInfoPage: `im` (InfoManager)
- ProcessesPage: `im` (InfoManager)
- UninstallerPage: `tm` (ToolManager)
- ResourcesPage: `im` (InfoManager)
- SettingsPage: `apm` (AppManager), `mSettingManager` (SettingManager)

**Pages that only use `::ins()` in methods (need new members):**
- SystemCleanerPage: needs `mAppManager`, `mSignalMapper`, `mCleanerService`, `mScheduleManager`
- SearchPage: needs `mSettingManager`, `mInfoManager`
- ServicesPage: needs `mToolManager`
- APTSourceManagerPage: needs `mToolManager`, `mSignalMapper`
- DashboardPage: also needs `mAppManager`, `mSignalMapper` (partially)
- UninstallerPage: also needs `mAppManager`, `mSignalMapper`

### 2. Worker Thread Lambda Captures

Several pages use `::ins()` inside `QtConcurrent::run()` lambdas (worker threads):
- **UninstallerPage** lines 267-272, 315-323: `SignalMapper::ins()`, `ToolManager::ins()`
- **APTSourceManagerPage** lines 346-350: `SignalMapper::ins()`, `ToolManager::ins()`
- **SystemCleanerPage** lines 192, 306, 310: `CleanerService::ins()`
- **ServicesPage** line 36: `ToolManager::ins()`

When switching from `::ins()` to member pointers, these lambdas must capture `this` or the member pointer. Since they already capture `this` (or `[=]`), this is not an issue — the member will be accessible.

### 3. SignalMapper Special Case

`SignalMapper` is never stored as a member anywhere. It's used purely for:
- `connect()` calls (passing as sender/receiver)
- `emit` calls (emitting signals through it)

For DI, we would store it as a member (e.g., `mSignalMapper`) and use that member in connects/emits. The default would remain `SignalMapper::ins()`.

### 4. Static Utility Classes (DockerTool, GnomeSettingsTool)

These use static methods exclusively. They cannot be injected as instances. To make pages that use them testable, FR-34 (abstract base classes) would need to wrap them behind virtual interfaces first. For FR-35's scope, pages using only static utility classes (DockerPage, GnomeSettingsPage) need **no DI changes**.

### 5. Child Widget Dependencies

Some child widgets created by pages also access singletons internally:
- `CircleBar` → `SignalMapper::ins()`, `AppManager::ins()`
- `HistoryChart` → `SignalMapper::ins()`, `AppManager::ins()`
- `DiskUsageLauncherWidget` → `SignalMapper::ins()`, `SettingManager::ins()`, `AppManager::ins()`
- `ServiceItem` → `ToolManager::ins()`
- `APTSourceRepositoryItem` → `ToolManager::ins()`
- `APTSourceEdit` → `ToolManager::ins()`

These are **out of scope for FR-35** (page-level DI). They would need their own DI treatment in a future pass. For unit tests of page logic, mock managers injected at the page level would not propagate to child widgets unless those widgets are also updated.

### 6. Production Call Sites in app.cpp

All 14 page constructors are called in `shared/nexis/app.cpp` `App::init()` (lines 51-96). Since all new parameters have default values (`= nullptr`), **zero changes are needed in app.cpp**. The existing `new DashboardPage(mSlidingStacked)` calls will continue to work, falling back to singletons.

### 7. Header Include Changes

Pages that don't currently include a manager header but will need a new member will need the include added. For example:
- `system_cleaner_page.h` does not include `signal_mapper.h` (it's in `.cpp`). The forward declaration + include would be needed.
- Using forward declarations in headers and full includes in `.cpp` is the cleanest approach.

---

## Dependency Complexity Ranking

Sorted by number of injectable manager dependencies (determines implementation effort):

| Rank | Page | # Managers | Managers |
|------|------|-----------|----------|
| 1 | SettingsPage | 4 | AppManager, SettingManager, InfoManager, ScheduleManager |
| 2 | DashboardPage | 4 | InfoManager, SettingManager, AppManager, SignalMapper |
| 3 | SystemCleanerPage | 4 | AppManager, SignalMapper, CleanerService, ScheduleManager |
| 4 | UninstallerPage | 3 | ToolManager, AppManager, SignalMapper |
| 5 | SearchPage | 2 | SettingManager, InfoManager |
| 6 | APTSourceManagerPage | 2 | ToolManager, SignalMapper |
| 7 | HardwareInfoPage | 1 | InfoManager |
| 8 | ProcessesPage | 1 | InfoManager |
| 9 | ResourcesPage | 1 | InfoManager |
| 10 | ServicesPage | 1 | ToolManager |
| 11 | StartupAppsPage | 0 | — |
| 12 | HelpersPage | 0 | — |
| 13 | GnomeSettingsPage | 0 | — |
| 14 | DockerPage | 0 | — |

---

## Risks and Edge Cases

1. **Thread safety:** Manager pointers stored as members will be captured by worker-thread lambdas. Since singletons are already accessed cross-thread today, the same thread-safety guarantees (or lack thereof) apply. No new risk.

2. **Lifetime management:** Injected manager pointers must outlive the page. In production, singletons live for the app's lifetime. In tests, the test harness must keep mock managers alive for the duration of the page test.

3. **Binary compatibility:** Adding parameters with defaults does not change the ABI for existing callers (they continue to use the default). However, the mangled name of the constructor changes. Since this is an application (not a library), this is not a concern.

4. **FR-34 dependency:** FR-35 explicitly depends on FR-34 (abstract base classes). Without abstract interfaces, tests would need to instantiate real managers (which may have side effects like file I/O, system calls). FR-34 provides the mock-able interfaces.

5. **Four pages need zero changes:** StartupAppsPage, HelpersPage, GnomeSettingsPage, and DockerPage have no injectable manager dependencies. They can be left as-is or given empty DI signatures for future-proofing.
