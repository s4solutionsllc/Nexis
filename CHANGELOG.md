# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
- **Crowdin translation integration (FR-08):** Automated PR workflows for 25 languages.
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
