# Nexis — Application Overview

> A comprehensive reference for what Nexis does and how it is built.
> Last updated: 2026-04-21 (Bundle D) | Version 2.2.17 (unreleased changes pending)

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
- ~36,000 lines of C++ code across 281 source files
- 16 application pages
- 15 system info providers (BatteryInfo, CpuInfo, DiskHealthInfo, DiskInfo, FanInfo, GpuInfo, MemoryInfo, NetworkInfo, PowerProfileInfo, ProcessInfo, StartupInfo, SystemInfo, ThermalInfo, UpdateInfo) — 13 wired through InfoManager, StartupInfo standalone, PowerProfileInfo added in v2.1.16
- 6 tool classes (package management, services, Docker, APT sources, GNOME settings, file search)
- 8 domain services (StartupService, FileSearchService, HostService, ProcessService, SystemServiceManager, DockerService, PackageService, DuplicateFinderService)
- 3 utility classes
- 7 manager singletons
- 3 themes (Dark, Light, Auto)
- 34 languages
- 17 test suites with ~242 test methods (Qt Test + CTest)
- 88 features implemented, 102 bugs fixed since fork

---

## Platform Support

Nexis runs natively on **Linux** and **macOS** (Intel + Apple Silicon). The codebase uses compile-time platform selection: shared code in `shared/`, with platform-specific implementations in `linux/` and `macos/`.

### Always-visible pages (both platforms)
Dashboard, Hardware Info, Startup Apps, System Cleaner, Disk Tools, Search, Services, Processes, Uninstaller, Resources, Helpers, System Logs, Settings

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
| Thermal sensors | `/sys/class/hwmon/` | SMC (System Management Controller) |
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
- Kiosk mode — fullscreen dashboard-only view (hides sidebar + title bar), state persisted across sessions. Three entry/exit methods:
  - **Keyboard:** F11 to toggle, ESC to exit
  - **System tray:** Checkable "Kiosk Mode (F11)" action in tray context menu
  - **Dashboard button:** Floating fullscreen/collapse icon at top-right corner, swaps between enter/exit icons
  - On activation, a transient "Press ESC to exit kiosk mode" overlay fades in then out (~3.5s)

### 2. Hardware Info

Comprehensive static hardware inventory displayed in tabular sections. Includes an **Export System Report** button that generates a plain text file summarizing all visible hardware tables, with aligned columns and section headers. Default filename: `nexis-report-YYYY-MM-DD.txt`.

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

### 4. System Cleaner

Scan and remove system junk files across 9 categories.

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

**Exclusion rules:**
- Manage exclusion rules via gear button on the categories page → opens ExclusionManagerDialog
- Add file or folder exclusions via native file chooser; remove via dialog
- Right-click any scan result item → "Always exclude this" to instantly persist an exclusion rule and remove the item from the tree
- Excluded paths are skipped during scanning (file exact match, folder prefix match, symlink-aware)
- Exclusions persist across app restarts via JSON in QSettings

**UI features:**
- Hierarchical tree view with checkboxes (category > individual files)
- Sort by name (A-Z, Z-A) or size (small-large, large-small)
- Total size display of selected items
- Schedule indicator showing next/last automated clean time

**Scheduled cleaning integration:**
- Automated background scans/cleans via OS-native scheduler (launchd, systemd, cron)
- Headless CLI mode: `./nexis --clean <schedule-id>`, `./nexis --check-threshold`
- Frequencies: Daily, Every N Days, Weekly, Monthly
- Category selection, minimum file age filter, threshold alerts

### 5. Disk Tools

Two-mode page for finding space-wasting files, accessible via the MANAGE sidebar section.

**Mode 1 — Large & Old Files (FR-62):**
- Directory picker with smart defaults (Home, Downloads, Documents)
- Configurable filters: size threshold (MB/GB), age threshold (days/months/years)
- Three match modes: Either (large OR old), Large only, Old only
- Recursive QDirIterator scan in background thread with cancellation support
- Results in sortable QTreeWidget with columns: Name, Path, Size, Last Accessed, Last Modified
- Checkboxes for selective deletion via QFile::moveToTrash()

**Mode 2 — Duplicate Finder (FR-63):**
- Shared directory picker (synced with Large & Old mode)
- Configurable minimum file size (KB/MB/GB) and optional glob pattern filter
- DuplicateFinderService: 3-stage pipeline (size grouping → partial 4KB SHA-256 → full SHA-256)
- Progress bar with stage-by-stage status messages
- Results in grouped QTreeWidget: parent = duplicate group (wasted space), children = files
- First file in each group unchecked (kept), remaining pre-checked for deletion
- Cancellable scan with Cancel button

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
- Right-click context menu: open, open location, copy path

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

