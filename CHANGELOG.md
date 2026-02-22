# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.0.1] - 2026-02-22

### Fixed
- **Release workflow:** Delete pre-existing GitHub releases for inherited upstream tags before creating new Nexis releases. Mark new releases as latest.

## [2.0.0] - 2026-02-22

### Added — Complete UI Redesign
- **Bento dashboard with collapsible sidebar (FR-42):** Complete visual overhaul replacing CircleBar/LineBar gauges with MetricTile widgets featuring sparkline charts, progress bars, and trend indicators. Collapsible sidebar with grouped sections (MONITOR/MANAGE/SYSTEM), smooth 250ms animation, icon-rail at 64px, Ctrl+B shortcut. Gradient sidebar logo (wordmark expanded, lettermark collapsed). Command Palette (Ctrl+K) for fuzzy navigation and actions. System summary card on dashboard. Footer status bar with version and refresh rate.
- **HeroCard widget (FR-43):** Combined CPU + Memory tile with vertical divider. Each half is a MetricTile in Hero display mode with sparkline history.
- **DiskTile with donut chart (FR-43, FR-44):** Custom-painted donut chart replacing sparkline MetricTile for disk usage. Drive health badge with verdict and numeric percentage (e.g., "Apple SSD: Good (92%)").
- **NetworkTile (FR-44):** Two-row layout with Download and Upload labels each paired with a separate sparkline chart, horizontal divider, and active interface name.
- **Pixel-perfect mockup alignment (FR-44):** 29 individual fixes aligning the implementation to approved SVG mockups, including MetricTile display modes, sidebar polish, and grid reorganization.
- **Disk tile gear icon selector (FR-45):** Gear icon in top-right corner of DiskTile (visible when 2+ disks detected) opens dropdown to switch displayed disk; selection persisted.
- **Temperature tile gear icon selector (FR-46):** Replaced QComboBox with gear icon + QMenu dropdown matching DiskTile pattern. Gear button added to shared MetricTile widget (opt-in, hidden by default).
- **Kiosk mode UI controls (FR-30):** Three new entry/exit methods for kiosk mode: checkable tray menu action, floating fullscreen/collapse toggle button on Dashboard, and transient "Press ESC to exit" overlay with fade animation.

### Added — Architecture Overhaul
- **Service layer abstraction (FR-42):** 7 new domain services (StartupService, FileSearchService, HostService, ProcessService, SystemServiceManager, DockerService, PackageService) separating business logic from UI. NexisPage base class with `onPageActivated()`/`onPageDeactivated()` lifecycle hooks. All `QtConcurrent::run` and `CommandUtil::exec` calls removed from pages.
- **Abstract base classes (FR-34):** Pure virtual interfaces for all 10 Info classes and 4 Tool classes. Platform implementations become named subclasses (e.g., `CpuInfoLinux`, `CpuInfoMacOS`). Managers use `std::unique_ptr<Interface>` with compile-time factory construction.
- **Dependency injection (FR-35):** Optional `nullptr`-default manager pointer parameters on all 10 page constructors. Constructor initializers use ternary fallback. All `::ins()` calls in page bodies replaced with member variable access.
- **Centralized DataRefreshService (FR-37):** Replaced ~25 per-page QTimers with a single service owning 4 QTimers (1s/5s/30s/configurable). 10 typed data-change signals. Pages subscribe as reactive consumers. Pause/resume on app minimize (kiosk mode overrides pause).
- **QSS token validation (FR-32):** Runtime validation that all `@tokens` in `style.qss` have corresponding values in `values.ini`, and that color values are valid hex format.
- **Explicit CMake source lists (FR-31):** Confirmed all CMakeLists.txt files use explicit `set()` source lists — no `file(GLOB_RECURSE ...)` calls.

### Added — Testing & CI
- **Qt Test infrastructure (FR-33):** Test directory, CMake test target, CTest integration, CI test step.
- **Unit test suite (FR-36):** 63 tests across 6 executables covering FormatUtil, FileUtil, CommandUtil, DiskHealthInfo, ScheduleManager, and theme token validation.
- **CI screenshot regression tests (FR-41):** Automated screenshot capture of each page across themes with perceptual diff comparison.

