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

## LOW Severity

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
  - **Resolved:** Repositioned Scheduled Cleaning section to rows 9–13 (above footer at row 15). Replaced all 8 hardcoded `setStyleSheet()` color calls with `setObjectName()`/`setProperty("accessibleName")` targeting new QSS rules using `@color06`, `@warningColor`, `@destructiveColor`, `@cardBg`, `@borderColor`. Added `accessibleName="dialog-title"` titles to all 3 dialogs. Applied `"primary"`/`"danger"` button styling. Added drop shadows to new Settings widgets. Removed emoji icon from schedule indicator.

## Notes

<!-- Claude Code: append new bugs here. Use the next available BUG-XX id. -->
