# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.2.13] - 2026-04-15

### Fixed
- **Compact window layout (FR-94):** Resolved window-too-large issues on small high-DPI screens (e.g., 2160×1440 12-inch laptops). System Cleaner Scan button is now visible without resizing or dragging. Changes: reduced System Cleaner top spacer (100 px → 20 px) and pre-scan fixed spacer (30 px → 10 px); scan results Size column is always visible via proportional column sizing (no more hard-coded 600 px filename column); empty-state placeholder height reduced from 200 px to 100 px on Startup Apps, Services, APT Repository Manager, and Uninstaller; sidebar button height reduced from 36 px to 32 px; window minimum size enforced at 700×480 px.

## [2.2.12] - 2026-04-08

### Changed
- **QSS migration (FR-89):** Migrated 13 remaining inline `setStyleSheet()` calls in repo detail panel and network diagnostics widget to property-driven QSS selectors. All widget colors now come from theme tokens via the global stylesheet.
- **Attribution:** Added explicit copyright lines for Oğuzhan İnan (original Stacer, 2017-2020) and S4 Solutions, LLC (Nexis fork, 2025-2026) to `LICENSE`. Reworded the homepage footer to lead with S4 Solutions maintenance while preserving Stacer attribution.

## [2.2.11] - 2026-04-01

### Added
- **Homebrew Cask (FR-92):** macOS users can now install via `brew tap s4solutionsllc/nexis && brew install --cask nexis`. Custom tap auto-updates on new releases via GitHub Actions.
- **macOS code signing and notarization (FR-93):** macOS .dmg releases are now signed with a Developer ID certificate and notarized by Apple. The app opens without Gatekeeper warnings on first launch.

## [2.2.10] - 2026-04-01

### Fixed
- **PPA (FR-90):** Fixed Jammy (22.04) PPA build by injecting `libqt6opengl6-dev` via per-series workflow patch. The Debian alternatives approach didn't work because apt short-circuits when `qt6-base-dev` is already in the dep list.

## [2.2.9] - 2026-03-31

### Fixed
- **PPA (FR-90):** Fixed Jammy (22.04) PPA build failure by adding missing `libqt6opengl6-dev` build dependency. Qt6OpenGL dev headers are a separate package on Jammy but bundled in `qt6-base-dev` on Noble/Questing.

## [2.2.8] - 2026-03-30

### Added
- **Website "Release Notes" modal (FR-91):** Replaced the "View on GitHub" button on the website landing page with a "Release Notes" button. Clicking it opens a modal that displays all new features and bug fixes from the latest release, parsed from CHANGELOG.md at build time. Uses native `<dialog>` element with dark theme styling, click-outside-to-close, and fallback message if changelog is unavailable.
- **PPA repository (FR-90):** Ubuntu users can now install via `sudo add-apt-repository ppa:s4solutionsllc/nexis && sudo apt install nexis` with automatic updates. Supports Ubuntu 22.04 (Jammy), 24.04 (Noble), and 25.04 (Plucky) on x86_64 and ARM64.

## [2.2.3] - 2026-03-26

### Added
- **Ask Claude.ai button (FR-90):** Replaced "Search Online" button in APT Repository Manager with an "Ask Claude.ai" button that opens a pre-filled Claude.ai query for repository troubleshooting.

### Fixed
- **BUG-104:** APT health checker now handles flat repo URL formats and no longer reports false "unreachable" status for path-based repositories.
- **BUG-105:** Skipped suite mismatch check on third-party repos that use non-standard suite naming.
- **BUG-106:** DEB822 multi-suite stanzas are now expanded into separate entries for correct per-suite health checking.

## [2.2.2] - 2026-03-25

### Fixed
- **BUG-103:** Fixed schedule indicator rendering as a white square on initial System Cleaner page load.
- Reduced System Cleaner category icons from 64px to 48px and scaled labels/checkboxes to match.
- Moved Select All toggle directly below the Scan button for better discoverability.
- Added missing QSS styles for Docker tree widgets.
- Fixed QSS audit issues: missing styles, orphaned selectors, and hardcoded colors.

### Changed
- **FR-88:** Migrated inline styles to central QSS in four phases — helper widgets, static inline styles, semi-dynamic property selectors, and cleanup of empty methods and unused includes.