Uninstall applications and packages. Labeled "Applications" on macOS. Three-tab layout: System Packages, Snap Packages, Orphan Packages.

- Package tree view grouped by type (Formula/Cask on macOS; installed/universe on Linux)
- Search filter with auto-expand matching sections
- Multi-select checkboxes for batch uninstall
- Purge option (Linux only) — removes config files in addition to package
- Dry-run confirmation showing dependencies that would also be removed (APT)
- Async background loading with progress indicator
- **Orphan Packages tab** — Lists packages no longer required by any installed package. Removal via platform `autoremove` command (all-or-nothing). Supported on APT, DNF, Pacman, and Homebrew.

**Platform backends:**
- Linux: `apt-get remove/purge`, `dnf remove`, `pacman -R`, `snap remove`, `apt-get autoremove` / `dnf autoremove` / `pacman -Rns`
- macOS: `brew uninstall` for Homebrew packages; Finder AppleScript Trash for `.app` bundles; `brew autoremove` for orphans

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

**Disk Usage Launcher:**
- Quick-launch card for platform-appropriate disk analyzer tools
- Configurable preference in Settings (Linux: Baobab, Filelight, QDirStat, ncdu; macOS: GrandPerspective, DaisyDisk, OmniDiskSweeper; or custom path)

### 11. Helpers

Miscellaneous utility tools.

**Flush DNS Cache** — One-click button to clear the local DNS cache. macOS: `dscacheutil -flushcache` + `killall -HUP mDNSResponder` (with admin elevation). Linux: tries `resolvectl`, `systemd-resolve`, or `nscd` in order of availability. Confirmation dialog before action, success/failure feedback.

**Power Profile Switcher (Linux only)** — Segmented control with three buttons (Power Saver / Balanced / Performance) for switching CPU power profiles. Uses `power-profiles-daemon` (`powerprofilesctl`) as the primary backend (no root needed). Falls back to raw sysfs governor writes via `pkexec` on systems without PPD. Automatically detects available profiles, hides Balanced if the driver only supports two governors (e.g., `intel_pstate`). Warns if TLP or auto-cpufreq is active. Hidden on macOS and systems without cpufreq support.

**macOS Maintenance Actions** — Three macOS-only one-click buttons added programmatically to the nav bar (`#ifdef Q_OS_MACOS`):
- **Rebuild Spotlight** — Deletes and rebuilds the Spotlight search index (`sudo mdutil -E /`). Warns that search will be temporarily unavailable during reindexing.
- **Verify Disk** — Runs `diskutil verifyVolume /` with a 5-minute timeout. Displays full diagnostic output in a scrollable dialog with pass/fail status indicator.
- **Rebuild Launch Services** — Rescans the Launch Services database using the safe `-r` flag (no `-kill`, which is dangerous on macOS 14+/15+) and restarts Finder. Fixes incorrect default apps and missing "Open With" entries.

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

### 12. APT Repository Manager / Homebrew

Manage package repositories and sources. Conditional: shown only when the relevant package manager is detected.

**Linux (APT):**
- Repository list from `/etc/apt/sources.list.d/`
- Dual-format support: legacy one-line `.list` files and modern deb822 `.sources` stanzas
- APT-RPM support for ALT Linux, PCLinuxOS, Vine Linux
- Add, edit, delete repositories (requires sudo)
- Enable/disable without deleting
- Structured editor: type (deb/deb-src), URIs, suites, components, Signed-By
- Search filter

**macOS (Homebrew):**
- Package tree view grouped by Formula/Cask
- Install new packages by name
- Multi-select batch uninstall with checkboxes
- Search with auto-expand
- Async background loading via `brew info --json=v2`

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

### 15. System Logs

Filterable, searchable table of recent system logs for quick triage. Programmatic layout (no `.ui` file).

**UI layout:**
- **Filter toolbar** — Severity dropdown (All / Error+ / Warning+ / Info+), search field, refresh button
- **Log table** — `QTableView` with columns: Timestamp, Severity, Unit/Subsystem, Message
- **Status bar** — Entry count and time range

**Platform backends** via `LogProvider` abstraction:
- **Linux:** `journalctl --output=json --no-pager --lines=500 --reverse`
- **macOS:** `log show --style ndjson --last 1h`

