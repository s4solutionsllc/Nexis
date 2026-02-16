# Feature Requests

> Managed by Claude Code. Updated across sessions.
> Status: `[ ]` = planned, `[~]` = in progress, `[x]` = done

## From Other Forks (QuentiumYT, etc.)

- [ ] **FR-01: deb822 APT source file support** — Modern `.sources` format used by Debian 13+ / Ubuntu 24.04+. QuentiumYT commit `87279f6`.
- [x] **FR-02: Single-instance enforcement** — Prevent multiple app copies via QLockFile/QSharedMemory. Focus existing window on re-launch. QuentiumYT issue #12. **Resolved:** Added QLockFile-based enforcement in main.cpp.
- [x] **FR-03: Expanded cache cleaning (Electron apps, npm, gradle, etc.)** — Scan `~/.config/*/Cache` and `~/.config/*/GPUCache` for Electron apps (Discord, Slack, VSCode). Also npm, bun, gradle, expo caches. QuentiumYT v1.5.0. **Resolved:** Added "Dev Tool Caches" as a 6th System Cleaner category. Scans Electron app caches (~/.config/*/Cache on Linux, ~/Library/Application Support/*/Cache on macOS) and well-known dev tool paths (~/.npm, ~/.gradle/caches, ~/.cargo/registry, etc.). Ambiguous "Cache"/"GPUCache" entries are renamed to "appName/Cache" for clarity.
- [x] **FR-04: Background thread cleanup on exit** — Wait for QtConcurrent threads to finish before quitting. QuentiumYT commit `fa70178`.
- [x] **FR-05: LC_ALL=C for system command parsing** — Force English output so `lscpu` and similar commands parse correctly on non-English systems. QuentiumYT commit `e0b957f`. **Resolved:** Changed LANG=C to LC_ALL=C in lscpu calls.
- [ ] **FR-06: ARM64 Linux architecture support** — Build for armhf, arm64, i386, powerpc, ppc64el, riscv64, s390x.
- [x] **FR-07: SVG logo and colorful tray icon** — Redesigned logo in SVG with tray icon that works on light and dark themes. QuentiumYT commit `347bcbe`. **Resolved:** Created logo.svg and tray-icon.svg; updated app icon and tray icon to use SVGs.
- [x] **FR-08: Crowdin translation integration** — Professional translation management with automated PR workflows. **Resolved:** Added `crowdin.yml`, GitHub Actions for sync (`crowdin-sync.yml`) and lupdate normalization (`lupdate.yml`). Fixed Galician language attribute, added Romanian to `languages.json`, refreshed source strings with lupdate (417 strings). README updated with Crowdin badge and maintainer setup instructions.
- [ ] **FR-09: APT-RPM support (ALT Linux, PCLinuxOS, Vine Linux)** — QuentiumYT PR #31.
- [ ] **FR-10: Startup app customization enhancements** — Expanded options for managing auto-start applications. QuentiumYT v1.5.0.

## From Community Issues (oguzhaninan/Stacer)

