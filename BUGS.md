# Bugs

> Managed by Claude Code. Updated across sessions.
> Status: `[ ]` = open, `[~]` = in progress, `[x]` = fixed
> Severity: HIGH, MEDIUM, LOW

## HIGH Severity

- [x] **BUG-01: Memory info calculation — swapped variables** (HIGH)
  - **File:** `linux/stacer-core/Info/memory_info.cpp:33-34`
  - **Description:** `sreclaimable` and `shmem` are assigned to the wrong indices when parsing `/proc/meminfo`. `Shmem` is at index 6 and `SReclaimable` at index 7, but the code assigns them backwards, causing incorrect memory usage display.
  - **Upstream:** [#535](https://github.com/oguzhaninan/Stacer/issues/535), [#525](https://github.com/oguzhaninan/Stacer/issues/525)
  - **Fix complexity:** Trivial (swap two lines)
  - **Resolved:** Swapped assignments so shmem=index 6 and sreclaimable=index 7

- [ ] **BUG-02: System Cleaner deletes entire directories with `rm -rf`** (HIGH)
  - **File:** `linux/stacer/Pages/SystemCleaner/system_cleaner_page.cpp:229`
  - **Description:** `getAppCaches()` returns both files and directories (`QDir::Dirs`), and the cleaner calls `sudo rm -rf` on them. This deletes entire log/cache directories rather than just their contents, breaking services like Apache2/Nginx that need the directory to exist.
  - **Upstream:** [#548](https://github.com/oguzhaninan/Stacer/issues/548), [#459](https://github.com/oguzhaninan/Stacer/issues/459)
  - **Fix complexity:** Moderate (change deletion logic to empty contents, not remove directories)

- [x] **BUG-03: No single-instance enforcement** (HIGH)
  - **File:** `shared/stacer/main.cpp`
  - **Description:** No `QLockFile`, `QSharedMemory`, or any mechanism to prevent multiple instances. Duplicate launches cause race conditions, especially dangerous for `/etc/hosts` editing.
  - **Upstream:** [#274](https://github.com/oguzhaninan/Stacer/issues/274)
  - **Fix complexity:** Moderate (standard Qt single-instance pattern)
  - **Resolved:** Added QLockFile in main.cpp with warning dialog on duplicate launch

## MEDIUM Severity

- [x] **BUG-04: CPU speed shows 0 GHz on modern kernels** (MEDIUM)
  - **Files:** `linux/stacer-core/Info/cpu_info.cpp:74-101`, `shared/stacer/Pages/Dashboard/dashboard_page.cpp:150-176`
  - **Description:** Code reads "cpu MHz" from `/proc/cpuinfo`, which modern kernels don't populate. Falls back to `lscpu` but that can also fail. Should use `/sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq`. Dashboard degrades to showing only `%`.
  - **Upstream:** [#409](https://github.com/oguzhaninan/Stacer/issues/409)
  - **Fix complexity:** Moderate (add sysfs fallback path)
  - **Resolved:** Added sysfs cpufreq fallback in both cpu_info.cpp and system_info.cpp

- [ ] **BUG-05: Background threads not cleaned up on exit** (MEDIUM)
  - **Files:** `shared/stacer/Pages/Uninstaller/uninstaller_page.cpp:40-51,238-246`, `shared/stacer/app.cpp:118-123`
  - **Description:** `QtConcurrent::run()` calls discard `QFuture` objects. `closeEvent()` calls `qApp->quit()` without waiting for threads. App processes linger after close; in-progress package uninstalls may be interrupted.
  - **Upstream:** [QuentiumYT #18](https://github.com/QuentiumYT/Stacer/issues/18), [QuentiumYT #26](https://github.com/QuentiumYT/Stacer/pull/26)
  - **Fix complexity:** Moderate (store QFuture objects, wait in destructor)

- [ ] **BUG-06: Slow startup with large /etc/hosts file** (MEDIUM)
  - **File:** `shared/stacer/Pages/Helpers/host_manage.cpp:57,62-100`
  - **Description:** Entire hosts file is read and parsed into UI model at startup with no lazy loading or pagination. Systems with large hosts files (ad-blockers, Pi-hole exports with 10,000+ entries) experience UI freezing.
  - **Upstream:** [#492](https://github.com/oguzhaninan/Stacer/issues/492)
  - **Fix complexity:** Moderate (defer loading, add pagination or virtual scrolling)

## LOW Severity

- [ ] **BUG-07: HiDPI / 4K scaling issues** (LOW)
  - **Scope:** UI-wide (QWidget-based)
  - **Description:** QWidget UI doesn't scale properly on HiDPI displays. Text truncation, garbled service lists on 4K monitors. Full fix requires QML migration.
  - **Upstream:** [#111](https://github.com/oguzhaninan/Stacer/issues/111), [#482](https://github.com/oguzhaninan/Stacer/issues/482)
  - **Fix complexity:** Hard (architectural — would need QML migration)

- [ ] **BUG-08: Wayland compatibility** (LOW)
  - **Scope:** Platform / Qt level
  - **Description:** App fails to launch with `QT_QPA_PLATFORM=wayland`.
  - **Upstream:** [#494](https://github.com/oguzhaninan/Stacer/issues/494)
  - **Fix complexity:** Moderate

- [x] **BUG-09: Non-English locale parsing failures** (LOW)
  - **File:** `linux/stacer-core/Info/cpu_info.cpp` and other system command parsers
  - **Description:** Commands like `lscpu` output localized text, but code filters for English strings (`"^CPU MHz"`). Fails on non-English systems. Fixed in QuentiumYT fork with `LC_ALL=C`.
  - **Fix complexity:** Trivial (prefix commands with `LC_ALL=C`)
  - **Resolved:** Changed LANG=C to LC_ALL=C in cpu_info.cpp and system_info.cpp

- [ ] **BUG-10: Memory leak in System Cleaner** (LOW)
  - **Scope:** System Cleaner page
  - **Description:** Long-running sessions see memory grow from ~150MB to 2GB+ due to improper C++ memory management in the cleaner component.
  - **Upstream:** [#229](https://github.com/oguzhaninan/Stacer/issues/229)
  - **Fix complexity:** Moderate (audit and fix object lifecycle)

- [x] **BUG-11: macOS crash on launch — double CFRelease in GPU detection** (HIGH)
  - **File:** `macos/stacer-core/Info/gpu_info.cpp:71-84`
  - **Description:** In `detectVendor()`, when `vendorRef` is a `CFDataRef` with length >= 2, the code calls `CFRelease(vendorRef)` at line 77 then falls through to a second `CFRelease(vendorRef)` at line 83 if the vendor ID doesn't match any known value (AMD/NVIDIA/Intel). The double-free triggers `EXC_BREAKPOINT` in `CoreFoundation::CF_IS_OBJC`, crashing the app immediately on startup.
  - **Fix complexity:** Trivial (add early return after first CFRelease to prevent fallthrough)
  - **Resolved:** Added `return "Unknown"` after the vendor-ID checks to prevent double CFRelease

- [x] **BUG-12: Missing icon fallback for mDefaultIcon on macOS** (LOW)
  - **Files:** `macos/stacer/Pages/SystemCleaner/system_cleaner_page.cpp:16`, `linux/stacer/Pages/SystemCleaner/system_cleaner_page.cpp:16`
  - **Description:** `mDefaultIcon` used `QIcon::fromTheme("application-x-executable")` with no fallback argument. On macOS (and Linux systems without a full icon theme), this returns a null icon, causing blank icons in the System Cleaner tree view entries.
  - **Fix complexity:** Trivial (add bundled fallback icon as second argument)
  - **Resolved:** Added `QIcon(":/static/themes/common/img/package.png")` as fallback on both platforms

## Notes

<!-- Claude Code: append new bugs here. Use the next available BUG-XX id. -->