**Features:**
- Color-coded severity cells (red for Error+, yellow for Warning, theme-token-resolved colors)
- Text search across all columns via `QSortFilterProxyModel`
- Severity filtering re-populates from cached entries
- Manual refresh only (no auto-polling — logs are static history)
- Initial load: last 500 entries (Linux) or last 1 hour (macOS)

### 16. Settings

Configure Nexis application preferences.

- **Language** — 34+ languages via Crowdin translations
- **Color Scheme** — Auto / Light / Dark mode
- **Font** — Choose application font family (Inter, Ubuntu, JetBrains Mono, System Default); applied live via `@fontFamily` QSS token
- **Start Page** — Choose which page opens on launch
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
│                    UI Pages (16)                        │
│  Dashboard, HardwareInfo, StartupApps, SystemCleaner,   │
│  DiskTools, Search, Services, Processes, Uninstaller,   │
│  Resources, Helpers, AptSourceManager, GnomeSettings,   │
│  Docker, SystemLogs, Settings                           │
├─────────────────────────────────────────────────────────┤
│                  Manager Layer (8)                       │
│  InfoManager, AppManager, SettingManager, ToolManager,  │
│  CleanerService, DuplicateFinderService,                │
│  ScheduleManager, DataRefreshService                    │
├─────────────────────────────────────────────────────────┤
│                Core Library (nexis-core)                 │
│  Info: CpuInfo, MemoryInfo, DiskInfo, NetworkInfo,      │
│        SystemInfo, ProcessInfo, ThermalInfo, GpuInfo,   │
│        BatteryInfo, DiskHealthInfo, FanInfo              │
│  Tools: ServiceTool, PackageTool, AptSourceTool,        │
│         GnomeSettingsTool, DockerTool                    │
│  Utils: FileUtil, CommandUtil, FormatUtil                │
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
| `ThermalInfo` | Temperature sensors | SMC | `/sys/class/hwmon/` |
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
| `GnomeSettingsTool` | Read/write GNOME settings | `gsettings` CLI |
| `DockerTool` | Manage Docker resources | `docker` CLI (shared implementation) |

### Utility Classes (3)

| Class | Purpose |
|-------|---------|
| `FileUtil` | File read/write, directory listing, file size |
| `CommandUtil` | Process execution (`QProcess`), sudo elevation, timeout handling |
| `FormatUtil` | Byte formatting (KB/MB/GB/TB with binary units) |

---

## Manager Layer

Seven singleton managers mediate between UI pages and the core library.

| Manager | Role |
|---------|------|
| `InfoManager` | Facade over all 15 Info classes via `std::unique_ptr<Interface>`. Centralized refresh methods (`updateMemoryInfo()`, `updateGpuInfo()`, etc.) ensure data consistency. |
| `AppManager` | Theme/stylesheet loading, language management, system tray icon, color scheme detection. |
| `SettingManager` | `QSettings` wrapper with 30+ typed getters/setters for persistent preferences. |
| `ToolManager` | Facade over all 5 Tool classes via `std::unique_ptr<Interface>`. Platform-aware routing (e.g., `uninstallPackages()` calls Homebrew on macOS, APT on Debian). |
| `CleanerService` | Reusable scan/clean logic shared between the System Cleaner UI and headless scheduled cleaning. |
| `DuplicateFinderService` | 3-stage duplicate detection pipeline (size grouping → partial 4KB SHA-256 hash → full SHA-256 hash). Background thread via QtConcurrent with QAtomicInt cancellation. |
| `ScheduleManager` | CRUD for cleaning schedules, JSON persistence via QSettings, OS-native scheduler sync (launchd/systemd/cron). |
| `DataRefreshService` | Centralized polling service with 4 QTimers (1s/5s/30s/configurable). Polls InfoManager once per interval, emits 10 typed data-change signals. Pages subscribe as reactive consumers. Supports pause/resume on app minimize (kiosk mode overrides pause). |

**Cross-component events** are handled by `SignalMapper`, a singleton `QObject` with 9 global signals:
- `sigChangedAppTheme()` — triggers stylesheet/icon refresh across all pages
- `sigUninstallStarted()` / `sigUninstallFinished()` — progress feedback
- `sigKioskToggleRequested()` — Dashboard button requests kiosk toggle from App
- `sigKioskModeChanged(bool)` — App broadcasts kiosk state to Dashboard button and tray menu
- `sigAppVisibilityChanged(bool)` — App broadcasts visibility state for DataRefreshService pause/resume
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
  screenshots/          (FR-41 screenshot regression tests)
  reference_screenshots/  (per-platform reference PNGs: {linux,macos}/{dark,light}/)
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

