# Nexis — Architecture Review

> A deep and comprehensive review of the Nexis architecture: how logic and UI work together, what's working well, what should change, and where the application should go next.
> Last updated: 2026-06-10 | Version 2.3.13
>
> By-the-numbers counts (LOC, page/service/signal/timer/test totals) are kept in a **single canonical table** in [`APPLICATION_OVERVIEW.md` → "By the numbers"](APPLICATION_OVERVIEW.md#project-identity). This document references those figures rather than repeating them, so a single edit there keeps both docs in sync. The CI check `scripts/check_doc_versions.sh` fails the build if either header drifts from `PROJECT_VERSION`.

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

Nexis is structured as a **four-tier desktop application**. Tier sizes (page/service/manager/info/tool/util counts) come from the canonical table in [`APPLICATION_OVERVIEW.md`](APPLICATION_OVERVIEW.md#project-identity); the diagram below names the modules at each tier.

```
┌────────────────────────────────────────────────────────────────────┐
│  UI Layer: 15 always-visible QWidget pages + 3 conditional         │
│  Each page owns its .ui file and presentation logic                │
│  Files: shared/nexis/Pages/*/*.cpp                                 │
├────────────────────────────────────────────────────────────────────┤
│  Service Layer: 9 Domain Services + NexisPage base class           │
│  StartupService, FileSearchService, HostService, ProcessService,   │
│  SystemServiceManager, DockerService, PackageService,              │
│  DuplicateFinderService, SnapshotService                           │
│  Files: shared/nexis/Services/*.cpp                                │
├────────────────────────────────────────────────────────────────────┤
│  Manager Layer: 8 Singletons                                       │
│  AppManager, InfoManager, ToolManager, SettingManager,             │
│  CleanerService, ScheduleManager, ProcessPrefsManager,             │
│  DataRefreshService                                                │
│  Files: shared/nexis/Managers/*.cpp                                │
├────────────────────────────────────────────────────────────────────┤
│  Core Library: nexis-core (static lib)                             │
│  Info providers + Tool classes + Utils (see canonical table)       │
│  Files: shared/nexis-core/**/*.cpp + {platform}/nexis-core/**      │
└────────────────────────────────────────────────────────────────────┘
```

**Key architectural decisions:**
- **Compile-time platform abstraction** — Abstract base classes with pure virtual methods; platform subclasses selected via `#ifdef Q_OS_MACOS` at construction time
- **Domain service layer (FR-42)** — Business logic extracted from pages into singleton services (`Services/` directory). Services handle async operations via `QThreadPool::start()` and emit result signals. Pages are thin UI consumers. Follows the `CleanerService` pattern.
- **Singleton managers with DI escape hatches** — Static `ins()` accessors with `std::unique_ptr` members; all page and widget constructors accept optional dependency pointers for test injection (FR-35, FR-42)
- **Centralized polling** — `DataRefreshService` singleton owns 5 QTimers (1s fast, 5s medium, 30s slow, configurable process, 1h update) and emits typed data signals; pages subscribe as reactive consumers. The process timer starts paused and is only active while the Processes page is visible (BUG-72). Bundle B (FR-103) added **subscriber gating**: pages call `subscribe(Signal::Cpu)` etc. in `onPageActivated`, and `onFastTick`/`onMediumTick` skip entire sample blocks when no page has subscribed to the matching signal — so a user sitting on Settings or Processes pays zero cost for CPU/memory/network/GPU/disk samples. Bundle B (FR-105) also introduced **PowerMode tiers** (Normal 1/5/30 s, Battery 2/10/60 s, Unfocused 5/30/60 s) driven by the new `sigAppFocusChanged` from `QEvent::WindowActivate/WindowDeactivate` plus the existing battery signal's `isPluggedIn` bit.
- **Page lifecycle hooks** — `NexisPage` base class with virtual `onPageActivated()`/`onPageDeactivated()` called by `App::pageClick()`. Three heavyweight pages (Dashboard, Resources, Processes) inherit from `NexisPage` and gate their slot handlers on visibility — tile repaints, chart updates, and process polling stop when the page is hidden (BUG-72). Dashboard and Resources additionally `subscribe/unsubscribe` to `DataRefreshService::Signal` values in their lifecycle hooks (FR-103).
- **Async subprocess helper** — `CommandUtil::execAsync(cmd, args) -> QFuture<ExecResult>` (FR-99) wraps `execWithStatus` in `QtConcurrent::run`. A debug-build UI-thread audit, gated on `NEXIS_ASSERT_ASYNC_EXEC`, catches new synchronous exec calls added on the main thread.
- **Streaming subprocess pattern** — two long-lived `QProcess` children using `readyReadStandardOutput`: `NettopStreamer` (FR-102, macOS) streams per-process network deltas from a single `nettop -P -d -s1` child (lifecycle tied to the `ProcessInfo::mCollectNetIO` toggle — FR-108 — so it only runs while the Processes page's network columns are visible); `NvidiaSmiStreamer` (FR-106 Step C, Linux) streams GPU utilization and fan speed from a single `nvidia-smi -l 1` child (app-lifetime). Both publish into a mutex-guarded `QHash` that the synchronous `ProcessInfo*::updateProcesses` / `GpuInfoLinux::updateGpuInfo` / `FanInfoLinux::readNvidiaSpeed` callers read from. `NvidiaSmiCache` is a thin facade namespace over `NvidiaSmiStreamer` so call sites don't change.
- **Linux /proc walk** — `ProcessInfoLinux::updateProcesses` (FR-127) reads `/proc/<pid>/{stat,status,cmdline}` directly every tick instead of forking `ps ax`. Parsing lives in a pure `ProcInfoParser` module — no Linux-specific includes — so fixture-based tests run on every platform. `sysconf` values, `/proc/stat btime`, and `/proc/meminfo MemTotal` are cached in the constructor; uid/gid → name lookups memoise `getpwuid_r`/`getgrgid_r` results.
- **Pre-clean snapshots (FR-112)** — `SnapshotService` wraps the platform system-restore tool. `CleanerService::maybeTakeSnapshot()` is called from both `clean()` and `SystemCleanerPage::systemClean()` (the direct path that bypasses `clean`), gated on the `PreCleanSnapshotEnabled` setting and `SnapshotService::isAvailable()`. Snapshot failure is non-fatal.
- **Cleaner trend history (FR-114)** — `CleanerService::scan()` persists per-category sizes into `CleanerCategoryTrends` (rolling 20-sample JSON blob in QSettings); `getCategoryTrend()` exposes the series; `CategorySparkline` (flat QPainter, no QChart) renders it under each System Cleaner category.
- **Move-to-Trash clean path (FR-113)** — `cleanFiles(paths, minAge, moveToTrashInstead=true)` substitutes `QFile::moveToTrash` for the default `rm -rf`. Used only by the new `DOWNLOADS_AGED` category so users can recover files if the age threshold was set wrong. Same code path still does `rm -rf` for every other category.
- **macOS crumbs scanner (FR-123)** — After `trashApps` / cask-uninstall, `CrumbsScanner` walks the six standard `~/Library/*` roots for leaf names prefixed by any uninstalled app's bundle id. `CrumbsReviewDialog` presents the hits in a table; Delete Selected moves via `QFile::moveToTrash`. Bundle ids come from the new `PlistUtil::readAppBundleInfo` helper and are now carried on the shared `Package` struct.
- **Per-process GPU collection (FR-115)** — Linux-only. The existing `/proc` walk in `ProcessInfoLinux::updateProcesses` (FR-127) grew a third pass gated on a new `mCollectGpu` toggle that follows the same column-visibility pattern as FR-108 (disk/net I/O). Intel/AMD read `/proc/<pid>/fdinfo/*` via the pure `ProcInfoParser::parseDrmFdinfo`; NVIDIA falls back to the new `NvidiaSmiPmonStreamer` — a two-stream persistent child process manager in the same pattern as `NvidiaSmiStreamer` (FR-106 Step C) and `NettopStreamer` (FR-102). Per-PID engine-ns baselines (`mPrevGpuEngineNs`) deliver the `%GPU` delta.
- **Pinned-at-top process sort (FR-116)** — New `PinSortFilterProxyModel` overrides `lessThan` to compare a custom `PinnedRole` before delegating to the base class, so pinned rows stay at the top regardless of sort column or direction. Pin and threshold state persists in a new `ProcessPrefsManager` singleton (JSON-in-QSettings, same shape as `ScheduleManager` / `CleanerExclusions`). Row-level context menu is distinct from the existing header context menu.
- **Threshold alerts with per-(name, metric) hysteresis (FR-116)** — `ProcessesPage::evaluateThresholdAlerts` aggregates RSS and CPU% across PIDs sharing a comm, compares against thresholds set via `ProcessAlertDialog`, and fires `QSystemTrayIcon::Warning` notifications with `mAlertArmed` hysteresis keyed by `"<name>::<metric>"` — so a breach fires once, re-arms when the reading falls back below threshold.
- **Listening-port trusted-binder audit (FR-121)** — `OpenPortsWidget` resolves each PID's binary path (`readlink /proc/<pid>/exe` on Linux, `proc_pidpath` on macOS) and flags any path that doesn't start with a platform-trusted prefix or a user-configured extra. The default-refresh path is cheap (single stat per PID). `codesign -dv` runs lazily behind a macOS "Verify Signatures" button via `QThreadPool` with per-path caching.
- **Helpers tuning cards (FR-81, FR-117, FR-118)** — Three new Helpers-page stacked cards following the FirewallWidget template (async `QThreadPool` fetch → statusFetched signal → `refreshThemeColors` bound to `sigChangedAppTheme`). Write paths all go through `CommandUtil::sudoExec` (pkexec). Because `sudoExec` swallows errors, every write is followed by a sysfs re-read to verify the change landed before reporting success.
- **Root-file writer (FR-81)** — New `FileUtil::writeRootFile(path, content)` wraps `sudoExec("tee", {path}, content)` with a read-back byte compare. Linux-only; the macOS build compiles to a no-op returning false. Shared by FR-81's sysctl.d persistence.
- **CPU tuning sysfs helper (FR-117)** — New `CpuTuning` namespace in `linux/nexis-core/Info/cpu_tuning.{h,cpp}` centralises sysfs reads/writes for scaling_{min,max}_freq, scaling_governor, intel_pstate/no_turbo, and cpufreq/boost. Handles both per-core and glob-write shell idioms with injection-safe governor validation. Used by `CpuTuningWidget` and by `App::init`'s persist-on-launch re-apply.
- **QSS theming** — Single stylesheet template with `@token` replacement at runtime
- **Qt signals** — `SignalMapper` singleton as a lightweight global event bus (10 signals after BUG-82 cleanup + FR-105's `sigAppFocusChanged` — see canonical "By the numbers" table)
- **Dashboard widgets** — Dashboard gauges replaced with a family of interchangeable tile widgets inheriting from `MetricTileBase` (abstract base class, FR-53). 8 widget styles available: `MetricTile` (sparkline), `GaugeTile` (¾-arc), `HybridTile` (arc + mini sparkline), `RingTile` (360° ring), `SpeedometerTile` (needle dial), `VuMeterTile` (segmented bar), `DiskTile` (donut chart), and `HealthScoreTile` (composite score). The base class provides 6 shared protected helpers — `createGearButton()`, `repositionGearButton()`, `createFooterLayout()`, `updateTrend()`, `updateGearIcon()`, `applyActionButtonStyle()` — and 5 shared UI members (`mGearButton`, `mLblSubtitle`, `mLblTrend`, `mBtnAction`, `mCurrentTrend`) so each subclass contains only its unique visualization code (FR-77). All styles support three `DisplayMode` values (Normal/Hero/Large) via QSS dynamic properties — mode changes trigger `unpolish()`/`polish()` to re-evaluate QSS selectors for per-mode font sizing. CPU and Memory are independent tile instances (the combined `HeroCard` was removed in FR-50). `NetworkTile` uses dual `QChart` instances (separate RX/TX sparklines) and is excluded from style switching. `DiskTile` uses custom `QPainter` donut chart and exposes `setDriveHealth()` for cross-tile data flow. `DashboardTileWrapper` (FR-51) uses the decorator pattern to add edit-mode mouse handling (drag-and-drop, snap-to-grid resizing) and style switching (`setInnerWidget()` swaps the inner tile at runtime, FR-53). Per-tile style persisted in layout JSON via SettingManager. `DashboardPage::createTile()` factory method instantiates the correct tile class by style string. `HealthScoreTile` (FR-73) is a specialized tile showing a composite 0–100 health score aggregated from 6 components (CPU, memory, disk, temperature, battery, SMART) via `HealthScoreCalculator` helper; unavailable components have their weights redistributed proportionally. In Large/Hero mode, per-component breakdown bars are drawn in `paintEvent`
- **Maintenance Wizard** — `MaintenanceWizardDialog` (FR-83) is a modal QDialog launched from the Health Score tile's quick action button. It orchestrates 4 existing services (CleanerService, ToolManager, InfoManager, HealthScoreCalculator) via `QtConcurrent::run()` with `QMetaObject::invokeMethod()` for cross-thread result marshaling. Thread-safe completion counting via `QAtomicInt`. Custom types (`CleanerService::ScanResult`, `QList<OrphanPackage>`, `UpdateCheckResult`) registered with `qRegisterMetaType` for cross-thread signals. Follows the `ExclusionManagerDialog` pattern (programmatic QDialog, no .ui file, DI constructor with singleton fallbacks).
- **Sidebar** — Sidebar navigation buttons are built programmatically in `app.cpp` (not defined in `.ui`) with grouped sections and collapse animation driven by sidebar collapse animation
- **CommandPalette** — `Ctrl+K` global command palette widget (`command_palette.h/.cpp`) for keyboard-driven navigation and actions

**Scale:** see the canonical "By the numbers" table in [`APPLICATION_OVERVIEW.md`](APPLICATION_OVERVIEW.md#project-identity) for LOC, page/service/manager counts, themes, translations, and tests.

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

**Assessment:** This pattern provides **compiler-enforced platform contracts** while maintaining the pragmatic simplicity of the original include-path architecture. The 17 abstract base classes (12 Info + 5 Tool) cover all platform-specific code. `PowerProfileInfo` (FR-70) is among the latest additions: abstract base with PPD + sysfs dual-backend Linux subclass and macOS stub. It does not poll via DataRefreshService — the profile is read on demand when the Helpers page is activated. `LogProvider` (FR-71) follows the same two-tier pattern for system log reading: abstract base with `LogProviderLinux` (journalctl JSON) and `LogProviderMacOS` (log show ndjson) subclasses, selected via `createForPlatform()` factory. `RepoHealthChecker` (repository health) adds another platform-specific tool: abstract base with 6-check Linux subclass (`RepoHealthCheckerLinux`: connection, release 404, GPG expiry, suite mismatch, duplicates, format) and 4-check macOS subclass (`RepoHealthCheckerMac`: tap reachable, outdated, deprecated, pinned), both polled by DataRefreshService after update checks.

---

### 2. Singleton Manager Facades

Eight manager singletons (see canonical "By the numbers" table) act as **stable API surfaces** over the core library:

```cpp
// shared/nexis/Managers/info_manager.h — facade over the Info classes
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
    // ... 50+ methods across the info providers

private:
    std::unique_ptr<CpuInfo> ci;     // Platform subclass via factory
    std::unique_ptr<MemoryInfo> mi;
    std::unique_ptr<DiskInfo> di;
    // ... one unique_ptr per Info class — #ifdef Q_OS_MACOS in constructor
};
```

**Why this works:**
- **Stable API for pages** — `InfoManager::ins()->getCpuPercents()` won't change even if the underlying `CpuInfo` class is refactored
- **Lazy instance caching** — Info objects are constructed once and reused across all pages
- **Centralized refresh** — `updateMemoryInfo()` ensures a single refresh call feeds all consumers
- **Minimal boilerplate** — no DI frameworks, no factories, no configuration files

**Assessment:** For a single-process Qt desktop app with the test coverage tracked in the canonical "By the numbers" table, singletons are **pragmatically correct**. The anti-pattern critique of singletons assumes multi-threaded systems or test-heavy codebases. Nexis is single-threaded GUI with a single user. The managers are effectively namespaced globals — and that's fine at this scale.

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
- **Zero hardcoded colors** — all C++ widgets accept token name strings (e.g., `"@cpuColor"`) and resolve colors from `values.ini` at runtime, including chart series, sparklines, progress bars, shadows, and overlays (BUG-47). 24 extended tokens cover network upload colors, overlay/shadow colors (8-digit `#AARRGGBB` hex), and a 20-color chart series palette. Static and semi-dynamic styles use central QSS selectors (including `[status="success/warning/error"]` property selectors for runtime color choices); only per-instance dynamic styles (metric tile colors, chart series) use inline `setStyleSheet()` with token-resolved values (FR-88).

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
// shared/nexis/signal_mapper.h
class SignalMapper : public QObject {
    Q_OBJECT
signals:
    void sigChangedAppTheme();
    void sigUninstallStarted();
    void sigUninstallFinished();
    void sigKioskToggleRequested();
    void sigKioskModeChanged(bool enabled);
    void sigAppVisibilityChanged(bool visible);
    void sigAppFocusChanged(bool focused);         // FR-105: drives PowerMode cadence
    void sigNavigateToPage(const QString &pageTitle);
    void sigCleanableSizeChanged(quint64 bytes);
    void sigDashboardFooterChanged(bool visible);
};
```

**Usage pattern:**
- Settings page changes theme → emits `sigChangedAppTheme()`
- Every page listens → reloads theme-dependent icons, GIF loaders, and colors
- Dashboard kiosk button → emits `sigKioskToggleRequested()` → App toggles kiosk mode → emits `sigKioskModeChanged(bool)` → Dashboard button swaps icon, tray action syncs checkmark
- App minimized/restored → emits `sigAppVisibilityChanged(bool)` → DataRefreshService pauses/resumes polling
- No page needs a pointer to any other page — complete decoupling

**Assessment:** With 10 signals today (BUG-82 cleanup removed unused signals; FR-105 added `sigAppFocusChanged`; the unused `sigDashboardLayoutReset` was removed when Reset Layout switched to a direct method call), this is still **appropriately simple**. The kiosk mode signals (FR-30), visibility signal (FR-37), focus signal (FR-105), `sigNavigateToPage` (FR-42), cleanable-size signal (FR-44), and dashboard footer visibility signal (FR-75) demonstrate the pattern working well for decoupled communication between App, DashboardPage, DataRefreshService, SystemCleanerPage, SettingsPage, and the CommandPalette. A full event bus library (like eventpp) would be overkill until the signal count grows significantly (15+). SignalMapper remains stable and focused on app-wide lifecycle events, while DataRefreshService handles domain-specific data delivery signals (see the canonical "By the numbers" table for the current `DataRefreshService` signal count).

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
- `CleanerService` — 300+ lines of scan logic across 9 categories (including Browser Privacy, Snap/Flatpak Revisions), file partitioning, min-age filtering, statistics collection, exclusion rule management (file/folder path matching with symlink resolution)
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

**Status:** Unit test suite implemented in Phase 7 (FR-36), then significantly expanded in FR-76. Now 22 CTest executables with ~304 test methods covering utilities (FormatUtil, FileUtil, CommandUtil), core library parsing (DiskHealthInfo, MemoryInfo, CpuInfo, GpuInfo, FanInfo, ThermalInfo, BatteryInfo, DiskInfo), tool parsing (AptSourceTool, PackageTool), widget parsing (NetworkDiag, OpenPorts, Firewall), service logic (HostService), manager logic (ScheduleManager), power profile parsing (PowerProfileInfo), theme token validation, and screenshot regression tests.

**Refactoring for testability:**
- `parseSmartctlJson()` deduplicated from 2 platform files into shared public static `parseSmartctlJsonInto()`
- `deriveHealthVerdict()` made public static (pure struct logic)
- `getNextRunTime()` accepts optional `now` parameter for deterministic testing
- `PROJECT_SOURCE_DIR` compile definition enables theme tests to locate source-tree files
- **FR-76 static parser pattern:** Parsing logic extracted from instance methods into public static methods on shared base classes (`*_shared.cpp` files). Static methods accept raw text/data and return structured results, enabling fixture-based testing without mocking CommandUtil or the filesystem. Applied to: MemoryInfo (3 methods), CpuInfo (5 methods), GpuInfo (4 methods), AptSourceTool (2 methods), FanInfo (3 methods), ThermalInfo (2 methods), BatteryInfo (2 methods), DiskInfo (1 method), HostService (promoted to static)

**Remaining barriers to broader testing:**
- ~~Singleton coupling (§2) blocks mock injection~~ — resolved in Phase 6 (FR-35)
- ~~Info classes read real OS state — no abstraction for test data~~ — resolved in Phase 5 (FR-34) and FR-76 (static parser extraction)
- Pages tightly bound to `.ui` files and Qt widgets (UI testing out of scope)
- CleanerService in GUI executable target — not linkable as library for unit tests (deferred to BUG-93)

---

### ~~6. Fragmented Timer/Polling~~ (Resolved)

**Status:** Resolved in Phase 8 (FR-37). All per-page QTimers in Dashboard (3), Resources (2), and Processes (1) replaced with a centralized `DataRefreshService` singleton that owns 5 QTimers (1s fast, 5s medium, 30s slow, configurable process, 1h update) and emits 12 typed data-change signals. Pages subscribe as reactive consumers — no monitoring page owns a QTimer. The `mUpdateTimer` (FR-60) uses `QtConcurrent::run()` because update checks (esp. `softwareupdate -l`) are too slow for the main thread; a `mUpdateCheckRunning` bool prevents overlapping async checks.

**Fixes delivered:**
- 6 per-page QTimers → 5 centralized QTimers (in one location)
- Eliminated duplicate `updateMemoryInfo()`, `updateGpuInfo()`, and `refreshDiskHealth()` calls
- Fixed `getCpuPercents()` static-delta bug (two callers consuming same function-scope statics)
- Added pause/resume via `sigAppVisibilityChanged(bool)` — all polling stops when minimized to tray (kiosk mode overrides)
- Pages are simpler — receive typed data payloads, don't manage polling or call InfoManager directly
- Process timer independently pauseable via `pauseProcessTimer()`/`resumeProcessTimer()` — starts paused, only runs while Processes page is visible (BUG-72)
- **BUG-72 Tier 2/3:** Eliminated remaining performance hotspots:
  - Replaced `iostat` subprocess (1s main-thread block per tick) with IOKit `IOBlockStorageDriver` API (~0.5ms, correct read/write separation)
  - Cached `getAvgClock()` result (eliminated 2 subprocess calls per tick on Apple Silicon)
  - Moved `discoverDrives()` to `QtConcurrent::run()` (300-1000ms off main thread every 30s)
  - In-place `QStandardItemModel` updates in ProcessesPage (eliminated 6,800 item allocations per tick)
  - Changed signal signatures to `const&` (eliminated ~2,800 deep string copies per emission)
  - Merged duplicate `getifaddrs()` walks into single `updateNetworkBytes()` method
  - Cached `hw.memsize`, `host_page_size()`, and GPU registry entry IDs (read once, not per tick)

---

### ~~7. QSS Token Validation Gap~~ (Addressed)

**Status:** Addressed in Phase 3 (FR-32). `AppManager::updateStylesheet()` now includes two runtime validation passes before token replacement:

1. **Token existence check:** Scans the raw QSS template for `@token` patterns (regex `@([a-zA-Z][a-zA-Z0-9_]*)`), skips `@dpN` DPI tokens, and emits `qWarning()` for any token not found in the active theme's `values.ini`.
2. **Color format validation:** Iterates all `values.ini` entries (excluding `@themeName`) and warns if any value is not a valid CSS hex color (`#rgb`, `#rgba`, `#rrggbb`, or `#rrggbbaa`).

Both checks emit `qWarning()` at runtime (visible in debug output) without altering application behavior. This catches typos, missing tokens, and malformed color values during development and theme switching.

**Remaining gap (largely resolved):** Hardcoded inline `setStyleSheet()` calls were eliminated in BUG-47. FR-88 migrated 35 static and semi-dynamic inline styles to central QSS, introducing generic `[status="..."]` property selectors for runtime color choices (success/warning/error). The remaining inline stylesheets (~8 calls in FR-89) are complex cases requiring per-severity scoped button rules or dynamically-created widget styling. All remaining inline styles use dynamically-resolved token values, not hardcoded hex colors.

---

## Recommended Improvements

### Priority 1: Critical (Foundational)

#### ~~1A. Replace GLOB_RECURSE with Explicit Source Lists~~ (Done)

**Status:** Completed in Phase 2. All 9 `GLOB_RECURSE` calls replaced with inline `set()` blocks using Option A (inline lists). Source files organized into core shared/platform and GUI shared/platform categories with platform-conditional `if(APPLE)/else()` blocks.

---

#### ~~1B. Add Abstract Base Classes for Platform Code~~ (Done)

**Status:** Completed in Phase 5 (FR-34). All 12 Info classes (including FanInfo FR-56, PowerProfileInfo FR-70) and 4 Tool classes converted to abstract base + platform subclass pattern. 31 new platform subclass headers created. InfoManager and ToolManager use `std::unique_ptr<Interface>` with `#ifdef Q_OS_MACOS` factory construction. PackageTool unified from divergent platform APIs (dpkg/rpm/pacman vs Homebrew) into a single abstract interface. ToolManager consolidated from 2 platform `.cpp` files to 1 shared file with `#ifdef` only in the constructor.

---

### Priority 2: High (Scalability)

#### ~~2A. Centralized DataRefreshService~~ (Done)

**Status:** Completed in Phase 8 (FR-37). Created `DataRefreshService` singleton (`shared/nexis/Managers/data_refresh_service.{h,cpp}`) with 5 QTimers (1s fast, 5s medium, 30s slow, configurable process, 1h update) and 15 typed data signals — `cpuUpdated`, `memoryUpdated`, `networkUpdated`, `networkPerInterfaceUpdated`, `diskIOUpdated`, `gpuUpdated`, `tempUpdated`, `fanUpdated`, `batteryUpdated`, `diskUsageUpdated`, `diskHealthUpdated`, `processesUpdated`, `systemUpdatesChecked`, `repoHealthChecked`, plus Linux-only `psiUpdated` (FR-124). `networkPerInterfaceUpdated` (SSO-351) carries a `QHash<QString, NetInterfaceStats>` snapshot so `NetUsageTracker` can record traffic for every up+running interface; the legacy `networkUpdated(rx, tx)` is retained for the live-rate displays which only need default-iface totals. The `memoryUpdated` signal was updated in FR-57 to use a `MemorySnapshot` struct (replacing 4 separate `quint64` parameters) carrying wired/active/inactive/compressed/available/pressureLevel fields alongside the original used/total/swapUsed/swapTotal. The `fanUpdated` signal was added in BUG-70 (previously fan updates piggybacked on `tempUpdated`). The `systemUpdatesChecked` signal was added in FR-60 for the hourly system update check, which runs via `QtConcurrent::run()` to avoid blocking the UI thread. This signal is consumed by `App` (sidebar badge update + tray notification) and `APTSourceManagerPage` (Available Updates tree widget), not by `DashboardPage` — the original dashboard tile approach was replaced with a sidebar badge + package page integration during the FR-60 redesign. The `repoHealthChecked` signal was added for repository health dashboard validation (chained after update checks, emits `RepoHealthCache` with per-repo health results), consumed by `APTSourceManagerPage` for card enrichment and side panel display.

Converted Dashboard (removed 3 timers), Resources (removed 2 timers), and Processes (removed 1 timer) to reactive signal subscribers. Added `sigAppVisibilityChanged(bool)` to SignalMapper for pause/resume (kiosk mode overrides). DI constructor parameter follows FR-35 pattern.

**Results:** 6 per-page QTimers → 5 centralized QTimers. Zero duplicate InfoManager calls. Fixed `getCpuPercents()` static-delta bug. Battery optimization via pause on minimize. Page-aware data gating (BUG-72 Tier 1): Dashboard, Resources, and Processes pages inherit `NexisPage` and gate slot handlers on visibility — tile repaints, chart updates, and process polling stop when the page is hidden. Delta-tracking statics (network/disk I/O) are maintained even when hidden to prevent data spikes on reactivation. BUG-72 Tier 2/3: All signal signatures use `const&` to avoid deep copies on emission. `getDiskIO()` replaced `iostat` subprocess with IOKit API. `getAvgClock()` result cached (constant on Apple Silicon). `discoverDrives()` runs async via `QtConcurrent::run()`. ProcessesPage updates model rows in-place instead of nuke-and-rebuild. Network bytes collected in single `getifaddrs()` walk. Memory constants and GPU registry IDs cached on first call.

**Per-process I/O delta tracking (FR-58/FR-59):** `ProcessInfoMacOS` and `ProcessInfoLinux` subclasses maintain `QHash<pid_t, QPair<quint64,quint64>>` maps for previous disk I/O counters and a `QElapsedTimer` to compute per-process byte rates (read/write per second). macOS additionally parses `nettop -x -P -L1 -J bytes_in,bytes_out` output for per-process network bandwidth. Linux reads `/proc/<pid>/io` for disk I/O; network columns show N/A on Linux (no viable non-privileged per-process network API). The 4 new fields (`diskReadRate`, `diskWriteRate`, `netDownRate`, `netUpRate`) are carried in the `Process` struct and displayed as hidden-by-default columns on the Processes page.

---

#### ~~2B. Dependency Injection for Managers~~ (Done)

**Status:** Completed in Phase 6 (FR-35). All 10 page classes with manager dependencies now accept optional `nullptr`-default constructor parameters with ternary fallback to `::ins()` singletons. All `::ins()` calls in page method bodies replaced with member variable access. Production call sites in `app.cpp` unchanged. 4 pages with zero manager dependencies (StartupAppsPage, HelpersPage, GnomeSettingsPage, DockerPage) required no changes. Child widgets still use `::ins()` directly (known limitation, future scope).

---

#### ~~2C. QSS Token Validation~~ (Done)

**Status:** Completed in Phase 3 (FR-32). Two validation passes added to `updateStylesheet()`: token existence check (QSS `@tokens` vs `values.ini` keys) and color format validation (hex format check on all non-`@themeName` values). Both emit `qWarning()` diagnostics.

---

### Priority 3: Medium (Testing & Quality)

#### 3A. ~~Basic Unit Test Suite~~ (Implemented + Expanded)

**Completed (Phase 7, FR-36; expanded FR-76, FR-79/FR-80, FR-82, FR-66, FR-68).** 22 test executables with ~304 test methods:
1. **Utility classes** — FormatUtil (10), FileUtil (10), CommandUtil (9)
2. **Info class parsing** — DiskHealthInfo (20), MemoryInfo (14), CpuInfo (19), GpuInfo (23), FanInfo (16), ThermalInfo (11), BatteryInfo (12), DiskInfo (17)
3. **Tool parsing** — AptSourceTool (14), PackageTool (16)
4. **Widget parsing** — NetworkDiag (24), OpenPorts (20), Firewall (15)
5. **Service logic** — HostService (25)
6. **Manager logic** — ScheduleManager (15), CleanerExclusions (11)
7. **Theme validation** — ThemeTokens (7)

**Key refactoring:** FR-36 established the pattern with `parseSmartctlJsonInto()` shared static (dedup), `deriveHealthVerdict()` public static, `getNextRunTime()` injectable `now` parameter. FR-76 scaled this to 10 additional classes by extracting parsing logic into public static methods on shared base classes (`*_shared.cpp` files). Fixture data files in `tests/fixtures/` provide deterministic test input. CleanerExclusions tests link against `nexis-gui` library to access CleanerService.

---

#### 3B. CI Screenshot Regression Tests (local-only)

**Status:** Implemented (FR-41), then **descoped from CI** in WI-19. The `test-ScreenshotTests` executable captures the always-visible pages in both Dark and Light themes, compares against committed reference PNGs using a Qt-native pixel diff with configurable per-page tolerance, and produces visual diff artifacts on failure. The build workflow now explicitly excludes the suite from `ctest` runs via `-E ScreenshotTests` (see `.github/workflows/build.yml`) — environment-dependent rendering on hosted runners produced too many false positives. Screenshot diffs are run locally via `scripts/update_screenshots.sh`; CI continues to assert the always-visible pages compile and link by building `test-ScreenshotTests` itself.

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

**When:** SignalMapper + DataRefreshService signal counts are tracked in the canonical "By the numbers" table — well within comfort zone. Only consider migration if those counts grow significantly, or if events need filtering/prioritization.

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
| Existing investment | All Pages (see canonical table), 29 .ui files, comprehensive QSS | Complete rewrite required |
| Migration effort | N/A | 3-6 months minimum |
| Community contributions | C++/QSS (common skills) | QML (niche skills) |

**Recommendation: Stay with QWidgets.**

The QWidget stack is working well. HiDPI is solved. The theme system is elegant. The `.ui` files provide visual editing. A QML migration would be a multi-month rewrite with minimal user-facing benefit — the app already looks and works well. The effort would be far better spent on new features.

QML should only be reconsidered if a future feature genuinely requires it (e.g., complex data visualizations with smooth animations that QCharts can't handle).

---

### Testing Strategy

**Phase 1 (Done):** Test infrastructure — Qt Test + CTest + CI integration (FR-33).

**Phase 2 (Done):** Unit test suite — 6 test executables, 63 test methods (FR-36). Covers utility functions (FormatUtil, FileUtil, CommandUtil), DiskHealthInfo parsing + verdict logic, ScheduleManager next-run-time calculations, and theme token validation. Refactored production code for testability without changing behavior.

**Phase 3 (Done):** Expanded test coverage (FR-76) — 8 new test suites, ~151 additional test methods. Extracted parsing logic from 10 Info/Tool/Service classes into public static methods on shared base classes, created fixture data files in `tests/fixtures/`, and wrote comprehensive parser tests. Covers MemoryInfo, CpuInfo, GpuInfo, AptSourceTool, FanInfo, ThermalInfo, BatteryInfo, DiskInfo, and HostService.

**Phase 4 (Done):** UI regression testing (FR-41):
- Screenshot comparison (local-only since WI-19) — always-visible pages × Dark/Light per platform
- Qt-native pixel diff with configurable per-page tolerance
- Visual diff artifact upload on CI failure for manual review
- Non-blocking initially (`continue-on-error`) until references stabilize

**Phase 5 (Future):** Remaining coverage gaps:
- CleanerService (requires extracting logic from GUI executable, blocked by BUG-93)
- PackageTool / UpdateInfo (dpkg/pacman/brew output parsing)
- AppManager token replacement (BUG-49 regression tests)
- SettingManager defaults and overrides
- Integration tests for manager CRUD operations

**Current state:** see the canonical "By the numbers" table in [`APPLICATION_OVERVIEW.md`](APPLICATION_OVERVIEW.md#project-identity) for the live CTest executable / test-method counts. The suites cover core library parsers, utilities, tool parsing, widget parsing, service logic, manager logic, and theme validation, plus 1 screenshot regression test. Build system refactored to extract `nexis-gui` static library for test linkage. Static parser extraction pattern established for future test additions.

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
4. ~~**Centralized DataRefreshService**~~ — Done (Phase 8, FR-37): 5 centralized timers (fast/medium/slow/process/update) instead of 6 per-page QTimers, with pause/resume for battery optimization
5. **QSS token validation** — Build-time warnings for theme inconsistencies
6. ~~**20-30 unit tests**~~ — Done (Phase 7, FR-36; expanded FR-76, FR-79/FR-80, FR-82, FR-66, FR-68): see the canonical "By the numbers" table in [`APPLICATION_OVERVIEW.md`](APPLICATION_OVERVIEW.md#project-identity) for the live test-method count; coverage spans core parsers, utilities, tools, widget parsers, services, managers, and theme validation
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
| `shared/nexis/signal_mapper.h` | Global event bus (10 signals — see canonical "By the numbers" table) — monitor signal count growth |
| `shared/nexis/Pages/Dashboard/metric_tile_base.h/.cpp` | Abstract base class for all dashboard tile styles; defines common interface (setValue, addDataPoint, setDisplayMode, etc.), 6 shared helpers (gear button, footer layout, trend computation, action button styling), and 5 shared UI members (FR-53, FR-77) |
| `shared/nexis/Pages/Dashboard/metric_tile.h/.cpp` | Sparkline style: QtCharts sparkline + progress bar + trend indicator; DisplayMode (Normal/Hero/Large) via QSS dynamic properties |
| `shared/nexis/Pages/Dashboard/gauge_tile.h/.cpp` | Gauge style: ¾-circle arc with conical gradient, percentage centered, QPainter-based (FR-53) |
| `shared/nexis/Pages/Dashboard/ring_tile.h/.cpp` | Ring style: full 360° activity ring with percentage inside, progress bar below (FR-53) |
| `shared/nexis/Pages/Dashboard/hybrid_tile.h/.cpp` | Hybrid style: compact gauge arc + mini QChartView sparkline below (FR-53) |
| `shared/nexis/Pages/Dashboard/speedometer_tile.h/.cpp` | Speedometer style: needle dial with tick marks and green→red gradient arc (FR-53) |
| `shared/nexis/Pages/Dashboard/vumeter_tile.h/.cpp` | VU Meter style: segmented vertical bar with position-based coloring + stats panel (FR-53) |
| `shared/nexis/Pages/Dashboard/network_tile.h/.cpp` | Dashboard network tile with dual QChart instances (separate RX/TX sparklines), two-row layout |
| `shared/nexis/Pages/Dashboard/dashboard_tile_wrapper.h/.cpp` | Decorator pattern wrapper providing drag-and-drop reordering, snap-to-grid resizing, and per-tile style switching via paintbrush button + QMenu (FR-51, FR-53) |
| `shared/nexis/Pages/Dashboard/disk_tile.h/.cpp` | Donut style (disk default): custom QPainter donut chart + setDriveHealth() cross-tile data flow (FR-43/FR-44) |
| `shared/nexis/Pages/Dashboard/maintenance_wizard_dialog.h/.cpp` | System Checkup dialog orchestrating 4 parallel checks (FR-83) |
| `shared/nexis/Widgets/CommandPalette/command_palette.h/.cpp` | Ctrl+K command palette for keyboard-driven navigation and actions |
| `shared/nexis-core/Tools/repo_health_checker.h` | Abstract base class for repository health validation; pure virtuals for platform checks; emits RepoHealthCache via DataRefreshService |
| `linux/nexis-core/Tools/repo_health_checker_linux.h/.cpp` | Platform implementation: 6 checks (connection, release 404, GPG expiry, suite mismatch, duplicates, format) with detailed issue reporting |
| `macos/nexis-core/Tools/repo_health_checker_mac.h/.cpp` | Platform implementation: 4 checks (tap reachable, outdated, deprecated, pinned) for Homebrew taps |
| `shared/nexis-core/Tools/repo_knowledge_base.h/.cpp` | 30+ entry knowledge base mapping repository URI patterns to display names and descriptions; Release file + domain fallback |
| `shared/nexis-core/Tools/repo_health_types.h` | Data types: RepoHealthResult (per-check findings), RepoHealthStatus (enum: Healthy/Warning/Error), RepoHealthCache (map of repo path to results) |
| `shared/nexis/Pages/AptSourceManager/repo_detail_panel.h/.cpp` | Side detail panel widget: repo name, status badge, description, metadata, issue list with severity colors, action buttons (Edit/Open URI/Disable/Repair) |
| `shared/nexis/Managers/cleaner_service.cpp` | Example of thick manager with real business logic |
| `shared/nexis/Managers/schedule_manager.cpp` | Example of OS-native integration complexity |
