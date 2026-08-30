# Nexis — Architecture Review

> A deep and comprehensive review of the Nexis architecture: how logic and UI work together, what's working well, what should change, and where the application should go next.
> Last updated: 2026-08-31 (SSO-23855, SSO-23896, SSO-3469 / WI-05.a, SSO-3497, SSO-3383, SSO-3391 / WI-29, SSO-3396 / WI-33, SSO-3738 / FW-10, SSO-3739 / FW-11, SSO-3740 / FW-12, SSO-3743 / FW-15, GH#191, GH#182, GH#214, SSO-15566, SSO-17776, SSO-21680, BUG-56, SSO-23861, SSO-23862, SSO-23867, SSO-23853, SSO-23854) | Version 2.10.0

> **Packaging note (SSO-3376, 2026-06):** The Flatpak (Flathub) distribution channel was retired. There is no `flatpak-spawn`/sandbox-detection layer in the codebase — the privileged-host operations Nexis depends on (`pkexec`, `systemctl`, `smartctl`, `fstrim`, `nvidia-smi`, `/proc` and `/sys` reads) are architecturally unsuited to a bubblewrap sandbox, and adding one would require routing `CommandUtil` through `flatpak-spawn --host` plus holding the `org.freedesktop.Flatpak` portal — i.e. eliminating the sandbox benefit. Linux ships via `.deb` (PPA + GitHub releases), AppImage, and AUR. **Reaffirmed 2026-08-02 (SSO-21643/SSO-21680):** a second relitigation hit the same architectural blocker, not new staleness; the `s4solutionsllc/flathub` fork is archived, and re-opening requires a fresh CEO decision with new technical facts.

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
│  UI Layer: 16 always-visible QWidget pages + 3 conditional         │
│  Each page owns its .ui file and presentation logic                │
│  (File Shredder is programmatic — no .ui, like Boot Analysis/      │
│  System Logs/Uninstaller)                                          │
│  Files: shared/nexis/Pages/*/*.cpp                                 │
├────────────────────────────────────────────────────────────────────┤
│  Service Layer: 10 Domain Services + NexisPage base class          │
│  StartupService, FileSearchService, HostService, ProcessService,   │
│  SystemServiceManager, DockerService, PackageService,              │
│  DuplicateFinderService, SnapshotService, FileShredderService       │
│  Files: shared/nexis/Services/*.cpp                                │
├────────────────────────────────────────────────────────────────────┤
│  Manager Layer: 9 Singletons                                       │
│  AppManager, InfoManager, ToolManager, SettingManager,             │
│  CleanerService, CleaningProfilesService (FW-12),                  │
│  ScheduleManager, ProcessPrefsManager, DataRefreshService          │
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
- **Unified exec-layer error contract (SSO-3367, audit A1)** — Every `CommandUtil` entry point (`exec`, `sudoExec`, `execWithStatus`, `sudoExecWithStatus`, `execAsync`) runs through a single internal `runProcess()` helper that never throws and always populates an `ExecResult { output, error, exitCode, ok() }`. The pre-SSO-3367 contract had three contradictory shapes — `exec()` threw `QString`, `execWithStatus`/`execAsync` returned `ExecResult`, and `sudoExec()` swallowed every failure into an empty string — which forced a verify-after-write read on top of every elevated write (FR-81, FR-117, FR-118). The legacy QString-returning wrappers (`exec`, `sudoExec`) are kept as thin shims for unmigrated call sites; they log the failure via `qCritical` and return an empty string instead of throwing or vanishing. New code, and any code that branches on success vs. failure, should call the `*WithStatus` variants directly and read `ExecResult::ok()`. Privileged callers can route through the `NEXIS_SUDO_BYPASS=1` testing seam so unit tests exercise the failure path without pkexec/osascript popping an auth dialog. Per-subsystem migration of the ~130 legacy call sites is tracked under SSO-3367 child issues.
- **QProcess cancel pattern** — Any helper that owns a `QProcess*` member and exposes a `cancel()` must disconnect the finished slot, copy the member into a local, and null the member *before* calling `kill()` / `waitForFinished()`. `waitForFinished()` runs the event loop on the calling thread, so a connected slot that clears the member would otherwise fire synchronously and leave `cancel()` dereferencing null on the next line. `LogProvider::cancel()` is the reference implementation (SSO-3363 / audit H2); reuse the same shape in any new background-fetch helper.
- **Streaming subprocess pattern** — two long-lived `QProcess` children using `readyReadStandardOutput`: `NettopStreamer` (FR-102, macOS) streams per-process network deltas from a single `nettop -P -d -s1` child (lifecycle tied to the `ProcessInfo::mCollectNetIO` toggle — FR-108 — so it only runs while the Processes page's network columns are visible); `NvidiaSmiStreamer` (FR-106 Step C, Linux) streams GPU utilization and fan speed from a single `nvidia-smi -l 1` child (app-lifetime). Both publish into a mutex-guarded `QHash` that the synchronous `ProcessInfo*::updateProcesses` / `GpuInfoLinux::updateGpuInfo` / `FanInfoLinux::readNvidiaSpeed` callers read from. `NvidiaSmiCache` is a thin facade namespace over `NvidiaSmiStreamer` so call sites don't change.
- **Linux /proc walk** — `ProcessInfoLinux::updateProcesses` (FR-127) reads `/proc/<pid>/{stat,status,cmdline}` directly every tick instead of forking `ps ax`. Parsing lives in a pure `ProcInfoParser` module — no Linux-specific includes — so fixture-based tests run on every platform. `sysconf` values, `/proc/stat btime`, and `/proc/meminfo MemTotal` are cached in the constructor; uid/gid → name lookups memoise `getpwuid_r`/`getgrgid_r` results. SSO-15376 added an unconditional 4th per-pid read, `/proc/<pid>/cgroup`, for Apps/Background classification (systemd `app-*.scope` match) — same cost tier as the existing stat/status reads, not gated like the FR-108 disk/net-IO columns since grouping is core page structure, not a hideable column. App-classified pids additionally resolve an icon against an XDG `.desktop` `Exec=` index built once (lazily, on first App pid seen) and cached for the app's lifetime — no new per-tick I/O beyond the cgroup read.
- **Pre-clean snapshots (FR-112)** — `SnapshotService` wraps the platform system-restore tool. `CleanerService::maybeTakeSnapshot()` is called from both `clean()` and `SystemCleanerPage::systemClean()` (the direct path that bypasses `clean`), gated on the `PreCleanSnapshotEnabled` setting and `SnapshotService::isAvailable()`. Snapshot failure is non-fatal.
- **Cleaner trend history (FR-114)** — `CleanerService::scan()` persists per-category sizes into `CleanerCategoryTrends` (rolling 20-sample JSON blob in QSettings); `getCategoryTrend()` exposes the series; `CategorySparkline` (flat QPainter, no QChart) renders it under each System Cleaner category.
- **Move-to-Trash clean path (FR-113)** — `cleanFiles(paths, minAge, moveToTrashInstead=true)` substitutes `QFile::moveToTrash` for the default `rm -rf`. Used only by the new `DOWNLOADS_AGED` category so users can recover files if the age threshold was set wrong. Same code path still does `rm -rf` for every other category.
- **Policy-denial seam for macOS 27 cross-team container access (SSO-3732 / FW-05)** — `CleanerService::cleanFiles` and the recursive `removeDirContentsRespectingExclusions` route individual deletions through a new `virtual CleanerService::removeFile(path)` seam that returns `Removed` / `NotRemoved` / `AccessDeniedByPolicy`. The default implementation calls `QFile::remove` and inspects `QFile::error()` + `errorString()` for `Operation not permitted` (EPERM, the TCC fingerprint on macOS) and `Permission denied` (EACCES), mapping both to the policy outcome. The per-call denial count is exposed via `lastAccessDeniedCount()` and rolled into `CleanResult::accessDeniedPaths` by `clean()`. A queued `accessNeededDetected(message, deepLink)` signal fires once per affected `cleanFiles()` invocation and once per `scan()` when the `~/Library/Containers` "exists but empty" probe trips; `SystemCleanerPage` connects with `Qt::QueuedConnection` because cleaning runs on the `QtConcurrent::run` worker. Bytes never credit on the denied branch (un-cleanable space is not reported as freed), so the macOS 27 acceptance criterion — cleaning either works with FDA or surfaces the actionable banner, never silently no-ops — holds end-to-end.
- **Async Search-page deletes (SSO-3365, audit H4 → SSO-3367, audit A1)** — `FileSearchService::moveToTrash` / `deleteFile` dispatch their `mv`/`rm` invocations through `QtConcurrent::run`. Under SSO-3365 the worker wrapped `CommandUtil::exec` in `try/catch (const QString&)` because the legacy contract threw on QProcess error; under SSO-3367 the unified `ExecResult` contract guarantees no exception escapes, and the worker now calls `execWithStatus` / `sudoExecWithStatus` and branches on `ExecResult::ok()` instead. Completion flows back through `fileOperationFinished(op, path, hadError, errorMessage)`; `SearchPage` removes the corresponding row only when the worker reports success and otherwise surfaces the error inline. Same shape as `FileSearchService::search`. File-op timeout raised to 5 minutes.
- **macOS crumbs scanner (FR-123, async streaming SSO-15567)** — After `trashApps` / cask-uninstall, `CrumbsScanner::scanCrumbs()` walks the six standard `~/Library/*` roots for leaf names prefixed by any uninstalled app's bundle id, streaming each match through an `itemFoundCb` as it's discovered (per scan-location) rather than blocking until the whole walk finishes; the final list is still sorted by size desc. `CrumbsScanRunner` (a `QObject` worker, same incremental-emission pattern as `TrustSafetyRunner`, SSO-15380) runs the walk off the UI thread via `QtConcurrent` and emits `itemFound()`/`scanFinished()`. `CrumbsReviewDialog` populates its table row-by-row as matches stream in, holding interaction (checkboxes, Delete Selected) off until `scanFinished()` settles the list into its final sort order; Delete Selected then moves the checked rows via `QFile::moveToTrash`. Bundle ids come from the `PlistUtil::readAppBundleInfo` helper and are carried on the shared `Package` struct. The pure walk is exposed test-seamed as `scanCrumbsUnderHome(homeDir, ...)` (mirrors `DirSizeScanner::scanSynchronous`'s explicit-rootPath pattern) so `CrumbsScannerMacOSTests` can exercise it against a `QTemporaryDir` fixture without touching the real `$HOME`.
- **macOS app trashing — no AppleScript surface (SSO-3366, audit S1)** — `PackageToolMacOS::trashApps` previously built a `tell application "Finder" to delete POSIX file "<path>"` string with `QString::arg` and ran it through `osascript -e`. A bundle name containing `"` (legal on macOS) escaped the AppleScript string literal and let the rest of the name run as AppleScript, including `do shell script`, executing arbitrary code when the user clicked Uninstall. The path is now passed to `QFile::moveToTrash` (`NSFileManager::trashItemAtURL:` on Darwin), which takes an `NSURL` — no shell, no AppleScript, no string interpolation between user-controlled data and an interpreter.
- **Per-process GPU collection (FR-115)** — Linux-only. The existing `/proc` walk in `ProcessInfoLinux::updateProcesses` (FR-127) grew a third pass gated on a new `mCollectGpu` toggle that follows the same column-visibility pattern as FR-108 (disk/net I/O). Intel/AMD read `/proc/<pid>/fdinfo/*` via the pure `ProcInfoParser::parseDrmFdinfo`; NVIDIA falls back to `NvmlProcessSampler` (SSO-15374, replacing the original `NvidiaSmiPmonStreamer` CLI-parsing streamer). Per-PID engine-ns baselines (`mPrevGpuEngineNs`) deliver the `%GPU` delta for the fdinfo path.
- **NVML per-process sampler (SSO-15374)** — `NvmlProcessSampler` (app-lifetime singleton, same lazy-construct pattern as the old pmon streamer) queries every NVIDIA device in index order once per collection tick via an injected `NvmlBackend` seam: `nvmlDeviceGetComputeRunningProcesses`/`GraphicsRunningProcesses` for per-PID VRAM (max, not sum, across compute/graphics on the same device — same underlying allocation) and `nvmlDeviceGetProcessUtilization` for per-PID `%GPU` (latest sample per PID within a device's query window, then summed across devices). No subprocess — NVML calls are in-process, replacing the two persistent `nvidia-smi` child processes (`pmon`/`--query-compute-apps`) the old streamer ran. `NvmlLibraryBackend` is the real implementation: it `dlopen`s `libnvidia-ml.so.1` and resolves the exported NVML symbols directly (declaring its own ABI-matching struct/typedef mirrors) rather than requiring the CUDA toolkit's `nvml.h` as a build dependency; `isAvailable()` probes once and caches, so a system without the NVIDIA driver/library pays one failed `dlopen` and never re-probes. `NvmlProcessSamplerTests` exercises the aggregation logic against a `FakeNvmlBackend`, independent of real hardware.
- **Pinned-at-top process sort (FR-116)** — New `PinSortFilterProxyModel` overrides `lessThan` to compare a custom `PinnedRole` before delegating to the base class, so pinned rows stay at the top regardless of sort column or direction. Pin and threshold state persists in a new `ProcessPrefsManager` singleton (JSON-in-QSettings, same shape as `ScheduleManager` / `CleanerExclusions`). Row-level context menu is distinct from the existing header context menu.
- **Threshold alerts with per-(name, metric) hysteresis (FR-116)** — `ProcessesPage::evaluateThresholdAlerts` aggregates RSS and CPU% across PIDs sharing a comm, compares against thresholds set via `ProcessAlertDialog`, and fires `QSystemTrayIcon::Warning` notifications with `mAlertArmed` hysteresis keyed by `"<name>::<metric>"` — so a breach fires once, re-arms when the reading falls back below threshold.
- **Per-row kill icon (GH#174)** — A fixed column `kKillCol` (index 19) at the far right of the Processes table holds a painting-only `KillButtonDelegate` that draws ✕ in `@destructiveColor`. The item is `Qt::ItemIsEnabled` only (not selectable), so clicking it fires `QTableView::clicked` without selecting the row; `ProcessesPage::onKillColumnClicked` reads the PID from column 0 via the proxy-to-source mapping and calls `ProcessService::killProcess`. The column is not added to `mHeaders`, so `loadHeaderMenu()` ignores it and it cannot be hidden.
- **Listening-port trusted-binder audit (FR-121)** — `OpenPortsWidget` resolves each PID's binary path (`readlink /proc/<pid>/exe` on Linux, `proc_pidpath` on macOS) and flags any path that doesn't start with a platform-trusted prefix or a user-configured extra. The default-refresh path is cheap (single stat per PID). `codesign -dv` runs lazily behind a macOS "Verify Signatures" button via `QThreadPool` with per-path caching.
- **Helpers tuning cards (FR-81, FR-117, FR-118)** — Three Helpers-page stacked cards following the FirewallWidget template (async `QThreadPool` fetch → statusFetched signal → `refreshThemeColors` bound to `sigChangedAppTheme`). `SnapshotManagerWidget` (SSO-23867, macOS-only APFS/Time Machine local snapshot list/create/delete via `tmutil`) follows the same template. All Helpers-page widgets and their write paths (`SwappinessWidget::applySwappiness` FR-81, `TrimWidget::toggleTimer` FR-118, `FirewallWidget::toggleFirewall`, plus `HelpersPage`'s Flush DNS / Rebuild Spotlight / Rebuild Launch Services / Verify Disk actions) were migrated to `sudoExecWithStatus`/`execWithStatus` under SSO-3469 (WI-05.a); `ExecResult::ok()` on the pkexec'd call is now the authoritative success check and the sysfs/status re-read that used to sit on top of every write has been removed. `CpuTuningWidget`'s own write path (FR-117) went through the same migration under SSO-3471 (WI-05.c): the sysfs read-back that stood in for a success check pre-migration has been dropped from `CpuTuning` (FR-117) and `PowerProfileInfoLinux::setSysfs` (FR-118), matching the FR-81 root-file writer below.
- **Root-file writer (FR-81)** — `FileUtil::writeRootFile(path, content)` is the first writer migrated to `sudoExecWithStatus("tee", {path}, content)` (SSO-3367). pkexec returns 0 only if both auth and the wrapped `tee` succeeded, so the pre-SSO-3367 read-back byte compare has been removed. Linux-only; the macOS build compiles to a no-op returning false.
- **CPU tuning sysfs helper (FR-117)** — New `CpuTuning` namespace in `linux/nexis-core/Info/cpu_tuning.{h,cpp}` centralises sysfs reads/writes for scaling_{min,max}_freq, scaling_governor, intel_pstate/no_turbo, and cpufreq/boost. Handles both per-core and glob-write shell idioms with injection-safe governor validation. Write paths branch on `sudoExecWithStatus`'s `ExecResult::ok()` (SSO-3471); the pre-migration sysfs read-back verification has been removed. Used by `CpuTuningWidget` and by `App::init`'s persist-on-launch re-apply.
- **QSS theming** — Single stylesheet template with `@token` replacement at runtime
- **Qt signals** — `SignalMapper` singleton as a lightweight global event bus (12 signals — see canonical "By the numbers" table)
- **Dashboard widgets and fixed-cell responsive grid (GH#191)** — Dashboard gauges replaced with a family of interchangeable tile widgets inheriting from `MetricTileBase` (abstract base class, FR-53). 8 widget styles available: `MetricTile` (sparkline), `GaugeTile` (¾-arc), `HybridTile` (arc + mini sparkline), `RingTile` (360° ring), `SpeedometerTile` (needle dial), `VuMeterTile` (segmented bar), `DiskTile` (donut chart), and `HealthScoreTile` (composite score). The base class builds the **unified tile chrome** (GH#191 follow-up) so mixed body styles line up and read as one set: `buildChrome()` creates the root layout plus a two-line header band (type-colored `mAccentBar`, `mLblTitle` type label, gear pinned to a fixed top-right slot, `mLblSource` input/source line) and returns the layout so the subclass appends its body; `appendFooter()` adds the footer band (`mLblValue` hero value + muted `mLblValueSub` secondary + `mLblTrend` pill + `mBtnAction`). Helpers `setSource()`/`setHeroValue()`/`setHeroSecondary()`/`applyAccentColor()`/`applyChromeForMode()` plus geometry helpers `bodyTop()`/`bodyBottom()` let painted-dial subclasses draw their visualization in the shared body region without re-implementing chrome; `repositionGearButton()` (still called from each subclass `resizeEvent`) now re-elides the source line to the tile width rather than moving an overlaid gear. The four shape styles (donut, gauge, ring, hybrid) render the primary value centered in the shape, sized by the pure helper `TileValueFit::fittedPixelSize` (ideal size derived from the shape diameter, shrunk via `QFontMetrics` to fit the clear inner width, 10px floor, drawn with `Qt::TextDontClip` — GH#214); ring and hybrid additionally show a footer trend pill; gauge and donut do not; speedometer keeps its value in the footer. The footer (`appendFooter()`) is a **fixed-height band** (`FOOTER_HEIGHT`) shared by every style so the footer looks consistent regardless of body style; `updateFooterVisibility()` hides the footer when it has no content (Gauge/Donut center their value in the shape) so the body expands via `bodyBottom()`. All styles support four `DisplayMode` values (Normal/Hero/Large/Compact); `applyChromeForMode()` collapses the header source line on Compact (the footer value no longer scales per mode — shape styles paint their own scaled value). CPU and Memory are independent tile instances (the combined `HeroCard` was removed in FR-50). `NetworkTile` uses dual `QChart` instances (separate RX/TX sparklines) and is excluded from style switching. `DiskTile` uses custom `QPainter` donut chart and no longer has a bespoke layout — the base class `MetricTileBase` owns `setInputName()` (friendly drive name in the title row with SMART model as tooltip) and `setDriveHealthSegment()` (SMART verdict color-coded and appended to the sub-header for every disk style, surviving style switches). The pure helper `DriveTileFormat::usageText` centralizes used/total formatting across disk tile styles. `DashboardTileWrapper` (FR-51) uses the decorator pattern to add edit-mode mouse handling (drag-and-drop, snap-to-grid resizing) and style switching (`setInnerWidget()` swaps the inner tile at runtime, FR-53). It also gives each tile **depth** via `applyDepthTreatment()`: it sets `WA_StyledBackground` + an inline (theme-resolved) stylesheet for the warm `@cardBgElevated` surface, applies a `Utilities::addDropShadow`, and re-applies on `sigChangedAppTheme` (tiles are built before style values load, so colors are resolved on the theme signal). Tile track colors use `@chartGridColor` (not `@color02`, which is identical to the elevated surface). Per-tile style persisted in layout JSON via SettingManager. `DashboardPage::createTile()` factory method instantiates the correct tile class by style string. `HealthScoreTile` (FR-73) is a specialized tile showing a composite 0–100 health score aggregated from 6 components (CPU, memory, disk, temperature, battery, SMART) via `HealthScoreCalculator` helper; unavailable components have their weights redistributed proportionally. In Large/Hero mode, per-component breakdown bars are drawn in `paintEvent`. **GH#191 dashboard enhancement (responsive grid + multi-instance tiles):** The grid model changed from proportional cell sizing to fixed-size cells (1×1 = 120×98px, 10px gap). Column count adapts to window width (clamped 4–16); tiles reflow when the count changes. A vertical `QScrollArea` hosts the grid; layout versioning (`{"version":2,"tiles":[...]}`) enables auto-migration of legacy bare-array layouts. Each tile is uniquely identified by `uid` and carries a `DisplayMode` (Compact/Normal/Large) computed from its cell area via pure helper `DashboardLayout::tierForArea(span)`. Per-tile `input` binding replaces global Temperature/Fan/GPU/Disk/Network selection — each tile stores which sensor, fan, drive, GPU, or network interface it monitors. The layout JSON schema now includes `{id, uid, type, displayMode, input, x, y, width, height, style, customColor, visibility}`. Small tiles (≤2 cells) render in Compact tier, dropping gauge/sparkline and showing only title + value for readability. Update routing no longer broadcasts all sensor/fan/GPU/disk/network deltas to every tile; instead, `DashboardPage` maintains a `QMap<uid, wrapper>` and routes each per-type update signal to only the wrappers of that type, passing the resolved input value. Existing layouts (pre-version-2) without `input` fields are upgraded on load: a template tile of each type is assigned to the first detected input of that type, and the layout reflowed via `DashboardLayout::reflow(columns)`. The pure `DashboardLayout` helper (`shared/nexis-core/Utils/dashboard_layout.{h,cpp}`) contains all grid geometry logic (`columnsForWidth`, `tierForArea`, `migrate`, `reflow`, `uidHelpers`); no platform-specific code. Per-tile input-switching and edit-mode "Add tile" palette is managed by `DashboardPage` UI code
- **Maintenance Wizard** — `MaintenanceWizardDialog` (FR-83) is a modal QDialog launched from the Health Score tile's quick action button. It orchestrates 4 existing services (CleanerService, ToolManager, InfoManager, HealthScoreCalculator) via `QtConcurrent::run()` with `QMetaObject::invokeMethod()` for cross-thread result marshaling. Thread-safe completion counting via `QAtomicInt`. Custom types (`CleanerService::ScanResult`, `QList<OrphanPackage>`, `UpdateCheckResult`) registered with `qRegisterMetaType` for cross-thread signals. Follows the `ExclusionManagerDialog` pattern (programmatic QDialog, no .ui file, DI constructor with singleton fallbacks). **Worker lifetime (SSO-3362):** the dialog uses `WA_DeleteOnClose`, so detached workers outliving the dialog would dereference deleted memory. Each lambda captures a `QPointer<MaintenanceWizardDialog>` (no raw `this`) and singleton manager pointers by value, marshals results through the context-object + functor overload of `QMetaObject::invokeMethod` (Qt drops queued calls when the context dies), and the dialog stores every `QFuture<void>` as a member and `waitForFinished()`s them all in its destructor as a hard backstop.
- **Menu-bar monitor** (macOS, FW-20 / SSO-3748, extended SSO-23853) — `MenuBarMonitor` owns its own `HealthScoreCalculator` instance (it is not a page and can't reach `DashboardPage`'s lazily-constructed `HealthScoreTile`) and subscribes to `DataRefreshService`'s Cpu/Memory/DiskUsage signals via the same FR-103 subscriber-counting mechanism a page uses, computing its CPU/memory/disk component scores via the shared `HealthScoreInputs` helper (`shared/nexis-core/Utils`, extracted in SSO-23854 so `DashboardPage::onHealthCpuUpdated`/`onHealthMemoryUpdated`/`onHealthDiskUpdated`'s formulas have one implementation instead of being copied per surface) so the composite scores agree across surfaces; temperature/battery/SMART are deliberately left unavailable (not polled) to keep the always-on background monitor cheap. The status item's `NSMenu` "Clean Now" action reuses `CleanerService::safeCategories()` (the same curated category list as the Maintenance Wizard's "Clean Safe Items", extracted to `CleanerService` as a single source of truth) and follows `MaintenanceWizardDialog`'s cross-thread contract verbatim: `QtConcurrent::run()` + `QPointer<MenuBarMonitor>` capture + `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` for the result, and a `QFuture<void>` member `waitForFinished()`'d in the destructor as a backstop.
- **Tray health monitor** (Linux, SSO-23854, tray counterpart to the macOS menu-bar monitor above) — `TrayHealthMonitor` is `MenuBarMonitor`'s Linux/`QSystemTrayIcon` sibling: same `HealthScoreCalculator` instance ownership, same `DataRefreshService` Cpu/Memory/DiskUsage subscription, same `HealthScoreInputs` formulas, same `MenuBarFormatUtil::formatHealthTitle()` string, and the identical `QtConcurrent::run()` + `QPointer<TrayHealthMonitor>` + `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` + destructor `waitForFinished()` contract for "Clean Now". It owns no UI itself — `App::createTrayActions()` adds a disabled label `QAction` and a "Clean Now" `QAction` to the existing tray menu (built once, alongside the FR-125 Quick Actions submenu and the SSO-23896 sidebar-derived groups) and `App::init()` wires `TrayHealthMonitor::scoreTextChanged`/`cleanStateChanged` into their text/enabled state and the tray icon's tooltip. Both actions stay hidden and the monitor stays unsubscribed unless `SettingManager::getTrayHealthScoreEnabled()` is true (off by default, toggled live via `SignalMapper::sigTrayHealthScoreToggled` from Settings → General, mirroring `MenuBarMonitorEnabled`); the surface reuses whichever icon the FR-48 tray icon style selector has picked rather than adding a new one.
- **Mini-monitor window** (both platforms, SSO-23855 / Glanceable Surfaces epic SSO-15365) — `MiniMonitorWindow` (`shared/nexis/Pages/MiniMonitor/`) is the cross-platform sibling of `MenuBarMonitor`/`TrayHealthMonitor` above: same FR-103 subscriber-counting pattern against `DataRefreshService`'s Cpu/Memory/DiskUsage signals and the same `HealthScoreInputs` formulas feeding its own `HealthScoreCalculator` instance, but as a plain `QWidget` (`Qt::Window | Qt::WindowStaysOnTopHint` — deliberately not `Qt::Tool`, which some window managers hide when the parent window minimizes, defeating the glanceable-while-minimized purpose) rather than a native `NSStatusItem`/`QSystemTrayIcon`, so one implementation covers Linux and macOS. Subscribe/unsubscribe is driven by `showEvent`/`hideEvent` rather than an explicit `setEnabled()` call; `closeEvent` ignores the native close and calls `hide()` instead so the window survives being closed (state and `saveGeometry()`/`restoreGeometry()` bytes persist via `SettingManager::MiniMonitorVisible`/`MiniMonitorGeometry`). A `visibilityToggled` signal lets `App` keep the tray menu's checkable "Mini Monitor" action in sync when the window is hidden by any of its three entry points (Settings checkbox, tray action, native close button) without a signal loop, since `SignalMapper::sigMiniMonitorToggled` only flows one direction (Settings/tray → `MiniMonitorWindow::setVisible`). New pure-logic helper `MiniMonitorFormatUtil` (`shared/nexis-core/Utils/`) mirrors `MenuBarFormatUtil`'s split — formatting/aggregation kept out of the QWidget so it's unit-testable without a display.
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

**Assessment:** This pattern provides **compiler-enforced platform contracts** while maintaining the pragmatic simplicity of the original include-path architecture. The 17 abstract base classes (12 Info + 5 Tool) cover all platform-specific code. `PowerProfileInfo` (FR-70) is among the latest additions: abstract base with PPD + sysfs dual-backend Linux subclass and macOS stub. It does not poll via DataRefreshService — the profile is read on demand when the Helpers page is activated. `LogProvider` (FR-71) follows the same two-tier pattern for system log reading: abstract base with `LogProviderLinux` (journalctl JSON) and `LogProviderMacOS` (log show ndjson) subclasses, selected via `createForPlatform()` factory. The macOS subclass stream-parses ndjson incrementally via `MacOsLogStreamParser` and kills `log show` once `mMaxEntries` records have been accepted (SSO-3384 / WI-22) so the page no longer buffers a full hour of output before parsing. `RepoHealthChecker` (repository health) adds another platform-specific tool: abstract base with 6-check Linux subclass (`RepoHealthCheckerLinux`: connection, release 404, GPG expiry, suite mismatch, duplicates, format) and 4-check macOS subclass (`RepoHealthCheckerMac`: tap reachable, outdated, deprecated, pinned), both polled by DataRefreshService after update checks.

**`RepositoryTool` (SSO-3390 / WI-28, audit A4):** Software-source backends used to live under a single `AptSourceTool` interface — including the macOS Homebrew adapter, which had to overload `APTSource.uri`/`.suites` for package names/types and provide no-op `changeSource`/`changeStatus`. The shared APT page then branched around the mismatch with ~30 `Q_OS_MAC` directives. The refactor introduces a platform-neutral `RepositoryTool` interface (`isAvailable`, `listRepositories`, `addRepository`, `removeRepository`, `capabilities()`) with a generic `Repository` value type. `AptSourceTool` now extends it on Linux with the APT-specific operations (`getSourceList`, `changeStatus`, `changeSource`, `removeAPTSource`), and macOS gets a dedicated `HomebrewToolMacOS` implementing only the platform-neutral interface — no field overloading. The macOS Homebrew UI moved into a dedicated `HomebrewPage` under `macos/nexis/Pages/Homebrew/` driven by `PackageTool` data, and `APTSourceManagerPage` relocated to `linux/nexis/Pages/AptSourceManager/` with all `Q_OS_MAC` directives removed. `ToolManager` exposes `repositoryTool()` cross-platform and `aptSourceTool()` on Linux only.

**APT deb822 round-trip (SSO-3728 / FW-01):** APT 3.1 made deb822 the default for new sources and removed `apt-key`. The Linux `AptSourceTool::changeSource()` rewrite path used to be a private 130-line lambda in `linux/nexis-core/Tools/apt_source_tool.cpp` that knew about Types/URIs/Suites/Components/Enabled and silently round-tripped (or lost) everything else. That lambda is now a pure static method `AptSourceTool::serializeDeb822Stanza(originalStanza, matchEntry, newSource, binaryType, sourceType)` on the shared base class, with two invariants the unit suite enforces: (1) a no-op edit returns the original stanza byte-for-byte (`fixture_ubuntu26_04_noopRoundTripIsByteStable`); (2) any field the model doesn't know about (`Languages:`, `Targets:`, embedded multi-line `Signed-By` GPG keys, etc.) is preserved verbatim. The model added an `architectures` field that captures both deb822 `Architectures:` and legacy `[arch=…]` (comma list normalised to space-separated). A second new static, `AptSourceTool::buildDeb822Stanza(source, …)`, constructs a fresh stanza from scratch for the "add a new repo" path. `AptSourceToolLinux` adds `addRepositoryDeb822(fileStem, source)` which `tee`s the stanza into `/etc/apt/sources.list.d/<stem>.sources`, and `addRepository()` auto-routes structured `deb [signed-by=…] uri suite components` inputs through it when the system already uses deb822 by default (`prefersDeb822()`, detected by `ubuntu.sources`/`debian.sources` presence) — `ppa:` short-forms still fall through to `add-apt-repository` for launchpad key fetch. The fixture `tests/fixtures/apt/ubuntu_26_04.sources` is the canonical 26.04 shape (multi-suite, `Architectures`, keyring `Signed-By`, leading comments) that every future change to the deb822 path must keep stable.

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

**Facade-only access from `shared/nexis/Pages` (WI-27 / SSO-3389, audit A3).** All shared UI pages must reach platform-specific Info subclasses through `InfoManager` (or a `createForPlatform()` factory like `LogProvider`) — never by stack-constructing `SystemInfoLinux` / `SystemInfoMacOS`, `CpuInfoLinux` / `CpuInfoMacOS`, `BootAnalysisInfoLinux` / `BootAnalysisInfoMacOS`, etc. behind `#ifdef Q_OS_*`. As of WI-27, `BootAnalysisInfo` and `StartupInfo` are also owned by `InfoManager` and exposed via `bootAnalysisInfo()` / `startupInfo()` accessors. A CI gate (`scripts/check-pages-no-platform-headers.sh`, invoked from `.github/workflows/build.yml`) blocks any include of `*_macos.h` / `*_linux.h` from `shared/nexis/Pages/**`. Adding a new platform should not require editing any page in that tree.

**Exception: macOS-only BTM view on Startup Apps (SSO-3738 / FW-10).** The `BtmRow` and `BtmResetDialog` widgets live under `shared/nexis/Pages/StartupApps/` but are wrapped in `#ifdef Q_OS_MACOS` and only registered into the macOS GUI target. They consume `BtmRecord` (defined in `macos/nexis-core/Info/btm_parser.h`) through `StartupService::getBtmRecords()` / `StartupService::resetBtm()` rather than including a platform-specific Info header directly, so the facade rule is preserved — the platform-specific type leaks through the service signature, not through page-level includes. The CI gate continues to enforce no `*_macos.h` / `*_linux.h` includes from `Pages/**`.

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
- **Zero hardcoded colors** — all C++ widgets accept token name strings (e.g., `"@cpuColor"`) and resolve colors from `values.ini` at runtime, including chart series, sparklines, progress bars, shadows, and overlays (BUG-47). 24 extended tokens cover network upload colors, overlay/shadow colors (8-digit `#AARRGGBB` hex), and a 20-color chart series palette. Static and semi-dynamic styles use central QSS selectors (including `[status="success/warning/error"]` property selectors for runtime color choices); only per-instance dynamic styles (metric tile colors, chart series) use inline `setStyleSheet()` with token-resolved values (FR-88). **Important:** per-widget `setStyleSheet()` strings are NOT run through the global token-substitution pass — only `qApp->setStyleSheet()` is. Inline stylesheets must therefore resolve every token via `sv->value("@tokenName", fallback).toString()` and interpolate the resolved value with `.arg(...)`; leaving a raw `@token` literal in a per-widget stylesheet causes Qt to drop the declaration silently (audit WI-25). The `tests/theme/test_theme_tokens.cpp::noRawTokensInPerWidgetStyleSheet` regression scans `shared/` C++ to keep this invariant enforced.

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

**The pattern applies at four levels:**
1. **Page level** — Docker, GNOME Settings, and APT/Homebrew pages are hidden entirely if tools aren't detected
2. **Widget level** — Battery, GPU, and temperature gauges hide if hardware is absent; disk health info is shown inline on the DiskTile when available
3. **Feature level** — macOS filters Apple system agents from startup apps; purge option hidden on non-APT systems
4. **Surface level (SSO-23896)** — the system tray menu derives its MONITOR/Manage/System grouping from the sidebar's `mSections` model (`buildTrayMenuGroups()`, `shared/nexis/Managers/tray_menu_model.cpp`); the same `button->isHidden()` check that removes a page from the sidebar also drops it from its tray group, and a group left with no visible members is omitted rather than rendered as a dead submenu

**Why this matters:** Cross-platform apps often show all features with "not available on this platform" messages, which clutters the UI. Nexis's approach presents a **clean, relevant interface** tailored to each system's actual capabilities.

**Defense-in-depth on hidden tools (audit WI-29).** Hiding a sidebar page is a UI gesture; if the platform abstraction underneath the page still answers `isAvailable() == true`, a future regression to the visibility guard can re-expose dangerous behavior. The macOS `GnomeSettingsTool` adapter previously did exactly that — its mapping table collapsed GNOME interface keys onto `AppleInterfaceStyle` and dock `orientation`, so the page being hidden was the only thing preventing `defaults write NSGlobalDomain …`. The adapter is now a hard no-op stub (`isAvailable()` returns false, every setter returns false without invoking `defaults`), `ToolManager::checkGnomeSettings()` short-circuits to false on macOS, and the constants header is wiped to empty strings. **Rule:** when a tool is hidden on a platform because its semantics don't translate, the platform-specific adapter should also refuse — graceful degradation needs to hold even if a guard regresses.

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

**Assessment:** With 12 signals today (BUG-82 cleanup removed unused signals; FR-105 added `sigAppFocusChanged`; the unused `sigDashboardLayoutReset` was removed when Reset Layout switched to a direct method call; SSO-23853/SSO-23855 added the menu-bar and mini-monitor toggle signals), this is still **appropriately simple**. The kiosk mode signals (FR-30), visibility signal (FR-37), focus signal (FR-105), `sigNavigateToPage` (FR-42), cleanable-size signal (FR-44), and dashboard footer visibility signal (FR-75) demonstrate the pattern working well for decoupled communication between App, DashboardPage, DataRefreshService, SystemCleanerPage, SettingsPage, and the CommandPalette. A full event bus library (like eventpp) would be overkill until the signal count grows significantly (15+). SignalMapper remains stable and focused on app-wide lifecycle events, while DataRefreshService handles domain-specific data delivery signals (see the canonical "By the numbers" table for the current `DataRefreshService` signal count).

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

**Status:** Unit test suite implemented in Phase 7 (FR-36), then significantly expanded in FR-76. Now 22 CTest executables with ~310 test methods covering utilities (FormatUtil, FileUtil, CommandUtil), core library parsing (DiskHealthInfo, MemoryInfo, CpuInfo, GpuInfo, FanInfo, ThermalInfo, BatteryInfo, DiskInfo), tool parsing (AptSourceTool, PackageTool), widget parsing (NetworkDiag, OpenPorts, Firewall), service logic (HostService), manager logic (ScheduleManager), power profile parsing (PowerProfileInfo), theme token validation, and screenshot regression tests.

**Refactoring for testability:**
- `parseSmartctlJson()` deduplicated from 2 platform files into shared public static `parseSmartctlJsonInto()`
- `deriveHealthVerdict()` made public static (pure struct logic)
- `getNextRunTime()` accepts optional `now` parameter for deterministic testing
- `PROJECT_SOURCE_DIR` compile definition enables theme tests to locate source-tree files
- **FR-76 static parser pattern:** Parsing logic extracted from instance methods into public static methods on shared base classes (`*_shared.cpp` files). Static methods accept raw text/data and return structured results, enabling fixture-based testing without mocking CommandUtil or the filesystem. Applied to: MemoryInfo (3 methods), CpuInfo (5 methods), GpuInfo (4 methods), AptSourceTool (2 methods), FanInfo (3 methods), ThermalInfo (2 methods), BatteryInfo (2 methods), DiskInfo (1 method), HostService (promoted to static)
- **WI-33 macOS parser coverage:** Extended the FR-76 pattern to macOS live-tool parsers. `disk_health_info.cpp`'s anonymous `parsePlist` moved to public `MacosPlistParser::parse()`; `NettopStreamer::parseCsvLine` and `BootAnalysisInfoMacOS::parseKernBoottime` exposed as static methods. Real `nettop`/`diskutil`/`sysctl` output captured under `tests/fixtures/macos/` and parsed via the FR-127 compile-source-into-test pattern, so coverage runs on every host even though the parsers ship only on macOS. For uninstall command construction, added a `runSudoCommand`/`runCommand` virtual seam on `PackageTool` (mirroring `TestableRepairEngine`); test subclasses capture the (cmd, argv) tuple each package manager constructs (apt purge/remove, dnf/yum/pacman/snap/brew flags, snap-revision flag), and `CommandUtil::buildMacOsSudoShellCommand()` was extracted as a public helper so the macOS osascript shell escaping is unit-tested too — replacing the single `pkexec`-gated test that previously `QSKIP`-ed in CI.

**Remaining barriers to broader testing:**
- ~~Singleton coupling (§2) blocks mock injection~~ — resolved in Phase 6 (FR-35)
- ~~Info classes read real OS state — no abstraction for test data~~ — resolved in Phase 5 (FR-34) and FR-76 (static parser extraction)
- Pages tightly bound to `.ui` files and Qt widgets (UI testing out of scope)
- CleanerService in GUI executable target — exclusion logic and the destructive `cleanFiles` / `cleanTrash` paths are now testable by linking the test binary against `nexis-gui` (see `CleanerExclusionTests` and `CleanerServiceTests`). The deletion paths use `protected virtual` seams (`removeElevated()`, `trashRoot()`) so a `TestableCleanerService` subclass can bypass `sudoExec` and the real Trash directory — same pattern as `TestableRepairEngine`'s pkexec bypass

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
  - Moved disk-health discovery to `QtConcurrent::run()` (300-1000ms off main thread every 30s). Worker now builds a fresh `QList<DriveHealth>` via `DiskHealthInfo::collectDriveHealth()` (no shared-state mutation) and publishes via `InfoManager::setDriveHealth()` inside the `QMetaObject::invokeMethod` hop — mirrors the `DiskInfo::collectDiskInfo()`/`setDisks()` pattern. A `QMutex` on the base class additionally guards every `mDrives` read/write (`getDrives`, `hasDrives`, `setDrives`, both `refreshHealthElevated*` paths). pkexec/sudo invocations run outside the lock to keep UI snapshots responsive (SSO-3364 / audit H3).
  - Moved per-second process collection off the UI thread (SSO-3383 / audit M2): `DataRefreshService::onProcessTick()` now mirrors `onSlowTick()`, dispatching a `QtConcurrent` worker that calls `ProcessInfo::collectProcesses()` (builds a fresh `QList<Process>` into a local, never touches `processList`) and publishes via `InfoManager::setProcessList()` from the `QMetaObject::invokeMethod` hop. A `mProcessRunning` flag rejects re-entrant ticks. A `QMutex` on the `ProcessInfo` base class guards every `processList` read/write; a second mutex serialises concurrent collect calls so the per-PID delta state (`mPrev*`, timers) stays coherent if a sync caller (HTML report) races the worker tick. The Hardware Info SMART "Unlock Drive" / "Unlock All" buttons dispatch `refreshDiskHealthElevated[Batch]()` through `QtConcurrent::run` + `QFutureWatcher`, and the pkexec/osascript timeout is bumped to 5 minutes so a slow polkit password entry still completes the unlock.
  - **Maintenance Wizard worker (SSO-3385 / audit M4):** the health-score `QtConcurrent::run` previously called `InfoManager::getDisks()` and `getThermalTemperature(0)` directly, racing with `DataRefreshService::onMediumTick()` (which republishes `DiskInfo::disks` via `setDisks()` on the UI thread) and with the macOS SMC IOConnect calls. The wizard now snapshots `coreCount`, load averages, the disk list, `hasThermalSensors`/`hasBattery`/`hasDiskHealth`, and the temperature on the UI thread and captures them by value into the worker lambda — no provider calls happen off-thread. This is the "preferred" snapshot path from the audit (avoids locking) and is the contract documented on `DiskInfo::collectDiskInfo`/`setDisks` since FR-101.
  - **macOS AppleSMC connection (SSO-3385):** `smcOpen()` now uses `std::call_once` so the IOService is opened exactly once for the process lifetime — closes the double-open/leak window introduced by the previous `if (sConn) ...` check-then-`IOServiceOpen`. The whole get-info/read pair in `smcReadKey` runs under a process-wide `QMutex` so concurrent callers (UI medium tick + future workers) cannot interleave the two `IOConnectCallStructMethod` halves on the shared connection.
  - In-place `QStandardItemModel` updates in ProcessesPage (eliminated 6,800 item allocations per tick)
  - Changed signal signatures to `const&` (eliminated ~2,800 deep string copies per emission)
  - Merged duplicate `getifaddrs()` walks into single `updateNetworkBytes()` method
  - Cached `hw.memsize`, `host_page_size()`, and GPU registry entry IDs (read once, not per tick)

---

### ~~7. QSS Token Validation Gap~~ (Addressed)

**Status:** Addressed in Phase 3 (FR-32). `AppManager::updateStylesheet()` now includes two runtime validation passes before token replacement:

1. **Token existence check:** Scans the raw QSS template for `@token` patterns (regex `@([a-zA-Z][a-zA-Z0-9_]*)`), skips `@dpN` DPI tokens, and emits `qWarning()` for any token not found in the active theme's `values.ini`.
2. **Color format validation:** Iterates all `values.ini` entries (excluding `@themeName`) and warns if any value is not a valid CSS hex color (`#rgb`, `#rgba`, `#rrggbb`, or `#rrggbbaa`).
3. **Inline-stylesheet raw-token check (SSO-3387 / audit WI-25):** `tests/theme/test_theme_tokens.cpp` walks every `*.cpp`/`*.h`/`*.mm` under `shared/`, paren-balance-extracts the argument of every `setStyleSheet(...)` call, and fails the build if any raw `@tokenName` literal remains in the string (with `value("@…")` lookup keys excluded, since those are keys, not stylesheet content). Per-widget `setStyleSheet()` strings are NOT run through `AppManager::updateStylesheet()` token substitution — only `qApp->setStyleSheet()` is — so a raw `@token` literal inside an inline stylesheet is silently dropped by Qt as an invalid declaration. This is what previously let the Network Usage cap-bar groove render unthemed (`background: @borderColor;` literal in `mCapBar->setStyleSheet(...)`).
4. **C++ token coverage (SSO-3386 / audit WI-24):** `tests/theme/test_theme_tokens.cpp` also walks every `*.cpp`/`*.h`/`*.mm` under `shared/`, extracts every `@token` literal passed to `QSettings::value("@…")`, and asserts each one is present in both themes. This guards `getStyleValues()` reads with the same coverage QSS tokens already had — closing the gap that previously let `@networkDownloadColor` slip past CI while the RX bar silently fell back to the hard-coded `#5294e2` default.

The runtime checks (1, 2) emit `qWarning()` at runtime (visible in debug output) without altering application behavior; checks (3, 4) are CTest assertions that fail CI on regression. Together they catch typos, missing tokens, malformed color values, raw `@token` literals smuggled into inline stylesheets, and C++ `getStyleValues()` reads pointing at tokens absent from `values.ini`.

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

**Status:** Completed in Phase 8 (FR-37). Created `DataRefreshService` singleton (`shared/nexis/Managers/data_refresh_service.{h,cpp}`) with 5 QTimers (1s fast, 5s medium, 30s slow, configurable process, 1h update) and typed data signals (live count in the canonical "By the numbers" table): `cpuUpdated`, `memoryUpdated`, `networkUpdated`, `networkPerInterfaceUpdated`, `diskIOUpdated`, `gpuUpdated`, `tempUpdated`, `fanUpdated`, `batteryUpdated`, `diskUsageUpdated`, `diskHealthUpdated`, `processesUpdated`, `systemUpdatesChecked`, `repoHealthChecked`, plus Linux-only `psiUpdated` (FR-124) and `oomdUpdated` (FW-11 / SSO-3739, systemd-oomd + cgroup v2 `memory.events`, medium tick). `networkPerInterfaceUpdated` (SSO-351) carries a `QHash<QString, NetInterfaceStats>` snapshot so `NetUsageTracker` can record traffic for every up+running interface; the legacy `networkUpdated(rx, tx)` is retained for the live-rate displays which only need default-iface totals. The `memoryUpdated` signal was updated in FR-57 to use a `MemorySnapshot` struct (replacing 4 separate `quint64` parameters) carrying wired/active/inactive/compressed/available/pressureLevel fields alongside the original used/total/swapUsed/swapTotal. The `fanUpdated` signal was added in BUG-70 (previously fan updates piggybacked on `tempUpdated`). The `systemUpdatesChecked` signal was added in FR-60 for the hourly system update check, which runs via `QtConcurrent::run()` to avoid blocking the UI thread. This signal is consumed by `App` (sidebar badge update + tray notification) and `APTSourceManagerPage` (Available Updates tree widget), not by `DashboardPage` — the original dashboard tile approach was replaced with a sidebar badge + package page integration during the FR-60 redesign. The `repoHealthChecked` signal was added for repository health dashboard validation (chained after update checks, emits `RepoHealthCache` with per-repo health results), consumed by `APTSourceManagerPage` for card enrichment and side panel display.

Converted Dashboard (removed 3 timers), Resources (removed 2 timers), and Processes (removed 1 timer) to reactive signal subscribers. Added `sigAppVisibilityChanged(bool)` to SignalMapper for pause/resume (kiosk mode overrides). DI constructor parameter follows FR-35 pattern.

**Results:** 6 per-page QTimers → 5 centralized QTimers. Zero duplicate InfoManager calls. Fixed `getCpuPercents()` static-delta bug. Battery optimization via pause on minimize. Page-aware data gating (BUG-72 Tier 1): Dashboard, Resources, and Processes pages inherit `NexisPage` and gate slot handlers on visibility — tile repaints, chart updates, and process polling stop when the page is hidden. Delta-tracking statics (network/disk I/O) are maintained even when hidden to prevent data spikes on reactivation. BUG-72 Tier 2/3: All signal signatures use `const&` to avoid deep copies on emission. `getDiskIO()` replaced `iostat` subprocess with IOKit API. `getAvgClock()` result cached (constant on Apple Silicon). `discoverDrives()` runs async via `QtConcurrent::run()`. ProcessesPage updates model rows in-place instead of nuke-and-rebuild. Network bytes collected in single `getifaddrs()` walk. Memory constants and GPU registry IDs cached on first call.

**Per-process I/O delta tracking (FR-58/FR-59):** `ProcessInfoMacOS` and `ProcessInfoLinux` subclasses maintain `QHash<pid_t, QPair<quint64,quint64>>` maps for previous disk I/O counters and a `QElapsedTimer` to compute per-process byte rates (read/write per second). macOS additionally runs a persistent `nettop -P -d -s1` streamer (`NettopStreamer`, FR-102) for per-process network bandwidth, harvesting cumulative byte-count deltas. Linux reads `/proc/<pid>/io` for disk I/O; per-process network has no procfs equivalent, so `ProcessInfoLinux` runs an analogous persistent `nethogs -t` streamer (`NethogsStreamer`, SSO-15379) — nethogs already reports a KB/s rate per refresh cycle rather than cumulative bytes, so there's no delta baseline on the Linux side, unlike disk I/O or macOS's net path. `nethogs` needs `CAP_NET_RAW`/`CAP_NET_ADMIN` or root; when missing, `ProcessInfo::NetIoAvailability` (threaded through `InfoManager` to a Processes-page header tooltip) distinguishes "tool not installed" from "present but couldn't get a capture socket" so the em-dash sentinel reads as "unavailable, here's why" rather than silent zero. The 4 fields (`diskReadRate`, `diskWriteRate`, `netDownRate`, `netUpRate`) are carried in the `Process` struct and displayed as hidden-by-default columns on the Processes page.

---

#### ~~2B. Dependency Injection for Managers~~ (Done)

**Status:** Completed in Phase 6 (FR-35). All 10 page classes with manager dependencies now accept optional `nullptr`-default constructor parameters with ternary fallback to `::ins()` singletons. All `::ins()` calls in page method bodies replaced with member variable access. Production call sites in `app.cpp` unchanged. 4 pages with zero manager dependencies (StartupAppsPage, HelpersPage, GnomeSettingsPage, DockerPage) required no changes. Child widgets still use `::ins()` directly (known limitation, future scope).

---

#### ~~2C. QSS Token Validation~~ (Done)

**Status:** Completed in Phase 3 (FR-32). Two validation passes added to `updateStylesheet()`: token existence check (QSS `@tokens` vs `values.ini` keys) and color format validation (hex format check on all non-`@themeName` values). Both emit `qWarning()` diagnostics.

---

### Priority 3: Medium (Testing & Quality)

#### 3A. ~~Basic Unit Test Suite~~ (Implemented + Expanded)

**Completed (Phase 7, FR-36; expanded FR-76, FR-79/FR-80, FR-82, FR-66, FR-68).** 22 test executables with ~310 test methods:
1. **Utility classes** — FormatUtil (10), FileUtil (10), CommandUtil (9)
2. **Info class parsing** — DiskHealthInfo (20), MemoryInfo (14), CpuInfo (19), GpuInfo (23), FanInfo (16), ThermalInfo (11), BatteryInfo (12), DiskInfo (17)
3. **Tool parsing** — AptSourceTool (14), PackageTool (16)
4. **Widget parsing** — NetworkDiag (24), OpenPorts (20), Firewall (15)
5. **Service logic** — HostService (25), DuplicateFinder (19)
6. **Manager logic** — ScheduleManager (15), CleanerExclusions (11)
7. **Theme validation** — ThemeTokens (7)

**FW-08 (SSO-3736):** `DuplicateFinderService` gained a virtual `moveToTrash()` seam plus a virtual `loadExclusions()` seam so the destructive `trashFiles()` path is exercised without touching the user's real trash or settings. The service now consults the cleaner exclusion engine across all three scan modes (duplicates, top-N largest, empty folders) and enforces the never-delete-last-copy invariant at the service layer via `filterSafeTrashCandidates()` — the UI is no longer the only line of defense.

**SSO-17858:** added a virtual `runsAsynchronously()` seam (default `true`, production unchanged). `TestDuplicateFinder` overrides it to `false` so `scan()`/`scanLargest()`/`scanEmptyFolders()` run their pipeline and emit `*Finished` synchronously instead of via `QtConcurrent::run()`. This removes the suite from a cross-thread queued-connection delivery hop that intermittently exceeded `QSignalSpy::wait()`'s timeout under CPU-throttled CI runners (SSO-13969, SSO-14443) — the earlier fixes patched symptoms (raising timeouts, pre-warming the thread pool); this removes the nondeterministic hop these tests never needed to exercise.

**Key refactoring:** FR-36 established the pattern with `parseSmartctlJsonInto()` shared static (dedup), `deriveHealthVerdict()` public static, `getNextRunTime()` injectable `now` parameter. FR-76 scaled this to 10 additional classes by extracting parsing logic into public static methods on shared base classes (`*_shared.cpp` files). Fixture data files in `tests/fixtures/` provide deterministic test input. CleanerExclusions tests link against `nexis-gui` library to access CleanerService.

---

#### 3B. CI Screenshot Regression Tests (local-only)

**Status:** Implemented (FR-41), re-enabled in CI under NEX-3381, hardened under NEX-3382. The `test-ScreenshotTests` executable captures all 12 always-visible pages in both Dark and Light themes (24 screenshots per platform), compares against committed reference PNGs, and writes actual/reference/diff PNGs to `build/tests/test_screenshots/failures/` on mismatch.

**Comparison model (NEX-3382):** Instead of a single whole-page percentage threshold (which was both AA-brittle and insensitive to ≤10% regressions), the comparator now (a) builds a per-page mask of dynamic-data regions by walking the page widget tree for declared child classes (`HistoryChart`, `BarChartWidget`, `DashboardTileWrapper`, `MetricTileBase`, `NetworkTile`, `QAbstractItemView`) and named widgets (`systemSummary`), (b) applies a small per-channel fuzz (default 8, env-overridable via `NEXIS_SCREENSHOT_CHANNEL_FUZZ`) when comparing the remaining unmasked pixels, and (c) requires the unmasked diff to stay below a tight 1% default tolerance (overridable via `NEXIS_SCREENSHOT_TOLERANCE`). A missing page class or missing reference PNG `QFAIL`s loudly; a wholly absent platform/theme baseline directory `QSKIP`s with the regeneration instructions so the gap is explicit, never vacuously green. Seven self-test slots inside the suite validate the mask + fuzz contract against synthetic images on every run, so a broken comparator can't pass alongside green real-page checks.

**CI execution:** Runs as a separate **non-blocking** (`continue-on-error: true`) step in `.github/workflows/build.yml`, after the gating Unit Tests step (which still excludes ScreenshotTests via `-E ScreenshotTests`). The step is skipped on the `ubuntu-24.04-arm` runner — ScreenshotTests hangs indefinitely there under xvfb (see commit `5c173c7`), so it runs on Linux x64 (xvfb) and macOS only. The whole `build/tests/test_screenshots/` directory (actuals + failures) is uploaded as the `screenshot-diffs-*` artifact for maintainer review and as the input for baseline refreshes.

**Baseline refresh:** Manually triggered via the `Regenerate Screenshot Baselines` workflow (`.github/workflows/screenshot-baselines.yml`, `workflow_dispatch`), which runs the test with `NEXIS_GENERATE_REFS=1` and uploads the freshly captured PNG set as an artifact. The maintainer downloads it, visually confirms the rendering is intended, and commits the contents under `tests/reference_screenshots/{platform}/{theme}/` on a baseline-refresh PR. The release runbook (`RELEASE.md` §0) requires the latest baselines to be green (or an explicit waiver) before tagging.

**Architecture change:** The GUI sources were extracted into a `nexis-gui` static library so that both the `nexis` executable and the screenshot test can link against them without duplicating the source list. Reference images are stored in-repo under `tests/reference_screenshots/{platform}/{theme}/`. Linux baselines are *not* committed yet — the previous `.gitkeep`-only placeholders were removed under NEX-3382 so the gap is explicit; until a maintainer commits regenerated Linux PNGs (via `Regenerate Screenshot Baselines`), the Linux runs cleanly `QSKIP` with a "no reference baselines for linux/{dark,light}" message rather than silently passing.

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

**Phase 4 (Done):** UI regression testing (FR-41, NEX-3381):
- Screenshot comparison in CI — 12 pages × 2 themes per platform
- Qt-native pixel diff with configurable per-page tolerance
- Visual diff artifact upload on every CI run for manual review
- Non-blocking (`continue-on-error: true`) until references stabilize
- Skipped on ARM64 Linux runners (hangs under xvfb; commit `5c173c7`); runs on Linux x64 and macOS

**Phase 5 (Future):** Remaining coverage gaps:
- CleanerService (requires extracting logic from GUI executable, blocked by BUG-93)
- PackageTool / UpdateInfo (dpkg/pacman/brew output parsing)
- AppManager token replacement (BUG-49 regression tests)
- SettingManager defaults and overrides
- Integration tests for manager CRUD operations

**Current state:** see the canonical "By the numbers" table in [`APPLICATION_OVERVIEW.md`](APPLICATION_OVERVIEW.md#project-identity) for the live CTest executable / test-method counts. The suites cover core library parsers, utilities, tool parsing, widget parsing, service logic, manager logic, and theme validation, plus 1 screenshot regression test that covers 24 page/theme combinations per platform. Build system refactored to extract `nexis-gui` static library for test linkage. Static parser extraction pattern established for future test additions.

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
| `shared/nexis/signal_mapper.h` | Global event bus (12 signals — see canonical "By the numbers" table) — monitor signal count growth |
| `shared/nexis/Pages/Dashboard/metric_tile_base.h/.cpp` | Abstract base class for all dashboard tile styles; defines common interface (setValue, addDataPoint, setDisplayMode, etc.) and builds the unified tile chrome — two-line header (type + input/source + accent bar + pinned gear) via `buildChrome()` and value/trend footer via `appendFooter()`, plus `setSource`/`setHeroValue`/`setHeroSecondary`/`applyAccentColor`/`applyChromeForMode`/`bodyTop`/`bodyBottom` helpers (FR-53, FR-77, GH#191) |
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
| `shared/nexis/Managers/dir_size_scanner.h/.cpp` | SSO-3737 / FW-09: off-thread recursive directory-size aggregation that backs the built-in treemap visualizer. Skips symlinks; (dev,inode) dedup for hard links. Pure `scanSynchronous()` exposed for unit tests. |
| `shared/nexis/Pages/Resources/disk_map_view.h/.cpp` | SSO-23862: abstract `QWidget` base for the three disk-map visualization modes. Owns the shared `DirSizeNode` tree/focus/drill-stack, theme colours, hover bookkeeping, and the reveal/trash/drill-into context menu; subclasses only implement `rebuildLayout()` (turn the focus's children into tiles/circles/wedges) plus paint/mouse-event hit-testing against that geometry. Keeps all three modes on identical scan data and interaction contract with no duplicated tree-walking. |
| `shared/nexis/Pages/Resources/treemap_view.h/.cpp` | SSO-3737 / FW-09 (refactored onto `DiskMapView` in SSO-23862): squarified treemap rendering on raw `QPainter`; hover tooltips, drill-down, context menu hooks. |
| `shared/nexis/Pages/Resources/bubble_map_view.h/.cpp` | SSO-23862: bubble-map (circle-packing) mode. Radii ∝ `sqrt(size)`; siblings are packed via a golden-angle-spiral seed plus ~200 iterations of pairwise-separation/centroid-pull relaxation (approximate but tight and dependency-free, O(n² · iterations)). |
| `shared/nexis/Pages/Resources/sunburst_view.h/.cpp` | SSO-23862: sunburst (radial) mode. Renders the focus's children as a single donut ring (wedge angle ∝ size, centre hole labelled with the focus node) rather than a multi-level ring stack, so drill-down/hover/context-menu match `TreemapView`/`BubbleMapView` exactly — one level shown at a time, double-click drills in. |
| `shared/nexis/Pages/Resources/disk_treemap_dialog.h/.cpp` | SSO-3737 / FW-09, extended in SSO-23862: wraps `DirSizeScanner` + all three `DiskMapView` modes behind a `QStackedWidget` and a toolbar picker. Every scan/drill/theme change is applied to all three views in lockstep (not just the visible one) so switching modes is instant and never re-scans or re-derives from a different tree snapshot. Routes "Move to trash" through `FileSearchService` to share the cleaner's trash path — a reversible action, so it keeps the lightweight `QMessageBox::question` confirm used elsewhere for trash (e.g. `DiskToolsPage::onLargeOldTrash`), not the File Shredder's heavier preview/irreversibility-warning dialog (SSO-15381), which exists because shredding can't be undone. SSO-23861: the dialog listens for `FileSearchService::fileOperationFinished` and re-scans `mLastScannedPath` on a successful trash so all three views reflect the removal. |
| `shared/nexis/Common/trust_safety_runner.h/.cpp`, `trust_safety_preview_dialog.h/.cpp`, `trust_safety_types.h` | SSO-15380: shared explain-before-run / itemized-preview / cancel / dry-run component for maintenance surfaces. `TrustSafetyRunner` follows the `DirSizeScanner` off-thread + pollable-cancel-flag pattern; `TrustSafetyPreviewDialog` takes any `TrustSafetyActionProvider`. See `shared/nexis/Common/README.md` for adoption. |
| `macos/nexis-core/Info/sparkle_update_downloader.h/.cpp`, `sparkle_update_installer.h/.cpp` | SSO-17776 (gated by the SSO-17775 design review): the real `SparkleSignatureVerifier` call site. `SparkleUpdateDownloader` is a GUI-thread `QObject` driven entirely by `QNetworkAccessManager` signals (no blocking `QEventLoop`, unlike `SparkleUpdateScanner::fetchFeed`) so the GUI stays responsive and `cancel()` is safe to call at any time. `HomebrewPage` marshals the downloaded bytes to a `QtConcurrent::run()` worker (pkgutil/unzip/plutil are blocking) and posts the result back via `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` — the same cross-thread-result pattern as `DirSizeScanner`/`TrustSafetyRunner`, but returning through a plain callback instead of a signal. `SparkleUpdateInstaller::verifyAndInstall()` itself takes injectable `Launcher`/`Revealer` `std::function` seams instead of calling `QDesktopServices::openUrl` directly, so its fail-closed contract is unit-testable without a live installer prompt. |