## [2.2.1] - 2026-03-25

### Added
- **APT Repository Health Dashboard (FR-87):** Added a health dashboard to the APT Repository Manager page with status indicators, detail panel, repo descriptions from a built-in knowledge base, and macOS Homebrew health checking. Includes repair actions: disable/enable/remove sources, duplicate removal, legacy-to-deb822 conversion, and connection diagnosis with inline results.

### Fixed
- Fixed detail panel gray background and text padding in repo health dashboard.
- Normalized health checker cache keys, added dynamic_cast safety, and implemented macOS tap/pinned checks.

## [2.1.19] - 2026-03-18

### Fixed
- **BUG-101:** Injected tag version into binary at build time to fix version display in release builds.

## [2.1.18] - 2026-03-18

### Added
- **FR-86:** Added Unlock All and Make Permanent buttons for SMART disk health elevation.

### Fixed
- **BUG-98:** Added smartctl Unlock button to Hardware Info and fixed false health verdicts for locked drives.
- **BUG-99:** Detected system icon theme name for AppImage tray icon rendering.
- **BUG-100:** Batched SMART elevation prompts into a single dialog and fixed unlock crash from concurrent pkexec calls.
- **BUG-101:** Replaced unstyled QToolButtons with QPushButton on Hardware Info page.
- **BUG-102:** Added color token to Download button for correct light mode rendering.

## [2.1.17] - 2026-03-18

### Fixed
- **BUG-97:** Added system icon theme paths for AppImage environments so tray icon renders correctly.

## [2.1.16] - 2026-03-17

### Added
- **Maintenance wizard (FR-83):** Added a guided system checkup wizard accessible from the Dashboard.
- **System theme tray icon (FR-86):** Added a "System Theme" option for the tray icon that uses the desktop environment's native icon.

### Fixed
- Used `utilities-system-monitor` icon name for system theme tray icon compatibility.

## [2.1.15] - 2026-03-11

### Added
- **GPU diagnostics (FR-84, FR-85):** Detected simple-framebuffer GPUs and added a diagnostic report for systems without dedicated GPU drivers.

## [2.1.14] - 2026-03-10

### Added
- **Cleaner exclusion rules (FR-18):** Added configurable exclusion rules for the System Cleaner with a floating gear button overlay.
- **Firewall status widget (FR-68):** Added a firewall status card to the Helpers page showing UFW/PF state.
- **Open ports viewer (FR-66):** Added an open ports and active connections viewer to the Helpers page.
- **Network diagnostics panel (FR-82):** Added a network diagnostics panel to the Helpers page.
- **Snap/Flatpak cleanup (FR-79, FR-80):** Added Snap and Flatpak cache cleanup categories and orphan package detection to System Cleaner and Uninstaller.
- **CPU governor switcher (FR-70):** Added a CPU governor and power profile switcher to the Power page.

## [2.1.13] - 2026-03-07

### Added
- **Theme-aware splashscreen (FR-78):** Splashscreen now adapts to the system color scheme.
- **Expanded unit tests (FR-76):** Added unit test coverage for critical-risk code paths.

### Fixed
- **BUG-76:** Used DRM card order and improved multi-GPU display sorting.
- **BUG-82:** Removed 3 unused SignalMapper signals and associated dead code.
- **BUG-83:** Connected remaining hardcoded hex colors to the theme token system.
- **BUG-86:** Included Core sources in `qt_create_translation()` for complete i18n coverage.
- **BUG-90:** Standardized error handling and added logging to previously silent failures.
- **BUG-91:** Removed dead ioreg code block in macOS system_info.cpp.
- **BUG-92:** Parented QSystemTrayIcon to qApp for proper cleanup on exit.
- **BUG-93:** Checked `isSymLink()` before `isDir()` in cleaner to prevent symlink traversal data loss.
- **BUG-94:** Emptied Linux trash directories instead of deleting them (preserving Trash structure).
- **BUG-95:** Stripped inline `#` comments when parsing host file entries.
- Removed 9 unused legacy resource files from QRC.
- Removed orphaned upstream resource files.

### Changed
- **FR-77:** Deduplicated dashboard tile subclasses via MetricTileBase helper methods.

