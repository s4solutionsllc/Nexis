# Nexis — Application Overview

> A comprehensive reference for what Nexis does and how it is built.
> Last updated: 2026-06-12 | Version 2.4.0

---

## Table of Contents

1. [Project Identity](#project-identity)
2. [Platform Support](#platform-support)
3. [Application Features](#application-features)
   - [Dashboard](#1-dashboard)
   - [Hardware Info](#2-hardware-info)
   - [Startup Apps](#3-startup-apps)
   - [System Cleaner](#4-system-cleaner)
   - [Disk Tools](#5-disk-tools)
   - [Search](#6-search)
   - [Services](#7-services)
   - [Processes](#8-processes)
   - [Uninstaller](#9-uninstaller)
   - [Resources](#10-resources)
   - [Network Usage](#10a-network-usage)
   - [Helpers](#11-helpers)
   - [APT Repository Manager / Homebrew](#12-apt-repository-manager--homebrew)
   - [Docker](#13-docker)
   - [GNOME Settings](#14-gnome-settings)
   - [System Logs](#15-system-logs)
   - [Settings](#16-settings)
4. [Architecture Overview](#architecture-overview)
5. [Core Library](#core-library)
6. [Manager Layer](#manager-layer)
7. [Build System](#build-system)
8. [Theme System](#theme-system)
9. [Configuration and Settings](#configuration-and-settings)
10. [Translation System](#translation-system)
11. [Application Lifecycle](#application-lifecycle)
12. [Data Flow](#data-flow)

---

## Project Identity

Nexis is a **cross-platform (Linux + macOS) system optimizer and monitoring tool** built with C++17 and Qt 6. It provides real-time system monitoring, cleaning, optimization, and configuration management through a unified graphical interface.

**Origin:** Nexis began as a fork of [Stacer](https://github.com/oguzhaninan/Stacer), the popular Linux system optimizer that went inactive in 2020 with 38+ known bugs. After porting to Qt 6, adding native macOS support, fixing those inherited bugs, and adding GPU monitoring, hardware health tracking, scheduled cleaning, Docker management, and more, the project was rebranded as **Nexis** to reflect that it had become something distinct.

**By the numbers:**
- ~37,000 lines of C++ code across 288 source files
- 17 application pages
- 16 system info providers (BatteryInfo, BootAnalysisInfo, CpuInfo, DiskHealthInfo, DiskInfo, FanInfo, GpuInfo, MemoryInfo, NetworkInfo, PowerProfileInfo, ProcessInfo, StartupInfo, SystemInfo, ThermalInfo, UpdateInfo) — all 15 now wired through InfoManager (BootAnalysisInfo + StartupInfo added in WI-27 / SSO-3389), PowerProfileInfo added in v2.1.16
- 6 tool classes (package management, services, Docker, APT sources, GNOME settings, file search)
- 8 domain services (StartupService, FileSearchService, HostService, ProcessService, SystemServiceManager, DockerService, PackageService, DuplicateFinderService)
- 3 utility classes
- 7 manager singletons
- 3 themes (Dark, Light, Auto)
- 34 languages
- 88 features implemented, 103 bugs fixed since fork

**By the numbers (canonical table — referenced by [`ARCHITECTURE_REVIEW.md`](ARCHITECTURE_REVIEW.md)):**

| Metric | Value | Source of truth |
|--------|-------|-----------------|
| Version | 2.3.14 | `project(... VERSION ...)` in `CMakeLists.txt` |
| Source LOC (C++) | ~48,700 | `shared/`, `linux/`, `macos/` (`*.cpp`/`*.h`/`*.mm`) |
| Source files (C++) | 338 | same |
| Test LOC | ~6,050 | `tests/` |
| Test executables | 35 (34 unit + 1 screenshot) | `tests/CMakeLists.txt` |
| Test methods | ~501 | `private slots:` in `tests/*/test_*.cpp` |
| Always-visible pages | 15 | `shared/nexis/Pages/` (Dashboard, HardwareInfo, StartupApps, BootAnalysis, SystemCleaner, DiskTools, Search, Services, Processes, Uninstaller, Resources, Network, Helpers, SystemLogs, Settings) |
| Conditional pages | 3 | APTSourceManager / Docker / GnomeSettings — guarded in `app.cpp` by `ToolManager` capability checks |
| Info providers | 17 | `shared/nexis-core/Info/` (15 cross-platform + `PsiInfo` + `OomdInfoLinux` Linux-only); all wired through `InfoManager` (`BootAnalysisInfo`/`StartupInfo` added in WI-27 / SSO-3389; `OomdInfoLinux` added in FW-11 / SSO-3739) |
| Tool classes | 8 | `shared/nexis-core/Tools/` — 6 wired through `ToolManager` (`ServiceTool`, `PackageTool`, `AptSourceTool`, `GnomeSettingsTool`, `RepoHealthChecker`, `RepoRepairEngine`) plus `DockerTool` and `FileSearchTool` consumed directly by their services |
| Utility classes | 5 | `CommandUtil`, `DisplayServerUtil`, `FileUtil`, `FormatUtil`, `HeadlessUtil` in `shared/nexis-core/Utils/` |
| Manager singletons | 9 | `shared/nexis/Managers/` (`AppManager`, `InfoManager`, `ToolManager`, `SettingManager`, `CleanerService`, `CleaningProfilesService`, `ScheduleManager`, `ProcessPrefsManager`, `DataRefreshService`) |
| Domain services | 9 | `shared/nexis/Services/` (`StartupService`, `FileSearchService`, `HostService`, `ProcessService`, `SystemServiceManager`, `DockerService`, `PackageService`, `DuplicateFinderService`, `SnapshotService`) |
| `SignalMapper` signals | 10 | `shared/nexis/signal_mapper.h` |
| `DataRefreshService` QTimers | 5 | fast (1 s) / medium (5 s) / slow (30 s) / process (configurable) / update (1 h) |
| `DataRefreshService` signals | 16 | 14 cross-platform + Linux-only `psiUpdated`/`oomdUpdated` (FW-11/SSO-3739) |
| Themes | 2 (Dark = `default/`, Light = `light/`) | `shared/nexis/static/themes/` |
| Color schemes | 3 (Auto / Light / Dark) | `AppManager::resolveThemeName()` |
| Translations | 34 languages | `shared/translations/*.ts` |

Update the table whenever the underlying value changes — the same numbers are referenced verbatim by `docs/ARCHITECTURE_REVIEW.md`, so a single edit here keeps both docs honest.

---

## Platform Support

Nexis runs natively on **Linux** and **macOS** (Intel + Apple Silicon). The codebase uses compile-time platform selection: shared code in `shared/`, with platform-specific implementations in `linux/` and `macos/`.

### Linux display server (Wayland / X11)

Nexis is a Qt6 application and runs as a **native Wayland client** under Wayland sessions (including Ubuntu 26.04 / GNOME 50, which removes the X11 session entirely) and as a native **X11** client under X.Org. There are no `X11`/`xcb`/`QX11Info` dependencies in the codebase — windowing, screen enumeration, and screenshot capture all use Qt abstractions (`QScreen`, `QWidget::grab()`). The only path that ever forces a non-default QPA is the scheduled-clean entry point (`--clean` / `--check-threshold`), which sets `QT_QPA_PLATFORM=offscreen` so cron and systemd-user timer invocations don't need a live display server (SSO-3368). No feature requires XWayland; the optional `DisplayServerUtil` (`shared/nexis-core/Utils/display_server_util.h`) provides a single canonical detector for any future XWayland-gated feature.

### Always-visible pages (both platforms)
Dashboard, Hardware Info, Startup Apps, Boot Analysis, System Cleaner, Disk Tools, Search, Services, Processes, Uninstaller, Resources, Network Usage, Helpers, System Logs, Settings

### Conditional pages
| Page | Condition | Linux | macOS |
|------|-----------|-------|-------|
| APT Repository Manager | APT detected (`apt-get` in PATH) | Yes | No |
| Homebrew | `brew` binary found | No | Yes |
| Docker | `docker` binary found | Yes | Yes |
| GNOME Settings | `gsettings` available (GNOME DE) | Yes | No |

Pages that don't apply to the current platform are hidden entirely — no grayed-out buttons or "not available" messages.

### Platform-specific backends

| Subsystem | Linux | macOS |
|-----------|-------|-------|
| CPU info | `/proc/stat`, `/proc/cpuinfo`, sysfs cpufreq | `sysctl`, Mach `host_processor_info` |
| Memory info | `/proc/meminfo` | `sysctl hw.memsize`, Mach `vm_statistics64` |
| Disk I/O | sysfs (`/sys/block/`) | IOKit |
| GPU info | sysfs (AMD), `nvidia-smi` (NVIDIA), sysfs (Intel) | IOKit IOAccelerator, Metal |
| Battery info | `/sys/class/power_supply/` | IOKit `IOPMPowerSource` |
| Thermal sensors | `/sys/class/hwmon/` (incl. vendor WMI surfaces — `asus`, `hp`, `legion`, `ideapad`) | SMC (System Management Controller) |
| Fan sensors | `/sys/class/hwmon/*/fan*_input`, ThinkPad `/proc/acpi/ibm/fan`, Dell `/proc/i8k`, `nvidia-smi` | SMC (`FNum`, `F{N}Ac` keys, fpe2 decoding) |
| Disk health | `smartctl` | `smartctl` + `diskutil` plist |
| Process listing | `/proc/[pid]/` | `sysctl` KERN_PROC |
| Network info | `/sys/class/net/` + `QNetworkInterface` | `QNetworkInterface` |
| Services | `systemctl` (systemd) | `launchctl` (stubbed) |
| Packages | APT/DNF/Pacman/Snap | Homebrew + native `.app` bundles |
| Autostart | `~/.config/autostart/*.desktop` | `~/Library/LaunchAgents/*.plist` |
| Sudo elevation | `pkexec` / `sudo` | `osascript` (AppleScript admin prompt) |
| Scheduled cleaning | systemd timers / cron | launchd plists |

---

## Application Features

### 1. Dashboard

Real-time system monitoring at a glance in a **customizable bento grid layout** of specialized widgets, replacing the earlier circular gauge (CircleBar) design. All metric tiles inherit from `MetricTileBase`, an abstract base class supporting three `DisplayMode` values — **Normal**, **Hero**, and **Large** — each with distinct font sizes for value/label/sublabel, selected via QSS dynamic properties with `unpolish()`/`polish()` cycling. Tiles can be rearranged and resized via edit mode, and each tile's **visual style can be changed** independently.

**Default tile layout:**
- **CPU** — Independent `MetricTile` with sparkline history (1s refresh)
- **Memory** — Independent `MetricTile` with sparkline history (1s refresh). Subtitle shows swap usage plus platform-specific breakdown: wired/active/compressed on macOS, available memory on Linux. On macOS, tile accent color dynamically reflects memory pressure state (green=normal, yellow=warning, red=critical) via `kern.memorystatus_vm_pressure_level` sysctl; on Linux, pressure derived from PSI or MemAvailable heuristic. User-set custom colors (FR-55) take priority over pressure indication.
- **Disk** — `DiskTile` with custom-painted donut chart showing usage percentage, capacity text, and drive health badge with verdict and numeric percentage (e.g., "Apple SSD: Good (92%)") via `setDriveHealth()` (5s refresh). Gear icon at the top-left next to the tile title (visible when 2+ disks detected) opens a dropdown menu to switch the displayed disk; selection is persisted.
- **Network** — `NetworkTile` with two-row layout: Download and Upload labels each paired with a separate `QChart` sparkline instance (dual RX/TX charts), horizontal divider, and active interface name (1s refresh)
- **GPU** — Utilization percentage with device name subtitle. Gear icon at the top-left next to the tile title (visible when 2+ GPUs detected) opens a dropdown menu to switch the monitored device; selection is persisted (1s refresh; tile hidden if no GPU detected)
- **Temperature** — Selectable sensor via gear icon menu (2+ sensors) with sensor name subtitle, sparkline history (1s refresh; hidden if no sensors)
- **Fans** — Fan RPM with selectable sensor via gear icon menu (2+ fans), percentage gauge based on rpm/maxRpm, teal accent (`@fanColor`), dedicated `fanUpdated` signal (1s refresh; hidden if no fans detected)
- **Battery** — Charge level percentage (5s refresh; hidden if no battery)
- **Health Score** — `HealthScoreTile` displaying a composite 0–100 system health score computed by `HealthScoreCalculator`. Aggregates six components with weighted scoring: CPU load (15%), memory usage (20%), disk space (25%), temperature (15%), battery health (10%), SMART disk health (15%). Unavailable components (no battery, no thermal sensors, no SMART) are excluded and weights redistributed proportionally. Color-coded: green (Excellent, ≥80), amber (Good/Fair, 60–79), red (Poor, <60). In Large/Hero display modes, shows per-component breakdown bars with 3-letter labels, colored fill proportional to score, and numeric values. Always visible (not conditional), hideable via edit mode like any other tile. Quick action button: **"System Checkup"** launches the Maintenance Wizard dialog (FR-83).
**System summary bar** (full width) — hostname in bold followed by OS, CPU model, and RAM total inline (single-line compact layout).

**Footer status bar** — Displays app version and refresh interval at the bottom edge. The system summary bar and footer status bar can be hidden via a toggle in Settings (FR-75); kiosk mode also hides them automatically.

**Widget styles** — Each tile (except Network) can be switched between 6 visual styles via a paintbrush icon visible during edit mode:
- **Sparkline** (default) — line chart showing recent history with progress bar and trend indicator
- **Gauge** — classic ¾-circle arc gauge with percentage in center, conical gradient fill
- **Hybrid** — compact gauge arc combined with a mini sparkline chart below
- **Ring** — full 360° activity ring (Apple Watch style) with percentage inside
- **Speedometer** — analog dial with needle, tick marks, and green→red gradient arc
- **VU Meter** — segmented vertical bar with bottom-up fill and stats panel
- **Donut** (Disk tile only) — custom-painted donut chart with usage text (Disk tile default)

**Edit mode** (pencil icon next to kiosk button, or **Ctrl+E**):
- Activates drag-and-drop reordering of dashboard tiles with visual feedback
- Snap-to-grid resizing via corner handles — tiles support 1x1, 1x2, 2x1, and 2x2 grid cell sizes
- **Per-tile style and color selector** — paintbrush icon button (top-right) on each switchable tile opens a menu with two sections: (1) visual styles with checkmark on the current style (selecting swaps the tile widget), and (2) a customization section that adapts to the active style: for most styles a color palette with 16 preset swatches plus "Default" to revert to the theme color; for Speedometer and VuMeter styles a "Color Range" picker with four directional presets (Green→Red, Red→Green, Blue→Red, Teal→Orange) that control the gradient arc or bar segment color progression — useful for indicating whether high values are good or bad. Network tile color override applies to both download and upload sparklines (upload derived via HSL shift).
- **Remove widget** — orange X button (top-right) on each tile hides it from the dashboard; hidden tiles are excluded from the grid and occupancy tracking
- Edit toolbar with **Reset Layout** button to restore default tile arrangement (including default styles, colors, and all removed tiles)
- Layout persisted as JSON in `settings.ini` across sessions (tile positions, sizes, styles, colors, and visibility)
- Edit mode and kiosk mode are mutually exclusive — entering one exits the other
- Implemented via `DashboardTileWrapper` (decorator pattern providing edit-mode mouse handling, style/color switching around each tile widget)

**Additional features:**
- Update checker — compares installed version against GitHub releases
- **Maintenance Wizard** (FR-83) — "System Checkup" modal dialog launched from Health Score tile quick action. Runs 4 checks in parallel via `QtConcurrent::run()`: (1) cleanable junk scan, (2) orphan package detection, (3) pending system updates, (4) health score calculation. Each check shows a running/complete/warning/error icon with detailed results. "Clean Safe Items" button auto-cleans low-risk categories (package cache, crash reports, app logs/caches, dev tool caches, broken symlinks, snap/flatpak revisions on Linux). Theme-aware with step status colors from `@successColor`/`@warningColor`/`@destructiveColor` tokens.
- Reset Layout also available on the Settings page
- **Quick Actions tray submenu (FR-125)** — "Quick Actions" submenu in the system tray right-click menu providing one-click access to: Open Command Palette, Run System Cleaner Scan (navigates to the cleaner page and starts a full scan), and Power Profile switch (Linux only — shows available profiles from the detected backend as a checkable submenu; checked state refreshes on every open).
- Kiosk mode — fullscreen dashboard-only view (hides sidebar + title bar), state persisted across sessions. Three entry/exit methods:
  - **Keyboard:** F11 to toggle, ESC to exit
  - **System tray:** Checkable "Kiosk Mode (F11)" action in tray context menu
  - **Dashboard button:** Floating fullscreen/collapse icon at top-right corner, swaps between enter/exit icons
  - On activation, a transient "Press ESC to exit kiosk mode" overlay fades in then out (~3.5s)

### 2. Hardware Info

Comprehensive static hardware inventory displayed in tabular sections. Two export options:
- **Export System Report** — plain text file with aligned columns. Default: `nexis-report-YYYY-MM-DD.txt`.
- **Export as HTML (FR-126)** — self-contained HTML file (inline CSS, no external dependencies) containing a system snapshot (CPU %, memory, GPU, battery), all hardware tables, top-10 processes by CPU at export time, and pending update count. Default: `nexis-report-YYYY-MM-DD.html`.

**9 sections:**
- **System** — Hostname, OS, distribution, kernel, architecture, desktop environment
- **Processor** — Model name, physical/logical core count, base clock, L1/L2/L3 cache sizes
- **Graphics** — GPU name(s) and vendor(s)
- **Memory** — Total RAM, total swap
- **Battery** (if present) — Design capacity, current max capacity, cycle count, health percentage
- **Fans** (if present) — Per-fan RPM, device name, label; macOS reads SMC FNum/F{N}Ac keys. Linux: primary hwmon scan (`/sys/class/hwmon/*/fan*_input`), with fallback detection chain for ThinkPad (`/proc/acpi/ibm/fan`), Dell (`/proc/i8k`), and NVIDIA proprietary (`nvidia-smi` fan percentage)
- **Storage** — Per-drive: name, size, model, SMART health verdict (Good/Caution/Critical/Unknown), color-coded. On Linux, if any drive requires elevated permissions for SMART data, an **Unlock All Drives** button appears at the top of the section. Clicking it shows a confirmation dialog with an optional **"Also grant permanent access"** checkbox. Confirming runs a single `pkexec sh -c "..."` call that reads all locked drives in one password prompt; if the checkbox is checked, `setcap cap_sys_rawio+ep` is applied in the same elevation so no password is needed on future launches. Per-drive Unlock buttons remain in the table as a granular fallback.
- **Network** — Interface name, MAC address, IP addresses
- **Thermal** — Sensor readings (if available)

### 3. Startup Apps

Manage applications that auto-start at login.

- List view with enable/disable toggles per entry
- Real-time search filter
- Add new autostart entry: Name, Command, Comment, Delay (seconds), GenericName, Icon (Linux)
- Edit existing entries (`.desktop` files on Linux, `.plist` files on macOS)
- Delete autostart entries
- Real app icons via `QIcon::fromTheme()` (Linux) or `QFileIconProvider` (macOS)
- macOS automatically filters out `com.apple.*` system agents
- **macOS:** Items grouped into three categories with section headers:
  - **User Agents** (`~/Library/LaunchAgents`) — full edit/toggle/delete control; enabled state sourced from `launchctl print-disabled user/<uid>`
  - **System Agents** (`/Library/LaunchAgents`) — read-only; shows plist path
  - **System Daemons** (`/Library/LaunchDaemons`) — read-only; shows plist path
  - **BTM Records** (`sfltool dumpbtm`, SSO-3738 / FW-10) — read-only; surfaces every entry the macOS Background Task Management database knows about (Login Items, Launch Agents/Daemons, helper launchers, app extensions, MDM-managed items), not just the items resolvable from `~/Library/LaunchAgents`. Per-row badges flag orphaned records (executable + plist both missing on disk), duplicate identifiers/executable paths, Apple-managed entries under `/System/Library/Launch{Daemons,Agents,Angels}` or `/usr/libexec`, and current enabled state. A destructive-styled **Repair BTM…** header button opens a confirmation dialog and runs `sudo sfltool resetbtm` — the standard repair when the Tahoe 26.4 `backgroundtaskmanagementd` bug stalls Login Items — once the user types `RESET` to acknowledge that every background item will re-prompt on next login.
- File path shown as subtitle on every row

### 3a. Boot Analysis

Ranked breakdown of which services and processes slow down system startup.

- **Linux**: Runs `systemd-analyze blame` to enumerate per-service startup times; `systemd-analyze` for total boot time. Entries ranked descending by duration, labeled High (≥ 5 s) / Medium (1–5 s) / Low (< 1 s). Shows "not available" if systemd-analyze is absent.
- **macOS**: Shows total uptime since last boot via `sysctl kern.boottime`. Per-service timing is not available without elevated privileges.
- Async analysis (QtConcurrent) — UI stays responsive; Refresh button triggers a new analysis on demand.
- Data class: `BootAnalysisInfo` (platform-specific subclasses `BootAnalysisInfoLinux` / `BootAnalysisInfoMacOS`).

### 4. System Cleaner

Scan and remove system junk files across 10 categories.

**Scan categories:**
1. **Package Cache** — APT, DNF/YUM, Pacman, or Homebrew caches (platform-detected)
2. **Crash Reports** — System crash dumps
3. **Application Logs** — `/var/log`, `~/.local/share/**/logs`
4. **Application Caches** — `~/.cache` (Linux), `~/Library/Caches` (macOS)
5. **Trash** — User trash bin contents
6. **Dev Tool Caches** — npm, cargo, gradle, Electron app caches, pip cache
7. **Broken Symlinks** — Detects broken symbolic links in `~/.local/`, `~/bin/`, Homebrew prefix (macOS) or `/usr/local/bin` (Linux)
8. **Browser Privacy** — Browser caches (Chrome, Edge, Brave, Firefox, Safari), session data, and OS-level recent file lists (macOS LSSharedFileList, Linux recently-used.xbel)
9. **Snap/Flatpak Revisions** *(Linux only)* — Stale disabled snap revisions and unused Flatpak runtimes; cleaned via `snap remove --revision` and `flatpak uninstall --unused`
10. **Application Profiles (FW-12)** — Data-driven category sourced from declarative JSON profiles (see `docs/CLEANING_PROFILES.md`). Bundled profiles cover 31 Linux and 32 macOS application footprints (browsers, IDEs, Xcode caches, package-manager / build-tool caches, container runtimes, comms apps). Users add their own under `~/.config/Nexis/cleaning_profiles/`; user profiles override bundled by `id`. Aggressive profiles (e.g. Maven `~/.m2`, Cargo registry, Mail Downloads) are gated behind `SettingManager::CleanerAggressiveProfilesEnabled` so the default "scan + clean" cadence never crosses the rebuild-required line without explicit opt-in.

**Exclusion rules:**
- Manage exclusion rules via gear button on the categories page → opens ExclusionManagerDialog
- Add file or folder exclusions via native file chooser; remove via dialog
- Right-click any scan result item → "Always exclude this" to instantly persist an exclusion rule and remove the item from the tree
- Excluded paths are skipped during scanning (file exact match, folder prefix match, symlink-aware)
- Exclusions are enforced at **every depth** of the recursive deletion walk, not just on the top-level scan entry — a protected child or sub-folder inside a scanned cache directory is preserved even when the cache directory itself is the cleanup target (NEX-3370)
- Exclusions persist across app restarts via JSON in QSettings
- **macOS 27 Privacy & Security awareness (SSO-3732 / FW-05):** macOS 27 silently denies cross-team app-container reads/deletes without Full Disk Access. The cleaner detects `EPERM` / `EACCES` outcomes through a `removeFile()` seam that classifies them as `AccessDeniedByPolicy` (vs. generic I/O errors), refuses to credit un-cleanable bytes as freed, and exposes the count via `CleanResult::accessDeniedPaths`. On detection (cleaning or the scan-time `~/Library/Containers` "exists but empty" probe) the page posts a persistent banner with an "Open Privacy & Security…" link that drops the user directly into the Full Disk Access pane — cache cleaning is never a silent no-op on macOS 27

**UI features:**
- **Page 0 — Category Cards (FR-130):** Two-column card grid with per-card name, path subtitle, post-scan size label, and checkbox. Checked cards gain a highlighted border. A persistent footer shows estimated total recoverable space and a "Clean selected" button (enabled after scanning). "View scan results →" navigates to the detailed tree view.
- **Page 1 — Scan Results tree:** Hierarchical tree view with checkboxes (category > individual files)
- Sort by name (A-Z, Z-A) or size (small-large, large-small)
- Total size display of selected items
- Schedule indicator showing next/last automated clean time
- Sidebar badge counts checked categories (shown on the System Cleaner nav button)

**Scheduled cleaning integration:**
- Automated background scans/cleans via OS-native scheduler (launchd, systemd, cron)
- Headless CLI mode: `./nexis --clean <schedule-id>`, `./nexis --check-threshold`
- Frequencies: Daily, Every N Days, Weekly, Monthly
- Category selection, minimum file age filter, threshold alerts
- **Every-N-days cadence on systemd:** systemd has no native every-N-days trigger, so the timer fires daily and `CleanerService::cleanSchedule()` consults `ScheduleManager::isEveryNDaysGateBlocked()` to skip runs where `lastRun.daysTo(now) < everyNDays`. cron's `*/N` operator and launchd's `StartInterval` give the right cadence directly

### 5. Disk Tools

Two-mode page for finding space-wasting files, accessible via the MANAGE sidebar section.

**Mode 1 — Large & Old Files (FR-62):**
- Directory picker with smart defaults (Home, Downloads, Documents)
- Configurable filters: size threshold (MB/GB), age threshold (days/months/years)
- Three match modes: Either (large OR old), Large only, Old only
- Recursive QDirIterator scan in background thread with cancellation support
- Results in sortable QTreeWidget with columns: Name, Path, Size, Last Accessed, Last Modified
- Checkboxes for selective deletion via QFile::moveToTrash()

**Mode 2 — Duplicate Finder (FR-63 / FW-08):**
- Shared directory picker (synced with Large & Old mode)
- Configurable minimum file size (KB/MB/GB) and optional glob pattern filter
- DuplicateFinderService: 3-stage pipeline (size grouping → partial 4KB SHA-256 → full SHA-256)
- Progress bar with stage-by-stage status messages
- Results in grouped QTreeWidget: parent = duplicate group (wasted space), children = files
- First file in each group unchecked (kept), remaining pre-checked for deletion
- Cancellable scan with Cancel button
- FW-08 (SSO-3736): scans honor the CleanerService exclusion engine; trashing
  routes through `DuplicateFinderService::trashFiles()` which enforces the
  never-delete-last-copy invariant (retains the lexicographically smallest path
  of any duplicate group whose entire surviving set was selected) and drops
  excluded paths before the moveToTrash seam
- FW-08 additional service surface (UI hookup pending): `scanLargest(topN)`
  for explicit top-N largest-file ranking and `scanEmptyFolders()` for empty-
  directory cleanup; both honor exclusions and share the same cancel path

**Shared features:**
- Segmented control (QButtonGroup + QStackedWidget) for mode switching
- Confirmation dialog before trashing files
- Selection tracking label showing count and total size of checked files

### 6. Search

Advanced file search across the filesystem.

- Directory path selector (browse button)
- File name pattern filter
- File type/extension filter
- Size range filter (min/max)
- Date range filter (modified date)
- Search as root option (sudo)
- Results table with path, size, modified date (sortable columns)
- Double-click to open files in default application
- Right-click context menu: open folder, Move to Trash, Delete
- Move-to-Trash and Delete run on a worker thread and report success/failure through `FileSearchService::fileOperationFinished` — long bulk operations no longer freeze the UI or crash the app on timeout (SSO-3365, audit H4).

### 7. Services

Manage system services (daemons).

- Service list with status indicators (color-coded)
- Status filters: Running/Not Running, Enabled/Disabled
- Actions: Start/Stop service, Enable/Disable auto-start (all require sudo)
- Linux: `systemctl` for systemd services
- macOS: `launchctl` for launchd services

### 8. Processes

View and manage running processes.

- Process table with 17 columns: PID, name, user, CPU%, memory%, command line, Disk Read/s, Disk Write/s, Net Down/s, Net Up/s, and more
- Disk Read/s and Disk Write/s columns show per-process disk I/O rates via `proc_pid_rusage()` (macOS) or `/proc/<pid>/io` (Linux), using delta-based calculation with `QElapsedTimer`
- Net Down/s and Net Up/s columns show per-process network bandwidth via `nettop` parsing (macOS only; Linux shows N/A)
- All 4 new I/O columns are hidden by default (toggled via header context menu)
- Real-time search filter
- Sortable column headers
- Refresh rate slider (1s to 10s, user-configurable)
- End process action (sudo if owned by another user)
- Right-click context menu with copy PID
- Column visibility toggles via header menu

### 9. Uninstaller

Uninstall applications and packages. Labeled "Applications" on macOS. Three-tab layout (four on hosts with APT 3.1+): System Packages, Snap Packages, Orphan Packages, and APT History.

- Package tree view grouped by type (Formula/Cask on macOS; installed/universe on Linux)
- Search filter with auto-expand matching sections
- Multi-select checkboxes for batch uninstall
- Purge option (Linux only) — removes config files in addition to package
- Dry-run confirmation showing dependencies that would also be removed (APT)
- Async background loading with progress indicator
- **Orphan Packages tab** — Lists packages no longer required by any installed package. Removal via platform `autoremove` command (all-or-nothing). Supported on APT, DNF, Pacman, and Homebrew.
- **APT History tab (Linux, APT 3.1+)** — Lists recent `apt` transactions parsed from `apt history-list` (id, date, operation, command). One-click **Undo Last Transaction**, and per-row **Undo Selected** / **Rollback To Selected** wired to `apt history-undo` / `apt history-rollback`. A **Why? / Why Not?** panel surfaces `apt why` / `apt why-not` output for any package the user names. The tab and its nav button are hidden when `apt --version < 3.1`, so older Debian/Ubuntu users see no broken affordance (FW-07 / SSO-3735).

**Platform backends:**
- Linux: `apt-get remove/purge`, `dnf remove`, `pacman -R`, `snap remove`, `apt-get autoremove` / `dnf autoremove` / `pacman -Rns`; on APT 3.1+ also `apt history-list/-info/-undo/-rollback` and `apt why/why-not`
- macOS: `brew uninstall` for Homebrew packages; `QFile::moveToTrash` (`NSFileManager::trashItemAtURL:`) for `.app` bundles — no AppleScript/`osascript` is involved, so bundle names containing quotes or other metacharacters cannot inject arbitrary code (SSO-3366, audit S1); `brew autoremove` for orphans

### 10. Resources

Historical time-series charts for system resource usage.

**Charts (60-second sliding window, 1s refresh unless noted):**
- CPU per-core utilization
- CPU 1/5/15-minute load averages
- GPU per-device utilization (if GPU detected)
- Disk read/write bytes/sec with dynamic Y-axis scaling
- Memory used, swap, plus 2 platform-specific series: Wired% and Compressed% on macOS; Available% and Active% on Linux (4 series total)
- Network download/upload bytes/sec
- Disk temperature per-drive (30s refresh, if SMART supported)
- **CPU Pressure Stall (FR-124, Linux only)** — 3-series chart (avg10, avg60, avg300) sourced from `/proc/pressure/cpu`. Shows the percentage of time at least one task was stalled waiting for CPU. Only created when the PSI file is present (kernel 4.20+, `CONFIG_PSI=y`). Zero cost when hidden (uses DataRefreshService subscription gating).

**OOM Kills panel (FW-11 / SSO-3739, Linux only):** systemd-oomd / cgroup v2 observability card appended after the PSI chart. Shows the oomd service state (active / inactive / masked / not installed), oomd-attributed totals (`OOMKills`, `ManagedOOMKills`), the kernel-side `oom_kill` counter from `/sys/fs/cgroup/memory.events`, and the most recent kill events (timestamp, unit, cgroup path, reason) parsed from `journalctl -u systemd-oomd.service`. Hides itself entirely when no oomd or cgroup-v2 signal is available, and shows a defensive warning on the rare host that doesn't expose the v2 unified hierarchy (systemd 259 / Ubuntu 26.04 is v2-only — v1 hosts will not boot the new systemd). Subscribes to `DataRefreshService::Signal::Oomd` on the 5 s medium tick.

**Disk Usage Launcher:**
- Quick-launch card for platform-appropriate disk analyzer tools
- Configurable preference in Settings (Linux: Baobab, Filelight, QDirStat, ncdu; macOS: GrandPerspective, DaisyDisk, OmniDiskSweeper; or custom path)
- **Built-in Treemap (FW-09, SSO-3737):** secondary "Built-in Treemap" button on the same card opens a built-in `DiskTreemapDialog` that runs a `DirSizeScanner` on a `QtConcurrent` worker, then renders a squarified treemap with `TreemapView` (pure `QPainter`). Supports drill-down (double-click), hover tooltips, "Reveal in file manager", and "Move to trash" (reuses `FileSearchService` → cleaner trash path). Skips symlinks and dedups hard links so byte counts match what Baobab/DaisyDisk would report. The external-tool launcher remains as a parallel option.

### 10a. Network Usage

Continuous per-interface network data usage tracker with monthly cap management. Located in the MONITOR sidebar section between Resources and Helpers.

**Usage Tracking:** A `NetUsageTracker` singleton subscribes to `DataRefreshService::networkPerInterfaceUpdated` (1s fast tick) at startup — always accumulating even when the page is not open. Each tick carries a `QHash<QString, NetInterfaceStats>` snapshot of every non-loopback up+running interface; the tracker computes per-interface deltas, adds them to each interface's today bucket, and debounces a 60-second write to `SettingManager` as a JSON blob. The accumulator handles reboot/iface restart counter resets by detecting when a new absolute value is less than the previous one (skip the delta, update the baseline). Tracking the full snapshot (rather than just the default interface) means Wi-Fi (`wlp*`, `wlan0`) is recorded even when it isn't the system default route, and a newly-connected interface is picked up on the next tick (SSO-351).

**History:** 90-day rolling window of daily RX+TX buckets, stored as compact JSON in `SettingManager`'s INI file (see [Configuration and Settings](#configuration-and-settings) for the resolved path on each platform).

**Page UI:**
- Interface selector (All Interfaces or individual adapter)
- Live rate display (↓ download / ↑ upload bytes/sec)
- Summary cards: Today / This Week / This Month totals
- 30-day bar chart (stacked RX+TX per day, auto-scaled)
- Monthly cap progress bar (hidden when cap = 0 GB); color-coded green/amber/red at 0%/75%/90%
- Settings card: cap in GB (0 = disabled), billing cycle reset day (1–28), alert toggle

**Tray Alerts:** When cap is set and usage crosses 75%, 90%, or 100% of the cap within the current billing period, a system tray notification fires once per tier. The last-alerted tier persists across restarts in `SettingManager`.

### 11. Helpers

Miscellaneous utility tools, organized into two clearly labelled header sections:

**TOOLS section** — Tab-style checkable buttons that navigate a `QStackedWidget` below. One tool is active at a time. Buttons adapt to a two-row compact layout when the window is narrow (`resizeEvent` + `applyNavLayout()`).

**MAINTENANCE section** — Horizontal row of clickable `QFrame` cards (`#maintenanceCard`). Each card shows a title and plain-English description. Clicking triggers the corresponding confirmation dialog and system action. Cards are built dynamically in `buildMaintenanceSection()`:
- **Flush DNS Cache** (both platforms) — clears the local DNS cache. macOS: `dscacheutil -flushcache` + `killall -HUP mDNSResponder`. Linux: tries `resolvectl`, `systemd-resolve`, or `nscd` in order.
- **Rebuild Spotlight** (macOS only) — deletes and rebuilds the Spotlight search index (`sudo mdutil -E /`).
- **Verify Disk** (macOS only) — runs `diskutil verifyVolume /` with a 5-minute timeout, shows output in a scrollable result dialog.
- **Rebuild Launch Services** (macOS only) — rescans the app database (`lsregister -r`) and restarts Finder.

**Battery Charge Threshold (Linux only)** — Slider/preset control for limiting the maximum battery charge level, reducing long-term battery wear. Preset buttons: Maximize (100%) and Preserve (80%); a Custom preset exposes a slider from 50–100%. Writes `charge_control_end_threshold` (and `charge_control_start_threshold` when the node exists) via `pkexec`/`tee`. Performs a read-back verify after every write. Optionally persists the threshold across reboots by writing a udev rule to `/etc/udev/rules.d/99-nexis-battery-threshold.rules`. Hidden on hardware that does not expose the sysfs node. Implemented in `BatteryChargeThreshold` (`linux/nexis-core/Info/battery_charge_threshold.cpp`) and `BatteryChargeThresholdWidget`.

**Power Profile Switcher (Linux only)** — Segmented control with three buttons (Power Saver / Balanced / Performance) for switching CPU power profiles. Uses `power-profiles-daemon` (`powerprofilesctl`) as the primary backend (no root needed). Falls back to raw sysfs governor writes via `pkexec` on systems without PPD. Automatically detects available profiles, hides Balanced if the driver only supports two governors (e.g., `intel_pstate`). Warns if TLP or auto-cpufreq is active. Hidden on macOS and systems without cpufreq support.

**Hosts File Manager** — GUI editor for `/etc/hosts`:
- Add, edit, delete entries (IP address, hostname, aliases)
- Input validation: IPv4/IPv6 via `QHostAddress`, hostname format per RFC 1123 (with underscore tolerance), alias validation
- Save changes with confirmation dialog showing change summary (N added, N modified, N deleted)
- Automatic backup to `/etc/hosts.nexis-backup` before each save (permission-preserving via `sudo cp -p`)
- Writes via `sudo tee` (no temp file — content piped through stdin for security)
- Error feedback: auth cancellation and write failures shown via `QMessageBox`; success shown in status label
- Lazy-loaded: file parsed only when user navigates to the page

**Network Diagnostics** — One-click connectivity checklist with four sequential tests:
1. **Ping default gateway** — discovers gateway via `route -n get default` (macOS) or `ip route show default` (Linux), then pings it
2. **Ping external IP (1.1.1.1)** — verifies WAN/internet connectivity
3. **DNS resolution** — resolves `cloudflare.com` via Qt's `QHostInfo::fromName()` with latency measurement
4. **DNS server discovery** — lists configured DNS servers from `scutil --dns` (macOS) or `resolvectl status` / `/etc/resolv.conf` (Linux)
- Results displayed as pass/fail checklist with latency values in a themed card widget
- Self-contained `NetworkDiagWidget` (stacked widget page, like Hosts File Manager)
- Runs on `QThreadPool` worker thread to avoid UI blocking
- "Re-test" button for iterative debugging; lazy-loaded on first navigation
- Theme-aware: uses `@successColor`/`@destructiveColor` for pass/fail indicators

**Open Ports & Connections** — Tabular view of listening ports and active network connections:
- Parses `lsof -iTCP -P -n` (macOS) or `ss -tnp` (Linux, with `netstat` fallback)
- 8-column table: Protocol, Local Address, Port, Remote Address, Remote Port, PID, Process, State
- "Listening Only" toggle (default) pre-filters at the command level; "All Connections" shows established, close-wait, time-wait, etc.
- Text search filter across all columns (process name, address, port)
- Color-coded state column: green (LISTEN), orange (ESTABLISHED), red (CLOSE_WAIT/TIME_WAIT)
- Sortable columns via `QSortFilterProxyModel`; runs on `QThreadPool` worker thread
- Self-contained `OpenPortsWidget` (stacked widget page index 2); lazy-loaded on first click

**Firewall Status (Phase 1)** — Lightweight firewall status indicator with enable/disable toggle:
- Detects active firewall backend: macOS Application Firewall (`socketfilterfw`), `ufw` (Debian/Ubuntu), or `firewalld` (Fedora/RHEL)
- Status display: colored dot (green=enabled, red=disabled), backend name, toggle button
- macOS detail rows: Stealth Mode, Block All Incoming, App Rule Count (from `--listapps`)
- Enable/disable toggle with confirmation dialog and privilege elevation (`sudoExec`)
- Not-available state: warning label with `?` help button (tooltip lists installation commands for `ufw` and `firewalld`)
- Self-contained `FirewallWidget` (stacked widget page index 3); lazy-loaded on first click
- Theme-aware: uses `@successColor`/`@destructiveColor` for status dot, `@warningColor` for not-available state

**Wake-on-LAN (FR-120, cross-platform)** — Send magic packets to wake sleeping devices on the local network:
- "Discover Hosts" button reads the system ARP cache (Linux: `/proc/net/arp`; macOS: `arp -a`) on a `QThreadPool` worker thread
- Results shown in a 5-column table: IP, MAC, Hostname, Friendly Name (editable inline), Wake button
- "Wake" sends a standard 102-byte UDP magic packet (6×0xFF + 16×MAC) to the broadcast address on port 9; no root required
- Friendly names persist across sessions in `SettingManager` (JSON map keyed by MAC)
- Warning shown if no hosts are found (tip: ping the device first to populate the ARP cache)

- "Cancel" button kills the running process; "Run Again" available after completion
- Does not bundle the scanner — works with whatever is installed on the system

### 12. APT Repository Manager / Homebrew

Manage package repositories and sources. Conditional: shown only when the relevant package manager is detected.

**Linux (APT):**
- Repository list from `/etc/apt/sources.list.d/`
- Dual-format support: legacy one-line `.list` files and modern deb822 `.sources` stanzas
- APT-RPM support for ALT Linux, PCLinuxOS, Vine Linux
- Add, edit, delete repositories (requires sudo)
- Enable/disable without deleting
- Structured editor: type (deb/deb-src), URIs, suites, components, **Signed-By keyring path, Architectures**
- New repos written as deb822 `.sources` with an explicit `Signed-By` on systems where deb822 is the norm (Ubuntu 26.04+ / Debian trixie+, detected by `ubuntu.sources` or `debian.sources` presence); legacy `.list` editing kept for older distros. No `apt-key` invocation anywhere — APT 3.1 removed it, and Nexis writes the keyring path directly to `Signed-By:`. Edits round-trip byte-stable for unchanged fields and preserve unrecognised deb822 keys (`Languages:`, `Targets:`, embedded multi-line GPG keys) verbatim.
- Search filter

**macOS (Homebrew):**
- Package tree view grouped by Formula/Cask
- Install new packages by name
- Multi-select batch uninstall with checkboxes
- Search with auto-expand
- Async background loading via `brew info --json=v2`
- Implemented as a dedicated `HomebrewPage` (under `macos/nexis/Pages/Homebrew/`) backed by `HomebrewToolMacOS`, which implements the platform-neutral `RepositoryTool` interface. Linux's APT page lives under `linux/nexis/Pages/AptSourceManager/` and uses the extended `AptSourceTool` interface. (Prior to SSO-3390, both platforms shared a single page that branched on `Q_OS_MAC` and the macOS adapter shoehorned brew packages into the APT-source data model — that's gone.)

**Available Updates section (both platforms):**
At the top of the APT Source Manager / Homebrew page, an "Available Updates" section displays outdated packages in a 3-column tree widget (Source, Package, Version). A "Check Now" button triggers an on-demand refresh. Data comes from hourly background checks via `QtConcurrent::run()` in DataRefreshService (`mUpdateTimer`, 1h interval). macOS: `softwareupdate -l` + `brew outdated`. Linux: apt/dnf/pacman/zypper/snap/flatpak. Tray notification when update count goes from 0 to >0 (toggleable in Settings). The sidebar Homebrew/APT button shows an updates badge — full count when the sidebar is expanded, a colored dot (using `@updatesColor` theme token) when collapsed.

**Repository Health Dashboard (FR-87, BETA):**
Periodic background health checks via `DataRefreshService` (chained after update checks, 1h interval + manual Refresh Health button) validate repository integrity and detect common issues. Linux performs 6 checks: connection status, release file 404 errors, GPG key expiry, suite/components mismatch, duplicate sources, and deprecated format. macOS performs 4 checks: tap reachable, outdated packages, deprecated/disabled packages, and pinned versions. Each repository card displays a status dot (green/yellow/red), a colored left border, and a description line from the 30+ entry knowledge base (mapping URI patterns to friendly names and descriptions). Toggleable side detail panel (QSplitter-based) shows: repo name, status badge, full description, platform-specific metadata (file/suite/format on Linux), issue list with severity-colored severity cards, and action buttons (Edit/Open URI/Disable/Repair).

**Repair actions (FR-87 Phase 2):**
When a health check identifies an issue, actionable repair buttons appear in the detail panel alongside each issue card. Six repair actions are supported: disable source, enable source, remove source, remove duplicate entries, convert legacy `.list` format to modern deb822 `.sources`, and diagnose connection (inline ping/curl results). Each action presents a confirmation dialog showing the exact command before execution. Operations requiring root use `pkexec` elevation. After a repair completes, health checks automatically re-run to verify the fix. Implemented via `RepoRepairEngine` abstract base class with typed `RepoRepairAction` dispatch.

### 13. Docker

Manage Docker images, containers, and volumes. Conditional: shown only when Docker CLI is installed.

**Three tabs:**
1. **Images** — Grouped by In Use / Dangling / Other
2. **Containers** — Grouped by Running / Exited / Paused / etc., with Start/Stop actions
3. **Volumes** — Grouped by In Use / Unused

**Features:**
- Remove selected items, prune all dangling/unused
- Search filter across all tabs
- Container status filter
- Daemon status detection (shows error if Docker daemon not running)
- Lazy-loaded tabs (data fetched only when tab activated)
- Cross-platform shared implementation

### 14. GNOME Settings

Configure GNOME desktop environment settings. Conditional: shown only when `gsettings` is available (Linux/GNOME only).

**Four tabs:**
1. **Appearance** — GTK theme, icon theme, cursor theme (dropdowns populated from `/usr/share/themes/`, `/usr/share/icons/`), font rendering
2. **Window Manager** — Titlebar buttons, titlebar font, focus mode, action keys
3. **Mouse & Touchpad** — Speed sliders (debounced 200ms), acceleration, natural scrolling, tap-to-click
4. **Desktop** — Clock format, show weekday, show battery percentage, desktop icons

Changes apply immediately via `gsettings set`. Error feedback with inline messages if setting fails. Font fields use `QFontComboBox` with live preview; monospace combo filtered to fixed-pitch families.

> **macOS:** the GNOME Settings page is hidden in the sidebar and `ToolManager::checkGnomeSettings()` returns false. The macOS `GnomeSettingsTool` adapter is a hard no-op stub (`isAvailable()` returns false; setters never invoke `defaults write`) so no code path can write GNOME-mapped values into Apple preference domains, even if the sidebar guard were to regress (audit WI-29).

### 15. System Logs

Filterable, searchable table of recent system logs for quick triage. Programmatic layout (no `.ui` file).

**UI layout:**
- **Filter toolbar** — Severity dropdown (All / Error+ / Warning+ / Info+), search field, refresh button
- **Log table** — `QTableView` with columns: Timestamp, Severity, Unit/Subsystem, Message
- **Status bar** — Entry count and time range

**Platform backends** via `LogProvider` abstraction:
- **Linux:** `journalctl --output=json --no-pager --lines=500 --reverse`
- **macOS:** `log show --style ndjson --last 5m` (stream-parsed; child killed as soon as the entry cap is reached). `--info` / `--debug` are appended only when the active severity filter would surface those levels.

**Features:**
- Color-coded severity cells (red for Error+, yellow for Warning, theme-token-resolved colors)
- Text search across all columns via `QSortFilterProxyModel`
- Severity filtering re-populates from cached entries
- Manual refresh only (no auto-polling — logs are static history)
- Initial load: last 500 entries on either platform (Linux gets them via `--lines`; macOS stream-parses up to the cap within the last 5 minutes via `MacOsLogStreamParser`, which keeps memory bounded to `mMaxEntries × sizeof(LogEntry)` instead of the previous full-hour ndjson buffer)
- `LogProvider::cancel()` is safe to call from `~SystemLogsPage()` mid-fetch — disconnects the finished slot, takes ownership of the QProcess locally, and nulls the member before kill/waitForFinished, so the synchronous CrashExit signal from `kill()` cannot drive a double-delete or null deref (SSO-3363 / audit H2)

### 16. Settings

Configure Nexis application preferences.

- **Language** — 34+ languages via Crowdin translations
- **Color Scheme** — Auto / Light / Dark mode
- **Font** — Choose application font family (Inter, Ubuntu, JetBrains Mono, System Default); applied live via `@fontFamily` QSS token
- **Start Page** — Choose which page opens on launch. Persisted as a stable untranslated id (`dashboard`, `systemCleaner`, `uninstaller`, …) via `QComboBox::itemData`, so the preference survives a UI language change and resolves consistently across platforms (SSO-3388 / audit Q3). Legacy installs that stored the localized combo text are migrated to ids by `SettingManager::migrateStartPageId()`; unrecognized values fall back to `dashboard`.
- **Autostart** — Launch Nexis at login (creates `.desktop` or `.plist`)
- **Minimize to Tray** — When enabled, closing or minimizing the window hides it to the system tray instead of quitting or staying in the taskbar; clicking the tray icon restores the window (FR-52)
- **Disk Partition** — Select partition to monitor on Dashboard
- **Alert Thresholds** — CPU%, memory%, disk%, battery health% triggers for tray notifications
- **Tray Icon Style** — Choose system tray icon appearance (Color, Symbolic, Outline, Accent, System Theme); applied live via `AppManager::updateTrayIcon()`. "System Theme" uses `QIcon::fromTheme()` to load the icon from the desktop's icon theme (e.g., Papirus), falling back to the bundled color icon (FR-86)
- **Disk Analyzer** — Preferred disk usage tool (platform-specific list + custom path)
- **Disk Health Alert** — Toggle tray alerts for failing drives
- **Show Dashboard Footer** — Toggle visibility of the system summary bar and status footer on the Dashboard (default: visible; FR-75)
- **Dashboard Layout** — Reset Layout button to restore default tile arrangement (mirrors edit toolbar action)
- **Scheduled Cleaning** — Quick-setup toggle, schedule manager dialog, threshold alerts, cleaning notifications, history viewer
- **Version Display** — Current version from `APP_VERSION` compile definition
- **GitHub Profile Link** — Opens developer profile
- **Feedback Button** — Links to GitHub Issues templates

---

## Architecture Overview

Nexis follows a **three-tier architecture**:

```
┌─────────────────────────────────────────────────────────┐
│         UI Pages (15 always-visible + 3 conditional)    │
│  Always: Dashboard, HardwareInfo, StartupApps,          │
│    BootAnalysis, SystemCleaner, DiskTools, Search,      │
│    Services, Processes, Uninstaller, Resources,         │
│    Network (Usage), Helpers, SystemLogs, Settings       │
│  Conditional: AptSourceManager, Docker, GnomeSettings   │
├─────────────────────────────────────────────────────────┤
│                  Manager Layer (8)                       │
│  AppManager, InfoManager, ToolManager, SettingManager,  │
│  CleanerService, ScheduleManager, ProcessPrefsManager,  │
│  DataRefreshService                                     │
├─────────────────────────────────────────────────────────┤
│              Domain Services (9)                         │
│  StartupService, FileSearchService, HostService,        │
│  ProcessService, SystemServiceManager, DockerService,   │
│  PackageService, DuplicateFinderService, SnapshotService│
├─────────────────────────────────────────────────────────┤
│                Core Library (nexis-core)                 │
│  Info: BatteryInfo, BootAnalysisInfo, CpuInfo,          │
│   DiskHealthInfo, DiskInfo, FanInfo, GpuInfo,           │
│   MemoryInfo, NetworkInfo, PowerProfileInfo, ProcessInfo│
│   StartupInfo, SystemInfo, ThermalInfo, UpdateInfo,     │
│   PsiInfo + OomdInfoLinux (Linux only)                  │
│  Tools: AptSourceTool, DockerTool, FileSearchTool,      │
│   GnomeSettingsTool, PackageTool, RepoHealthChecker,    │
│   RepoRepairEngine, ServiceTool                         │
│  Utils: CommandUtil, FileUtil, FormatUtil               │
└─────────────────────────────────────────────────────────┘
```

**Data flows downward** (pages call managers, managers call core library). **Events flow upward** via Qt signals (core library emits updates, managers relay to pages via `SignalMapper`).

**Platform abstraction** is compile-time: shared headers define abstract base classes with pure virtual methods; platform subclasses (e.g., `CpuInfoLinux`, `CpuInfoMacOS`) implement them. Managers use `std::unique_ptr<Interface>` with `#ifdef Q_OS_MACOS` factory construction.

---

## Core Library

The `nexis-core` static library provides platform-abstracted system information and tool APIs.

### Info Providers (12 classes)

| Class | Purpose | macOS Backend | Linux Backend |
|-------|---------|---------------|---------------|
| `CpuInfo` | Core counts, per-core utilization, clock speeds | `sysctl`, Mach APIs | `/proc/stat`, `/proc/cpuinfo`, sysfs cpufreq |
| `MemoryInfo` | RAM/swap total, free, used; wired, active, inactive, compressed, available; pressure level | `sysctl`, Mach `vm_statistics64`, `kern.memorystatus_vm_pressure_level` | `/proc/meminfo` (key-value map parser), `/proc/pressure/memory` (PSI) |
| `DiskInfo` | Partitions, usage, I/O rates | `QStorageInfo`, IOKit | `QStorageInfo`, sysfs |
| `NetworkInfo` | Interfaces, RX/TX bytes | `QNetworkInterface` | sysfs `/sys/class/net/` |
| `SystemInfo` | Hostname, OS, kernel, CPU model | `sysctl` | `/etc/os-release`, `lscpu` |
| `ProcessInfo` | Process list with CPU/memory/disk I/O/network stats | `sysctl` KERN_PROC, `proc_pid_rusage()`, `nettop` | `/proc/[pid]/stat`, `/proc/[pid]/io` |
| `ThermalInfo` | Temperature sensors | SMC | `/sys/class/hwmon/` (vendor WMI surfaces resolved via `friendlyDeviceName`: ASUS, HP, Legion, IdeaPad) |
| `GpuInfo` | GPU devices, utilization | IOKit, Metal | sysfs, `nvidia-smi` |
| `BatteryInfo` | Charge, health, cycles, capacity | IOKit `IOPMPowerSource` | `/sys/class/power_supply/` |
| `DiskHealthInfo` | SMART attributes, health verdicts | `smartctl`, `diskutil` | `smartctl`, sysfs |
| `FanInfo` | Fan RPM, sensor list | SMC (`FNum`, `F{N}Ac`, fpe2) | hwmon + ThinkPad/Dell procfs + nvidia-smi fallbacks |
| `PowerProfileInfo` | CPU power profile (Performance/Balanced/Power Saver) | Stub (not supported) | `powerprofilesctl` (PPD) + sysfs governor fallback |
| (via `SystemInfo`) | Cleaner scan paths | Platform-specific paths | Platform-specific paths |

### Tool Classes (5)

| Class | Purpose | Backend |
|-------|---------|---------|
| `PackageTool` | List/remove packages | APT, DNF, Pacman, Snap (Linux); Homebrew, `.app` bundles (macOS) |
| `ServiceTool` | List/start/stop/enable services | `systemctl` (Linux); `launchctl` (macOS, partial) |
| `AptSourceTool` | Manage APT repositories | `/etc/apt/sources.list.d/` parsing |
| `GnomeSettingsTool` | Read/write GNOME settings (Linux only — macOS implementation is a hard no-op stub, see GNOME Settings section) | `gsettings` CLI |
| `DockerTool` | Manage Docker resources | `docker` CLI (shared implementation) |

### Utility Classes (3)

| Class | Purpose |
|-------|---------|
| `FileUtil` | File read/write, directory listing, file size |
| `CommandUtil` | Process execution (`QProcess`), sudo elevation, timeout handling — unified non-throwing `ExecResult` contract across `exec` / `sudoExec` / `execWithStatus` / `sudoExecWithStatus` / `execAsync` (SSO-3367) |
| `FormatUtil` | Byte formatting (KB/MB/GB/TB with binary units) |

---

## Manager Layer

Eight singleton managers mediate between UI pages and the core library (count: see the canonical table above).

| Manager | Role |
|---------|------|
| `InfoManager` | Facade over the cross-platform Info classes via `std::unique_ptr<Interface>`. Centralized refresh methods (`updateMemoryInfo()`, `updateGpuInfo()`, etc.) ensure data consistency. |
| `AppManager` | Theme/stylesheet loading, language management, system tray icon, color scheme detection. |
| `SettingManager` | `QSettings` wrapper with 30+ typed getters/setters for persistent preferences. |
| `ToolManager` | Facade over the cross-platform Tool classes via `std::unique_ptr<Interface>`. Platform-aware routing (e.g., `uninstallPackages()` calls Homebrew on macOS, APT on Debian). |
| `CleanerService` | Reusable scan/clean logic shared between the System Cleaner UI and headless scheduled cleaning. On macOS 27, classifies a removal failure as `AccessDeniedByPolicy` when TCC denies cross-team app-container access, emits `accessNeededDetected(message, deepLink)` for the Privacy & Security banner, and surfaces the count via `CleanResult::accessDeniedPaths` (SSO-3732 / FW-05). |
| `ScheduleManager` | CRUD for cleaning schedules, JSON persistence via QSettings, OS-native scheduler sync (launchd/systemd/cron). |
| `ProcessPrefsManager` | Persistent per-process state (pin flags, alert thresholds) used by `ProcessesPage`. JSON-in-QSettings, same shape as `ScheduleManager` / `CleanerExclusions`. |
| `DataRefreshService` | Centralized polling service with 5 QTimers (fast/medium/slow/process/update). Polls InfoManager once per interval, emits 16 typed data-change signals (14 cross-platform + Linux-only `psiUpdated`/`oomdUpdated`). Pages subscribe as reactive consumers. Supports pause/resume on app minimize (kiosk mode overrides pause). |

**Cross-component events** are handled by `SignalMapper`, a singleton `QObject` with 10 global signals (see `shared/nexis/signal_mapper.h`):
- `sigChangedAppTheme()` — triggers stylesheet/icon refresh across all pages
- `sigUninstallStarted()` / `sigUninstallFinished()` — progress feedback
- `sigKioskToggleRequested()` — Dashboard button requests kiosk toggle from App
- `sigKioskModeChanged(bool)` — App broadcasts kiosk state to Dashboard button and tray menu
- `sigAppVisibilityChanged(bool)` — App broadcasts visibility state for DataRefreshService pause/resume
- `sigAppFocusChanged(bool)` — App broadcasts main-window focus state so `DataRefreshService` can downshift cadence when another app has focus (FR-105)
- `sigNavigateToPage(QString)` — CommandPalette triggers page navigation
- `sigCleanableSizeChanged(quint64)` — System Cleaner broadcasts total cleanable size for cross-tile data flow
- `sigDashboardFooterChanged(bool)` — Settings page broadcasts dashboard footer visibility preference

---

## Build System

### CMake Structure

```
CMakeLists.txt          (root — project config, all targets)
shared/
  cmake/cxxbasics/      (build cache, faster linkers, defaults)
  nexis-core/           (shared core library sources)
  nexis/                (shared GUI sources)
  translations/         (34 .ts files)
macos/
  nexis-core/           (macOS core implementations)
  nexis/                (macOS GUI implementations)
linux/
  nexis-core/           (Linux core implementations)
  nexis/                (Linux GUI implementations)
tests/
  CMakeLists.txt        (test target configuration with add_nexis_test() macro)
  utils/                (FormatUtil, FileUtil, CommandUtil tests)
  core/                 (DiskHealth, Memory, CPU, GPU, AptSource, Fan, Thermal, Battery, Disk, HostService tests)
  managers/             (ScheduleManager tests)
  theme/                (theme token validation tests)
  fixtures/             (sample system output files for fixture-based testing)
  screenshots/          (FR-41 screenshot regression tests; mask + per-channel fuzz comparator, NEX-3382)
  reference_screenshots/  (per-platform reference PNGs: macos/{dark,light}/ committed; linux baselines not yet committed — NEX-3382 removed the empty placeholders so the gap is explicit)
```

### Build Targets

**`nexis-core`** — Static library
- Sources: `shared/nexis-core/**/*.cpp` + `{platform}/nexis-core/**/*.cpp`
- Dependencies: Qt6::Core, Qt6::Network, IOKit + CoreFoundation (macOS only)

**`nexis-gui`** — Static library (all GUI code except main.cpp)
- Sources: `shared/nexis/**/*.cpp` + `{platform}/nexis/**/*.cpp` + `static.qrc`
- Dependencies: `nexis-core`, Qt6::Core, Gui, Widgets, Charts, Svg, Concurrent
- Shared by both the `nexis` executable and the screenshot test

**`nexis`** — GUI executable
- Sources: `main.cpp` + translations + icon
- Dependencies: `nexis-gui` (inherits all Qt and core dependencies)
- macOS: `.app` bundle with icon, installs to `/Applications`. Signed with Developer ID and notarized by Apple (no Gatekeeper warnings)
- Linux: Binary to `/usr/bin`, `.desktop` file, hicolor icons (16x16 through 256x256)
- Linux PPA: `ppa:s4solutionsllc/nexis` — Ubuntu 22.04+ (Jammy, Noble, Plucky), amd64 and arm64, automatic updates via apt
- macOS Homebrew: `brew tap s4solutionsllc/nexis && brew install --cask nexis` — Apple Silicon, auto-updated on new releases

**Linux distribution channels:** `.deb` (PPA + GitHub releases), AppImage, AUR. The Flatpak (Flathub) channel was retired in 2026-06; the sandbox model is incompatible with Nexis's system-maintenance feature set (privileged host operations like `systemctl`, `pkexec`, `smartctl`, `fstrim` are unavailable or no-ops inside bubblewrap). The product feature that detects and cleans unused Flatpak runtimes installed on the host is unaffected.

**Test executables** — see the canonical table at the top of this doc for counts
- Unit test executables registered via the `add_nexis_test()` CMake macro (QTEST_MAIN requires one main per executable); plus one screenshot regression test linked against `nexis-gui`
- Static parser pattern: parsing logic extracted into public static methods on shared base classes, tested with fixture data files in `tests/fixtures/`. macOS live-tool output (`nettop` CSV, `diskutil` plist, `sysctl kern.boottime`) is covered by fixtures under `tests/fixtures/macos/` and exercised via the FR-127 compile-source-into-test pattern (WI-33); the parsers are exposed as pure static methods so the tests run on any host. Package-manager uninstall command construction (apt/dnf/yum/pacman/snap/brew argv + macOS osascript shell escaping) is exercised via the same seam pattern as `TestableRepairEngine`.
- Screenshot test: captures 12 pages × 2 themes, masks declared dynamic-data regions (charts, tables, dashboard tiles, live system summary), then per-channel-fuzz compares the remaining chrome against reference PNGs under a tight 1% default unmasked-diff threshold; missing page class or reference PNG `QFAIL`s loudly, missing platform/theme baseline directory `QSKIP`s with regeneration instructions (NEX-3381: non-blocking in CI on Linux x64 and macOS, skipped on ARM64 Linux due to xvfb hang; NEX-3382 hardened the comparator and added seven self-test slots that validate the mask + fuzz contract against synthetic images on every run)
- Dependencies: `nexis-core`/`nexis-gui`, Qt6::Test
- Gated behind `BUILD_TESTING` option (default ON)
- Run via: `ctest --test-dir build --output-on-failure`

### Qt 6 Modules Used

| Module | Purpose |
|--------|---------|
| Core | Event loop, QObject, containers, file I/O, QSettings, QProcess |
| Gui | Base primitives, QPalette, QIcon |
| Widgets | All UI components (QMainWindow, QWidget, QTreeWidget, etc.) |
| Charts | QChart, QLineSeries for Resources page time-series |
| Svg | QSvgRenderer for SVG icon rendering |
| Concurrent | `QtConcurrent::run` for async operations |
| Network | `QNetworkInterface` for network info |
| Test | Qt Test framework for unit tests (CTest integration) |
| LinguistTools | `qt_add_translation` (lrelease) for i18n |

### Build Commands

```bash
# Clean rebuild (macOS, from project root)
rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) && \
  cmake --build build -j$(sysctl -n hw.ncpu)

# Incremental rebuild
cmake --build build -j$(sysctl -n hw.ncpu)
```

### Release Build Hardening

Release / RelWithDebInfo / MinSizeRel builds enable an explicit hardening baseline so the AppImage, raw tarball, and macOS `.app` get the same protections distros layer on top of the `.deb`:

- `-fstack-protector-strong`, `-D_FORTIFY_SOURCE=2`, `CMAKE_POSITION_INDEPENDENT_CODE=ON` (both platforms)
- Linux link: `-Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -pie` (full RELRO, BIND_NOW, NX stack, PIE)
- macOS link: `-Wl,-bind_at_load` (immediate symbol binding)
- Distro overrides: gated behind `-DNEXIS_ENABLE_HARDENING=OFF` for packagers who want to drive flags exclusively from their toolchain.

---

## Theme System

Nexis uses a **QSS token replacement system** with three themes.

### Theme Structure

```
shared/nexis/static/themes/
  default/              (dark theme)
    style/style.qss     (main stylesheet with @token placeholders)
    style/values.ini    (color/spacing token definitions)
    img/                (theme-specific images)
  light/
    style/values.ini    (light mode color overrides)
  nexis/
    style/values.ini    (Nexis branded theme)
  common/
    img/                (shared images: checkboxes, icons, etc.)
```

### Token System

`values.ini` defines tokens:
```ini
color01=#1A1C22
accentColor=#FF6B1A
dp8=8px
borderColor=#5e5c64
cpuColor=#FF6B1A
memoryColor=#3B82F6
diskColor=#10B981
networkColor=#8B5CF6
gpuColor=#F59E0B
tempColor=#EF4444
```

`style.qss` references them:
```css
QDialog { background-color: @color01; }
QPushButton { background-color: @accentColor; border-radius: @dp8; }
```

At runtime, `AppManager::updateStylesheet()` reads the `.ini` file, replaces all `@token` occurrences in the QSS, and applies the result via `qApp->setStyleSheet()`. User-configurable tokens like `@fontFamily` are handled separately — they are stored in `SettingManager` (not `values.ini`) and replaced after the theme token loop.

### Color Scheme

- **Auto** — Follows system preference via `QStyleHints::colorSchemeChanged` (Qt 6.5+)
- **Light** — Loads `light/values.ini` overrides. The refined light theme uses a warm cream base (`#F5F0EB`) for reduced eye strain.
- **Dark** — Loads `default/values.ini`. The refined dark theme uses a deep charcoal base (`#1A1C22`) with a warm orange accent (`#FF6B1A`).

**Per-metric color tokens** (`@cpuColor`, `@memoryColor`, `@diskColor`, `@networkColor`, `@gpuColor`, `@tempColor`, `@fanColor`) are defined in `values.ini` and used by `MetricTile` sparklines and the Resources charts, giving each metric a consistent named color across all pages and themes.

**Extended tokens** added for full theme coverage (24 additional tokens per theme):
- **Network:** `@networkDownloadColor` — download (RX) sparkline/bar color; `@networkUploadColor` — upload (TX) sparkline/label color
- **Overlay/Shadow:** `@overlayBackground`, `@overlayText`, `@shadowColor` — kiosk overlay and drop shadow colors (8-digit hex `#AARRGGBB` for alpha support)
- **Chart Series Palette:** `@chartSeries01` through `@chartSeries20` — 20 colors for `HistoryChart` data lines, with light-theme variants optimized for white backgrounds

### Live Theme Refresh

All color-bearing widgets implement a `refreshThemeColors()` method connected to `SignalMapper::sigChangedAppTheme`. Constructors accept token name strings (e.g., `"@cpuColor"`) instead of resolved `QColor` values. On theme switch, each widget re-resolves its tokens from `AppManager::getStyleValues()` and updates all visual properties (series pens, area fills, chart backgrounds, inline stylesheets, progress bar chunks, shadow effects). This pattern ensures zero hardcoded colors in C++ code — every color comes from `values.ini`.

### DPI Scaling

QSS tokens include `@dpN` values (e.g., `@dp8`, `@dp12`) that are computed at stylesheet load time based on `devicePixelRatio()`. The `Dpi::scale()` utility class handles pixel scaling in C++ code. This solved HiDPI/4K display issues without requiring a QML migration.

### Accessibility (Keyboard Focus)

Interactive controls accept keyboard focus and render a visible focus ring under both themes (SSO-3502). The ring is token-driven via `@focusRingColor` (resolved per theme in `values.ini`) and applied through `:focus` selectors in `style.qss` covering `QPushButton`, `QToolButton`, `QCheckBox`, `QRadioButton`, `QSlider`, `QComboBox`, `QLineEdit`, `QPlainTextEdit`, `QSpinBox`, `QDoubleSpinBox`, `QTreeView/Widget`, `QTableView/Widget`, and `QListView/Widget`. The `#sidebar QPushButton:focus` selector reuses the existing 3px left-edge stripe so sidebar nav items show focus without layout shift.

The pre-SSO-3502 code used `setFocusPolicy(Qt::NoFocus)` on most interactive widgets, which broke keyboard, screen-reader, and switch-control navigation. After the SSO-3502 sweep, the only `Qt::NoFocus` call sites that remain in shared/macos `.cpp` code are deliberate: read-only data displays (`NoSelection + NoEditTriggers`) and the command palette result list (whose focus is forwarded from the search box via an event filter). New interactive controls **must not** add `setFocusPolicy(Qt::NoFocus)` — see `CONTRIBUTING.md` §2 for the rule and `tests/theme/test_focus_visible.cpp` for the style-test guard. Follow-up: SSO-3502 removed the `.cpp` declarations only; `.ui` files still contain a number of `<enum>Qt::NoFocus</enum>` entries that should be swept in a follow-up issue.

---

## Configuration and Settings

### Storage Backend

`SettingManager` wraps Qt's `QSettings`, written explicitly as an INI file at `QStandardPaths::AppConfigLocation/settings.ini` (see `shared/nexis/Managers/setting_manager.cpp`). With `applicationName == "nexis"` (set in `main.cpp`) and no organization name, this resolves to:
- **Linux:** `~/.config/nexis/settings.ini` (or `$XDG_CONFIG_HOME/nexis/settings.ini` if set)
- **macOS:** `~/Library/Application Support/nexis/settings.ini`

Forcing INI format on both platforms keeps the on-disk layout identical for both inspection and the headless `--clean` / `--check-threshold` codepaths. There is **no** `com.nexis.plist` and **no** `nexis.conf`; older docs that named those paths were stale.

### Settings Keys (30+)

**Appearance:** ThemeName, Language, ColorScheme, AppFont
**Behavior:** StartPage, KioskMode, DashboardLayout (JSON), AppQuitDialogDontAsk/Choice
**Thresholds:** CPUAlertPercent, MemoryAlertPercent, DiskAlertPercent, BatteryAlertPercent
**Tools:** DiskAnalyzerTool, DiskAnalyzerCustomPath, DiskName, TempSensorId, GpuDeviceId, FanSensorId
**Cleaning:** Schedules (JSON), CleaningNotificationsEnabled, ThresholdAlertEnabled, ThresholdGB
**Health:** BatteryAlertLastHealth, BatteryAlertSnoozedUntil, DiskHealthAlertEnabled

### Scheduled Cleaning Persistence

Cleaning schedules are stored as JSON in QSettings. Each schedule includes: ID, name, enabled flag, frequency (Daily/EveryNDays/Weekly/Monthly), timing, selected categories, minimum file age, and execution history.

OS-native scheduling:
- **macOS:** Generates `.plist` files in `~/Library/LaunchAgents/com.nexis.clean.<id>.plist`
- **Linux:** Creates systemd timer units or cron entries (auto-detected). Both pin `QT_QPA_PLATFORM=offscreen` on the generated lines (cron via inline `KEY=VALUE` prefix; systemd via `Environment=`), and `main()` also forces `QT_QPA_PLATFORM=offscreen` when it sees `--clean`/`--check-threshold` and the user has not pinned a platform — without this, cron and boot-catch-up runs aborted during `QApplication` construction because no display is available, silently skipping the clean (audit H6 / SSO-3368).

---

## Translation System

34 languages supported via Qt Linguist (`.ts` → `.qm`):

Arabic, Afrikaans, Catalan, Chinese (Simplified/Traditional), Czech, Danish, Dutch, English, Finnish, French, Galician, German, Greek, Hebrew, Hindi, Italian, Japanese, Kannada, Korean, Malayalam, Norwegian, Occitan, Polish, Portuguese, Romanian, Russian, Serbian, Spanish, Swedish, Turkish, Ukrainian, Vietnamese

**Build:** `qt_add_translation()` compiles the committed `.ts` files into binary `.qm` files via `lrelease` (source-string extraction with `lupdate` is handled separately by the `lupdate.yml` workflow). The `.qm` files are installed to `share/nexis/translations/` so they ship in the AppImage and `.deb` packages.
**Runtime:** `AppManager` loads the selected language via `QTranslator::load()` once at startup, searching the FHS install path (`../share/nexis/translations`) first, then beside the binary for dev builds. Changing the language in Settings requires a restart — Nexis prompts the user and offers to relaunch (there is no live `retranslateUi()`).
**Management:** Crowdin integration with automated PR workflows for community translations.

---

## Application Lifecycle

### Startup Sequence (`main.cpp`)

1. **Pre-QApplication headless gate (SSO-3368):** Scan `argv` for `--clean` / `--check-threshold`. If headless and `QT_QPA_PLATFORM` is unset, `qputenv("QT_QPA_PLATFORM", "offscreen")`. Done before `QApplication` so cron/timer runs do not abort on missing QPA platforms.
2. Create `QApplication`
3. Install custom message handler (logs to `~/.config/nexis/nexis.log`)
4. **Headless mode check:** `--clean <id>` or `--check-threshold` → run `CleanerService`, exit
5. **Single-instance enforcement:** `QLockFile` at `/tmp/nexis.lock` — shows warning dialog if already running
6. macOS icon theme setup: add Homebrew icon paths to `QIcon::themeSearchPaths()`
7. Set fallback icon theme to "Adwaita"
8. Load Ubuntu font from embedded resources
9. Show theme-aware splash screen matching Color Scheme setting (unless `--nosplash`)
10. Create `App` main window
11. Show window (unless `--hide`)
12. Enter Qt event loop: `app.exec()`

### Main Window Initialization (`App`)

1. Create `SlidingStackedWidget` (animated page container)
2. Instantiate every always-visible page (and the three conditional pages when their tools are detected)
3. Conditionally show/hide pages based on platform and tool availability:
   - APT Source Manager: only if `ToolManager::checkSourceRepository()` returns true
   - Docker: only if `ToolManager::checkDocker()` returns true
   - GNOME Settings: only if `ToolManager::checkGnomeSettings()` returns true
4. Add pages to stacked widget
5. Connect sidebar buttons to `pageClick()` slot
6. Create system tray icon with context menu (mirrors sidebar navigation + kiosk mode toggle)
7. Connect `SignalMapper::sigKioskToggleRequested` to `App::toggleKioskMode` (Dashboard button → App)
8. Restore kiosk mode state if previously saved

### Navigation

The sidebar is **collapsible**, organized into three labelled groups — **MONITOR**, **MANAGE**, and **SYSTEM** — matching the logical grouping of the pages. Each group has a **clickable section header** with a chevron indicator that toggles the group's visibility. Collapsed groups hide their child buttons with a smooth 200ms height animation; the collapsed/expanded state is persisted per-section across sessions as JSON in QSettings. Navigating to a page in a collapsed group (via tray menu, command palette, or `sigNavigateToPage`) auto-expands that group. The full sidebar can also collapse to a 64 px icon-rail showing only page icons plus section indicator dots; the collapse and expand transitions use a smooth width animation. The sidebar can be toggled with the **Ctrl+B** keyboard shortcut or the collapse button at the top of the panel. The sidebar header displays a **gradient logo** (full wordmark when expanded, lettermark when collapsed) above a **separator line**, with a **version label** below. Active page badges use a cleaner dot indicator in collapsed mode and are hidden when their section is collapsed. The Homebrew/APT button displays an **updates badge** showing the pending update count when the sidebar is expanded, or a colored dot (using `@updatesColor`) when collapsed.

A **Command Palette** (activated with **Ctrl+K**) provides a fuzzy-search popup for navigating directly to any page and executing common actions (e.g., "run clean", "toggle kiosk") without touching the sidebar.

Sidebar buttons trigger `SlidingStackedWidget::slideInIndex()` with horizontal slide animation. The tray icon context menu provides the same page navigation plus a checkable kiosk mode toggle. F11, the tray action, and the Dashboard button all toggle kiosk mode through synchronized signals.

---

## Data Flow

### Example: CPU usage on Dashboard

```
[OS kernel data]
    ↓
CpuInfo::getCpuPercents()           ← Core library reads /proc/stat or Mach APIs
    ↓
InfoManager::ins()->getCpuPercents() ← Manager facade delegates to CpuInfo instance
    ↓
DataRefreshService::onFastTick()     ← Centralized service polls every 1s
    ↓
emit cpuUpdated(percents, clock, loadAvgs) ← Typed signal with data payload
    ↓
DashboardPage::onCpuUpdated(...)     ← Page receives data as reactive subscriber
    ↓
mCpuTile->setValue(average)          ← MetricTile widget updates sparkline and label
```

### Example: Uninstalling a Homebrew package

```
[User clicks "Uninstall" in UninstallerPage]
    ↓
UninstallerPage::on_btnUninstall_clicked()
    ↓
ToolManager::ins()->uninstallPackages(packages)  ← Manager routes to correct backend
    ↓
PackageTool::homebrewRemovePackages(packages)    ← Tool class on macOS
    ↓
CommandUtil::exec("brew", ["uninstall", ...])    ← Spawns QProcess with timeout
    ↓
[brew CLI executes uninstall]
```

### Example: Theme change

```
[User selects new theme in Settings]
    ↓
SettingsPage::on_cmbTheme_changed()
    ↓
AppManager::ins()->updateStylesheet()  ← Reads values.ini, replaces @tokens in QSS
    ↓
qApp->setStyleSheet(processedQSS)     ← All widgets update immediately
    ↓
emit SignalMapper::ins()->sigChangedAppTheme()  ← Global event
    ↓
[Every page reload theme-dependent assets (icons, GIF loaders, etc.)]
```

### Refresh Timing

All periodic polling is centralized in `DataRefreshService`, which owns 5 QTimers and emits 16 typed data signals (14 cross-platform + Linux-only `psiUpdated`/`oomdUpdated`). Pages subscribe as reactive consumers — no page owns a QTimer.

| Data | Refresh Rate | Timer Tier | Signal |
|------|-------------|------------|--------|
| CPU, Memory, Network (default + per-iface), Disk I/O, GPU, Temp, Fan, Battery, PSI (Linux) | 1 second (Normal mode) | Fast (`mFastTimer`) | `cpuUpdated`, `memoryUpdated`, `networkUpdated`, `networkPerInterfaceUpdated`, `diskIOUpdated`, `gpuUpdated`, `tempUpdated`, `fanUpdated`, `batteryUpdated`, `psiUpdated` (Linux only) |
| Disk usage | 5 seconds | Medium (`mMediumTimer`) | `diskUsageUpdated` |
| Disk health (SMART) | 30 seconds | Slow (`mSlowTimer`) | `diskHealthUpdated` |
| Processes | 1–10 seconds | Configurable (`mProcessTimer`) | `processesUpdated` |
| System updates + repo health (chained) | 1 hour | Update (`mUpdateTimer`) | `systemUpdatesChecked`, `repoHealthChecked` |
| Services, packages | On demand | — | Manual refresh / page navigation |
| Hardware info | Once | — | Page construction |

FR-105 PowerMode tiers downshift the Fast timer cadence based on focus + battery state: Normal 1/5/30 s, Battery 2/10/60 s, Unfocused 5/30/60 s.

**Battery optimization:** When the app is minimized to tray, `DataRefreshService::pause()` stops all timers (unless kiosk mode is active). On restore, `resume()` fires immediate ticks then restarts timers.

**Page-aware polling (BUG-72):** Three data-heavy pages (`DashboardPage`, `ResourcesPage`, `ProcessesPage`) inherit from `NexisPage` and use `onPageActivated()`/`onPageDeactivated()` lifecycle hooks to reduce CPU usage when hidden:
- **ProcessesPage:** Calls `DataRefreshService::pauseProcessTimer()` / `resumeProcessTimer()`. The process timer (which spawns `ps ax`, `nettop`, and per-PID `proc_pid_rusage()`) only runs when the Processes page is active. It starts paused by default. The per-tick collection itself runs on a `QtConcurrent` worker (SSO-3383 / audit M2) — `ProcessInfo::collectProcesses()` builds a fresh `QList<Process>` into a local and the UI thread publishes via `setProcessList()` from a `QMetaObject::invokeMethod` hop, mirroring `onSlowTick()` for disk-health.
- **ResourcesPage:** Gates all HistoryChart update slots behind an `mActive` flag. QSplineSeries manipulation and chart repaints are skipped when the page is hidden. Delta-tracking statics for network/disk I/O are maintained to prevent spikes on re-activation.
- **DashboardPage:** Gates tile update slots (`setValue`, `addDataPoint`, `paintEvent` triggers) behind an `mActive` flag. Tray alert logic (CPU, memory, disk usage thresholds) continues to fire regardless of visibility.

---

## Key Directories

```
/
├── CMakeLists.txt              Root build configuration
├── BUGS.md                     Bug backlog (BUG-XX IDs)
├── FEATURE_REQUESTS.md         Feature backlog (FR-XX IDs)
├── CHANGELOG.md                Version history
├── docs/                       Project documentation
├── backlog/                    Active research/plan files
│   └── Archive/                Completed research/plans
├── shared/
│   ├── nexis-core/             Core library (shared)
│   │   ├── Info/               System info providers (see canonical table)
│   │   ├── Tools/              Tool classes (see canonical table)
│   │   └── Utils/              Utility classes (CommandUtil, FileUtil, FormatUtil)
│   ├── nexis/                  GUI application (shared)
│   │   ├── Managers/           8 manager singletons
│   │   ├── Services/           Domain services (see canonical table)
│   │   ├── Pages/              Page implementations (see canonical table)
│   │   │   ├── Dashboard/
│   │   │   ├── HardwareInfo/
│   │   │   ├── StartupApps/
│   │   │   ├── BootAnalysis/
│   │   │   ├── SystemCleaner/
│   │   │   ├── DiskTools/
│   │   │   ├── Search/
│   │   │   ├── Services/
│   │   │   ├── Processes/
│   │   │   ├── Uninstaller/
│   │   │   ├── Resources/
│   │   │   ├── Network/             (Network Usage page)
│   │   │   ├── Helpers/
│   │   │   ├── SystemLogs/
│   │   │   ├── AptSourceManager/    (conditional — APT/Homebrew detected)
│   │   │   ├── GnomeSettings/       (conditional — gsettings detected)
│   │   │   ├── Docker/              (conditional — docker CLI detected)
│   │   │   └── Settings/
│   │   └── static/             Themes, icons, fonts, translations
│   ├── translations/           34 language .ts files
│   └── cmake/                  CMake modules
├── macos/
│   ├── nexis-core/             macOS core implementations
│   └── nexis/                  macOS GUI implementations
├── linux/
│   ├── nexis-core/             Linux core implementations
│   └── nexis/                  Linux GUI implementations
└── icons/                      App icons (hicolor sizes)
```