**Test executables** — 16 CTest-registered executables (Qt Test + CTest)
- 15 unit test executables via `add_nexis_test()` CMake macro (QTEST_MAIN requires one main per executable)
- 1 screenshot regression test executable linked against `nexis-gui`
- ~214 unit test methods across 15 suites:
  - Utilities: FormatUtil (10), FileUtil (10), CommandUtil (9)
  - Core parsers (FR-76): MemoryInfo (14), CpuInfo (19), GpuInfo (23), AptSource (14), FanInfo (16), ThermalInfo (11), BatteryInfo (12), DiskInfo (17), DiskHealth (20), PowerProfile (24)
  - Services: HostService (25)
  - Managers: ScheduleManager (15)
  - Theme: ThemeTokens (7)
- Static parser pattern: parsing logic extracted into public static methods on shared base classes, tested with fixture data files in `tests/fixtures/`
- Screenshot test: captures 11 pages × 2 themes, compares against reference PNGs with per-page pixel tolerance
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
| LinguistTools | `qt_create_translation` for i18n |

### Build Commands

```bash
# Clean rebuild (macOS, from project root)
rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) && \
  cmake --build build -j$(sysctl -n hw.ncpu)

# Incremental rebuild
cmake --build build -j$(sysctl -n hw.ncpu)
```

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
- **Network:** `@networkUploadColor` — upload sparkline/label color
- **Overlay/Shadow:** `@overlayBackground`, `@overlayText`, `@shadowColor` — kiosk overlay and drop shadow colors (8-digit hex `#AARRGGBB` for alpha support)
- **Chart Series Palette:** `@chartSeries01` through `@chartSeries20` — 20 colors for `HistoryChart` data lines, with light-theme variants optimized for white backgrounds

### Live Theme Refresh

All color-bearing widgets implement a `refreshThemeColors()` method connected to `SignalMapper::sigChangedAppTheme`. Constructors accept token name strings (e.g., `"@cpuColor"`) instead of resolved `QColor` values. On theme switch, each widget re-resolves its tokens from `AppManager::getStyleValues()` and updates all visual properties (series pens, area fills, chart backgrounds, inline stylesheets, progress bar chunks, shadow effects). This pattern ensures zero hardcoded colors in C++ code — every color comes from `values.ini`.

### DPI Scaling

QSS tokens include `@dpN` values (e.g., `@dp8`, `@dp12`) that are computed at stylesheet load time based on `devicePixelRatio()`. The `Dpi::scale()` utility class handles pixel scaling in C++ code. This solved HiDPI/4K display issues without requiring a QML migration.

---

## Configuration and Settings

### Storage Backend

`SettingManager` wraps Qt's `QSettings`:
- **macOS:** `~/Library/Preferences/com.nexis.plist`
- **Linux:** `~/.config/nexis/nexis.conf`

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
- **Linux:** Creates systemd timer units or cron entries (auto-detected)

---

## Translation System

34 languages supported via Qt Linguist (`.ts` → `.qm`):

Arabic, Afrikaans, Catalan, Chinese (Simplified/Traditional), Czech, Danish, Dutch, English, Finnish, French, Galician, German, Greek, Hebrew, Hindi, Italian, Japanese, Kannada, Korean, Malayalam, Norwegian, Occitan, Polish, Portuguese, Romanian, Russian, Serbian, Spanish, Swedish, Turkish, Ukrainian, Vietnamese

**Build:** `qt_create_translation()` generates binary `.qm` files embedded in the executable.
**Runtime:** `AppManager` loads the selected language via `QTranslator::load()`.
**Management:** Crowdin integration with automated PR workflows for community translations.

---

## Application Lifecycle

### Startup Sequence (`main.cpp`)

1. Create `QApplication`
2. Install custom message handler (logs to `~/.config/nexis/nexis.log`)
3. **Headless mode check:** `--clean <id>` or `--check-threshold` → run `CleanerService`, exit
4. **Single-instance enforcement:** `QLockFile` at `/tmp/nexis.lock` — shows warning dialog if already running
5. macOS icon theme setup: add Homebrew icon paths to `QIcon::themeSearchPaths()`
6. Set fallback icon theme to "Adwaita"
7. Load Ubuntu font from embedded resources
8. Show theme-aware splash screen matching Color Scheme setting (unless `--nosplash`)
9. Create `App` main window
10. Show window (unless `--hide`)
11. Enter Qt event loop: `app.exec()`