## [2.1.12] - 2026-03-05

### Fixed
- **BUG-80:** Fixed update check regex to support multi-digit version numbers.

## [2.1.11] - 2026-03-05

### Fixed
- **BUG-76:** Sorted GPUs by PCI bus address instead of DRM card number for consistent ordering.
- **BUG-79:** Added QSS theme rules for Disk Tools display widgets.

## [2.1.10] - 2026-03-03

### Added
- **Hide footer option (FR-75):** Added a setting to hide the Dashboard footer info bar.

### Fixed
- **BUG-75:** Added Ubuntu 25.04+ .deb build to fix t64 dependency issue.
- **BUG-77:** Compensated for QSS padding in Hardware Info table column widths.
- **BUG-78:** Repositioned sidebar badges after animation completes instead of before.

## [2.1.9] - 2026-02-27

### Added
- **Disk Tools page (FR-62, FR-63):** Added a new Disk Tools page with large file finder and duplicate file finder using a 3-stage hash pipeline with cancellation support.
- **System Log Viewer (FR-71):** Added a System Logs page with filtering and platform-specific log provider backends (journalctl on Linux, log show on macOS).
- **Health Score tile (FR-73):** Added a composite health score tile to the Dashboard combining CPU, memory, disk, and temperature metrics with weighted scoring.
- **Collapsible sidebar groups (FR-74):** Added collapsible navigation groups (MONITOR/MANAGE/SYSTEM) to the sidebar.

### Fixed
- **BUG-72:** Eliminated subprocess blocking and reduced allocation churn; gated page updates on visibility to reduce idle CPU and memory usage.
- **BUG-73:** Corrected sidebar badge positioning with `mapTo` coordinate conversion.
- **BUG-74:** Resolved ScreenshotTests SEGFAULT and broken screenshot references.
- Fixed health score color mapping thresholds and disk score weighting by partition size.
- Fixed sidebar to show icon-only buttons in collapsed mode and hide dividers.

## [2.1.8] - 2026-02-25

### Added
- **Available Updates section (FR-60):** Moved software updates from Dashboard tile to a dedicated tree view section on the APT/Homebrew page with sidebar badge indicator.
- **Browser Privacy cleanup (FR-64):** Added a Browser Privacy category to System Cleaner for clearing browser caches and data.
- **macOS maintenance actions (FR-69):** Added macOS-specific maintenance actions to the Helpers page (rebuild Spotlight, flush DNS, repair permissions).

### Fixed
- **BUG-71:** Cleared sparkline history when switching GPU, temperature, or fan sensors.
- Matched updates section styling (title padding, tree header height, margins) with the package tree.

## [2.1.7] - 2026-02-25

### Added
- **Fan speed monitoring (FR-56):** Added fan speed monitoring with FANS dashboard tile, sensor selection, Hardware Info fans section, and platform-specific backends (hwmon sysfs on Linux, SMC on macOS).
- **Memory pressure visualization (FR-57):** Added memory pressure breakdown visualization to the Dashboard.
- **Per-process I/O columns (FR-58, FR-59):** Added Disk Read/s, Disk Write/s, Net Down/s, and Net Up/s columns to the Processes page with platform-specific collection.
- **Broken Symlinks cleaner (FR-61):** Added a Broken Symlinks category to System Cleaner.
- **DNS cache flush (FR-65):** Added a one-click DNS cache flush button to the Helpers page.
- **Export System Report (FR-72):** Added an Export System Report button to the Hardware Info page.

### Fixed
- **BUG-70:** Added dedicated `fanUpdated` signal and fallback detection paths for fan sensors.
- Hidden GNOME Settings page on macOS; renamed to "System Preferences" on macOS.

## [2.1.6] - 2026-02-24

### Fixed
- **BUG-69:** Added sensor and device subtitles to GPU and Temp dashboard tiles; fixed macOS GPU iteration order.
- Fixed CI retry logic for transient macOS brew install GHCR failures.

## [2.1.5] - 2026-02-24