### Added — Visual Consistency
- **Bundled fonts and font picker (FR-38):** Bundled Inter, JetBrains Mono, and Ubuntu fonts. `@fontFamily` QSS token with user-configurable font picker on Settings page.
- **SVG-only icons (FR-25):** Removed all `QIcon::fromTheme()` calls (except one legitimate dynamic app-icon lookup). Created 4 disk tool SVGs. Sidebar icons always use bundled SVGs.
- **Removed hardcoded Ubuntu font (FR-26):** Stripped all `<family>Ubuntu</family>` blocks from `.ui` files. Font family controlled by QSS token and font picker.

### Changed
- **Disk Health tile removed (FR-47):** Standalone Disk Health MetricTile removed from Dashboard grid. Health information now shown as badges on the DiskTile with numeric health percentage.
- **Quick Actions bar removed:** Replaced with expanded full-width system summary card.
- **Refined theme colors:** Dark theme uses deep charcoal base (`#1A1C22`) with warm orange accent (`#FF6B1A`). Light theme uses warm cream base (`#F5F0EB`). 24 additional theme tokens added for full coverage.

### Fixed
- **BUG-42:** GNOME Settings "Mouse & Touchpad" tab button renders correctly (escaped ampersand).
- **BUG-43:** Host Manager security and data integrity — 7 fixes including `sudo tee` instead of predictable temp files, error handling, input validation (IPv4/IPv6, RFC 1123 hostnames), backup before save, confirmation dialog with change summary.
- **BUG-45:** Kiosk mode toggle button icons changed from gray to Nexis orange.
- **BUG-46:** Kiosk mode "Press ESC" overlay centered on screen geometry instead of stacked widget.
- **BUG-47:** Theme switching now fully applies to all widgets — 24 new theme tokens, `refreshThemeColors()` methods on all tile widgets, zero hardcoded colors in C++.
- **BUG-48:** Qt resources now load correctly (`Q_INIT_RESOURCE(static)` added to main.cpp).
- **BUG-49:** QSS token replacement sorted by descending length to prevent substring collisions.
- **BUG-50:** Command palette "Toggle Theme" uses correct `getColorScheme()`/`setColorScheme()` methods.
- **BUG-51:** Disk tile percentage text uses theme color instead of system palette.
- **BUG-52:** Sidebar toggle changed from QPushButton to QToolButton for correct icon rendering on macOS.
- **BUG-53:** Duplicate drive health entries on macOS fixed — disk image filter and model-based deduplication.
- **BUG-54:** GNOME Settings Appearance tab now uses QGroupBox containers matching other tabs.
- **BUG-55:** Dashboard card borders and shadows improved — increased border contrast and shadow depth.
- **BUG-56:** Sidebar items correctly left-aligned after expanding from collapsed state (child widget re-polish).
- **BUG-57:** Disk selector filters virtual filesystems, snapshots, loopbacks, and hidden macOS system volumes.
- **BUG-58:** Search page button icon path corrected from `.png` to `.svg`.
- **BUG-59:** Removed upstream Stacer "BETA version" label from Search page.
- **BUG-60:** Dashboard system summary shows correct RAM (lazy-updated on first memory callback).
- **BUG-61:** Disk tile health badge shows only the selected disk's health with volume-to-physical-drive matching.
- Fixed double-free crash in `DiskTile::clearDriveHealth()` (QLayout inherits QLayoutItem — `takeAt()` returns the layout itself).
- Settings page: Disk Health Alert repositioned next to Disk Analyzer (from isolated column).

## [1.2.0] - 2026-02-18

