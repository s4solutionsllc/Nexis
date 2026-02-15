# Nexis Revision Plan

> A phased plan to restructure the Nexis codebase for efficiency, consistency, and extensibility. Each phase is designed to be independently committable and testable. Changes are ordered to minimize risk: safe cleanups first, then structural consolidation, then behavioral fixes.

---

## Guiding Principles

1. **Build must pass after every phase.** Each phase ends with `cmake --build build` succeeding. If a phase touches multiple files, all edits within that phase are atomic.
2. **No functional regressions.** Every phase preserves existing behavior unless explicitly fixing a bug. Visual changes (margins, icons) are intentional and noted.
3. **Shared code over duplicated code.** The project's `shared/ + platform/` architecture is sound but underutilized. The goal is to push as much code as possible into `shared/`, leaving only genuinely platform-specific logic in `macos/` and `linux/`.
4. **Bundled assets over system theme lookups.** The app should look identical on macOS and Linux. All icons should come from the Qt resource system, never from the host's icon theme.
5. **Archive, don't delete.** Assets removed from the build (QRC, code references) are moved to `archive/` at the project root, preserving the original directory structure under `archive/assets/`. The `archive/` folder is excluded from the build and can be pruned by the owner at their leisure. Add `archive/` to `.gitignore` if it should not be tracked, or commit it for historical preservation — owner's choice.
6. **One concern per commit.** Each phase maps to one or a small number of focused commits to keep git history reviewable.

---

## Phase 0 — Repository Hygiene

**Goal:** Remove dead weight from the repository before touching any code. Zero risk to the build.

### 0.1 Remove stale `output/` directory from git tracking

**Files:** `output/` (261 tracked files)

**What:** Run `git rm -r --cached output/` and commit. The `.gitignore` already has `output/`, so these files won't come back.

**Why:** These are old build artifacts from the original Stacer project, still using `stacer` naming. They bloat the repository (compiled binaries, `.o` files, MOC/UIC generated code) and confuse any contributor who clones the repo. They serve no purpose — the real build output goes to `build/`.

### 0.2 Clean up `.gitignore`

**File:** `.gitignore`

**What:**
- Remove `Nexis.pro.*` line (vestigial from qmake era, no `.pro` file exists)
- Uncomment or remove `#*.AppImage` (decide: if AppImages should be ignored, uncomment; if not, delete the line)

**Why:** Reduces confusion for contributors. Dead ignore rules suggest the project still uses tools it doesn't.

### 0.3 Remove orphaned image assets from QRC

**Files:** `shared/nexis/static.qrc`, and the actual image files

**What:** Remove from QRC and move the files to `archive/assets/`:
- `back.png` (default theme) — replaced by `chevron-left.svg` in BUG-20
- `logo.png` — only `logo.svg` is referenced in code
- `c_package.png`, `c_crash.png`, `c_logs.png`, `c_cache.png`, `c_trash.png` — only the SVG versions are used

