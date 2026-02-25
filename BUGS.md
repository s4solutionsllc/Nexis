# Bugs

> Managed by Claude Code. Updated across sessions.
> Status: `[ ]` = open, `[~]` = in progress, `[x]` = fixed
> Severity: HIGH, MEDIUM, LOW

## HIGH Severity

- [x] **BUG-01: Memory info calculation — swapped variables** (HIGH)
  - **File:** `linux/nexis-core/Info/memory_info.cpp:33-34`
  - **Description:** `sreclaimable` and `shmem` are assigned to the wrong indices when parsing `/proc/meminfo`. `Shmem` is at index 6 and `SReclaimable` at index 7, but the code assigns them backwards, causing incorrect memory usage display.
  - **Upstream:** [#535](https://github.com/oguzhaninan/Stacer/issues/535), [#525](https://github.com/oguzhaninan/Stacer/issues/525)
  - **Fix complexity:** Trivial (swap two lines)
  - **Resolved:** Swapped assignments so shmem=index 6 and sreclaimable=index 7

- [x] **BUG-02: System Cleaner deletes entire directories with `rm -rf`** (HIGH)
  - **File:** `linux/nexis/Pages/SystemCleaner/system_cleaner_page.cpp:229`
  - **Description:** `getAppCaches()` returns both files and directories (`QDir::Dirs`), and the cleaner calls `sudo rm -rf` on them. This deletes entire log/cache directories rather than just their contents, breaking services like Apache2/Nginx that need the directory to exist.
  - **Upstream:** [#548](https://github.com/oguzhaninan/Stacer/issues/548), [#459](https://github.com/oguzhaninan/Stacer/issues/459)
  - **Fix complexity:** Moderate (change deletion logic to empty contents, not remove directories)
  - **Resolved:** systemClean() now partitions paths into files vs directories; directories are emptied (contents removed) while preserving the directory itself

- [x] **BUG-03: No single-instance enforcement** (HIGH)
  - **File:** `shared/nexis/main.cpp`
  - **Description:** No `QLockFile`, `QSharedMemory`, or any mechanism to prevent multiple instances. Duplicate launches cause race conditions, especially dangerous for `/etc/hosts` editing.
  - **Upstream:** [#274](https://github.com/oguzhaninan/Stacer/issues/274)
  - **Fix complexity:** Moderate (standard Qt single-instance pattern)
  - **Resolved:** Added QLockFile in main.cpp with warning dialog on duplicate launch

## MEDIUM Severity

- [x] **BUG-04: CPU speed shows 0 GHz on modern kernels** (MEDIUM)
  - **Files:** `linux/nexis-core/Info/cpu_info.cpp:74-101`, `shared/nexis/Pages/Dashboard/dashboard_page.cpp:150-176`
  - **Description:** Code reads "cpu MHz" from `/proc/cpuinfo`, which modern kernels don't populate. Falls back to `lscpu` but that can also fail. Should use `/sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq`. Dashboard degrades to showing only `%`.
  - **Upstream:** [#409](https://github.com/oguzhaninan/Stacer/issues/409)
  - **Fix complexity:** Moderate (add sysfs fallback path)
  - **Resolved:** Added sysfs cpufreq fallback in both cpu_info.cpp and system_info.cpp

- [x] **BUG-05: Background threads not cleaned up on exit** (MEDIUM)
  - **Files:** `shared/nexis/Pages/Uninstaller/uninstaller_page.cpp:40-51,238-246`, `shared/nexis/app.cpp:118-123`
  - **Description:** `QtConcurrent::run()` calls discard `QFuture` objects. `closeEvent()` calls `qApp->quit()` without waiting for threads. App processes linger after close; in-progress package uninstalls may be interrupted.
  - **Upstream:** [QuentiumYT #18](https://github.com/QuentiumYT/Stacer/issues/18), [QuentiumYT #26](https://github.com/QuentiumYT/Stacer/pull/26)
  - **Fix complexity:** Moderate (store QFuture objects, wait in destructor)
  - **Resolved:** closeEvent() now calls QThreadPool::globalInstance()->waitForDone() before quitting; QFuture objects stored in UninstallerPage and SystemCleanerPage

- [x] **BUG-06: Slow startup with large /etc/hosts file** (MEDIUM)
  - **File:** `shared/nexis/Pages/Helpers/host_manage.cpp:57,62-100`
  - **Description:** Entire hosts file is read and parsed into UI model at startup with no lazy loading or pagination. Systems with large hosts files (ad-blockers, Pi-hole exports with 10,000+ entries) experience UI freezing.
  - **Upstream:** [#492](https://github.com/oguzhaninan/Stacer/issues/492)
  - **Fix complexity:** Moderate (defer loading, add pagination or virtual scrolling)
  - **Resolved:** Four fixes: (1) Deferred loading — file is only read when user navigates to Helpers page via `loadIfNeeded()` with `mLoaded` flag, eliminating startup impact entirely. (2) Batched model population — `blockSignals(true)` and `setDynamicSortFilter(false)` during bulk `appendRow()` loop, single `invalidate()`+`reset()` after. (3) Pre-compiled regex — `QRegularExpression("\\s+")` is now `static const`, compiled once. (4) Incremental updates — add/edit/delete operations modify only the affected model row instead of calling `loadTableData()` to rebuild everything.

- [x] **BUG-44: Settings page layout issues — inconsistent column count and minimum size too large** (MEDIUM)
  - **Scope:** Settings page
  - **Description:** The Settings page has layout problems that need a complete UI review: (1) Most rows use a 4-column grid layout, but the row ending with "Disk Health Alert" uses 5 columns, breaking alignment. (2) The page enforces a minimum size that prevents the application window from being reduced past a certain point, which is too restrictive. A full audit of the Settings page grid layout is needed to ensure consistent column counts, proper stretch factors, and reasonable minimum size constraints.
  - **Files:** `shared/nexis/Pages/Settings/settings_page.ui`, `shared/nexis/Pages/Settings/settings_page.cpp`
  - **Fix complexity:** Moderate (audit and restructure grid layout, fix column consistency, adjust minimum size policies)
  - **Resolved:** Complete layout redesign. Replaced flat 6-column QGridLayout with QScrollArea containing 5 QGroupBox card sections (General, Appearance, Alerts, Tools, Scheduled Cleaning). Each group uses a consistent 2-column label/control grid. Moved 7 programmatically-created Scheduled Cleaning widgets into the `.ui` file, eliminating fragile grid manipulation code from `initScheduledCleaning()`. Added QGroupBox QSS rules matching HardwareInfo/GnomeSettings card pattern (`@cardBg` background, `@borderColor` border, 12px radius). Checkboxes now use inline text labels. Minimum width reduced from ~1100px to ~450px. Page scrolls on small screens.

- [x] **BUG-64: Settings page combo boxes, buttons, and modals not respecting dark/light theme consistently** (MEDIUM)
  - **Files:** `shared/nexis/Pages/Settings/settings_page.cpp`, `shared/nexis/static/themes/default/style/style.qss`
  - **Description:** Multiple theming deficiencies: (1) SettingsPage has no `sigChangedAppTheme` listener beyond a credit-link lambda — no `refreshThemeColors()` method exists, (2) QComboBox dropdown popups may use macOS native Cocoa rendering that ignores QSS `QAbstractItemView` rules (no `setStyle("Fusion")` in app), (3) programmatic widgets (`settingsGroup`, `btnResetDashboardLayout`) lack QSS-matchable selectors, (4) modal dialogs (`manageSchedulesDialog`, `cleaningHistoryDialog`) rely on generic `QDialog` QSS but child QGroupBox/QPlainTextEdit widgets have no dialog-specific rules.
  - **Fix complexity:** Moderate (add dialog-specific QSS rules + theme listener; macOS combo popup may need platform-specific fix)
  - **Resolved:** Added `QDialog QGroupBox` and `QDialog QGroupBox::title` QSS rules for themed card styling in all dialogs. Added `setObjectName("scheduleEditorDialog")` to ScheduleEditorDialog. Added `refreshThemeColors()` method to SettingsPage connected to `sigChangedAppTheme` to refresh drop shadows on theme change. Note: macOS native QComboBox popup rendering may still require `setStyle("Fusion")` as a future enhancement.

## LOW Severity

- [x] **BUG-63: Mouse cursor not activating consistently over Edit Dashboard and Kiosk Mode icons** (LOW)
  - **Files:** `shared/nexis/Pages/Dashboard/dashboard_page.cpp` (`buildGrid()`, `exitEditMode()`, `onKioskModeChanged()`)
  - **Description:** The floating `QPushButton` icons (`mEditButton`, `mKioskButton`) are `raise()`-ed once at init, but `buildGrid()` reparents tile wrappers with `setParent(this)` + `show()`, pushing them above the buttons in z-order. `buildGrid()` is called 6 times post-init (reset, rebuild, drag, resize) without re-raising the buttons. Additionally, `exitEditMode()` and `onKioskModeChanged()` call `show()` without `raise()`.
  - **Fix complexity:** Trivial (add `raise()` calls after `buildGrid()` and show/hide transitions)
  - **Resolved:** Added `mEditButton->raise()` and `mKioskButton->raise()` at end of `buildGrid()`, in `exitEditMode()` after `mKioskButton->show()`, and in `onKioskModeChanged()` after `mEditButton->show()`.

- [x] **BUG-07: HiDPI / 4K scaling issues** (LOW)
  - **Scope:** UI-wide (QWidget-based)
  - **Description:** QWidget UI doesn't scale properly on HiDPI displays. Text truncation, garbled service lists on 4K monitors. Full fix requires QML migration.
  - **Upstream:** [#111](https://github.com/oguzhaninan/Stacer/issues/111), [#482](https://github.com/oguzhaninan/Stacer/issues/482)
  - **Fix complexity:** Hard (architectural — would need QML migration)
  - **Resolved:** Lightweight 3-pronged fix without QML migration: (1) Created `Dpi::scale()` utility class (`dpi.h`) that scales pixel values by `devicePixelRatio()` — applied to 12 C++ files covering headers, icons, margins, and chart offsets. (2) Extended QSS token system with `@dpN` tokens replaced at stylesheet load time — ~80 structural pixel values in `style.qss` converted. (3) Relaxed `.ui` file `maximumSize` constraints on 6 files (service items, startup apps, linebar, system cleaner icons/buttons) to allow layout-managed scaling.

- [x] **BUG-08: Wayland compatibility** (LOW)
  - **Scope:** Platform / Qt level
  - **Description:** App fails to launch with `QT_QPA_PLATFORM=wayland`.
  - **Upstream:** [#494](https://github.com/oguzhaninan/Stacer/issues/494)
  - **Fix complexity:** Moderate
  - **Resolved:** Guarded all 3 `primaryScreen()` call sites against null (the crash cause on Wayland where screen info arrives asynchronously). Replaced `raise()`/`activateWindow()` with `windowHandle()->requestActivate()` which uses the `xdg-activation` protocol on Wayland compositors.

- [x] **BUG-09: Non-English locale parsing failures** (LOW)
  - **File:** `linux/nexis-core/Info/cpu_info.cpp` and other system command parsers
  - **Description:** Commands like `lscpu` output localized text, but code filters for English strings (`"^CPU MHz"`). Fails on non-English systems. Fixed in QuentiumYT fork with `LC_ALL=C`.
  - **Fix complexity:** Trivial (prefix commands with `LC_ALL=C`)
  - **Resolved:** Changed LANG=C to LC_ALL=C in cpu_info.cpp and system_info.cpp

- [x] **BUG-10: Memory leak in System Cleaner** (LOW)
  - **Scope:** System Cleaner page
  - **Description:** Long-running sessions see memory grow from ~150MB to 2GB+ due to improper C++ memory management in the cleaner component.
  - **Upstream:** [#229](https://github.com/oguzhaninan/Stacer/issues/229)
  - **Fix complexity:** Moderate (audit and fix object lifecycle)
  - **Resolved:** Five fixes: (1) Added `mScanInProgress`/`mCleanInProgress` guards to prevent concurrent workers racing on shared QFileInfoList members — the primary cause of unbounded growth. (2) Destructor now calls `mWorkerFuture.waitForFinished()` before `delete ui` to prevent use-after-free. (3) Scoped SignalMapper lambda with `this` context for auto-disconnect. (4) Clear scan result lists after tree is built to release QFileInfo storage between scans. (5) Replaced redundant `FileUtil::getFileSize()` recursive directory traversals in `onCleanFinished()` with sum of already-stored `SortRole` data from tree items.

- [x] **BUG-11: macOS crash on launch — double CFRelease in GPU detection** (HIGH)
  - **File:** `macos/nexis-core/Info/gpu_info.cpp:71-84`
  - **Description:** In `detectVendor()`, when `vendorRef` is a `CFDataRef` with length >= 2, the code calls `CFRelease(vendorRef)` at line 77 then falls through to a second `CFRelease(vendorRef)` at line 83 if the vendor ID doesn't match any known value (AMD/NVIDIA/Intel). The double-free triggers `EXC_BREAKPOINT` in `CoreFoundation::CF_IS_OBJC`, crashing the app immediately on startup.
  - **Fix complexity:** Trivial (add early return after first CFRelease to prevent fallthrough)
  - **Resolved:** Added `return "Unknown"` after the vendor-ID checks to prevent double CFRelease

- [x] **BUG-12: Missing icon fallback for mDefaultIcon on macOS** (LOW)
  - **Files:** `macos/nexis/Pages/SystemCleaner/system_cleaner_page.cpp:16`, `linux/nexis/Pages/SystemCleaner/system_cleaner_page.cpp:16`
  - **Description:** `mDefaultIcon` used `QIcon::fromTheme("application-x-executable")` with no fallback argument. On macOS (and Linux systems without a full icon theme), this returns a null icon, causing blank icons in the System Cleaner tree view entries.
  - **Fix complexity:** Trivial (add bundled fallback icon as second argument)
  - **Resolved:** Added `QIcon(":/static/themes/common/img/package.png")` as fallback on both platforms

- [x] **BUG-13: Sidebar icons use fallback PNGs on macOS instead of Adwaita theme** (LOW)
  - **Files:** `shared/nexis/main.cpp`, `shared/nexis/app.cpp`
  - **Description:** `QIcon::fromTheme()` can't find Homebrew-installed Adwaita icons on macOS because Qt's icon theme search paths don't include `/opt/homebrew/share/icons` or `/usr/local/share/icons`. The `XDG_DATA_DIRS` environment variable is empty on macOS, so Qt falls back to basic bundled PNG silhouettes instead of the proper Adwaita theme icons.
  - **Fix complexity:** Trivial (add Homebrew icon paths to `QIcon::setThemeSearchPaths()`)
  - **Resolved:** Added macOS-specific search paths in main.cpp; fixed 2 icon names missing from Adwaita; deleted orphan light theme stylesheet

- [x] **BUG-14: NVIDIA GPU utilization always 0% — wrong device index** (LOW)
  - **File:** `linux/nexis-core/Info/gpu_info.cpp:100-108,148-163`
  - **Description:** `discoverGpus()` used the DRM card index (e.g. `card1` → index 1) as the nvidia-smi `--id=` parameter, but nvidia-smi uses its own 0-based enumeration. On systems where card0 is a simple framebuffer and card1 is the NVIDIA GPU, `--id=1` queries a non-existent device, returning nothing. Fix: use the PCI bus ID (e.g. `0000:07:00.0`) which nvidia-smi always resolves correctly.
  - **Fix complexity:** Trivial (extract PCI bus ID from device symlink, pass to nvidia-smi)
  - **Resolved:** Use PCI bus ID from `/sys/class/drm/cardN/device` symlink instead of DRM card index

- [x] **BUG-15: Uninstaller fails to find brew on macOS — PATH not set in GUI apps** (MEDIUM)
  - **File:** `macos/nexis-core/Tools/package_tool.cpp:7-9`
  - **Description:** `PackageTool::currentPackageTool` uses `CommandUtil::isExecutable("brew")` which relies on the shell PATH, but macOS GUI apps don't inherit the user's shell PATH. This causes `currentPackageTool` to be `UNKNOWN`, making the Uninstaller page show 0 packages. All `CommandUtil::exec("brew", ...)` calls throughout the file have the same problem.
  - **Fix complexity:** Trivial (use `findBrew()` pattern with absolute paths, same as apt_source_tool.cpp)
  - **Resolved:** Added `findBrew()` with well-known Homebrew binary paths; used absolute paths in all exec calls

- [x] **BUG-16: Uninstaller shows no descriptions for Homebrew packages** (LOW)
  - **File:** `macos/nexis-core/Tools/package_tool.cpp:26-61`
  - **Description:** `getHomebrewPackages()` uses `brew list --formula -1` and `brew list --cask -1` which only return package names with no metadata. All packages display with empty descriptions.
  - **Fix complexity:** Moderate (switch to `brew info --json=v2 --installed` for rich metadata)
  - **Resolved:** Rewrote to use `brew info --json=v2 --installed` JSON parsing with name + description for formulae and human-friendly name + description for casks

- [x] **BUG-17: Feedback form sends user data to defunct upstream Heroku endpoint** (MEDIUM)
  - **File:** `shared/nexis/feedback.cpp:19`
  - **Description:** The feedback dialog collects name, email, and message, then POSTs via `curl` to `https://stacer-web-api.herokuapp.com/feedback` — the original upstream author's Heroku backend. Heroku free tier was retired in 2022, so the endpoint is dead. Even if alive, data would go to the wrong party.
  - **Fix complexity:** Moderate (replace with GitHub Issues launcher)
  - **Resolved:** Replaced feedback form with quick-link dialog that opens GitHub Issues templates (bug report, feature request, general feedback). No user data collected or transmitted.

- [x] **BUG-18: Settings page version label hardcoded to v2.0.1** (LOW)
  - **File:** `shared/nexis/Pages/Settings/settings_page.ui:236`
  - **Description:** The `lblCreatedBy` label in the Settings page contained a hardcoded version string `v2.0.1` that was never updated. The version should be set dynamically from `qApp->applicationVersion()`, which is already populated from the cmake-derived `APP_VERSION` macro.
  - **Fix complexity:** Trivial (set label text in constructor using qApp->applicationVersion())
  - **Resolved:** Removed hardcoded version from .ui file; both platform settings_page.cpp files now set lblCreatedBy text dynamically

- [x] **BUG-19: System Cleaner sort dropdown shows duplicate "Name" and "Size" entries** (LOW)
  - **File:** `shared/nexis/Pages/SystemCleaner/system_cleaner_page.ui`
  - **Description:** The sort-by combobox contained four items labelled "Name", "Name", "Size", "Size" — differentiated only by tiny asc/dsc PNG arrow icons from the default theme. Users see two identical "Name" and two identical "Size" entries with no indication of sort direction. Additionally the icons used theme-specific PNGs (`asc.png`/`dsc.png`) instead of the common SVGs (`sort-asc.svg`/`sort-dsc.svg`) used elsewhere.
  - **Fix complexity:** Trivial (rename labels to include direction, swap PNGs for common SVGs)
  - **Resolved:** Labels changed to "Name (A–Z)", "Name (Z–A)", "Size (Small–Large)", "Size (Large–Small)"; icons switched to common/img/sort-asc.svg and sort-dsc.svg

- [x] **BUG-20: System Cleaner back button uses theme-specific PNG inconsistent with app styling** (LOW)
  - **File:** `shared/nexis/Pages/SystemCleaner/system_cleaner_page.ui`
  - **Description:** The "Back" button on the scan results page used `back.png` from the default theme — a bright blue circled arrow that clashes with the subtle grey Adwaita-style SVG icons used throughout the rest of the app. No `chevron-left.svg` existed in the common theme.
  - **Fix complexity:** Trivial (create matching SVG, update .ui reference)
  - **Resolved:** Created `chevron-left.svg` mirroring the existing `chevron-right.svg` style (#77767b stroke); updated button icon reference to use the common SVG at 12×12

- [x] **BUG-21: Homebrew repo manager tree view ignores dark theme — white background** (LOW)
  - **File:** `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp:47`
  - **Description:** The `QTreeWidget` created programmatically for the Homebrew package list had no `objectName`, so the QSS selectors for `#treeWidgetPackages` (which set `background-color: transparent`, themed item colors, etc.) never applied. The tree view kept its default white background in dark mode.
  - **Fix complexity:** Trivial (add `setObjectName("treeWidgetPackages")`)
  - **Resolved:** Added object name so existing QSS theme rules apply correctly

- [x] **BUG-23: Uninstaller and Homebrew tree views use card styling inconsistent with System Cleaner table layout** (LOW)
  - **Files:** `shared/nexis/static/themes/default/style/style.qss`, `shared/nexis/Pages/Uninstaller/uninstallerpage.ui`, `shared/nexis/Pages/Uninstaller/uninstaller_page.cpp`, `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp`
  - **Description:** The Uninstaller and Homebrew (APT Source Manager) pages used a card-style layout for tree items (rounded corners, card background, no header, no row dividers) while the System Cleaner scan results used a clean bordered table with a visible header, row dividers, and flat item styling. This inconsistency made the grouped tree views look different across pages despite serving the same purpose.
  - **Fix complexity:** Moderate (restyle QSS, enable headers in .ui and code)
  - **Resolved:** Replaced card-style QSS rules with table-style rules matching `#treeWidgetScanResult`; enabled visible header with column labels; configured header height to 30px

- [x] **BUG-22: Uninstaller and APT Source Manager tree views have no expand/collapse indicator** (LOW)
  - **File:** `shared/nexis/static/themes/default/style/style.qss`
  - **Description:** The `#treeWidgetPackages` QSS selector set `::branch` to transparent background but had no `::branch:has-children` pseudo-state rules to show chevron icons. Users had no visual cue that section headers could be expanded or collapsed, making the grouped layout confusing. The System Cleaner tree (`#treeWidgetScanResult`) already had the correct chevron rules — this was just missing from the packages tree.
  - **Fix complexity:** Trivial (add QSS branch pseudo-state rules matching System Cleaner pattern)
  - **Resolved:** Added `::branch:closed:has-children` (chevron-right.svg) and `::branch:open:has-children` (chevron-down.svg) rules to `#treeWidgetPackages`

- [x] **BUG-24: YUM/DNF getPackageCaches() returns Pacman paths (copy-paste bug)** (MEDIUM)
  - **File:** `linux/nexis/Managers/tool_manager.cpp:91-93`
  - **Description:** The `YUM`/`DNF` case in `getPackageCaches()` called `PackageTool::getPacmanPackageCaches()` instead of a YUM/DNF cache function. On Fedora/RHEL systems, this returns incorrect results (Pacman paths that don't exist).
  - **Fix complexity:** Moderate (add new `getYumDnfPackageCaches()` method)
  - **Resolved:** Added `PackageTool::getYumDnfPackageCaches()` scanning `/var/cache/dnf/` and `/var/cache/yum/`

- [x] **BUG-25: CircleBar potential double-delete of QChart** (MEDIUM)
  - **File:** `shared/nexis/Pages/Dashboard/circlebar.cpp`
  - **Description:** `QChartView` constructor takes ownership of the `QChart*`, but the `CircleBar` destructor also called `delete mChart`. This is a potential double-free crash.
  - **Fix complexity:** Trivial (remove manual delete)
  - **Resolved:** Removed `delete mChart` from destructor; Qt parent-child ownership handles cleanup

- [x] **BUG-26: DiskInfo raw pointer ownership — Rule of Three violation** (MEDIUM)
  - **Files:** `shared/nexis-core/Info/disk_info.h`, `shared/nexis-core/Info/disk_info_shared.cpp`
  - **Description:** `QList<Disk*>` with `new`/`qDeleteAll` but no copy constructor or assignment operator. If `DiskInfo` is copied, double-free occurs. `Disk` is a plain struct with no polymorphism — heap allocation unnecessary.
  - **Fix complexity:** Moderate (change to value semantics, update all call sites)
  - **Resolved:** Changed to `QList<Disk>` with value semantics; updated DiskInfo, InfoManager, DashboardPage, and both SettingsPage files

- [x] **BUG-27: Linux /proc/meminfo no bounds checking** (MEDIUM)
  - **File:** `linux/nexis-core/Info/memory_info.cpp`
  - **Description:** After regex-filtering `/proc/meminfo`, code accesses `lines.at(0)` through `lines.at(7)` with no size check. If the kernel omits a line or the regex doesn't match all 8 expected entries, the app crashes with an out-of-bounds exception.
  - **Fix complexity:** Trivial (add guard clause)
  - **Resolved:** Added `lines.size() < 8` guard with early return and warning log

- [x] **BUG-28: quint8 core count overflow at 256 threads** (LOW)
  - **Files:** `linux/nexis-core/Info/cpu_info.cpp`, `macos/nexis-core/Info/cpu_info.cpp`
  - **Description:** `getCpuCoreCount()` used `static quint8 count` which maxes at 255. AMD EPYC 9004 has 256 threads — overflow to 0 would cause division-by-zero in per-core CPU calculations.
  - **Fix complexity:** Trivial (change to `int`)
  - **Resolved:** Changed `quint8` to `int` on both platforms

- [x] **BUG-29: toLong() truncation for 64-bit values** (LOW)
  - **Files:** `linux/nexis-core/Info/memory_info.cpp`, `linux/nexis-core/Info/network_info.cpp`, `linux/nexis-core/Info/process_info.cpp`, `macos/nexis-core/Info/process_info.cpp`
  - **Description:** `toLong()` returns 32-bit on 32-bit platforms. Memory sizes (shifted left by 10), network byte counters, RSS, and VSIZE can all exceed 2^31. Using `toLongLong()` makes the 64-bit intent explicit.
  - **Fix complexity:** Trivial (search-and-replace)
  - **Resolved:** Changed all `toLong()` to `toLongLong()` in memory, network, and process info files

- [x] **BUG-30: Review Phase 2 margin changes for unintended layout side-effects** (LOW)
  - **Scope:** UI-wide — margins adjusted during Phase 2 of `revision_plan.md`
  - **Description:** During Phase 2 (UI Consistency & Spacing), margin and spacing values were standardised across multiple pages. These changes should be visually reviewed on both macOS and Linux to confirm no layout regressions (clipped text, collapsed sections, excessive whitespace, etc.).
  - **Fix complexity:** Review-only (visual QA pass)
  - **Resolved:** Reverted all Phase 2.1 margin changes — restored original per-page margin values across 10 .ui files

- [x] **BUG-31: GNOME Settings silently fails when `gsettings set` errors** (MEDIUM)
  - **Scope:** GNOME Settings page — all 4 tabs
  - **Description:** When `gsettings set` fails (invalid value, permission denied, locked key), the error is only logged via `qCritical()`. The UI widget retains the new value, giving the user no indication the change didn't apply. Should revert the widget to the previous value and show a brief inline error or status bar message.
  - **Files:** `shared/nexis/Pages/GnomeSettings/gnome_appearance_tab.cpp`, `gnome_wm_tab.cpp`, `gnome_mouse_tab.cpp`, `gnome_desktop_tab.cpp`
  - **Fix complexity:** Moderate (wrap `GnomeSettingsTool::set*` calls with success check, revert widget on failure, add user-visible feedback)
  - **Resolved:** Added `CommandUtil::execWithStatus()` for exit code checking; changed `GnomeSettingsTool::set*()` to return bool; all ~49 tab lambdas now check return value, revert widgets with QSignalBlocker on failure, and emit `settingFailed` signal to show transient inline error via `GnomeSettingsPage::showError()`

- [x] **BUG-32: GNOME Settings speed sliders spawn subprocess per pixel of drag** (LOW)
  - **Scope:** GNOME Settings → Mouse & Touchpad tab
  - **Description:** The mouse and touchpad speed sliders connect `QSlider::valueChanged` directly to `GnomeSettingsTool::setD()`, which spawns a `gsettings set` subprocess. Dragging the slider fires `valueChanged` on every pixel of movement, potentially spawning hundreds of subprocesses in a single drag gesture. Should debounce writes with a `QTimer` (e.g., 200ms delay) so only the final value is written.
  - **Files:** `shared/nexis/Pages/GnomeSettings/gnome_mouse_tab.cpp`
  - **Fix complexity:** Trivial (add a `QTimer::singleShot` or a member `QTimer` with 200ms interval to debounce slider writes)
  - **Resolved:** Added two member `QTimer*` (mMouseSpeedTimer, mTouchpadSpeedTimer) with 200ms single-shot interval. Slider `valueChanged` now updates the label immediately for responsive UX but only restarts the debounce timer; the actual `gsettings set` call fires on timeout after 200ms of inactivity.

- [x] **BUG-33: Uninstaller "Purge" checkbox text invisible in dark mode** (LOW)
  - **Scope:** Uninstaller page
  - **Description:** The `chkPurge` QCheckBox ("Purge (also remove configuration files)") has no explicit text color in the QSS. The stylesheet defines `QCheckBox::indicator` styling (custom toggle images) but never sets a `color` property on `QCheckBox` itself. In dark mode, Qt falls back to the system palette default, which is typically black or very dark text — invisible against the dark background. The "circle" variant checkboxes (used elsewhere) work because they explicitly set `color: @color06`, but standard checkboxes like `chkPurge` are unstyled. Fix should add a `QCheckBox { color: @color05; }` rule to the QSS so all standard checkboxes use the theme's primary text color.
  - **Files:** `shared/nexis/static/themes/default/style/style.qss`
  - **Fix complexity:** Trivial (add a single QSS rule for QCheckBox text color)
  - **Resolved:** Added `QCheckBox { color: @color05; }` rule to style.qss so all standard checkboxes use the theme's primary text color.

- [x] **BUG-34: Settings page "Luke Simpson" link uses hardcoded blue instead of Nexis orange** (LOW)
  - **Scope:** Settings page
  - **Description:** The `lblCreatedBy` QLabel in `settings_page.cpp` builds an HTML string with an inline `color:#007af4` (blue) for the "Luke Simpson" GitHub profile link. This doesn't match the Nexis brand accent color (`@accentColor` = `#E95420`). The link color should be the Nexis orange (`#E95420`) with an orange hover (`#accentHover` = `#c64516`). Since QLabel rich text doesn't support CSS hover, the fix should use QSS `a` link styling on the label or switch to hardcoding the accent orange in the inline span style. Both the `.ui` default text (line ~240) and the dynamic `setText()` call in the constructor (line ~25) contain the blue color and need updating.
  - **Files:** `shared/nexis/Pages/Settings/settings_page.cpp`, `shared/nexis/Pages/Settings/settings_page.ui`
  - **Fix complexity:** Trivial (replace `#007af4` with `#E95420` in both the .cpp and .ui HTML strings)
  - **Resolved:** Replaced `#007af4` with `#E95420` (Nexis accent orange) in both the `.cpp` constructor setText call and the `.ui` default label text.

- [x] **BUG-35: Remove "Donate" button from Settings page until donation method is established** (LOW)
  - **Scope:** Settings page
  - **Description:** The Settings page has a `btnDonate` QPushButton whose click handler (`on_btnDonate_clicked`) simply opens the GitHub profile URL. There is no actual donation mechanism in place. The button should be removed from both the `.ui` layout and the `.cpp` slot until a real donation method is identified. Remove the widget from the UI file and delete the `on_btnDonate_clicked` slot implementation.
  - **Files:** `shared/nexis/Pages/Settings/settings_page.ui`, `shared/nexis/Pages/Settings/settings_page.cpp`, `shared/nexis/Pages/Settings/settings_page.h`
  - **Fix complexity:** Trivial (remove widget from .ui, delete slot from .cpp/.h)
  - **Resolved:** Removed `btnDonate` widget from .ui, deleted `on_btnDonate_clicked` slot from .cpp/.h, removed from drop shadow widget list, cleaned up unused QDesktopServices/QUrl includes.

- [x] **BUG-36: System Cleaner "Total Size" label text invisible in dark mode** (LOW)
  - **Scope:** System Cleaner page (scan results view)
  - **Description:** The `lblTotalBytes` QLabel ("Total size: X.XX MB") has no explicit color styling — no QSS rule for `#lblTotalBytes` and no inline stylesheet. In dark mode, Qt falls back to the default palette text color (typically black/dark), making the text invisible against the dark background. Other labels on the same page (e.g. `#cleanerCategories QLabel` and `#lblRemovedTotalSize`) have explicit color rules in the QSS, but `lblTotalBytes` was missed. Fix should add a `#lblTotalBytes { color: @color05; }` rule to the QSS (or group it with existing label selectors) so it uses the theme's primary text color.
  - **Files:** `shared/nexis/static/themes/default/style/style.qss`, `shared/nexis/Pages/SystemCleaner/system_cleaner_page.ui`
  - **Fix complexity:** Trivial (add a QSS color rule for `#lblTotalBytes`)
  - **Resolved:** Added `#lblTotalBytes { font-size: 11pt; color: @color05; }` rule to style.qss, matching the style of neighbouring System Cleaner labels.

- [x] **BUG-37: System Cleaner scanLoading.gif animation not playing and delayed initialisation** (MEDIUM)
  - **Scope:** System Cleaner page
  - **Description:** The `scanLoading.gif` loading spinner shown after clicking the Scan button has two issues: (1) **Animation doesn't play** — the `QMovie` is created and `start()`-ed inside the `sigChangedAppTheme` lambda (line 79), but on every theme change a **new** `QMovie` is allocated without deleting the previous one (memory leak), and the movie may have finished or stopped by the time the user clicks Scan. (2) **Delayed initialisation** — `mLoadingMovie` is `nullptr` until the first `sigChangedAppTheme` signal fires during `App::init()`. If the signal hasn't fired yet, showing the label displays nothing. The same issues apply to `mLoadingMovie_2` / `lblLoadingCleaner`. Fix should: pre-initialise both `QMovie` objects in the constructor with the default theme; on theme change, update the movie filename and restart instead of allocating new objects; ensure `start()` is called just before `show()` in `on_btnScan_clicked()` and `on_btnClean_clicked()` to guarantee the animation is actively running when the label becomes visible.
  - **Files:** `shared/nexis/Pages/SystemCleaner/system_cleaner_page.cpp`, `shared/nexis/Pages/SystemCleaner/system_cleaner_page.h`
  - **Fix complexity:** Moderate (restructure QMovie lifecycle, add start() calls at show-time)
  - **Resolved:** Pre-initialised both QMovie objects in the constructor; theme-change lambda now reuses them via `setFileName()` (fixing the memory leak); added `start()` before `show()` in both `on_btnScan_clicked()` and `on_btnClean_clicked()`; added `stop()` in `onScanFinished()`, `onCleanFinished()`, and `on_btnBackToCategories_clicked()`.

- [x] **BUG-38: HardwareInfoPage table rows illegible in dark mode — alternating row colours use system palette** (LOW)
  - **Scope:** Hardware Info page
  - **Description:** All 7 `QTableWidget` instances in `hardware_info_page.ui` have `alternatingRowColors` enabled, but no `alternate-background-color` is defined in the QSS. Qt falls back to the macOS system palette (light grey/white), creating light-background alternating rows with white (`@color05`) text — illegible. The page-specific QSS also sets `background-color: transparent` on items, which lets the alternating colour show through rather than overriding it. No other page in the app uses `alternatingRowColors`. The fix should disable `alternatingRowColors`, use opaque `@color01` item backgrounds (matching the global `QTableView::item` rule), and remove the page-specific `QHeaderView::section` override to inherit the complete global rule.
  - **Files:** `shared/nexis/Pages/HardwareInfo/hardware_info_page.ui`, `shared/nexis/static/themes/default/style/style.qss`
  - **Fix complexity:** Trivial (remove UI property, update QSS rules)
  - **Resolved:** Removed `alternatingRowColors` from all 7 tables, added `frameShape: NoFrame`, updated QSS item rule to use opaque `@color01` background matching global `QTableView::item`, removed page-specific `QHeaderView::section` override to inherit complete global rule.

- [x] **BUG-39: `getDesktopValue()` truncates Exec lines containing `=` (env variables)** (MEDIUM)
  - **File:** `shared/nexis/utilities.h:29-40`
  - **Description:** `getDesktopValue()` uses `split("=")` + `directive.last()` to extract values from `.desktop` file keys. When the value itself contains `=` (common with `Exec=env QT_QPA_PLATFORM=xcb /usr/bin/app`), `split("=")` produces multiple segments and `last()` returns only the portion after the final `=`, losing the env variable and everything before it. QuentiumYT fixed this in commit `77a9928` by using `section('=', 1)` which returns everything after the first `=`. The fix is a one-line change.
  - **Fix complexity:** Trivial (change `split("=")` + `last()` to `section('=', 1)`)
  - **Resolved:** Changed to `indexOf('=')` + `mid()` approach which correctly returns everything after the first `=`

- [x] **BUG-40: FR-16 UI regressions — layout position and theme compliance** (MEDIUM)
  - **Scope:** Settings page, System Cleaner page, Schedule Editor Dialog, Manage Schedules Dialog, Cleaning History Dialog
  - **Description:** The FR-16 (Scheduled/Automated Cleaning) implementation introduced UI elements with two categories of issues: (1) **Layout position** — the "Scheduled Cleaning" section in Settings is placed at rows 11–15 in the grid, which is *below* the `lblCreatedBy` footer at row 10. The footer should always be the final visual element. (2) **Theme compliance** — all programmatically created widgets use hardcoded colors (`"color: gray"`, `"color: #d4a017"`, `"color: red"`, `rgba(128,128,128,30)`) instead of the app's `@token`-based QSS theme system. These hardcoded values override the global stylesheet and do not adapt to Auto/Light/Dark mode. Additionally, new buttons lack `accessibleName` properties and drop shadows that existing Settings widgets use.
  - **Files:** `shared/nexis/Pages/Settings/settings_page.cpp`, `shared/nexis/Pages/SystemCleaner/schedule_editor_dialog.cpp`, `shared/nexis/Pages/SystemCleaner/system_cleaner_page.cpp`
  - **Fix complexity:** Moderate (reposition grid rows, replace hardcoded colors with theme tokens, add object names/accessible names)
  - **Resolved:** Repositioned Scheduled Cleaning section to rows 9–13 (above footer at row 15). Replaced all 8 hardcoded `setStyleSheet()` color calls with `setObjectName()`/`setProperty("accessibleName")` targeting new QSS rules using `@color06`, `@warningColor`, `@destructiveColor`, `@cardBg`, `@borderColor`. Added `accessibleName="dialog-title"` titles to all 3 dialogs. Applied `"primary"`/`"danger"` button styling to all action buttons including Manage Schedules and View History. Added drop shadows to new Settings widgets. Removed emoji icon from schedule indicator. Added `QCheckBox:focus` QSS rule to suppress platform-default purple focus ring.

- [x] **BUG-41: Manage Schedules dialog scroll area viewport renders white in dark mode** (LOW)
  - **Scope:** Settings page → Manage Schedules dialog
  - **Description:** The `QScrollArea` viewport inside the Manage Cleaning Schedules dialog renders with a white background in dark mode, despite the dialog itself being correctly themed via the `QDialog { background-color: @color01; }` QSS rule. The viewport widget (`QScrollArea`'s internal `QWidget`) ignores the dialog's stylesheet and paints with the system palette default. Attempts to fix via `setAutoFillBackground(false)`, `WA_TranslucentBackground`, and `QPalette` propagation from the dialog have all been ineffective.
  - **Files:** `shared/nexis/Pages/Settings/settings_page.cpp` (`onManageSchedules()`)
  - **Fix complexity:** Moderate (need to find an approach that actually overrides the viewport background)
  - **Resolved:** Used inline `setStyleSheet("background-color:transparent")` on both the QScrollArea and its content QWidget, plus `setFrameShape(QFrame::NoFrame)`. This matches the pattern used by HardwareInfoPage's QSS rules (`#HardwareInfoPage #scrollArea, #scrollContent { background-color: transparent; }`). Palette-based approaches failed due to timing/precedence conflicts between Qt's palette system and QSS; inline stylesheet transparency lets the dialog's QSS-themed `@color01` background show through naturally.

- [x] **BUG-42: GNOME Settings "Mouse & Touchpad" tab button renders as "Mouse_Touchpad" on Linux** (LOW)
  - **Scope:** GNOME Settings page
  - **File:** `shared/nexis/Pages/GnomeSettings/gnome_settings_page.ui:90`
  - **Description:** The tab button for Mouse & Touchpad settings displays as "Mouse_Touchpad" on Ubuntu because Qt interprets a single `&` in button text as a mnemonic prefix (underlines the next character). The `&amp;` in the XML becomes `&` which Qt treats as a keyboard shortcut indicator rather than a literal ampersand.
  - **Fix complexity:** Trivial (escape `&` as `&&` in the `.ui` file so Qt renders a literal `&`)
  - **Resolved:** Changed `&amp;` to `&amp;&amp;` in the `.ui` XML, which Qt interprets as a literal `&` character.

- [x] **BUG-43: Host Manager — multiple data integrity and security issues** (MEDIUM)
  - **Scope:** Helpers page → Host Manage dialog
  - **Files:** `shared/nexis/Pages/Helpers/host_manage.cpp`, `shared/nexis/Pages/Helpers/host_manage.h`
  - **Description:** The Host Manager (`/etc/hosts` editor) has several issues that should be investigated and addressed:
    1. **Predictable temp file (security):** Save writes to hardcoded `/tmp/nexis_etc_host_new_content` then `sudo mv` to `/etc/hosts`. Predictable path enables symlink attacks. Should use `QTemporaryFile`.
    2. **No error handling on save:** `FileUtil::writeFile()` return value is ignored; `sudoExec("mv", ...)` failure is only logged to `qDebug()` with no user feedback. In-memory and on-disk states can silently diverge.
    3. **Empty line placeholders on delete:** Deleted entries are replaced with empty strings instead of removed from the list, causing file bloat and potential line number mapping drift on subsequent edits before save.
    4. **Line number drift after mixed add/delete:** Adding entries uses `mHostFileContent.size()` for line numbers, but deletions leave empty slots, causing line number references to desync from actual file content.
    5. **No input validation:** IP addresses, FQDNs, and aliases are not validated — invalid formats are accepted and written to `/etc/hosts`, potentially breaking name resolution.
    6. **No backup before write:** No backup of original `/etc/hosts` is created before overwriting; if the save introduces errors, there's no recovery path.
    7. **No confirmation dialog:** "Save Changes" immediately writes to `/etc/hosts` with no confirmation or diff view.
  - **Fix complexity:** Moderate (multiple targeted fixes across the save path, deletion logic, and validation)
  - **Resolved:** Seven fixes: (1) Replaced predictable temp file + `sudo mv` with `sudo tee` stdin pipe (no temp file, no symlink risk, preserves file ownership). (2) Added error detection — verifies tee success via stdout echo, shows `QMessageBox::critical()` on failure (auth cancelled, permission denied), success message in `lblChangesMsg`. (3) Fixed deletion to use `removeAt()` + full model rebuild instead of empty-string placeholders — no more blank lines in saved file. (4) Added input validation: IPv4/IPv6 via `QHostAddress`, hostname via RFC 1123 regex (with underscore tolerance), per-alias validation, inline error messages. (5) Added backup: `sudo cp -p /etc/hosts /etc/hosts.nexis-backup` before each save (warns but doesn't block on failure). (6) Added confirmation dialog with change summary (N added, N modified, N deleted) and no-change detection. (7) Removed unused `isAddHost` member.

- [x] **BUG-45: Kiosk mode toggle button icons display in gray instead of Nexis orange** (LOW)
  - **Scope:** Dashboard page — kiosk mode toggle button (FR-30)
  - **Files:** `shared/nexis/static/themes/common/img/fullscreen.svg`, `shared/nexis/static/themes/common/img/fullscreen-exit.svg`
  - **Description:** The fullscreen/collapse SVG icons added in FR-30 use `stroke="#77767b"` (gray), matching the chevron convention. The icons should use the Nexis accent color (`#E95420` orange) to make the kiosk toggle button visually prominent and on-brand. The accent color is identical across both dark and light themes, so a hardcoded orange in the common directory works universally.
  - **Fix complexity:** Trivial (change stroke color in 2 SVG files)
  - **Resolved:** Changed stroke color from `#77767b` to `#E95420` in both SVGs.

- [x] **BUG-46: Kiosk mode "Press ESC" overlay not centered on monitor** (LOW)
  - **Scope:** Dashboard page → kiosk mode overlay
  - **File:** `shared/nexis/app.cpp:440-475` (`showKioskOverlay()`)
  - **Description:** When entering kiosk mode, the "Press ESC to exit kiosk mode" overlay label is not centered on the monitor. The positioning code uses `mSlidingStacked->x()` and `mSlidingStacked->y()` as offsets and centers within that widget's bounds, but since the overlay is parented to `App` (the main window), those coordinates place it relative to the stacked widget's position within the window — not the center of the actual screen/window. In fullscreen mode the sidebar is hidden, but the offset calculation still factors in the stacked widget's geometry rather than centering on the full window. The overlay should be centered on the entire window (or screen) so it appears in the middle of the monitor regardless of internal widget layout.
  - **Fix complexity:** Trivial (center relative to the full window geometry instead of `mSlidingStacked`)
  - **Resolved:** Replaced `mSlidingStacked`-relative positioning with `QScreen::geometry()` centering, which is immediately available (no async `showFullScreen()` timing issue) and multi-monitor safe.

- [x] **BUG-47: Theme not fully applied when switching via Appearance dropdown** (MEDIUM)
  - **Scope:** App-wide — Dashboard tiles, Resources page, Hardware Info page, Settings page, Command Palette
  - **Description:** When the user switches themes via Settings → Appearance (Dark/Light/Auto), many widgets don't fully update their colors. Root causes: (1) Dashboard tiles (MetricTile, NetworkTile, DiskTile) bake `QColor` values at construction time from theme tokens but never re-resolve them on theme change. (2) `sigChangedAppTheme` listeners are incomplete — e.g., MetricTile only updates chart background, not sparkline pen, area fill, progress bar, or action button colors. (3) DiskTile has no theme listener at all. (4) Inline `setStyleSheet()` calls with hardcoded hex colors (`#2ec27e`, `#77767b`, `#E05454`, `#6B6E78`, `#E95420`) override the global QSS and don't respond to theme changes. (5) CommandPalette shadow color is hardcoded. (6) HardwareInfoPage health verdict table foreground colors are hardcoded.
  - **Files:** `metric_tile.h/.cpp`, `network_tile.h/.cpp`, `disk_tile.h/.cpp`, `dashboard_page.cpp`, `disk_usage_launcher_widget.cpp`, `hardware_info_page.h/.cpp`, `settings_page.cpp`, `command_palette.h/.cpp`, `default/style/values.ini`, `light/style/values.ini`
  - **Fix complexity:** Moderate (add `refreshThemeColors()` methods to all affected widgets, change constructors to accept token names instead of resolved QColor, expand existing theme listeners, replace all hardcoded colors with token lookups)
  - **Resolved:** Eliminated all hardcoded colors across 12 files. Added 24 new theme tokens to both values.ini files. All widgets now resolve colors from theme tokens at runtime via `refreshThemeColors()` methods connected to `sigChangedAppTheme`.

- [x] **BUG-48: Qt resources not loaded — Q_INIT_RESOURCE missing for static library** (HIGH)
  - **Scope:** App-wide — all `:/` resource paths (QSS, icons, fonts, images)
  - **Files:** `shared/nexis/main.cpp`, `CMakeLists.txt`
  - **Description:** The `static.qrc` resource file is compiled into the `nexis-gui` static library, but the final `nexis` executable never explicitly calls `Q_INIT_RESOURCE(static)`. The linker dead-strips the `qrc_static.cpp.o` object file because no other code references its symbols. Result: all `:/` resource paths return empty at runtime — no stylesheet (empty `qApp->setStyleSheet("")`), no icons, no fonts, no images. This is the root cause of font and theme switching appearing broken (the stylesheet was empty, so changing settings had no visible effect).
  - **Fix complexity:** Trivial (add `Q_INIT_RESOURCE(static)` before `QApplication` construction in `main.cpp`)
  - **Resolved:** Added `Q_INIT_RESOURCE(static)` to `main.cpp`

- [x] **BUG-49: Token replacement ordering causes substring collisions in QSS** (MEDIUM)
  - **Scope:** Theme engine — `AppManager::updateStylesheet()`
  - **File:** `shared/nexis/Managers/app_manager.cpp`
  - **Description:** `QSettings::allKeys()` returns keys in alphabetical order. When replacing `@sidebar` before `@sidebarDivider`, the shorter key matches inside the longer one, producing mangled values like `#EDE7E0Divider` instead of the correct color. Same issue with `@cardBg` vs `@cardBgElevated` and `@accentBgTint` vs `@accentColor`/`@accentHover`. The fix is to sort keys by length (longest first) before replacement.
  - **Fix complexity:** Trivial (sort allKeys by descending length)
  - **Resolved:** Added `std::sort` by descending length before the token replacement loop

- [x] **BUG-50: Command palette "Toggle Theme" uses wrong SettingManager methods** (LOW)
  - **Scope:** Command Palette → Toggle Theme action
  - **File:** `shared/nexis/app.cpp`
  - **Description:** The "Toggle Theme" command used `getThemeName()`/`setThemeName()` which read/write the legacy `ThemeName` key. But `AppManager::resolveThemeName()` reads `getColorScheme()`. The toggle had no effect because it wrote to a key that nothing reads.
  - **Fix complexity:** Trivial (switch to `getColorScheme()`/`setColorScheme()`)
  - **Resolved:** Changed to use `getColorScheme()`/`setColorScheme()` with `"dark"`/`"light"` values; removed redundant `sigChangedAppTheme` emit (already done by `updateStylesheet()`)

- [x] **BUG-51: Disk tile percentage text invisible in dark mode — QPainter uses system palette** (LOW)
  - **Scope:** Dashboard page → Disk tile donut chart
  - **File:** `shared/nexis/Pages/Dashboard/disk_tile.cpp`
  - **Description:** The disk usage percentage text drawn in the center of the donut chart used `palette().color(QPalette::WindowText)` via `QPainter`, which returns the system palette color (dark text on macOS) rather than the QSS-themed color. This made the percentage invisible against the dark donut chart background. The CPU and Memory tiles don't have this issue because they use QLabels styled by QSS with `color: @color05`. DiskTile paints manually with `QPainter`, bypassing QSS entirely.
  - **Fix complexity:** Trivial (resolve `@color05` from theme values, use in `paintEvent()`)
  - **Resolved:** Added `QColor mTextColor` member resolved from `@color05` in `refreshThemeColors()`, replaced `palette().color(QPalette::WindowText)` with `mTextColor` in `paintEvent()`

- [x] **BUG-52: Sidebar collapse/expand icon renders black in dark mode** (LOW)
  - **Scope:** Sidebar → collapse/expand toggle button
  - **Files:** `shared/nexis/app.cpp`, `shared/nexis/app.h`
  - **Description:** The sidebar collapse/expand SVG icons have correct per-theme fills (`#ffffff` in dark, `#3d3846` in light), and the SVG pixmap data is correct (verified by pixel sampling). However, `QPushButton` on macOS Qt6 does not render the icon at all — a `grab()` of the rendered button shows all pixels as fully transparent `(0,0,0,0)`. The user sees the dark sidebar background (`#222228`) through the transparent button, appearing as a "black" icon. The root cause is that macOS Qt6's `QPushButton` icon painting fails for icon-only buttons with `background: transparent` and no text.
  - **Fix complexity:** Trivial (change widget type from QPushButton to QToolButton)
  - **Resolved:** Changed `mBtnSidebarToggle` from `QPushButton` to `QToolButton` with `setAutoRaise(true)`. QToolButton correctly renders SVG icons on macOS Qt6. Pixel grab confirms center pixel renders as white `(211,211,211,211)` premultiplied. Removed unnecessary `QPainter` recoloring — direct `QIcon(svgPath)` works with QToolButton, matching the pattern used by all other sidebar icons.

- [x] **BUG-53: Duplicate drive health entries on macOS — disk tile shows same SSD 4 times** (MEDIUM)
  - **Scope:** Dashboard page → Disk tile drive health row
  - **File:** `macos/nexis-core/Info/disk_health_info.cpp`
  - **Description:** `discoverDrives()` iterates every item in `diskutil list -plist`'s `WholeDisks` array without deduplication. On Apple Silicon Macs, a single internal SSD appears as 4+ device nodes (`disk0`–`disk3`) all with `BusProtocol = "Apple Fabric"` and the same `MediaName`. Mounted `.dmg` disk images also appear with `BusProtocol = "Disk Image"`. The only filter was `protocol.isEmpty()`, so all duplicates and disk images passed through, resulting in "Apple SSD: Good" displayed 4 times and "Disk Image: Critical" twice on a single-drive Mac.
  - **Fix complexity:** Trivial (add "Disk Image" protocol filter + deduplicate by model name)
  - **Resolved:** Added `protocol == "Disk Image"` to the existing filter, and model-based deduplication that skips drives whose `model` string already exists in `mDrives`.

- [x] **BUG-54: Gnome Settings Appearance tab missing QGroupBox containers** (LOW)
  - **Scope:** Gnome Settings page → Appearance tab
  - **File:** `shared/nexis/Pages/GnomeSettings/gnome_appearance_tab.ui`
  - **Description:** The Appearance tab uses a flat `QGridLayout` with all 17 settings at the same level, while the other three tabs (Window Manager, Mouse, Desktop) use `QGroupBox` containers to visually group related settings into titled card sections. This breaks visual consistency — the Appearance tab lacks the border/radius/background styling applied by the `#GnomeSettingsPage QGroupBox` QSS rules.
  - **Fix complexity:** Trivial (restructure UI file layout only, no C++ changes)
  - **Resolved:** Replaced flat grid with `QVBoxLayout` containing 4 QGroupBoxes: Themes (5 settings), Fonts (6 settings), Interface (2 settings), Clock & Status (4 settings).

- [x] **BUG-55: Dashboard cards lack visible borders and depth** (LOW)
  - **Scope:** Dashboard page
  - **Description:** Dashboard cards (HeroCard, MetricTile, DiskTile, NetworkTile) appear borderless and flat. `@borderColor` is too close to `@cardBg` in both themes (only ~8 RGB points difference in dark theme), and drop shadows use zero offset with low opacity (alpha=60, ~24%), producing no perceptible depth.
  - **Files:** `shared/nexis/static/themes/default/style/values.ini`, `shared/nexis/static/themes/light/style/values.ini`, `shared/nexis/utilities.h`, `shared/nexis/Pages/Dashboard/dashboard_page.cpp`
  - **Fix complexity:** Trivial (adjust color tokens and shadow parameters)
  - **Resolved:** Increased `@borderColor` contrast in both themes (dark: `#3A3D4A`→`#4A4D5A`, light: `#E8E2DB`→`#D0C9C0`), added 2px downward shadow offset, increased shadow alpha from 60 to 80.

- [x] **BUG-56: Navbar items centered instead of left-aligned after expanding from collapsed state** (MEDIUM)
  - **Scope:** Sidebar / navbar
  - **Description:** When the app is closed with the navbar collapsed and then reopened, the navbar correctly initializes in collapsed state. However, when the user expands the navbar, the nav item labels are centered instead of left-aligned as they should be. Items display correctly if the app is started with the navbar expanded. Root cause: `applySidebarCollapse()` only re-polished the parent `#sidebar` widget after changing its `collapsed` dynamic property, but Qt does not recursively re-polish children — so child `QPushButton` items retained the stale `text-align: center` from the `#sidebar[collapsed="true"]` QSS rule.
  - **Files:** `shared/nexis/app.cpp`
  - **Fix complexity:** Trivial (add child widget re-polish loop)
  - **Resolved:** Added `unpolish()`/`polish()` calls on all child sidebar buttons (`mListSidebarButtons`, `btnFeedback`, `mBtnSidebarToggle`) after the parent property change, forcing Qt to re-evaluate property-dependent QSS selectors.

- [x] **BUG-57: Disk selector shows virtual filesystems, snapshots, and loopbacks** (MEDIUM)
  - **Scope:** Dashboard page → Disk tile gear menu; also affects disk usage display
  - **Description:** `DiskInfo::updateDiskInfo()` in `disk_info_shared.cpp` calls `QStorageInfo::mountedVolumes()` with only an `isValid()` check — no filtering of filesystem type, mount path, or device characteristics. This causes the disk selector to include virtual filesystems (tmpfs, devfs, sysfs, proc), APFS snapshots, mounted DMG disk images, Snap loopback devices (`/dev/loop*`), Docker overlayfs, Flatpak squashfs, and network mounts. On a typical Ubuntu system with Snap packages, 20+ spurious entries appear. On macOS, Time Machine snapshots and installer DMGs clutter the list.
  - **Files:** `shared/nexis-core/Info/disk_info_shared.cpp`
  - **Fix complexity:** Moderate (add cross-platform filtering by filesystem type, device path, and mount path)
  - **Resolved:** Added `shouldIncludeDisk()` with 5 filter layers: (1) size > 0, (2) filesystem type exclusion list (tmpfs, squashfs, overlay, devtmpfs, cgroup, etc.), (3) device path filter (pseudo-device names, `/dev/loop*` Snap loopbacks), (4) mount path filter (`/snap/`), (5) macOS hidden system APFS volumes (`/System/Volumes/Preboot`, `Recovery`, `VM`, `Update`, etc.). Applied to `updateDiskInfo()`, `devices()`, and `fileSystemTypes()`.

- [x] **BUG-58: Search page button shows blank orange rectangle — icon path references .png but file is .svg** (LOW)
  - **Scope:** Search page
  - **Description:** The `btnSearchAdvance` button in `search_page.ui` references `:/static/themes/default/img/sidebar-icons/search.png` but the actual resource file is `search.svg`. Qt fails to load the icon, leaving a blank orange button.
  - **Files:** `shared/nexis/Pages/Search/search_page.ui`
  - **Fix complexity:** Trivial (change `.png` to `.svg` in the icon path)
  - **Resolved:** Updated icon path to `search.svg`.

- [x] **BUG-59: Search page shows "BETA version" label from upstream Stacer** (LOW)
  - **Scope:** Search page
  - **Description:** A `lblBetaInfo` QLabel at the bottom-right of the Search page displays "BETA version" in orange text — a leftover from the original Stacer project. Not applicable to Nexis.
  - **Files:** `shared/nexis/Pages/Search/search_page.ui`
  - **Fix complexity:** Trivial (remove the label widget from the .ui file)
  - **Resolved:** Removed `lblBetaInfo` widget and its grid item.

- [x] **BUG-60: Dashboard system summary shows "0 Bytes RAM" — memory not populated at init time** (LOW)
  - **Scope:** Dashboard page → System summary card
  - **Description:** The system summary card in the dashboard displays "0 Bytes RAM" because `buildSystemSummary()` calls `im->getMemTotal()` during `DashboardPage::init()`, before `DataRefreshService::start()` has triggered the first `updateMemoryInfo()`. The `memTotal` field is initialized to 0 in the `MemoryInfo` constructor and is never pre-populated. The `onMemoryUpdated()` slot updates the memory tile but never refreshes `mSummaryRam` or the summary label.
  - **Files:** `shared/nexis/Pages/Dashboard/dashboard_page.cpp`
  - **Fix complexity:** Trivial (update `mSummaryRam` in `onMemoryUpdated()` and refresh the summary label)
  - **Resolved:** Added guard in `onMemoryUpdated()` that updates `mSummaryRam` with the real total and calls `refreshSummaryColors()` on the first callback where `total > 0`.

- [x] **BUG-61: Disk tile shows health badges for all physical drives instead of selected disk only** (MEDIUM)
  - **Scope:** Dashboard page → Disk tile health badges
  - **Description:** `onDiskHealthUpdated()` called `mDiskTile->setDriveHealth()` for every physical drive in the health list, displaying health badges for all drives regardless of which volume was selected in the disk gear menu. On a multi-drive system, all drives' health info appeared simultaneously. The health badge should only show the physical drive hosting the currently selected volume.
  - **Files:** `shared/nexis/Pages/Dashboard/dashboard_page.h`, `shared/nexis/Pages/Dashboard/dashboard_page.cpp`, `shared/nexis/Pages/Dashboard/disk_tile.h`, `shared/nexis/Pages/Dashboard/disk_tile.cpp`
  - **Fix complexity:** Moderate (add volume-to-physical-drive matching, cache health data, update badge on selection change)
  - **Resolved:** Added `clearDriveHealth()` to DiskTile for removing existing badges. Added `updateDiskHealthBadge()` to DashboardPage that matches the selected volume's device path to a physical drive's health data using `extractBaseDevice()` (handles Linux SATA/NVMe partition stripping and macOS APFS synthesized volume paths). Falls back to the sole physical drive when only one exists (common on single-drive Macs). Called from `onDiskHealthUpdated()`, `onDiskUsageUpdated()`, and `onDiskSelected()`.

- [x] **BUG-62: Dashboard grid has no empty tile slot support — resize and drag-to-empty blocked** (MEDIUM)
  - **Scope:** Dashboard page — tile layout system (FR-51 implementation)
  - **Description:** The dashboard grid layout was implemented without any concept of empty cells. Every grid position is occupied by a real tile widget, so: (1) **Resize is blocked** — `onTileResizeRequested()` rejects expansion when adjacent cells are occupied, which is almost always the case since the default layout packs 4 tiles across row 0 wall-to-wall. Only 2 of ~28 possible resize operations succeed in the default 7-tile layout. (2) **Drag-to-empty is impossible** — `gridCellAtPos()` resolves positions by hit-testing tile widgets; empty space returns 0 (unresolvable). `onTileDragFinished()` discards drops on empty cells. (3) **QGridLayout collapses** empty rows/columns since no spacer/placeholder widgets are inserted. Root causes: no occupancy grid data structure, `buildGrid()` only places real tiles, serialization only stores tile entries (no grid dimensions or empty markers). This is also a prerequisite for FR-49 (customizable dashboard with widget visibility), which requires hiding tiles and having remaining tiles expand into freed space.
  - **Files:** `shared/nexis/Pages/Dashboard/dashboard_page.cpp` (`buildGrid()` L912-931, `gridCellAtPos()` L958-969, `onTileResizeRequested()` L1042-1066, `onTileDragFinished()` L998-1040), `shared/nexis/Pages/Dashboard/dashboard_tile_wrapper.cpp` (resize span L93-102)
  - **Fix complexity:** Moderate (add occupancy grid, placeholder widgets for empty cells, pixel-to-cell resolution, drag-to-empty support, tile displacement or manual-clear-first resize policy)
  - **Resolved:** Added fixed 4x4 occupancy grid (`mOccupancy[4][4]`) with `rebuildOccupancy()` and `regionIsFree()` helpers. `buildGrid()` fills empty cells with invisible placeholder widgets (dashed border in edit mode via `#dashPlaceholder` QSS). Rewrote `gridCellAtPos()` as arithmetic cell resolution (works on empty cells). `onTileDragFinished()` supports drag-to-empty (move) and swap (via occupancy grid tileId lookup for multi-cell tiles). `onTileResizeRequested()` simplified to single `regionIsFree()` call. Added bounds clamping to `deserializeLayout()`. Swap validity uses `regionIsFree()` for full row+col overflow checking.

- [x] **BUG-65: Schedule indicator displaces System Cleaner category grid layout** (MEDIUM)
  - **Scope:** System Cleaner page → scheduled cleanup indicator panel
  - **Files:** `shared/nexis/Pages/SystemCleaner/system_cleaner_page.cpp` (`initScheduleIndicator()`)
  - **Description:** The schedule indicator (`mScheduleIndicator` QFrame) is appended to page 0's `QGridLayout` via `pageLayout->addWidget()`. Since the page uses a `QGridLayout`, the indicator becomes a grid cell participant at row 12, pushing other rows and altering the vertical distribution of spacers and category icons. The indicator should overlay the page as a floating panel anchored to the bottom edge, without participating in the grid layout.
  - **Fix complexity:** Moderate (reparent indicator as overlay with manual geometry management or restructure page layout)
  - **Resolved:** Converted indicator to floating overlay parented to page 0 but not added to its grid layout. Positioned via `repositionScheduleIndicator()` called from `resizeEvent()` and `updateScheduleIndicator()`. Uses `raise()` for z-order. Auto-hides when stackedWidget switches to scan results page (child of page 0).

- [x] **BUG-66: CPU widget percentage text renders too low — hard to read** (LOW)
  - **Scope:** Dashboard page → CPU SpeedometerTile
  - **Description:** On the CPU dashboard widget (speedometer style), the percentage text (e.g., "23%") was positioned as a QLabel below the dial rather than inside it. The text appeared cramped at the bottom of the tile, making it difficult to read.
  - **Files:** `shared/nexis/Pages/Dashboard/speedometer_tile.cpp`, `speedometer_tile.h`
  - **Fix complexity:** Moderate (refactor from QLabel layout to QPainter rendering inside dial)
  - **Resolved:** Moved percentage and secondary text from QLabel-below-dial to QPainter-inside-dial rendering. Text now draws centered below the needle pivot point in the arc's bottom opening area. Hidden QLabels kept for text storage. Added display-mode-scaled font sizes with secondary font capped at 13px. Used `footerTop` from actual subtitle geometry instead of hardcoded `footerHeight=50`.

- [x] **BUG-67: Memory widget percentage text too low and subtext font too large** (LOW)
  - **Scope:** Dashboard page → Memory GaugeTile
  - **Description:** Two issues on the Memory dashboard widget (gauge style): (1) The percentage text was positioned at ~65% from top of inner arc (10% offset + 55% height rect + AlignBottom), well below visual center. (2) The subtext font scaled linearly with diameter (`diameter/12`), producing fonts up to 25px at large tile sizes — too large for strings like "10.2 GiB / 16 GiB".
  - **Files:** `shared/nexis/Pages/Dashboard/gauge_tile.cpp`, `shared/nexis/Pages/Dashboard/hybrid_tile.cpp`
  - **Fix complexity:** Moderate (rewrite text centering math + font capping)
  - **Resolved:** Replaced the 55%/45% split + 10% offset with QFontMetrics-based centering (calculate total text block height, center vertically within inner rect). Capped secondary font at 13px max. Added `QFontMetrics::elidedText()` safety net for overflow. Same fixes applied opportunistically to HybridTile which had similar asymmetric offset (`side/8` up, `side/6` down) and unbounded secondary font scaling (`side/7`).

- [x] **BUG-68: Speedometer tick labels (0,25,50,75,100) clip outside tile bounds at certain window sizes** (MEDIUM)
  - **Scope:** Dashboard page → SpeedometerTile widget
  - **Description:** The speedometer widget's tick mark labels (0, 25, 50, 75, 100) sometimes render outside the tile's visible bounds, making them unreadable. The "25" (top) and "100" (bottom) labels are worst affected. The issue is intermittent and depends on the application window size — at certain resize points the labels clip. Root cause appears to be that `dialSize` is calculated from the available space without accounting for the extra radial extent of tick marks and labels that extend beyond the arc.
  - **Files:** `shared/nexis/Pages/Dashboard/speedometer_tile.cpp`
  - **Fix complexity:** Moderate (adjust dial sizing to reserve space for tick labels)
  - **Resolved:** When tick labels are shown (Large/Hero display modes), the paintEvent now computes the radial extent of tick marks + font metrics and subtracts it from dialSize before rendering. The dial shrinks to fit labels within tile bounds at all window sizes.

- [x] **BUG-69: GPU and Temperature tiles may display data for wrong selection** (MEDIUM)
  - **Scope:** Dashboard page — GPU and Temperature tiles
  - **Description:** User reports that the GPU and Temperature dashboard tiles appear to display data for a different sensor/device than what is selected in the gear menu or combo box. Two contributing factors identified: (1) **No subtitle feedback** — Neither tile shows which sensor/device is active. Temp tile displays "TEMP" + value, GPU tile displays "GPU" + value, with no subtitle indicating the selected sensor/device name. Users must open the gear menu to verify their selection, creating ambiguity. Compare with CPU/Memory/Battery tiles which all show descriptive subtitles. (2) **macOS GPU iteration order assumption** — `GpuInfoMacOS::updateGpuInfo()` re-enumerates IOAccelerators via `IOServiceGetMatchingServices` and writes utilization to `mDevices[idx]` by iteration position, assuming the same order as `discoverGpus()`. On multi-GPU Intel Macs with dynamic GPU switching, IOKit iteration order could change, causing utilization to be assigned to the wrong device. Linux GPU is not affected (uses per-device stored sysfs paths). Additional minor issues: silent fallback to index 0 when saved sensor/device ID not found; macOS thermal list composition can vary between launches; stateless temp signal requires cached index to remain valid.
  - **Files:** `shared/nexis/Pages/Dashboard/dashboard_page.cpp` (updateTempTile, onGpuUpdated, init), `macos/nexis-core/Info/gpu_info.cpp` (updateGpuInfo), `shared/nexis/Managers/data_refresh_service.cpp`
  - **Fix complexity:** Moderate (add subtitles to tiles, fix macOS GPU update to match by IORegistry ID or model name)
  - **Resolved:** Three fixes: (1) Added subtitles to Temp and GPU tiles showing the selected sensor label / device name, matching the CPU/Memory/Battery tile patterns. Subtitle updates on init from saved settings and on gear menu / combo selection change. (2) Refactored `GpuInfoMacOS::updateGpuInfo()` to match IOAccelerators by model name instead of iteration position — extracted `readUtilization()` helper, iterate and find matching `mDevices` entry by name. Single-GPU fallback when name matching is redundant. (3) Added `qWarning()` when saved sensor/device ID is not found in current hardware enumeration, making the silent fallback to index 0 traceable in logs.

- [x] **BUG-70: Fan monitoring broken — no refresh signal + limited detection paths** (MEDIUM)
  - **Scope:** Dashboard fan tile, Hardware Info fans section, fan sensor detection
  - **Description:** Two issues: (1) DataRefreshService had no `fanUpdated()` signal — the fan tile piggybacked on `tempUpdated()`, which only fires when thermal sensors exist. On systems with fans but no thermal sensors, fan RPM was never refreshed. (2) `FanInfoLinux::discoverSensors()` only scanned `/sys/class/hwmon/*/fan*_input`, missing systems where fan data is available through alternative paths (ThinkPad `/proc/acpi/ibm/fan`, Dell `/proc/i8k`, NVIDIA proprietary `nvidia-smi`).
  - **Files:** `data_refresh_service.h/.cpp`, `fan_info.h`, `fan_info_linux.h/.cpp`, `dashboard_page.cpp`
  - **Fix complexity:** Moderate (new signal + multi-source fan detection with dispatch)
  - **Resolved:** Added `fanUpdated()` signal to DataRefreshService, emitted independently in `onFastTick()` gated on `hasFanSensors()`. Connected dashboard fan tile to `fanUpdated()` instead of `tempUpdated()`. Added `FanSourceType` enum (Hwmon, ThinkpadProc, DellProc, NvidiaSmi) to `FanSensor` struct. Refactored `FanInfoLinux` with fallback detection chain: hwmon (primary) → ThinkPad procfs → Dell procfs → nvidia-smi. `getFanSpeed()` dispatches to source-specific read methods.

- [x] **BUG-71: Dashboard sparkline/trend not cleared when switching GPU, temp, or fan sensor** (MEDIUM)
  - **Scope:** Dashboard page — GPU, Temperature, Fan tiles (all platforms)
  - **Description:** When the user switches GPU device (combo box), temperature sensor (gear menu), or fan sensor (gear menu), the tile's `mDataBuffer` (sparkline history, up to 60 data points) is not cleared. The tile retains the previous device/sensor's historical data. The CircleBar/gauge value updates immediately to the new selection, but the sparkline graph and trend arrow continue reflecting the old device's pattern until enough new data points push it out. This makes it appear that the tile is "still showing the old device's data." Affects all tile types (MetricTile, GaugeTile, RingTile, etc.) since they all inherit `mDataBuffer` from `MetricTileBase`.
  - **Files:** `shared/nexis/Pages/Dashboard/metric_tile_base.h/.cpp`, `metric_tile.h/.cpp`, `hybrid_tile.h/.cpp`, `dashboard_page.cpp`
  - **Resolved:** Added virtual `clearDataPoints()` to `MetricTileBase` (resets `mDataBuffer` to zeros). Overridden in `MetricTile` and `HybridTile` to also rebuild the QLineSeries sparkline and trend indicator. Called in `onGpuDeviceChanged()`, `onTempSensorSelected()`, and `onFanSensorSelected()` before updating with new device data.

## Notes

<!-- Claude Code: append new bugs here. Use the next available BUG-XX id. -->