### Added
- **deb822 APT source file support (FR-01):** Dual-format parsing for modern `.sources` (deb822) and legacy `.list` files. Stanza-aware editing preserves field order, comments, and multi-line `Signed-By` values. Format-preserving writes. Renamed `distribution` → `suites` to match APT terminology. Edit dialog uses structured objects instead of format-specific strings. UX improvements: selection cleared after operations, search filter preserved, button feedback during add.
- **Docker management page (FR-20):** New sidebar page for managing Docker images, containers, and volumes. Three-tab interface with grouped tree views, batch remove/prune operations with confirmation dialogs, container start/stop, search filtering, lazy tab loading, and daemon status detection. Conditionally shown when Docker is installed.
- **APT-RPM support (FR-09):** Added support for APT-RPM package management (ALT Linux, PCLinuxOS, Vine Linux). Dynamic `binaryType()`/`sourceType()` helpers replace hardcoded `"deb"`/`"deb-src"` strings. `apt-repo` support for repository add/remove with fallback. RPM-format placeholder text in the APT Source Manager page.
- **Scheduled/automated cleaning (FR-16):** Settings page section with quick-setup toggle for weekly cleaning, custom schedule management (create/edit/delete via dialog), cleaning history viewer, threshold-based junk notifications, and post-clean tray notifications. Full schedule persistence via JSON with ScheduleManager and CleanerService backend.
- **Battery & disk health monitoring (FR-29):** Three-phase implementation — battery health CircleBar on Dashboard with capacity degradation alerts, SMART-based disk health monitoring via `smartctl`/IOKit with temperature and wear tracking, and Resources page integration with disk temperature history chart.
- **Startup Apps improvements (FR-10):** Search/filter bar, inline editor fields, and application icons in the startup app list.
- **ARM64 Linux CI (FR-06):** GitHub Actions workflow for ARM64 Linux builds and release artifacts.

### Fixed
- **BUG-06:** Large `/etc/hosts` files no longer freeze the UI at startup (deferred loading, batched model population, pre-compiled regex, incremental updates).
- **BUG-07:** HiDPI/4K scaling via `Dpi::scale()` utility, `@dpN` QSS tokens, and relaxed `.ui` size constraints.
- **BUG-08:** Wayland compatibility — guarded `primaryScreen()` null dereference, use `requestActivate()` for `xdg-activation` protocol.
- **BUG-10:** System Cleaner memory leak fixed — concurrent worker guards, proper destructor cleanup, scan result list clearing, eliminated redundant directory traversals.
- **BUG-39:** `getDesktopValue()` no longer truncates Exec lines containing `=` (env variables).
- **BUG-40:** FR-16 UI regressions — repositioned Scheduled Cleaning section above footer, replaced all hardcoded colors with QSS theme tokens, added dialog-title labels, primary/danger button styling, drop shadows, and focus ring fix.
- **BUG-41:** Manage Schedules dialog scroll area viewport now renders correctly in dark mode via transparent stylesheet pattern.
- Added generic `QDialog` and `QDialog QLabel` QSS rules for consistent dark mode dialog theming.
- Added `QCheckBox:focus` QSS rule to suppress platform-default purple focus ring.

## [1.1.2] - 2025-02-16

### Added
- **Hardware Info page (FR-12):** New sidebar tab between Dashboard and Startup Apps displaying System, Processor, Graphics, and Memory details. Platform-specific cache size retrieval (sysctl on macOS, sysfs on Linux).
- **Dashboard kiosk mode (FR-28):** F11 toggles fullscreen dashboard-only mode (hides sidebar and page title). ESC exits. State persisted across sessions. Temperature sensor and GPU device selections remembered.
- **Crowdin translations:** New translations via GitHub Action integration.

### Fixed
- **BUG-33:** Uninstaller "Purge" checkbox text now visible in dark mode (added `QCheckBox { color }` rule).
- **BUG-34:** Settings page author link changed from hardcoded blue to Nexis accent orange (`#E95420`).
- **BUG-35:** Removed non-functional "Donate" button from Settings page.
- **BUG-36:** System Cleaner "Total Size" label now visible in dark mode.
- **BUG-37:** System Cleaner `scanLoading.gif` animation now plays reliably; fixed QMovie memory leak on theme change.
- **BUG-38:** Hardware Info table rows now legible in dark mode; removed broken alternating row colours, added opaque item backgrounds.
- Hardware Info QGroupBox containers now size correctly (moved padding from QSS to layout margins).
- Hardware Info tables no longer show unnecessary scrollbars.

### Changed
- Dashboard system info panel removed (replaced by Hardware Info page).
- Dashboard temperature/GPU/network modules now fill full width in normal and kiosk modes.
- Hardware Info page simplified to 4 sections (System, Processor, Graphics, Memory); Storage, Network, and Thermal sections removed due to sizing limitations.

## [1.1.0] - 2025-01-16