### Added
- **Dashboard edit mode (FR-50, FR-51):** Added drag-and-drop tile repositioning, resize handles, edit mode toggle with toolbar, and kiosk mode mutual exclusion. Dashboard layout persisted to settings. Reset Layout button in Settings page.
- **Selectable widget styles (FR-53):** Added per-tile widget style selector (sparkline, speedometer, gauge, VU meter, hybrid) with theme-compliant rendering.
- **Remove widgets (FR-54):** Added ability to remove dashboard tiles in edit mode with empty tile slot support and occupancy grid.
- **Per-widget colors (FR-55):** Added per-widget color customization with color range presets for speedometer and VU meter styles.
- **Minimize to tray (FR-52):** Added a minimize-to-tray option in Settings; macOS hides dock icon when minimized.
- **Tray icon style selector (FR-48):** Added 3 monochrome tray icon SVG variants with a style combo box and live preview in Settings.

### Fixed
- **BUG-44:** Redesigned Settings page with grouped QGroupBox sections.
- **BUG-62:** Dashboard supports empty tile slots with proper resize and drag-drop handling for multi-cell tiles.
- **BUG-63:** Dashboard floating buttons re-raised after z-order disruptions.
- **BUG-64:** Synced QPalette with theme tokens and fixed Settings page QSS cascade; forced Fusion style for QComboBox popup theming on macOS.
- **BUG-65:** Converted schedule indicator to floating overlay on System Cleaner page.
- **BUG-66, BUG-67:** Improved text centering and font sizing in speedometer, gauge, and hybrid tiles.
- **BUG-68:** Reserved space for speedometer tick labels to prevent clipping.

## [2.0.2] - 2026-02-22

_This release was tagged after v2.1.3 in the repository history; it consolidates the full UI redesign, architecture overhaul, and all fixes from the v1.x and early v2.1.x line into a single rebased release. See [2.0.0] and [2.0.1] for the detailed breakdown of those changes._

### Added
- **Tray icon style selector (FR-48):** Added 3 monochrome tray icon SVG variants (light, dark, colored) with a style combo box and live preview in Settings.

## [2.1.3] - 2026-02-13

### Added
- **Dev Tool Caches category (FR-03):** Added Electron app caches and dev tool paths to System Cleaner, with sort dropdown label fixes (**BUG-19**) and consistent back button icon (**BUG-20**).
- **Disk Usage Analyzer launcher (FR-23):** Replaced the File System pie chart with an external disk analyzer tool launcher on the Resources page.
- **Configurable disk analyzer (FR-24):** Added a Settings combobox to choose the preferred disk usage analysis tool.

### Fixed
- **BUG-05:** Background threads now wait for completion on exit via `waitForDone()`.
- **BUG-21:** Applied theme styling to the Homebrew tree view.

## [2.1.2] - 2026-02-11

### Fixed
- **BUG-02:** System Cleaner now empties directory contents instead of deleting entire directories.
- **BUG-18:** Settings version label uses dynamic version from CMake instead of a hardcoded string.

## [2.1.1] - 2026-02-11

### Fixed
- **BUG-17:** Replaced defunct feedback form with GitHub Issues launcher.
- Updated splashscreen reference to version-independent filename.

## [2.1.0] - 2026-02-10

### Added
- **macOS platform support:** Added macOS support via `#ifdef` abstraction layer with unified cross-platform CMake build. Includes macOS thermal sensors, CPU speed, dashboard layout, theme system, app icon, and `.app` bundle packaging.
- **GPU monitoring (FR-11):** Added Dashboard CircleBar and Resources history chart for GPU utilization with multi-GPU selector.
- **macOS .app bundle uninstaller (FR-21):** Scans `/Applications`, parses `Info.plist`, moves to Trash.
- **Homebrew page rework (FR-22):** Grouped Formula/Cask sections with multi-select tree widget.

### Fixed
- **BUG-12:** Added icon fallback for System Cleaner on macOS.
- **BUG-13:** Fixed macOS sidebar icons, Homebrew integration, System Cleaner category icons (Adwaita SVGs), and NVIDIA GPU utilization reading.
- **BUG-14:** Fixed NVIDIA GPU utilization using PCI bus ID instead of DRM card index.
- **BUG-15, BUG-16:** Fixed Uninstaller Homebrew paths and added package descriptions via JSON API on macOS.
- Fixed macOS release re-signing of app bundle after `macdeployqt`.
- Derived app version from `git describe` at build time.

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