### Main Window Initialization (`App`)

1. Create `SlidingStackedWidget` (animated page container)
2. Instantiate all 16 pages
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

The sidebar is **collapsible**, organized into three labelled groups — **MONITOR**, **MANAGE**, and **SYSTEM** — matching the logical grouping of the 16 pages. Each group has a **clickable section header** with a chevron indicator that toggles the group's visibility. Collapsed groups hide their child buttons with a smooth 200ms height animation; the collapsed/expanded state is persisted per-section across sessions as JSON in QSettings. Navigating to a page in a collapsed group (via tray menu, command palette, or `sigNavigateToPage`) auto-expands that group. The full sidebar can also collapse to a 64 px icon-rail showing only page icons plus section indicator dots; the collapse and expand transitions use a smooth width animation. The sidebar can be toggled with the **Ctrl+B** keyboard shortcut or the collapse button at the top of the panel. The sidebar header displays a **gradient logo** (full wordmark when expanded, lettermark when collapsed) above a **separator line**, with a **version label** below. Active page badges use a cleaner dot indicator in collapsed mode and are hidden when their section is collapsed. The Homebrew/APT button displays an **updates badge** showing the pending update count when the sidebar is expanded, or a colored dot (using `@updatesColor`) when collapsed.

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
[All 16 pages reload theme-dependent assets (icons, GIF loaders, etc.)]
```

### Refresh Timing

All periodic polling is centralized in `DataRefreshService`, which owns 5 QTimers and emits typed data signals. Pages subscribe as reactive consumers — no page owns a QTimer.

| Data | Refresh Rate | Timer Tier | Signal |
|------|-------------|------------|--------|
| CPU, Memory, Network, Disk I/O, GPU, Temp, Battery | 1 second | Fast (mFastTimer) | `cpuUpdated`, `memoryUpdated`, `networkUpdated`, `diskIOUpdated`, `gpuUpdated`, `tempUpdated`, `batteryUpdated` |
| Disk usage | 5 seconds | Medium (mMediumTimer) | `diskUsageUpdated` |
| Disk health (SMART) | 30 seconds | Slow (mSlowTimer) | `diskHealthUpdated` |
| Processes | 1–10 seconds | Configurable (mProcessTimer) | `processesUpdated` |
| System updates | 1 hour | Update (mUpdateTimer) | `systemUpdatesChecked` |
| Services, packages | On demand | — | Manual refresh / page navigation |
| Hardware info | Once | — | Page construction |

**Battery optimization:** When the app is minimized to tray, `DataRefreshService::pause()` stops all timers (unless kiosk mode is active). On restore, `resume()` fires immediate ticks then restarts timers.

**Page-aware polling (BUG-72):** Three data-heavy pages (`DashboardPage`, `ResourcesPage`, `ProcessesPage`) inherit from `NexisPage` and use `onPageActivated()`/`onPageDeactivated()` lifecycle hooks to reduce CPU usage when hidden:
- **ProcessesPage:** Calls `DataRefreshService::pauseProcessTimer()` / `resumeProcessTimer()`. The process timer (which spawns `ps ax`, `nettop`, and per-PID `proc_pid_rusage()`) only runs when the Processes page is active. It starts paused by default.
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
│   │   ├── Info/               15 system info providers (incl. StartupInfo)
│   │   ├── Tools/              7 tool classes (incl. FileSearchTool)
│   │   └── Utils/              3 utility classes
│   ├── nexis/                  GUI application (shared)
│   │   ├── Managers/           7 manager singletons
│   │   ├── Services/           7 domain services
│   │   ├── Pages/              14 page implementations
│   │   │   ├── Dashboard/
│   │   │   ├── HardwareInfo/
│   │   │   ├── StartupApps/
│   │   │   ├── SystemCleaner/
│   │   │   ├── DiskTools/
│   │   │   ├── Search/
│   │   │   ├── Services/
│   │   │   ├── Processes/
│   │   │   ├── Uninstaller/
│   │   │   ├── Resources/
│   │   │   ├── Helpers/
│   │   │   ├── AptSourceManager/
│   │   │   ├── GnomeSettings/
│   │   │   ├── Docker/
│   │   │   └── Settings/ (placeholder — platform-split)
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