- [x] **FR-11: GPU load / temperature monitoring** — Issues [#105](https://github.com/oguzhaninan/Stacer/issues/105), [#405](https://github.com/oguzhaninan/Stacer/issues/405). **Resolved:** Added GpuInfo class with platform-specific implementations. Linux: AMD sysfs gpu_busy_percent, NVIDIA nvidia-smi, Intel frequency ratio. macOS: IOKit IOAccelerator PerformanceStatistics. Dashboard shows GPU CircleBar with multi-GPU selector. Resources page shows GPU utilization history chart. Temperature was already supported via ThermalInfo.
- [ ] **FR-12: Hardware info tab** — Issue [#527](https://github.com/oguzhaninan/Stacer/issues/527).
- [ ] **FR-13: CLI interface** — Issue [#411](https://github.com/oguzhaninan/Stacer/issues/411).
- [ ] **FR-14: Flatpak distribution** — Issue [#493](https://github.com/oguzhaninan/Stacer/issues/493).
- [x] **FR-15: Autostart delay option** — Issue [#424](https://github.com/oguzhaninan/Stacer/issues/424). **Resolved:** Added delay spinbox to startup app editor. Linux uses X-GNOME-Autostart-Delay; macOS uses shell sleep wrapper.
- [ ] **FR-16: Scheduled / automated cleaning** — Issue [#449](https://github.com/oguzhaninan/Stacer/issues/449).
- [x] **FR-17: Pip cache cleaning** — Issue [#481](https://github.com/oguzhaninan/Stacer/issues/481). **Resolved:** Pip cache already scanned under app caches; added PIP_CACHE_DIR env var support for non-standard locations.
- [ ] **FR-18: System Cleaner exclusion rules** — Issue [#484](https://github.com/oguzhaninan/Stacer/issues/484).
- [x] **FR-19: Purge vs remove option in uninstaller** — Issue [#484](https://github.com/oguzhaninan/Stacer/issues/484). **Resolved:** Added purge checkbox to uninstaller UI; APT uses `purge` instead of `remove` when checked. Hidden on macOS (not applicable).
- [ ] **FR-20: Docker image / volume management** — Issue [#454](https://github.com/oguzhaninan/Stacer/issues/454).

## macOS Platform

- [x] **FR-21: macOS native .app bundle uninstaller** — Scan /Applications and ~/Applications for .app bundles, parse Info.plist for display name and version, filter out Apple system apps (com.apple.*), move to Trash via Finder AppleScript. **Resolved:** Implemented in PackageTool::getInstalledApps() and trashApps(). Uninstaller page now shows "Applications" on macOS with "Name version" format.
- [x] **FR-22: Homebrew page tree widget with multi-select uninstall** — Replace flat QListWidget on macOS Homebrew page with QTreeWidget grouped by Formula/Cask sections, checkboxes for multi-select, search with auto-expand, dry-run dependency confirmation, and async background loading. **Resolved:** Programmatic QTreeWidget in APTSourceManagerPage, mirrors Uninstaller page layout. Install field retained.

## From Our Issues (lsimpsonsfdc/Nexis)

- [x] **FR-23: Disk Usage Analyzer launcher** — Provide a launcher card on the Resources page that detects the best disk-usage tool for the platform/DE (Baobab on GNOME, Filelight elsewhere, GrandPerspective on macOS) and offers launch or install actions. Issue [#2](https://github.com/lsimpsonsfdc/Nexis/issues/2). **Resolved:** Replaced custom DiskAnalyzerWidget with DiskUsageLauncherWidget that delegates to dedicated tools.
- [x] **FR-24: Configurable disk analyzer preference** — Allow users to choose their preferred disk usage analyzer from Settings instead of relying solely on auto-detection. Adds a "Disk Analyzer" combobox to the Settings page with platform-specific tool options (Linux: Baobab, Filelight, QDirStat, ncdu; macOS: GrandPerspective, DaisyDisk, OmniDiskSweeper) plus a "Custom..." option with a free-form executable path. The Resources page launcher widget reads this preference and respects the user's choice. Extends FR-23.

## UI Consistency

- [ ] **FR-25: Remove `QIcon::fromTheme()` — use bundled assets only** — Replace all `QIcon::fromTheme()` calls with direct `QIcon(...)` loading from bundled QRC assets. Currently Linux uses the system icon theme (Adwaita, Breeze, etc.) producing different visuals per desktop environment, while macOS already uses bundled assets. Affected files: `app.cpp`, `system_cleaner_page.cpp`, `uninstaller_page.cpp`, `apt_source_manager_page.cpp`, `disk_usage_launcher_widget.cpp`. Asset gap: needs a generic "disk/storage" SVG for the disk usage launcher. Formerly revision_plan.md Phase 10.1.
- [ ] **FR-26: Remove hardcoded Ubuntu font from `.ui` files** — Strip all `<family>Ubuntu</family>` font property blocks from `.ui` files so Qt uses the platform's native system font. The Ubuntu font is only available on Ubuntu-based distros; on macOS and other Linux distros Qt falls back unpredictably. Removing it ensures the app looks native everywhere. Formerly revision_plan.md Phase 10.2.

## GNOME Settings

- [ ] **FR-27: GNOME Settings — populate theme/font fields as dropdowns with installed values** — Replace the plain `QLineEdit` fields for GTK Theme, Icon Theme, Cursor Theme, UI Font, Document Font, Monospace Font, and Titlebar Font with `QComboBox` (or `QFontComboBox` for fonts) that enumerate installed options. For themes, scan `/usr/share/themes/`, `/usr/share/icons/`, `/usr/share/cursors/` etc. For fonts, use `QFontDatabase`. Users currently must type exact theme/font names with no guidance on what's available.

## Notes

<!-- Claude Code: append new feature requests here. Use the next available FR-XX id. -->