**Why:** Orphaned assets inflate the compiled binary (they're baked into the Qt resource system at build time). Every unused image adds to app launch time and memory footprint. Removing them from QRC prevents compilation into the binary while moving to `archive/` preserves them for future reference.

**Not removing:** `search.png`, `refresh.png`, `asc.png`, `dsc.png` — these are oversized (1024x1024) and unused, but they are candidates for resizing and potential future use. Flag for review in Phase 8.

---

## Phase 1 — Bug Fixes

**Goal:** Fix confirmed bugs that could cause crashes or incorrect behavior. These are isolated fixes with no structural dependencies.

### 1.1 Fix YUM/DNF `getPackageCaches()` copy-paste bug (BUG-A)

**File:** `linux/nexis/Managers/tool_manager.cpp` lines 92-94

**What:** The `YUM`/`DNF` case in `getPackageCaches()` currently calls `PackageTool::getPacmanPackageCaches()`. Change to call the correct method for YUM/DNF package caches. If no dedicated method exists, add one to `linux/nexis-core/Tools/package_tool.cpp` that scans the correct paths (`/var/cache/yum/`, `/var/cache/dnf/`).

**Why:** This is a copy-paste error from upstream Stacer. On Fedora/RHEL/CentOS systems using YUM or DNF, the package cache listing returns incorrect results (pacman paths that don't exist on those distros). This is a silent data corruption bug — it doesn't crash, but it shows wrong information.

**Risk:** Low. Only affects YUM/DNF codepath which is not exercised on macOS or Debian-based systems. Testable on a Fedora VM.

### 1.2 Fix CircleBar potential double-delete (BUG-B)

**File:** `shared/nexis/Pages/Dashboard/circlebar.cpp`

**What:** In Qt6, `QChartView::setChart(chart)` takes ownership of the chart. The destructor's `delete mChart` then double-frees. Fix by removing the manual `delete mChart` from the destructor and letting the `QChartView` (which is a child of the `CircleBar` widget) handle cleanup through Qt's parent-child ownership.

**Why:** This is a potential crash. In practice it may not trigger because Qt's parent-child cleanup order often disposes the chart view before the CircleBar destructor runs, but the behavior is undefined and fragile. Any change to widget destruction order (e.g., a future refactor) could trigger a double-free segfault.

**Risk:** Low. Removing a `delete` is a safe operation. The only risk is if Qt6 does NOT take ownership, in which case we'd leak the chart — but Qt6 documentation confirms `setChart()` transfers ownership.

### 1.3 Fix `DiskInfo` raw pointer ownership (BUG-C)

**Files:** `shared/nexis-core/Info/disk_info.h`, `shared/nexis-core/Info/disk_info.cpp`

**What:** Change `QList<Disk*>` to `QList<Disk>`. Replace `new Disk()` allocations in `updateDiskInfo()` with stack construction. Remove `qDeleteAll(disks)` calls from destructor and `updateDiskInfo()`. `Disk` is a plain struct with no polymorphism — there is no reason for heap allocation.

**Why:** The current code violates the Rule of Three: no copy constructor or assignment operator, but uses raw `new`/`delete`. If `DiskInfo` is ever copied (even accidentally by a container resize), both copies point to the same `Disk*` objects, leading to double-free. Switching to value semantics eliminates this entire class of bugs.

**Risk:** Low-Medium. Need to update any code that dereferences `Disk*` pointers (change `->` to `.`). Grep for all call sites.

### 1.4 Fix Linux `/proc/meminfo` bounds checking (BUG-D)

**File:** `linux/nexis-core/Info/memory_info.cpp`

**What:** Add a size check before accessing `lines.at(0)` through `lines.at(7)`. If the filtered list has fewer than 8 entries, log a warning and return gracefully with zero values rather than crashing.

**Why:** The current code applies a regex filter to `/proc/meminfo` and then accesses 8 hardcoded indices with no bounds check. If the kernel omits a line, reorders them, or the regex doesn't match all expected lines, the app crashes with an out-of-bounds exception.

**Risk:** Low. Adding a guard clause cannot break working code.

### 1.5 Fix `quint8` core count overflow (BUG-E)

**Files:** macOS and Linux `cpu_info.cpp`

**What:** Change `static quint8 count = 0` to `static int count = 0` in `getCpuCoreCount()`.

**Why:** `quint8` maxes at 255. AMD EPYC 9004 series has 256 threads. When the count overflows, it wraps to 0, which would cause division-by-zero errors in per-core CPU percentage calculations.

**Risk:** Trivial. `int` is a superset of `quint8` for all valid core counts.

### 1.6 Fix `toLong()` truncation for 64-bit values (BUG-F)

**Files:** Multiple core files (memory_info.cpp, network_info.cpp, process_info.cpp on both platforms)

**What:** Replace `toLong()` with `toLongLong()` for values that can exceed 2^31: memory sizes, network byte counters, PID, RSS, VSIZE.

**Why:** On 32-bit platforms (or 32-bit compat mode), `long` is 32 bits. A system with 4GB+ RAM, or network byte counters that have exceeded ~2GB since boot, would overflow. Even on 64-bit platforms where `long` is 64 bits, using `toLongLong()` makes the intent explicit and future-proof.

**Risk:** Trivial. `toLongLong()` returns the same value as `toLong()` for all values that fit in `long`.

---

## Phase 2 — Page Layout Normalization

**Goal:** Normalize all page layouts to a consistent zero-margin convention. This is a visual-only change with no logic impact.

### 2.1 Zero all page margins

**Files:** 12 `.ui` files (page-level layouts only, not sub-components)

**What:** Set `leftMargin`, `topMargin`, `rightMargin`, `bottomMargin` all to `0` on the root layout of every page:

| File | Current Margins | Target |
|------|----------------|--------|
| `dashboard_page.ui` | 5/5/5/5 | 0/0/0/0 |
| `system_cleaner_page.ui` | 15/0/15/15 | 0/0/0/0 |
| `services_page.ui` | 30/0/30/25 | 0/0/0/0 |
| `processes_page.ui` | 20/5/20/20 | 0/0/0/0 |
| `resources_page.ui` | 10/0/10/10 | 0/0/0/0 |
| `helpers_page.ui` | 15/10/15/10 | 0/0/0/0 |
| `search_page.ui` | 15/15/15/15 | 0/0/0/0 |
| `settings_page.ui` | 12/12/12/12 | 0/0/0/0 |
| `gnome_settings_page.ui` | 12/12/12/12 | 0/0/0/0 |
| `uninstallerpage.ui` | 30/5/30/20 | 0/0/0/0 |

**Not changing:** Sub-component `.ui` files (circlebar.ui, linebar.ui, service_item.ui, startup_app.ui, etc.) already have 0/0/0/0 or have margins that are internal to the component. Also not changing dialog `.ui` files (feedback.ui, apt_source_edit.ui, startup_app_edit.ui) because dialogs are standalone windows where margins create appropriate inset.

**Why:** You specified "margin conventions should be 0 0 0." Zero margins let the page content fill the available space edge-to-edge within the sidebar's content area. The `SlidingStackedWidget` container provides the structural boundary — individual pages should not add their own insets. If specific elements need breathing room, that should be handled by the elements themselves (via QSS padding or inner layouts), not by the page-level layout. This establishes a single consistent rule: **pages have zero margins, always.**

**Risk:** Low-Medium. The visual result will be that content shifts slightly outward on most pages. Some pages may look too tight without their previous padding — this would need a visual review after the change. If specific pages need inner padding, it should be added via QSS on the page's content container rather than the root layout margin.

---

## Phase 3 — Dead Code Removal

**Goal:** Remove code that is never executed, never referenced, or has been superseded. This reduces cognitive load for future development and eliminates potential confusion.

### 3.1 Clean up `package_tool_shared.h`

**File:** `shared/nexis-core/Tools/package_tool_shared.h`

**What:**
- Remove `static PackageTools currentPackageTool;` — this file-scope static in a header creates a separate uninitialized copy in every translation unit. The real `currentPackageTool` is the class static member in each platform's `package_tool.h`.
- Remove the `static` qualifier from the `friendlySectionName()` declaration — or remove it entirely since the platform `PackageTool` classes define their own method (see Phase 4 for consolidation).

**Why:** A `static` variable in a header is a C++ anti-pattern. It does not create a shared global — it creates N independent copies (one per `.cpp` file that includes the header). These copies are silently wasting memory and are never used because callsites use `PackageTool::currentPackageTool` and `PackageTool::friendlySectionName()` from the platform-specific class. This is confusing for anyone reading the code.

### 3.2 Remove dead functions

**Files and functions to remove:**

| File | Function | Reason |
|------|----------|--------|
| `macos/nexis-core/Tools/service_tool.cpp` | `discoverPlists()` | Static function defined but never called. `getServicesWithSystemctl()` uses `launchctl list` instead. |
| `shared/nexis/Pages/Resources/resources_page.h` | `chartColors` member | Declared but never used. `HistoryChart` has its own color palette. |
| `shared/nexis/Pages/Resources/history_chart.cpp` | `setSeriesList()` | No-op method — just calls `repaint()` without updating data. No call sites use it meaningfully. |
| `shared/nexis/Managers/app_manager.cpp` | Commented-out `loadThemeList()`/`getThemeList()` | Dead code since theme loading was rewritten. Commented-out code should not persist in the codebase; use git history for archaeology. |

**Why:** Dead code is a maintenance burden. It must be read and understood during code reviews, it can confuse developers into thinking it's used, and it can mask real bugs (e.g., if someone calls `setSeriesList()` expecting it to work). Removing it makes the codebase smaller and more trustworthy.

**Risk:** Low. Grep for all references before removing to confirm zero callers.

### 3.3 Guard platform-specific shared header declarations

**File:** `shared/nexis/Pages/StartupApps/startup_app_edit.h`

**What:**
- Wrap `buildPlistContent()` declaration in `#ifdef Q_OS_MACOS` so Linux doesn't need a dead stub.
- Consider wrapping the XDG `.desktop` regex macros (`NAME_REG`, `COMMENT_REG`, `EXEC_REG`, etc.) in `#ifndef Q_OS_MACOS` since they're only used on Linux.

**Then:** Remove the dead `buildPlistContent()` stub from `linux/nexis/Pages/StartupApps/startup_app_edit.cpp`.

**Why:** The shared header currently mixes macOS-specific (plist) and Linux-specific (desktop file) concerns. This forces each platform to provide stubs for the other's methods. The `#ifdef` guards make the header self-documenting about which parts apply to which platform and eliminate the need for dead stubs.

**Risk:** Low. The `#ifdef` approach is already used extensively in the codebase (e.g., `CMakeLists.txt`, `tool_manager`, `app.cpp`).

---

## Phase 4 — Core Library Consolidation

**Goal:** Eliminate duplicated code in `nexis-core` by moving shared logic into `shared/nexis-core/`. This is the foundation for Phase 5's UI consolidation.

### 4.1 Consolidate `friendlySectionName()`

**Files:**
- `shared/nexis-core/Tools/package_tool_shared.cpp` (standalone function)
- `macos/nexis-core/Tools/package_tool.cpp` (`PackageTool::friendlySectionName()`)
- `linux/nexis-core/Tools/package_tool.cpp` (`PackageTool::friendlySectionName()`)

**What:** Keep a single implementation in `shared/nexis-core/Tools/package_tool_shared.cpp`. Make it a proper `PackageTool::friendlySectionName()` static method (declared in the shared header). Remove the duplicate implementations from both platform files. Add the two macOS-specific entries (`"applications"`, `"user-applications"`) to the shared version (they're harmless on Linux — they just won't match).

**Why:** Three copies of a ~50-line hash table is the most egregious duplication in the codebase. Any time a new section name mapping is needed, it must be added in three places. The shared version already exists but is unused because each platform has its own copy.

**Risk:** Low. The function is pure (input → output, no side effects). As long as the shared version includes all entries from both platforms, behavior is identical.

### 4.2 Consolidate `findBrew()` on macOS

**Files:**
- `macos/nexis-core/Tools/apt_source_tool.cpp`
- `macos/nexis-core/Tools/package_tool.cpp`

**What:** Extract `findBrew()` to a shared macOS utility (e.g., `macos/nexis-core/Utils/brew_util.h` with an inline function, or add to an existing macOS utility file). Both `apt_source_tool.cpp` and `package_tool.cpp` call the shared version.

**Why:** Identical 8-line function in two files. If Homebrew changes its install path (e.g., a future Homebrew version), the fix must be made in two places. Single source of truth.

**Risk:** Trivial.

### 4.3 Consolidate Homebrew JSON parsing

**Files:**
- `macos/nexis-core/Tools/apt_source_tool.cpp` `getSourceList()`
- `macos/nexis-core/Tools/package_tool.cpp` `getHomebrewPackages()`

**What:** Extract the shared JSON parsing logic (`brew info --json=v2 --installed` → iterate `formulae` + `casks` arrays) into a shared helper function in the brew utility from 4.2. Each caller maps the parsed data to its own data structure (`APTSource` vs `Package`).

**Why:** ~60 lines of identical JSON parsing duplicated. When Homebrew's JSON schema changes (it has before), both must be updated.

**Risk:** Low-Medium. The two callers extract slightly different fields. The shared function should return a generic intermediate representation that each caller maps to its own type.

### 4.4 Move cross-platform core methods to shared `.cpp` files

**What:** For each of the following, move the identical implementation from both platform `.cpp` files into a new or existing `shared/nexis-core/` `.cpp` file:

| Methods | Current Location | Target |
|---------|-----------------|--------|
| `MemoryInfo` getters (getMemTotal, getMemFree, getMemUsed, getSwapTotal, getSwapFree, getSwapUsed) | macOS + Linux `memory_info.cpp` | `shared/nexis-core/Info/memory_info_shared.cpp` |
| `SystemInfo` getters (getHostname, getPlatform, getDistribution, getKernel, getCpuModel, getCpuSpeed, getCpuCore) | macOS + Linux `system_info.cpp` | `shared/nexis-core/Info/system_info_shared.cpp` |
| `GpuInfo` getters (getGpuDevices, hasGpu) | macOS + Linux `gpu_info.cpp` | `shared/nexis-core/Info/gpu_info_shared.cpp` |
| `Service` constructor | macOS + Linux `service_tool.cpp` | `shared/nexis-core/Tools/service_tool_shared.cpp` |
| `ProcessInfo::getProcessList()` | macOS + Linux `process_info.cpp` | `shared/nexis-core/Info/process_info_shared.cpp` |

**Why:** ~100 lines of identical code duplicated across platforms. When a getter needs to change (e.g., adding a field), both files must be updated identically. Moving to shared code ensures single source of truth.

**Approach:** The CMakeLists.txt already globs `shared/nexis-core/**/*.cpp`, so new shared `.cpp` files are automatically included. The platform files continue to provide the platform-specific methods (e.g., `updateMemoryInfo()`, `discoverGpus()`). Only the getters — which are pure accessors of member variables — move to shared.

**Risk:** Low. These are one-line getter methods that return member variables. The only concern is ensuring no platform-specific logic crept into any of them (grep confirms they're identical).

---

## Phase 5 — UI Code Consolidation ✅

**Goal:** Reduce the ~1,095 lines of duplicated platform-specific UI code by moving shared logic into `shared/nexis/`. This is the largest and most impactful phase. It should be done file-by-file, with a build and visual test after each.

**Status:** COMPLETE — Commits `3b24f9c` (5.1–5.2) and `a7b1202` (5.3–5.4). Consolidated 8 platform files into 5 shared files with `#ifdef Q_OS_MACOS` guards. Net -1,370 lines of duplicated code.

### Strategy

For each page with duplicated platform code, the approach is:

1. Create a `shared/nexis/Pages/{Page}/{page}_shared.cpp` file containing all platform-identical code.
2. Reduce each platform `.cpp` file to only the genuinely different lines.
3. The shared file calls virtual or conditionally-compiled helper methods for platform-specific behavior.

The preferred mechanism for platform differences is `#ifdef Q_OS_MACOS` / `#else` guards in the shared code, because:
- The differences are small (5-15 lines per file)
- Virtual methods would add unnecessary inheritance complexity
- The project already uses `#ifdef` extensively

### 5.1 Consolidate `system_cleaner_page.cpp` (~380 duplicated lines)

**Files:**
- `macos/nexis/Pages/SystemCleaner/system_cleaner_page.cpp`
- `linux/nexis/Pages/SystemCleaner/system_cleaner_page.cpp`

**What:** Move to `shared/nexis/Pages/SystemCleaner/system_cleaner_page.cpp` with `#ifdef` for:
1. `mDefaultIcon` initialization (line 16): macOS uses direct `QIcon(...)`, Linux uses `QIcon::fromTheme(...)` with fallback — wrap in `#ifdef`
2. Category icon setup in `init()` (lines 29-44): macOS renders SVGs directly, Linux uses `QIcon::fromTheme` with SVG fallback — wrap in `#ifdef`
3. File tree item icons (line 120): macOS uses `mDefaultIcon` directly, Linux uses `QIcon::fromTheme(text, mDefaultIcon)` — wrap in `#ifdef`
4. Trash path: `~/.Trash` (macOS) vs `~/.local/share/Trash` (Linux)
5. Trash deletion logic: macOS iterates entries; Linux removes `files/` and `info/` subdirectories

Differences 1-3 are icon-loading differences (~15 lines of `#ifdef`). Differences 4-5 are trash-handling differences (~15 lines of `#ifdef`). If Phase 10 (icon consistency) is applied later, the icon `#ifdef` blocks in 1-3 can be simplified to remove the `fromTheme` branches.

**Why:** 380 lines of duplicated code is the second-largest duplication in the project. Any bug fix or feature addition to the System Cleaner must be made in two places.

**Risk:** Medium. This is a significant refactor touching a page with file-deletion logic. Must verify the trash path and deletion logic for both platforms after consolidation.

### 5.2 Consolidate `search_page.cpp` (~400 duplicated lines)

**Files:**
- `macos/nexis/Pages/Search/search_page.cpp`
- `linux/nexis/Pages/Search/search_page.cpp`

**What:** Move to `shared/nexis/Pages/Search/search_page.cpp` with `#ifdef` for:
1. `find` permission flags (~15 lines): macOS uses `-perm +r/+w/+x`, Linux uses `-readable/-writable/-executable`
2. Trash path and metadata (~30 lines): macOS uses `~/.Trash` without `.trashinfo`; Linux uses `~/.local/share/Trash/files` with FreeDesktop `.trashinfo` metadata

~45 lines of platform-specific code within `#ifdef` blocks; ~400 lines consolidated.

**Why:** Largest single duplication. Same reasoning as 5.1.

**Risk:** Medium. Same considerations as 5.1 — file operations and trash handling need platform-specific testing.

### 5.3 Consolidate `settings_page.cpp` (~180 duplicated lines)

**Files:**
- `macos/nexis/Pages/Settings/settings_page.cpp`
- `linux/nexis/Pages/Settings/settings_page.cpp`

**What:** Move to `shared/nexis/Pages/Settings/settings_page.cpp` with `#ifdef` for:
1. Autostart path and file format (~20 lines): LaunchAgent plist vs XDG .desktop
2. Autostart creation logic (~25 lines): plist XML generation vs .desktop file generation
3. Disk analyzer tool list (~10 lines): macOS tools vs Linux tools

~55 lines of platform-specific code; ~180 lines consolidated.

**Risk:** Low-Medium. Settings page is less dangerous than System Cleaner/Search because it doesn't do file operations on user data.

### 5.4 Consolidate `startup_app.cpp` and `startup_apps_page.cpp`

**What:** These have less duplication (~50 and ~40 lines respectively) and more fundamental structural differences (plist vs .desktop parsing). Consolidate only the shared scaffolding (constructor, destructor, getters/setters, UI setup). Keep the toggle and file-parsing logic in platform files.

**Risk:** Low. The shared portions are trivial boilerplate.

### 5.5 Update `CMakeLists.txt` for new shared source files

**What:** After moving `.cpp` files from platform directories to `shared/`, the `GLOB_RECURSE` will automatically pick them up. But the platform directories will have fewer files. Verify that the build still works and no files are compiled twice.

**Risk:** Low, but must verify. The `GLOB_RECURSE` in CMakeLists.txt collects from both `shared/` and the platform directory. If a file exists in both, it will be compiled twice, causing linker duplicate symbol errors. Each consolidation step must REMOVE the platform `.cpp` file when moving to shared.

---

## Phase 6 — Code Quality Improvements ✅

**Goal:** Fix naming, style, and minor quality issues identified in the audit. These are low-risk, high-readability improvements.

**Status:** COMPLETE — Commit `603743a`. All 6 sub-tasks implemented: typo fixes, NexisDataRole enum, const-ref for loops, constexpr path macros, modern connects, getServices() rename.

### 6.1 Fix typos in identifiers

| Typo | Correct | File(s) |
|------|---------|---------|
| `lanuagesJson` / `lanuages` | `languagesJson` / `languages` | `app_manager.cpp` |
| `UNDEFIEND` | `UNDEFINED` | Search across codebase |
| `mSeletedRowModel` | `mSelectedRowModel` | `processes_page.h`, `processes_page.cpp` |

**Why:** Typos in identifiers make code harder to search and understand. They also look unprofessional in any code review or open-source contribution.

### 6.2 Replace magic number data roles with named constants

**Files:** `byte_tree_widget.cpp`, `host_manage.cpp`, and any file using unnamed roles

**What:** Define an enum (e.g., in a shared header):
```cpp
enum NexisDataRole {
    SortRole = Qt::UserRole + 1,   // replaces 0x0100 and 1
    LineNumberRole = Qt::UserRole + 2  // replaces 9
};
```

**Why:** Magic numbers obscure intent. `Qt::UserRole + 1` is the standard Qt pattern for custom data roles. Named constants are self-documenting and catch misuse at compile time.

### 6.3 Fix copy-by-value in range-based for loops

**Files:** Multiple files identified in the audit

**What:** Change `for (const QString diskName : diskNames)` to `for (const QString &diskName : diskNames)` (add `&`). Same for `QFileInfo`, `Service`, and other types copied by value in loops.

**Why:** Unnecessary copies waste CPU cycles and memory. For `QString` (which is implicitly shared / copy-on-write) the overhead is small, but for `QFileInfo` and `Service` (which may allocate) it's meaningful. The `&` makes intent clear: we're reading, not copying.

### 6.4 Replace `#define` path macros with `constexpr`

**Files:** Linux `cpu_info.cpp`, `memory_info.cpp`, `thermal_info.cpp`, `gpu_info.cpp`

**What:** Replace:
```cpp
#define PROC_CPUINFO "/proc/cpuinfo"
```
with:
```cpp
static constexpr const char* PROC_CPUINFO = "/proc/cpuinfo";
```

**Why:** `constexpr` variables are type-safe, visible in debuggers, and respect scope. `#define` macros are textual substitution with no type checking and pollute the global namespace.

### 6.5 Modernize the old-style SIGNAL/SLOT connect

**File:** `processes_page.cpp`

**What:** Replace the one remaining `connect(obj, SIGNAL(...), obj, SLOT(...))` with the type-safe `connect(obj, &Class::signal, obj, &Class::slot)` syntax.

**Why:** The old-style string-based connect fails silently at runtime if the signature is wrong. The new-style pointer-to-member connect fails at compile time, catching errors early.

### 6.6 Rename misleading method `getServicesWithSystemctl()`

**Files:** `shared/nexis-core/Tools/service_tool.h`, macOS and Linux `service_tool.cpp`

**What:** Rename to `getServices()` since the method name currently leaks the Linux implementation detail. On macOS it calls `launchctl`, not `systemctl`.

**Why:** Misleading names cause confusion when reading macOS code. The method's purpose is to list services — the mechanism is an implementation detail.

---

## Phase 7 — Build System Cleanup ✅

**Goal:** Modernize the CMakeLists.txt to follow modern CMake best practices. Low risk since these are build-system-only changes.

### 7.1 Consolidate `find_package(Qt6)` calls

**What:** Merge the two `find_package(Qt6)` calls into one:
```cmake
find_package(Qt6 COMPONENTS Core Gui Widgets Charts Svg Concurrent Network REQUIRED)
```

### 7.2 Replace global `include_directories()` with `target_include_directories()`

**What:** Convert `include_directories(...)` to `target_include_directories(nexis-core PRIVATE ...)` and `target_include_directories(nexis PRIVATE ...)`.

### 7.3 Replace global `add_definitions()` with `target_compile_definitions()`

**What:** Change `add_definitions(-DNEXISCORE_LIBRARY)` to `target_compile_definitions(nexis-core PRIVATE NEXISCORE_LIBRARY)`.

### 7.4 Add platform `.h` file glob for AUTOMOC

**What:** Add `file(GLOB_RECURSE GUI_PLAT_HDRS ...)` alongside `GUI_PLAT_SRCS` to ensure AUTOMOC processes platform-specific headers with `Q_OBJECT`.

---

## Phase 8 — Asset Normalization: Migrate to SVG ✅

**Goal:** Replace raster PNGs with resolution-independent SVGs wherever possible, and set display sizes in code/QSS. This eliminates dimension inconsistencies, supports HiDPI/Retina displays naturally, and reduces bundle size.

### 8.1 Migrate sidebar icons from PNG to SVG

**What:** SVG versions of all sidebar icons already exist alongside the PNGs (e.g., `dash.svg` + `dash.png`). The change is:
1. Update all code that loads sidebar icons to use the `.svg` files exclusively (the sidebar icon loader in `app.cpp` already uses SVGs on macOS — extend this to all platforms)
2. Set icon display size explicitly in code via `setIconSize(QSize(100, 100))` or equivalent
3. Remove the `.png` sidebar icon files from QRC and move them to `archive/assets/sidebar/` (`dash.png`, `startup-apps.png`, `cleaner.png`, `search.png`, `services.png`, `process.png`, `helpers.png`, `uninstaller.png`, `resources.png`, `ppa-manager.png`, `gnome-settings.png`, `settings.png`, `feedback.png`)

**Why:** SVGs render crisply at any size and any DPI. The current PNGs have inconsistent dimensions (28×28 to 128×128) which causes some icons to appear blurry when scaled. Using SVGs with explicit sizing in code means every icon renders identically regardless of the source file's intrinsic dimensions. This also eliminates the `gnome-settings.png` (28×28, severely blurry) problem entirely.

**Risk:** Low. The SVGs already exist and are already used on macOS. This extends that pattern to all platforms.

### 8.2 Migrate theme images from PNG to SVG where possible

**What:** For each oversized PNG, determine if an SVG replacement is viable:

| Image | Current | Action |
|-------|---------|--------|
| `clean.png` (1024×1024) | QSS `background: url(...)` | Create SVG version. Load via QSS `image: url(...)` with explicit size. Move PNG to `archive/assets/theme/`. |
| `clean-active.png` (1024×1024) | QSS `background: url(...)` | Same as clean.png. |
| `power.png` (512×512) | QSS `image: url(...)` | Create SVG version. Set size in QSS. Move PNG to `archive/assets/theme/`. |
| `run.png` (512×512) | QSS `image: url(...)` | Create SVG version. Set size in QSS. Move PNG to `archive/assets/theme/`. |
| `scanLoading.gif` (512×512) | QMovie animation | **Keep as GIF** — Qt does not support animated SVGs natively. Downsize from 512→200px (2× for Retina). |
| `refresh.png` (1024×1024) | Orphaned | Move to `archive/assets/theme/` (not referenced in code or QSS; superseded). |
| `asc.png` (1024×1024) | Orphaned | Move to `archive/assets/theme/` (superseded by `sort-asc.svg`). |
| `dsc.png` (1024×1024) | Orphaned | Move to `archive/assets/theme/` (superseded by `sort-dsc.svg`). |

**Why:** SVGs are resolution-independent and typically smaller than high-resolution PNGs. Setting display size in QSS (`image-width`, `image-height` or `width`/`height` constraints) keeps the sizing concern in one place — the stylesheet — rather than baking it into the image file. The `scanLoading.gif` is the one exception: Qt's `QMovie` requires a raster animated format, so it stays as a GIF but downsized.

**Risk:** Low-Medium. Creating SVG replacements for `clean.png`, `power.png`, and `run.png` requires either tracing the existing PNGs or sourcing new SVGs. The QSS `image:` property handles SVGs well in Qt6, but sizing behavior should be tested.

### 8.3 Remove `logo.png` from QRC

**What:** `logo.png` (733×714, non-square) is already flagged as orphaned — only `logo.svg` is used in code. Remove from `static.qrc` and move the file to `archive/assets/`. Do not modify the image file itself.

**Why:** No code references `logo.png`. Keeping it in QRC wastes binary size. Moving to `archive/` preserves the asset for potential future use.

---

## Phase 9 — Remaining Polish

**Goal:** Low-priority cleanup items that improve maintainability but have no functional impact.

### 9.1 Update translation `.ts` file paths

**What:** Batch find-and-replace `../stacer/` → `../nexis/` in all `.ts` file `<location>` tags.

**Why:** Stale paths from the pre-rebrand era. These are metadata used by Qt Linguist for jumping to source lines. Incorrect paths make translation maintenance harder.

### 9.2 Add missing languages to `languages.json`

**What:** Add `gl` (Galician) entry if the translation has meaningful content.

### 9.3 Un-suppress Qt warnings in debug builds

**File:** `shared/nexis/main.cpp`

**What:** Wrap the `QtWarningMsg` suppression in the message handler with `#ifdef QT_NO_DEBUG` or `#ifdef NDEBUG`. In debug builds, Qt warnings should be visible — they often indicate real problems.

### 9.4 Rename screenshots

**What:** Rename `screenshots/Stacer-2.0.1-*.png` to `screenshots/Nexis-*.png`. Update references in `README.md`. Ideally recapture with the new Nexis branding.

### 9.5 Reduce `CommandUtil::exec()` default timeout

**What:** Change the default timeout from 600 seconds (10 minutes) to 30 seconds. Add an optional timeout parameter for callers that need longer (e.g., `brew info` on large installs).

**Why:** If a command hangs, a 10-minute timeout freezes the UI thread. 30 seconds is a reasonable default for interactive use.

---

## Phase 10 — Icon and Font Consistency (Deferred)

**Goal:** Make the application visually identical on macOS and Linux by using bundled assets exclusively and removing hardcoded fonts. This phase is intentionally placed last — the owner may choose an alternative approach for icon/font strategy.

> **Note:** Phase 5 (UI code consolidation) does NOT depend on this phase. The consolidated files will use `#ifdef` blocks for the `QIcon::fromTheme()` differences. If this phase is later applied, those `#ifdef` blocks can be simplified by removing the `fromTheme` branches.

### 10.1 Remove all `QIcon::fromTheme()` calls — use bundled assets only

**Files:**

| File | Change |
|------|--------|
| `shared/nexis/app.cpp` (~line 301) | Remove Linux `QIcon::fromTheme()` branch. Use the macOS path (`QIcon(svgPath)`) for all platforms. |
| `system_cleaner_page.cpp` (shared, after Phase 5) | Remove `#ifdef` icon branches — use direct `QIcon(...)` for both platforms |
| `shared/nexis/Pages/Uninstaller/uninstaller_page.cpp` (lines 134, 157) | Replace `QIcon::fromTheme(pkg.name, fallbackIcon)` with just `fallbackIcon` |
| `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp` (line 175) | Replace `QIcon::fromTheme(pkg.name, fallbackIcon)` with just `fallbackIcon` |
| `shared/nexis/Pages/Resources/disk_usage_launcher_widget.cpp` (lines 87-95) | Replace all `QIcon::fromTheme()` calls with a bundled icon |

**Why:** Linux uses `QIcon::fromTheme()` which queries the system icon theme (Adwaita, Breeze, Papirus, etc.), producing different visuals depending on the user's desktop environment. macOS already uses bundled assets exclusively. All required fallback assets already exist in the QRC — the only gap is the disk usage launcher's third-party tool icons (Baobab, Filelight, QDirStat), which can use a generic icon.

**Asset gap to fill:** One SVG icon for "disk/storage" to use in the disk usage launcher widget. Either create one or repurpose an existing asset.

**Risk:** Low. Every `QIcon::fromTheme()` call already has a bundled fallback. We are switching from "try system, fall back to bundled" to "always bundled."

### 10.2 Remove hardcoded Ubuntu font from `.ui` files

**Files:** All `.ui` files that contain `<family>Ubuntu</family>`

**What:** Remove the `<font>` property blocks that specify the Ubuntu font family. Let Qt use the platform's default system font.

**Why:** The Ubuntu font is only available on Ubuntu-based Linux distributions. On macOS (SF Pro) and non-Ubuntu Linux distros (Cantarell, Noto, DejaVu, etc.), Qt falls back to an unpredictable alternative. Removing the hardcoded font means the app uses whatever the system default is, which looks native on every platform.

**Risk:** Low. The visual change is subtle — text may reflow slightly due to different font metrics.

---

## Execution Order Summary

```
Phase 0  Repository Hygiene         (zero-risk, no code changes)
Phase 1  Bug Fixes                  (isolated fixes, independent of each other)
Phase 2  Page Layout Normalization  (zero all margins)
Phase 3  Dead Code Removal          (subtractive, reduces complexity)
Phase 4  Core Library Consolidation (structural, enables Phase 5)
Phase 5  UI Code Consolidation      (largest structural change, depends on Phase 4)
Phase 6  Code Quality               (naming, style, minor improvements)
Phase 7  Build System Cleanup       (cmake-only changes)
Phase 8  Asset Normalization        (migrate PNGs to SVGs)
Phase 9  Remaining Polish           (low-priority cleanup)
Phase 10 Icon & Font Consistency    (deferred — owner may choose alternative approach)
```

Phases 0-3 are fully independent. Phase 5 depends on Phase 4 (core consolidation establishes the pattern). Phases 6-10 can run in any order. Phase 10 is intentionally last — if applied, it simplifies some `#ifdef` blocks created in Phase 5.

---

## Tracking

When work begins on any item, update `BUGS.md` and `FEATURE_REQUESTS.md` per the conventions in `CLAUDE.md`:
- New bugs discovered → append to `BUGS.md` with next sequential `BUG-XX` ID
- Items started → mark `[~]`
- Items completed → mark `[x]` with resolution note and commit hash