### Added
- **GPU monitoring (FR-11):** Dashboard CircleBar and Resources history chart for GPU utilization. Multi-GPU selector. Linux: AMD sysfs, NVIDIA nvidia-smi, Intel frequency ratio. macOS: IOKit IOAccelerator.
- **Hardware info research (FR-12):** Initial research and planning for hardware info tab.
- **Disk Usage Analyzer launcher (FR-23):** Resources page launcher for Baobab, Filelight, GrandPerspective, etc.
- **Configurable disk analyzer (FR-24):** Settings combobox to choose preferred disk usage tool.
- **GNOME Settings dropdowns (FR-27):** Replaced theme/font text fields with populated dropdowns.
- **Homebrew page tree widget (FR-22):** Grouped Formula/Cask sections with multi-select.
- **macOS .app bundle uninstaller (FR-21):** Scan /Applications, parse Info.plist, move to Trash.
- **Purge option in uninstaller (FR-19):** APT `purge` mode via checkbox.
- **Autostart delay option (FR-15):** Configurable delay for startup apps.
- **Expanded cache cleaning (FR-03):** Electron app caches and dev tool paths.
- **Single-instance enforcement (FR-02):** QLockFile prevents duplicate launches.
- **SVG logo and tray icon (FR-07):** Redesigned in SVG for all themes.
- **Crowdin translation integration (FR-08):** Automated PR workflows for 34 languages.
- **Pip cache cleaning (FR-17):** PIP_CACHE_DIR env var support.

### Fixed
- **BUG-01:** Swapped memory variables in `/proc/meminfo` parsing.
- **BUG-02:** System Cleaner no longer deletes entire cache directories (empties contents instead).
- **BUG-03:** Single-instance enforcement via QLockFile.
- **BUG-04:** CPU speed now reads from sysfs cpufreq as fallback.
- **BUG-05:** Background threads cleaned up on exit via `waitForDone()`.
- **BUG-09:** `LC_ALL=C` for system command parsing on non-English locales.
- **BUG-11:** macOS crash on launch from double CFRelease in GPU detection.
- **BUG-12:** Missing icon fallback for System Cleaner on macOS.
- **BUG-13:** Sidebar icons now use Adwaita theme on macOS via Homebrew icon paths.
- **BUG-14:** NVIDIA GPU utilization uses PCI bus ID instead of DRM card index.
- **BUG-15:** Uninstaller finds Homebrew via absolute paths on macOS.
- **BUG-16:** Uninstaller shows descriptions for Homebrew packages via JSON API.
- **BUG-17:** Feedback form replaced with GitHub Issues launcher (removed defunct Heroku endpoint).
- **BUG-18:** Settings version label now dynamic from CMake `PROJECT_VERSION`.
- **BUG-19:** System Cleaner sort dropdown labels now include direction (A-Z, Z-A, etc.).
- **BUG-20:** System Cleaner back button uses consistent SVG icon.
- **BUG-21:** Homebrew tree view respects dark theme via object name.
- **BUG-22:** Uninstaller/Homebrew tree views show expand/collapse chevrons.
- **BUG-23:** Uninstaller/Homebrew tree views use table-style layout matching System Cleaner.
- **BUG-24:** YUM/DNF cache paths fixed (was returning Pacman paths).
- **BUG-25:** CircleBar potential double-delete of QChart fixed.
- **BUG-26:** DiskInfo changed to value semantics (Rule of Three violation fixed).
- **BUG-27:** Linux `/proc/meminfo` bounds checking added.
- **BUG-28:** CPU core count changed from `quint8` to `int` (overflow at 256 threads).
- **BUG-29:** `toLong()` changed to `toLongLong()` for 64-bit safety.
- **BUG-30:** Reverted Phase 2 margin changes to restore original layouts.
- **BUG-31:** GNOME Settings now shows inline errors and reverts widgets on `gsettings set` failure.
- **BUG-32:** GNOME Settings speed sliders debounced with 200ms timer.
- Linux `.desktop` file: added `StartupWMClass` for GNOME X11 icon matching.
- Linux: hicolor icons installed, desktop filename set, AppImage icon path fixed.
- Build: CMake `PROJECT_VERSION` used instead of `git describe` for app version.
