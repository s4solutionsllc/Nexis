# Nexis — Architecture Review

> A deep and comprehensive review of the Nexis architecture: how logic and UI work together, what's working well, what should change, and where the application should go next.
> Last updated: February 2026

---

## Table of Contents

1. [Architecture at a Glance](#architecture-at-a-glance)
2. [Architecture Strengths](#architecture-strengths)
   - [Platform Abstraction via Abstract Base Classes](#1-platform-abstraction-via-abstract-base-classes)
   - [Singleton Manager Facades](#2-singleton-manager-facades)
   - [QSS Token System](#3-qss-token-system)
   - [Graceful Degradation](#4-graceful-degradation)
   - [SignalMapper for Cross-Component Events](#5-signalmapper-for-cross-component-events)
3. [Architecture Weaknesses](#architecture-weaknesses)
   - [~~No Formal Platform Interfaces~~ (Resolved)](#1-no-formal-platform-interfaces)
   - [~~Singleton Coupling Limits Testability~~ (Partially Resolved)](#2-singleton-coupling-limits-testability)
   - [Manager Layer Inconsistency](#3-manager-layer-inconsistency)
   - [CMake GLOB_RECURSE](#4-cmake-glob_recurse)
   - [No Automated Test Suite](#5-no-automated-test-suite)
   - [Fragmented Timer/Polling](#6-fragmented-timerpolling)
   - [QSS Token Validation Gap](#7-qss-token-validation-gap)
4. [Recommended Improvements](#recommended-improvements)
   - [Priority 1: Critical](#priority-1-critical-foundational)
   - [Priority 2: High](#priority-2-high-scalability)
   - [Priority 3: Medium](#priority-3-medium-testing--quality)
   - [Priority 4: Low](#priority-4-low-future-proofing)
5. [Strategic Direction](#strategic-direction)
   - [QWidgets vs QML](#qwidgets-vs-qml)
   - [Testing Strategy](#testing-strategy)
   - [Incremental Evolution vs Clean-Slate Redesign](#incremental-evolution-vs-clean-slate-redesign)
   - [Architectural Vision](#architectural-vision)

---

## Architecture at a Glance

Nexis is structured as a **four-tier desktop application**:

```
┌────────────────────────────────────────────────────────────────────┐
│  UI Layer: 15 QWidget Pages                                       │
│  Each page owns its .ui file and presentation logic               │
│  Files: shared/nexis/Pages/*/*.cpp                                │
├────────────────────────────────────────────────────────────────────┤
│  Service Layer: 8 Domain Services + NexisPage base class          │
│  StartupService, FileSearchService, HostService, ProcessService,  │
│  SystemServiceManager, DockerService, PackageService,             │
│  DuplicateFinderService                                           │
│  Files: shared/nexis/Services/*.cpp                               │
├────────────────────────────────────────────────────────────────────┤
│  Manager Layer: 7 Singletons                                      │
│  InfoManager, AppManager, SettingManager, ToolManager,            │
│  CleanerService, ScheduleManager, DataRefreshService              │
│  Files: shared/nexis/Managers/*.cpp                               │
├────────────────────────────────────────────────────────────────────┤
│  Core Library: nexis-core (static lib)                            │
│  14 Info providers + 7 Tools + 3 Utils                            │
│  Files: shared/nexis-core/**/*.cpp + {platform}/nexis-core/**     │
└────────────────────────────────────────────────────────────────────┘
```

**Key architectural decisions:**
- **Compile-time platform abstraction** — Abstract base classes with pure virtual methods; platform subclasses selected via `#ifdef Q_OS_MACOS` at construction time
- **Domain service layer (FR-42)** — Business logic extracted from pages into singleton services (`Services/` directory). Services handle async operations via `QThreadPool::start()` and emit result signals. Pages are thin UI consumers. Follows the `CleanerService` pattern.
- **Singleton managers with DI escape hatches** — Static `ins()` accessors with `std::unique_ptr` members; all page and widget constructors accept optional dependency pointers for test injection (FR-35, FR-42)
- **Centralized polling** — `DataRefreshService` singleton owns 4 QTimers (1s/5s/30s/configurable) and emits typed data signals; pages subscribe as reactive consumers
- **Page lifecycle hooks** — `NexisPage` base class with virtual `onPageActivated()`/`onPageDeactivated()` called by `App::pageClick()`. Enables lazy loading and pause-on-hide patterns.
- **QSS theming** — Single stylesheet template with `@token` replacement at runtime
- **Qt signals** — `SignalMapper` singleton as a lightweight global event bus (12 signals after FR-51)
- **Dashboard widgets** — Dashboard gauges replaced with a family of interchangeable tile widgets inheriting from `MetricTileBase` (abstract base class, FR-53). 7 widget styles available: `MetricTile` (sparkline), `GaugeTile` (¾-arc), `HybridTile` (arc + mini sparkline), `RingTile` (360° ring), `SpeedometerTile` (needle dial), `VuMeterTile` (segmented bar), and `DiskTile` (donut chart). All styles support three `DisplayMode` values (Normal/Hero/Large) via QSS dynamic properties — mode changes trigger `unpolish()`/`polish()` to re-evaluate QSS selectors for per-mode font sizing. CPU and Memory are independent tile instances (the combined `HeroCard` was removed in FR-50). `NetworkTile` uses dual `QChart` instances (separate RX/TX sparklines) and is excluded from style switching. `DiskTile` uses custom `QPainter` donut chart and exposes `setDriveHealth()` for cross-tile data flow. `DashboardTileWrapper` (FR-51) uses the decorator pattern to add edit-mode mouse handling (drag-and-drop, snap-to-grid resizing) and style switching (`setInnerWidget()` swaps the inner tile at runtime, FR-53). Per-tile style persisted in layout JSON via SettingManager. `DashboardPage::createTile()` factory method instantiates the correct tile class by style string
- **Sidebar** — Sidebar navigation buttons are built programmatically in `app.cpp` (not defined in `.ui`) with grouped sections and collapse animation driven by `sigSidebarCollapseToggled`
- **CommandPalette** — `Ctrl+K` global command palette widget (`command_palette.h/.cpp`) for keyboard-driven navigation and actions

**Scale:** ~6,000–7,000 lines of C++ across core library + services + GUI, 14 pages, 34 translations, 3 themes.

---

## Architecture Strengths

### 1. Platform Abstraction via Abstract Base Classes

**How it works:** Shared headers in `shared/nexis-core/` define abstract base classes with `virtual ... = 0` for platform-specific methods and concrete implementations for shared logic. Platform subclasses (e.g., `CpuInfoLinux`, `CpuInfoMacOS`) in `linux/` and `macos/` directories `override` pure virtuals with platform-specific implementations. CMake include-path precedence still resolves platform directories first, but the compiler now **enforces** the contract via pure virtual methods.

```cpp
// shared/nexis-core/Info/cpu_info.h — abstract interface
class CpuInfo {
public:
    virtual ~CpuInfo() = default;
    virtual int getCpuCoreCount() const = 0;  // compile-time enforcement
    // ...
};

// macos/nexis-core/Info/cpu_info_macos.h
class CpuInfoMacOS : public CpuInfo {
    int getCpuCoreCount() const override;  // sysctl
};
```

**Managers use factory construction:**
```cpp
// info_manager.cpp constructor
#ifdef Q_OS_MACOS
    ci = std::make_unique<CpuInfoMacOS>();
#else
    ci = std::make_unique<CpuInfoLinux>();
#endif
```

**Why this is good:**
- **Compile-time enforcement** — missing platform methods produce clear "unimplemented pure virtual" errors, not opaque linker failures
- **Clean separation** — shared logic stays in abstract base, platform deltas isolated in subclasses
- **Two-tier hierarchy** — avoids unnecessary interface/base/impl three-tier complexity
- **Negligible overhead** — virtual dispatch at 1-30 Hz polling rates adds no measurable cost
- **Future-proof** — adding a new platform (e.g., Windows) requires creating new subclasses; the compiler guides what to implement

**Assessment:** This pattern provides **compiler-enforced platform contracts** while maintaining the pragmatic simplicity of the original include-path architecture. The 15 abstract base classes (11 Info + 4 Tool) cover all platform-specific code. `FanInfo` (FR-56) is the latest addition, following the same ThermalInfo pattern exactly (abstract base with `FanSensor` struct, macOS SMC subclass, Linux hwmon sysfs subclass). No new DataRefreshService signal was added — fan data piggybacks on the existing `tempUpdated` signal since both are read at 1s intervals from the same hardware monitoring subsystem.

---

### 2. Singleton Manager Facades

Six manager singletons act as **stable API surfaces** over the core library:

```cpp
// shared/nexis/Managers/info_manager.h — facade over 11 Info classes
class InfoManager {
public:
    static InfoManager *ins();

    // CPU — delegates to CpuInfo via unique_ptr
    int getCpuCoreCount() const;
    QList<int> getCpuPercents() const;

    // Memory — delegates to MemoryInfo via unique_ptr
    void updateMemoryInfo();
    quint64 getMemUsed() const;
    quint64 getMemTotal() const;
    // ... 50+ methods across 11 info providers

private:
    std::unique_ptr<CpuInfo> ci;     // Platform subclass via factory
    std::unique_ptr<MemoryInfo> mi;
    std::unique_ptr<DiskInfo> di;
    // ... 11 total — #ifdef Q_OS_MACOS in constructor
};
```

**Why this works:**
- **Stable API for pages** — `InfoManager::ins()->getCpuPercents()` won't change even if the underlying `CpuInfo` class is refactored
- **Lazy instance caching** — Info objects are constructed once and reused across all pages
- **Centralized refresh** — `updateMemoryInfo()` ensures a single refresh call feeds all consumers
- **Minimal boilerplate** — no DI frameworks, no factories, no configuration files

**Assessment:** For a ~5,000-line desktop app with no unit tests, singletons are **pragmatically correct**. The anti-pattern critique of singletons assumes multi-threaded systems or test-heavy codebases. Nexis is single-threaded GUI with a single user. The managers are effectively namespaced globals — and that's fine at this scale.

---

### 3. QSS Token System

The theme system uses a single QSS template with placeholder tokens, replaced at runtime:

```cpp
// shared/nexis/Managers/app_manager.cpp:88-121
void AppManager::updateStylesheet() {
    QString themeName = resolveThemeName();  // "default" (dark), "light", or auto-detect

    // Load color/spacing values for this theme
    mStyleValues = new QSettings(
        QString(":/static/themes/%1/style/values.ini").arg(themeName), ...);

    // Load the single QSS template (always from "default")
    mStylesheetFileContent = FileUtil::readStringFromFile(
        ":/static/themes/default/style/style.qss");

    // Replace @tokens: @color01 → "#36363a", @accentColor → "#E95420", etc.
    for (const QString &key : mStyleValues->allKeys()) {
        mStylesheetFileContent.replace(key, mStyleValues->value(key).toString());
    }

    // Replace @dpN tokens with DPI-scaled pixel values
    static const QRegularExpression dpRx("@dp(\\d+)");
    // ... regex replacement loop ...

    qApp->setStyleSheet(mStylesheetFileContent);
    emit SignalMapper::ins()->sigChangedAppTheme();
}
```

**Why this is good:**
- **Single source of truth** — one `style.qss` file, not three duplicated stylesheets
- **Theme = data, not code** — switching themes means loading a different `values.ini`
- **DPI scaling without QML** — the `@dpN` tokens solved HiDPI scaling (BUG-07) in QSS, avoiding a costly QML migration
- **Live switching** — Auto mode responds to `QStyleHints::colorSchemeChanged` (Qt 6.5+)
- **Zero hardcoded colors** — all C++ widgets accept token name strings (e.g., `"@cpuColor"`) and implement `refreshThemeColors()` methods connected to `sigChangedAppTheme`. This ensures every color resolves from `values.ini` at runtime, including chart series, sparklines, progress bars, shadows, and overlays (BUG-47). 24 extended tokens cover network upload colors, overlay/shadow colors (8-digit `#AARRGGBB` hex), and a 20-color chart series palette.

**User-configurable tokens** (like `@fontFamily`) are handled separately from theme tokens — they live in `SettingManager` (not `values.ini`) and are replaced after the theme token loop. This avoids polluting `values.ini` with non-color values that would fail hex validation.

**Bundled assets:** All icons use bundled SVGs from QRC resources rather than `QIcon::fromTheme()`, ensuring consistent visuals across desktop environments. Four font families (Inter, Ubuntu, JetBrains Mono) are embedded in the binary via `QFontDatabase::addApplicationFont()`, with a user-configurable font picker on the Settings page.

**Assessment:** **Elegant and maintainable.** This approach is better than the common alternative of maintaining separate QSS files per theme, which leads to divergence and missed updates.

---

### 4. Graceful Degradation

Optional features hide themselves when their hardware or software dependencies are absent:

```cpp
// shared/nexis/app.cpp — conditional page registration
if (ToolManager::ins()->checkDocker()) {
    dockerPage = new DockerPage();
    mListPages.insert(dockerIdx, dockerPage);
} else {
    ui->btnDocker->hide();  // Sidebar button hidden entirely
}
```

**The pattern applies at three levels:**
1. **Page level** — Docker, GNOME Settings, and APT/Homebrew pages are hidden entirely if tools aren't detected
2. **Widget level** — Battery, GPU, and temperature gauges hide if hardware is absent; disk health info is shown inline on the DiskTile when available
3. **Feature level** — macOS filters Apple system agents from startup apps; purge option hidden on non-APT systems

**Why this matters:** Cross-platform apps often show all features with "not available on this platform" messages, which clutters the UI. Nexis's approach presents a **clean, relevant interface** tailored to each system's actual capabilities.

---

### 5. SignalMapper for Cross-Component Events

A lightweight singleton event bus handles app-wide notifications:

```cpp
// shared/nexis/Managers/signal_mapper.h
class SignalMapper : public QObject {
    Q_OBJECT
signals:
    void sigChangedAppTheme();
    void sigUninstallStarted();
    void sigUninstallFinished();
    void sigScheduledCleanStarted(QString scheduleName);
    void sigScheduledCleanFinished(quint64 bytesFreed, int fileCount);
    void sigKioskToggleRequested();
    void sigKioskModeChanged(bool enabled);
    void sigAppVisibilityChanged(bool visible);
    void sigCleanableSizeChanged(quint64 totalBytes);
    void sigDashboardLayoutReset();
};
```

**Usage pattern:**
- Settings page changes theme → emits `sigChangedAppTheme()`
- All 14 pages listen → reload theme-dependent icons, GIF loaders, and colors
- Dashboard kiosk button → emits `sigKioskToggleRequested()` → App toggles kiosk mode → emits `sigKioskModeChanged(bool)` → Dashboard button swaps icon, tray action syncs checkmark
- App minimized/restored → emits `sigAppVisibilityChanged(bool)` → DataRefreshService pauses/resumes polling
- No page needs a pointer to any other page — complete decoupling

**Assessment:** With 12 signals (after FR-51 added `sigDashboardLayoutReset`), this is still **appropriately simple**. The kiosk mode signals (FR-30), visibility signal (FR-37), UI redesign signals `sigSidebarCollapseToggled` and `sigNavigateToPage` (FR-42), cleanable-size signal (FR-44), and dashboard layout reset signal (FR-51) demonstrate the pattern working well for decoupled communication between App, DashboardPage, DataRefreshService, SystemCleanerPage, SettingsPage, and the CommandPalette. A full event bus library (like eventpp) would be overkill until the signal count grows significantly (15+).

---

## Architecture Weaknesses

### ~~1. No Formal Platform Interfaces~~ (Resolved)

**Status:** Resolved in Phase 5 (FR-34). All 11 Info classes (including FanInfo added in FR-56) and 4 platform-split Tool classes now use abstract base classes with pure virtual methods in shared headers. Platform implementations are named subclasses (e.g., `CpuInfoLinux`, `CpuInfoMacOS`) that `override` each pure virtual. Missing platform methods are caught at compile time with clear "unimplemented pure virtual" errors.

**Pattern adopted (two-tier hierarchy):**
```cpp
// shared/nexis-core/Info/cpu_info.h — abstract base
class CpuInfo {
public:
    virtual ~CpuInfo() = default;
    virtual int getCpuCoreCount() const = 0;
    // ... pure virtual for platform methods
};

// macos/nexis-core/Info/cpu_info_macos.h
class CpuInfoMacOS : public CpuInfo {
    int getCpuCoreCount() const override;  // uses sysctl
};
```

InfoManager and ToolManager hold `std::unique_ptr<Interface>` members with `#ifdef Q_OS_MACOS` factory construction. Virtual dispatch overhead is negligible at 1-30 Hz polling rates.

---

### 2. ~~Singleton Coupling Limits Testability~~ (Partially Resolved)

**Status:** Partially resolved in Phase 6 (FR-35). All 10 page classes with manager dependencies now accept optional manager pointers via constructor parameters with `nullptr` defaults. When `nullptr`, pages fall back to the `::ins()` singleton. Production call sites in `app.cpp` are unchanged.

```cpp
// After (FR-35) — backward compatible with default argument
explicit DashboardPage(QWidget *parent = nullptr,
                       InfoManager *infoManager = nullptr,
                       SettingManager *settingManager = nullptr,
                       AppManager *appManager = nullptr,
                       SignalMapper *signalMapper = nullptr);

// Constructor initializer — ternary fallback
im(infoManager ? infoManager : InfoManager::ins()),
mSettingManager(settingManager ? settingManager : SettingManager::ins()),
mAppManager(appManager ? appManager : AppManager::ins()),
mSignalMapper(signalMapper ? signalMapper : SignalMapper::ins())

// Test code can now inject mocks:
MockInfoManager mockIM;
DashboardPage testPage(nullptr, &mockIM);
```

**Remaining limitation:** Child widgets (CircleBar, HistoryChart, DiskUsageLauncherWidget, etc.) still use `::ins()` directly. These are out of scope for FR-35 — they would need their own DI parameters or a different approach to fully decouple from singletons.

---

### 3. Manager Layer Inconsistency

The manager layer has two distinct personalities that create confusion about where logic belongs:

**Thin facades** (pass-through wrappers):
```cpp
// info_manager.cpp — just delegates
int InfoManager::getCpuCoreCount() const { return ci.getCpuCoreCount(); }
quint64 InfoManager::getMemUsed() const  { return mi.getMemUsed(); }
```

**Thick services** (real business logic):
- `CleanerService` — 300+ lines of scan logic across 8 categories (including Browser Privacy), file partitioning, min-age filtering, statistics collection
- `ScheduleManager` — 500+ lines of CRUD operations, JSON persistence, OS-native scheduler sync (launchd plists, systemd timers, cron entries)

**The problem:** There's no clear architectural principle for when logic belongs in:
- **Info/Tool classes** — data acquisition and system operations
- **Managers** — business logic and orchestration
- **Pages** — presentation and user interaction

This leads to ambiguity. The `CleanerService` duplicates some scanning logic that originated in `SystemCleanerPage`. New features face the question "does this go in the Manager or the Page?" without a guiding principle.

---

### 4. ~~CMake GLOB_RECURSE~~ (Resolved)

**Status:** Resolved in Phase 2. All `GLOB_RECURSE` calls replaced with explicit `set()` source lists in `CMakeLists.txt`. Source files are now organized into 10 explicit lists (shared/platform, core/GUI, .cpp/.h) plus a translations list. When adding or removing a file, developers update the corresponding `set()` block — CMake reconfiguration is deterministic.

---

### 5. ~~No Automated Test Suite~~ (Resolved)

**Status:** Unit test suite implemented in Phase 7 (FR-36). 6 CTest executables with 63 test methods covering utilities (FormatUtil, FileUtil, CommandUtil), core library parsing (DiskHealthInfo verdict logic + smartctl JSON parsing), manager logic (ScheduleManager next-run-time + display text), and theme token validation (both themes, all tokens).

**Refactoring for testability:**
- `parseSmartctlJson()` deduplicated from 2 platform files into shared public static `parseSmartctlJsonInto()`
- `deriveHealthVerdict()` made public static (pure struct logic)
- `getNextRunTime()` accepts optional `now` parameter for deterministic testing
- `PROJECT_SOURCE_DIR` compile definition enables theme tests to locate source-tree files

**Remaining barriers to broader testing:**
- ~~Singleton coupling (§2) blocks mock injection~~ — resolved in Phase 6 (FR-35)
- ~~Info classes read real OS state — no abstraction for test data~~ — resolved in Phase 5 (FR-34)
- Pages tightly bound to `.ui` files and Qt widgets (UI testing out of scope)
- CleanerService in GUI executable target — not linkable as library for unit tests

---

### ~~6. Fragmented Timer/Polling~~ (Resolved)

**Status:** Resolved in Phase 8 (FR-37). All per-page QTimers in Dashboard (3), Resources (2), and Processes (1) replaced with a centralized `DataRefreshService` singleton that owns 5 QTimers (1s fast, 5s medium, 30s slow, configurable process, 1h update) and emits 12 typed data-change signals. Pages subscribe as reactive consumers — no monitoring page owns a QTimer. The `mUpdateTimer` (FR-60) uses `QtConcurrent::run()` because update checks (esp. `softwareupdate -l`) are too slow for the main thread; a `mUpdateCheckRunning` bool prevents overlapping async checks.

**Fixes delivered:**
- 6 per-page QTimers → 5 centralized QTimers (in one location)
- Eliminated duplicate `updateMemoryInfo()`, `updateGpuInfo()`, and `refreshDiskHealth()` calls
- Fixed `getCpuPercents()` static-delta bug (two callers consuming same function-scope statics)
- Added pause/resume via `sigAppVisibilityChanged(bool)` — all polling stops when minimized to tray (kiosk mode overrides)
- Pages are simpler — receive typed data payloads, don't manage polling or call InfoManager directly

---

### ~~7. QSS Token Validation Gap~~ (Addressed)

**Status:** Addressed in Phase 3 (FR-32). `AppManager::updateStylesheet()` now includes two runtime validation passes before token replacement:

1. **Token existence check:** Scans the raw QSS template for `@token` patterns (regex `@([a-zA-Z][a-zA-Z0-9_]*)`), skips `@dpN` DPI tokens, and emits `qWarning()` for any token not found in the active theme's `values.ini`.
2. **Color format validation:** Iterates all `values.ini` entries (excluding `@themeName`) and warns if any value is not a valid CSS hex color (`#rgb`, `#rgba`, `#rrggbb`, or `#rrggbbaa`).

Both checks emit `qWarning()` at runtime (visible in debug output) without altering application behavior. This catches typos, missing tokens, and malformed color values during development and theme switching.

**Remaining gap (largely resolved):** Hardcoded inline `setStyleSheet()` calls were eliminated across 12 files in BUG-47. All widgets now use `refreshThemeColors()` methods connected to `sigChangedAppTheme` to re-resolve token colors on theme switch. The only remaining inline stylesheets use dynamically-resolved token values, not hardcoded hex colors. Missing QSS rules for unstyled widgets remain a potential issue detectable by screenshot regression tests (FR-41).

---

## Recommended Improvements

### Priority 1: Critical (Foundational)

#### ~~1A. Replace GLOB_RECURSE with Explicit Source Lists~~ (Done)

**Status:** Completed in Phase 2. All 9 `GLOB_RECURSE` calls replaced with inline `set()` blocks using Option A (inline lists). Source files organized into core shared/platform and GUI shared/platform categories with platform-conditional `if(APPLE)/else()` blocks.

---

#### ~~1B. Add Abstract Base Classes for Platform Code~~ (Done)

**Status:** Completed in Phase 5 (FR-34). All 11 Info classes (including FanInfo, FR-56) and 4 Tool classes converted to abstract base + platform subclass pattern. 29 new platform subclass headers created. InfoManager and ToolManager use `std::unique_ptr<Interface>` with `#ifdef Q_OS_MACOS` factory construction. PackageTool unified from divergent platform APIs (dpkg/rpm/pacman vs Homebrew) into a single abstract interface. ToolManager consolidated from 2 platform `.cpp` files to 1 shared file with `#ifdef` only in the constructor.

---

### Priority 2: High (Scalability)

#### ~~2A. Centralized DataRefreshService~~ (Done)

**Status:** Completed in Phase 8 (FR-37). Created `DataRefreshService` singleton (`shared/nexis/Managers/data_refresh_service.{h,cpp}`) with 5 QTimers (1s fast, 5s medium, 30s slow, configurable process, 1h update) and 12 typed data signals (`cpuUpdated`, `memoryUpdated`, `networkUpdated`, `diskIOUpdated`, `gpuUpdated`, `tempUpdated`, `fanUpdated`, `batteryUpdated`, `diskUsageUpdated`, `diskHealthUpdated`, `processesUpdated`, `systemUpdatesChecked`). The `memoryUpdated` signal was updated in FR-57 to use a `MemorySnapshot` struct (replacing 4 separate `quint64` parameters) carrying wired/active/inactive/compressed/available/pressureLevel fields alongside the original used/total/swapUsed/swapTotal. The `fanUpdated` signal was added in BUG-70 (previously fan updates piggybacked on `tempUpdated`). The `systemUpdatesChecked` signal was added in FR-60 for the hourly system update check, which runs via `QtConcurrent::run()` to avoid blocking the UI thread. This signal is consumed by `App` (sidebar badge update + tray notification) and `APTSourceManagerPage` (Available Updates tree widget), not by `DashboardPage` — the original dashboard tile approach was replaced with a sidebar badge + package page integration during the FR-60 redesign.

Converted Dashboard (removed 3 timers), Resources (removed 2 timers), and Processes (removed 1 timer) to reactive signal subscribers. Added `sigAppVisibilityChanged(bool)` to SignalMapper for pause/resume (kiosk mode overrides). DI constructor parameter follows FR-35 pattern.

**Results:** 6 per-page QTimers → 5 centralized QTimers. Zero duplicate InfoManager calls. Fixed `getCpuPercents()` static-delta bug. Battery optimization via pause on minimize.

**Per-process I/O delta tracking (FR-58/FR-59):** `ProcessInfoMacOS` and `ProcessInfoLinux` subclasses maintain `QHash<pid_t, QPair<quint64,quint64>>` maps for previous disk I/O counters and a `QElapsedTimer` to compute per-process byte rates (read/write per second). macOS additionally parses `nettop -x -P -L1 -J bytes_in,bytes_out` output for per-process network bandwidth. Linux reads `/proc/<pid>/io` for disk I/O; network columns show N/A on Linux (no viable non-privileged per-process network API). The 4 new fields (`diskReadRate`, `diskWriteRate`, `netDownRate`, `netUpRate`) are carried in the `Process` struct and displayed as hidden-by-default columns on the Processes page.

---

#### ~~2B. Dependency Injection for Managers~~ (Done)

**Status:** Completed in Phase 6 (FR-35). All 10 page classes with manager dependencies now accept optional `nullptr`-default constructor parameters with ternary fallback to `::ins()` singletons. All `::ins()` calls in page method bodies replaced with member variable access. Production call sites in `app.cpp` unchanged. 4 pages with zero manager dependencies (StartupAppsPage, HelpersPage, GnomeSettingsPage, DockerPage) required no changes. Child widgets still use `::ins()` directly (known limitation, future scope).

---

#### ~~2C. QSS Token Validation~~ (Done)

**Status:** Completed in Phase 3 (FR-32). Two validation passes added to `updateStylesheet()`: token existence check (QSS `@tokens` vs `values.ini` keys) and color format validation (hex format check on all non-`@themeName` values). Both emit `qWarning()` diagnostics.

---

### Priority 3: Medium (Testing & Quality)

#### 3A. ~~Basic Unit Test Suite~~ (Implemented)

**Completed (Phase 7, FR-36).** 6 test executables with 63 test methods:
1. **Utility classes** — FormatUtil (10 methods), FileUtil (10 methods), CommandUtil (9 methods)
2. **Info class parsing** — DiskHealthInfo verdict logic (14 methods) + smartctl JSON parsing (6 methods)
3. **Manager logic** — ScheduleManager `getNextRunTime()` (11 methods) + `frequencyDisplayText()` (4 methods)
4. **Theme validation** — Token resolution, color format, theme parity, full substitution (7 methods)

**Key refactoring:** `parseSmartctlJsonInto()` shared static (dedup), `deriveHealthVerdict()` public static, `getNextRunTime()` injectable `now` parameter. CMake macro `add_nexis_test()` for per-file executables. MemoryInfo/CpuInfo tests deferred (Linux-only /proc parsing). CleanerService tests deferred (GUI lib dependency).

---

#### 3B. CI Screenshot Regression Tests ✅

**Status:** Implemented (FR-41). The `test-ScreenshotTests` executable captures all 11 always-visible pages in both Dark and Light themes (22 screenshots per platform), compares against committed reference PNGs using a Qt-native pixel diff with configurable per-page tolerance, and uploads visual diff artifacts on failure. CI runs screenshot tests as non-blocking (`continue-on-error`) until references stabilize. Linux CI uses Xvfb for headless GUI rendering.

**Architecture change:** The GUI sources were extracted into a `nexis-gui` static library so that both the `nexis` executable and the screenshot test can link against them without duplicating the source list. Reference images are stored in-repo under `tests/reference_screenshots/{platform}/{theme}/`.

---

### Priority 4: Low (Future-Proofing)

#### 4A. Plugin Architecture

**What:** Define a `NexisPlugin` interface that allows third-party code to add pages, cleaning categories, or monitoring widgets.

**When:** Not needed now. 14 built-in pages cover the core use cases. Consider this only if community demand appears for extensibility (e.g., custom cleaner categories for gaming platforms, Kubernetes monitoring, Raspberry Pi GPIO).

**Rough design:**

```cpp
class NexisPlugin {
public:
    virtual ~NexisPlugin() = default;
    virtual QString name() const = 0;
    virtual QIcon icon() const = 0;
    virtual QWidget *createPage() = 0;
};
// Load from ~/.nexis/plugins/*.so at startup
```

---

#### 4B. Event Bus Upgrade

**What:** Replace `SignalMapper` with a typed event bus library if the signal count grows beyond ~15.

**When:** Currently 12 signals (SignalMapper) + 11 signals (DataRefreshService) — well within comfort zone. Only consider migration if the signal count grows significantly, or if events need filtering/prioritization.

**Candidate:** [eventpp](https://github.com/wqking/eventpp) (header-only, C++11+, well-tested).

---

## Strategic Direction

### QWidgets vs QML

**Question:** Should Nexis migrate from QWidgets to QML for the UI layer?

**Context:** BUG-07 (HiDPI scaling) is commonly cited as motivation for QML migration. Nexis solved this within QWidgets using `Dpi::scale()` + `@dpN` QSS tokens.

**Analysis:**

| Factor | QWidgets (Current) | QML (Migration) |
|--------|-------------------|-----------------|
| HiDPI | Solved via Dpi::scale() + @dpN tokens | Native support |
| Animations | Basic (SlidingStackedWidget) | Rich, declarative |
| Development speed | Qt Designer + QSS (familiar tooling) | QML + JavaScript (new skills) |
| Existing investment | 14 pages, 29 .ui files, comprehensive QSS | Complete rewrite required |
| Migration effort | N/A | 3-6 months minimum |
| Community contributions | C++/QSS (common skills) | QML (niche skills) |

**Recommendation: Stay with QWidgets.**

The QWidget stack is working well. HiDPI is solved. The theme system is elegant. The `.ui` files provide visual editing. A QML migration would be a multi-month rewrite with minimal user-facing benefit — the app already looks and works well. The effort would be far better spent on new features.

QML should only be reconsidered if a future feature genuinely requires it (e.g., complex data visualizations with smooth animations that QCharts can't handle).

---

### Testing Strategy

**Phase 1 (Done):** Test infrastructure — Qt Test + CTest + CI integration (FR-33).

**Phase 2 (Done):** Unit test suite — 6 test executables, 63 test methods (FR-36). Covers utility functions (FormatUtil, FileUtil, CommandUtil), DiskHealthInfo parsing + verdict logic, ScheduleManager next-run-time calculations, and theme token validation. Refactored production code for testability without changing behavior.

**Phase 3 (Future):** Expand test coverage:
- MemoryInfo/CpuInfo parsing (requires Linux mock data or platform-specific tests)
- CleanerService (requires extracting logic from GUI executable into library)
- SettingManager defaults and overrides
- Integration tests for manager CRUD operations

**Phase 4 (Done):** UI regression testing (FR-41):
- Screenshot comparison in CI — 11 pages × 2 themes per platform
- Qt-native pixel diff with configurable per-page tolerance
- Visual diff artifact upload on CI failure for manual review
- Non-blocking initially (`continue-on-error`) until references stabilize

**Current state:** 7 CTest executables — 63 unit test methods covering core library, utilities, manager logic, and theme validation, plus 1 screenshot regression test covering 22 page/theme combinations. Build system refactored to extract `nexis-gui` static library for test linkage.

---

### Incremental Evolution vs Clean-Slate Redesign

**Two paths forward:**

**Path A: Incremental Evolution (Recommended)**
- Implement P1 and P2 improvements (explicit CMake, abstract interfaces, DI, DataRefreshService, QSS validation)
- Add tests incrementally
- Keep QWidgets, keep singleton managers (with DI escape hatches)
- Each improvement is a self-contained change that can be reviewed and merged independently
- Risk: low. Each change is small and reversible.

**Path B: Clean-Slate Redesign**
- Full DI framework (e.g., Google Fruit)
- Event-driven architecture replacing all timers and SignalMapper
- QML migration
- Comprehensive test suite from day one
- Risk: high. 3-6 month rewrite during which no features can ship. The old codebase is abandoned.

**Recommendation: Path A.** Nexis is feature-complete, stable, and actively used. The architecture's weaknesses are real but manageable. Incremental improvements deliver value continuously without risking the working product. Path B is only justified if the architecture becomes a genuine blocker for critical features — which it currently is not.

---

### Architectural Vision

**Where Nexis's architecture should be in 12 months:**

1. ~~**Explicit source lists in CMake**~~ — Done (Phase 2)
2. **Abstract base classes for all platform code** — Compile-time enforcement of platform parity
3. ~~**Dependency injection on all page constructors**~~ — Done (Phase 6, FR-35): testable without framework overhead
4. ~~**Centralized DataRefreshService**~~ — Done (Phase 8, FR-37): 4 timers instead of 6 per-page, with pause/resume for battery optimization
5. **QSS token validation** — Build-time warnings for theme inconsistencies
6. ~~**20-30 unit tests**~~ — Done (Phase 7, FR-36): 63 test methods across 6 executables covering core library, utilities, managers, and theme validation
7. **Still QWidgets** — Proven, stable, with the HiDPI problem solved
8. **Still singletons** — But with DI constructors as escape hatches for testing

The architecture doesn't need a revolution. It needs **targeted reinforcements** in the areas that have historically caused bugs (theme validation, missing platform methods) and **structural preparation** for the features and quality bar the project is growing toward (testing, battery optimization, maintainability at scale).

---

## Appendix: Key Files for Architectural Work

| File | Why It Matters |
|------|---------------|
| `CMakeLists.txt` | Replace GLOB_RECURSE (§1A) |
| `shared/nexis-core/Info/cpu_info.h` | ~~Template for abstract base class pattern (§1B)~~ Done — now an abstract base class with pure virtuals, 9 other Info classes follow this pattern |
| `shared/nexis/Managers/info_manager.h` | ~~Add DI constructor args (§2B)~~ Done (FR-35), holds all Info instances |
| `shared/nexis/Managers/app_manager.cpp` | ~~Add QSS token validation (§2C)~~ Done — token + color format validation added |
| `shared/nexis/Pages/Dashboard/dashboard_page.cpp` | ~~Primary refactor target for DataRefreshService (§2A)~~ Done — subscribes to DataRefreshService signals, zero timers |
| `shared/nexis/Pages/Resources/resources_page.cpp` | ~~Secondary refactor target~~ Done — subscribes to DataRefreshService signals, zero timers |
| `shared/nexis/signal_mapper.h` | Global event bus (12 signals after FR-51) — monitor signal count growth |
| `shared/nexis/Pages/Dashboard/metric_tile_base.h/.cpp` | Abstract base class for all dashboard tile styles; defines common interface (setValue, addDataPoint, setDisplayMode, etc.) and optional disk methods (FR-53) |
| `shared/nexis/Pages/Dashboard/metric_tile.h/.cpp` | Sparkline style: QtCharts sparkline + progress bar + trend indicator; DisplayMode (Normal/Hero/Large) via QSS dynamic properties |
| `shared/nexis/Pages/Dashboard/gauge_tile.h/.cpp` | Gauge style: ¾-circle arc with conical gradient, percentage centered, QPainter-based (FR-53) |
| `shared/nexis/Pages/Dashboard/ring_tile.h/.cpp` | Ring style: full 360° activity ring with percentage inside, progress bar below (FR-53) |
| `shared/nexis/Pages/Dashboard/hybrid_tile.h/.cpp` | Hybrid style: compact gauge arc + mini QChartView sparkline below (FR-53) |
| `shared/nexis/Pages/Dashboard/speedometer_tile.h/.cpp` | Speedometer style: needle dial with tick marks and green→red gradient arc (FR-53) |
| `shared/nexis/Pages/Dashboard/vumeter_tile.h/.cpp` | VU Meter style: segmented vertical bar with position-based coloring + stats panel (FR-53) |
| `shared/nexis/Pages/Dashboard/network_tile.h/.cpp` | Dashboard network tile with dual QChart instances (separate RX/TX sparklines), two-row layout |
| `shared/nexis/Pages/Dashboard/dashboard_tile_wrapper.h/.cpp` | Decorator pattern wrapper providing drag-and-drop reordering, snap-to-grid resizing, and per-tile style switching via paintbrush button + QMenu (FR-51, FR-53) |
| `shared/nexis/Pages/Dashboard/disk_tile.h/.cpp` | Donut style (disk default): custom QPainter donut chart + setDriveHealth() cross-tile data flow (FR-43/FR-44) |
| `shared/nexis/Widgets/CommandPalette/command_palette.h/.cpp` | Ctrl+K command palette for keyboard-driven navigation and actions |
| `shared/nexis/Managers/cleaner_service.cpp` | Example of thick manager with real business logic |
| `shared/nexis/Managers/schedule_manager.cpp` | Example of OS-native integration complexity |
