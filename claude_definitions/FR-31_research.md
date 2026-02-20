# FR-31 Research: Replace GLOB_RECURSE with Explicit Source Lists

## 1. Summary

The project's `CMakeLists.txt` uses 9 `file(GLOB_RECURSE ...)` calls to collect source files. Eight of these collect `.cpp` and `.h` files for the two build targets (`nexis-core` static library and `nexis` executable), and one collects `.ts` translation files. This document enumerates every file collected by each glob, documents the directory structure, and provides the data needed to write explicit `set()` lists.

## 2. All GLOB_RECURSE Calls in CMakeLists.txt

### 2.1 nexis-core (Static Library) — Lines 37-40

```cmake
file(GLOB_RECURSE CORE_SHARED_SRCS "${CORE_SHARED_DIR}/**.cpp")   # Line 37
file(GLOB_RECURSE CORE_SHARED_HDRS "${CORE_SHARED_DIR}/**.h")     # Line 38
file(GLOB_RECURSE CORE_PLAT_SRCS   "${CORE_PLAT_DIR}/**.cpp")     # Line 39
file(GLOB_RECURSE CORE_PLAT_HDRS   "${CORE_PLAT_DIR}/**.h")       # Line 40
```

Where:
- `CORE_SHARED_DIR` = `${PROJECT_ROOT}/shared/nexis-core` (line 34)
- `CORE_PLAT_DIR` = `${PROJECT_ROOT}/macos/nexis-core` or `${PROJECT_ROOT}/linux/nexis-core` (line 35, via PLATFORM_DIR set on lines 25-29)

Used in: `add_library(nexis-core STATIC ...)` on lines 42-45.

### 2.2 nexis (Executable) — Lines 77-80

```cmake
file(GLOB_RECURSE GUI_SHARED_SRCS "${GUI_SHARED_DIR}/**.cpp")     # Line 77
file(GLOB_RECURSE GUI_SHARED_HDRS "${GUI_SHARED_DIR}/**.h")       # Line 78
file(GLOB_RECURSE GUI_PLAT_SRCS   "${GUI_PLAT_DIR}/**.cpp")       # Line 79
file(GLOB_RECURSE GUI_PLAT_HDRS   "${GUI_PLAT_DIR}/**.h")         # Line 80
```

Where:
- `GUI_SHARED_DIR` = `${PROJECT_ROOT}/shared/nexis` (line 73)
- `GUI_PLAT_DIR` = `${PROJECT_ROOT}/macos/nexis` or `${PROJECT_ROOT}/linux/nexis` (line 74, via PLATFORM_DIR)

Used in: `add_executable(nexis ...)` on lines 116-124.

### 2.3 Translations — Line 83

```cmake
file(GLOB_RECURSE NEXIS_TRANSLATIONS "${SHARED_DIR}/translations/**.ts")  # Line 83
```

Used in: `qt_create_translation(QM_FILES NEXIS_TRANSLATIONS ...)` on line 85.

---

## 3. Complete File Enumeration by Target and Category

### 3.1 CORE_SHARED_SRCS — shared/nexis-core/**/*.cpp (14 files)

Directory: `shared/nexis-core/`

| # | Subdirectory | File | Notes |
|---|-------------|------|-------|
| 1 | Info/ | `battery_info_shared.cpp` | Shared battery info base |
| 2 | Info/ | `disk_health_info_shared.cpp` | SMART health shared logic |
| 3 | Info/ | `disk_info_shared.cpp` | Disk info shared logic |
| 4 | Info/ | `gpu_info_shared.cpp` | GPU info shared logic |
| 5 | Info/ | `memory_info_shared.cpp` | Memory info shared logic |
| 6 | Info/ | `process.cpp` | Process data model |
| 7 | Info/ | `process_info_shared.cpp` | Process info shared logic |
| 8 | Info/ | `system_info_shared.cpp` | System info shared logic |
| 9 | Tools/ | `docker_tool.cpp` | Docker CLI wrapper |
| 10 | Tools/ | `package_tool_shared.cpp` | Package manager detection |
| 11 | Tools/ | `service_tool_shared.cpp` | Service tool shared logic |
| 12 | Utils/ | `command_util_shared.cpp` | Shell command execution |
| 13 | Utils/ | `file_util.cpp` | File system utilities |
| 14 | Utils/ | `format_util.cpp` | Formatting utilities |

**Breakdown by subdirectory:** Info (8), Tools (3), Utils (3)

### 3.2 CORE_SHARED_HDRS — shared/nexis-core/**/*.h (20 files)

| # | Subdirectory | File | Notes |
|---|-------------|------|-------|
| 1 | (root) | `nexis-core_global.h` | DLL export macros |
| 2 | Info/ | `battery_info.h` | Battery info interface |
| 3 | Info/ | `cpu_info.h` | CPU info interface |
| 4 | Info/ | `disk_health_info.h` | Disk health interface |
| 5 | Info/ | `disk_info.h` | Disk info interface |
| 6 | Info/ | `gpu_info.h` | GPU info interface |
| 7 | Info/ | `memory_info.h` | Memory info interface |
| 8 | Info/ | `network_info.h` | Network info interface |
| 9 | Info/ | `process.h` | Process data model |
| 10 | Info/ | `process_info.h` | Process info interface (has Q_OBJECT) |
| 11 | Info/ | `system_info.h` | System info interface |
| 12 | Info/ | `thermal_info.h` | Thermal info interface |
| 13 | Tools/ | `apt_source_tool.h` | APT source tool interface |
| 14 | Tools/ | `docker_tool.h` | Docker tool interface |
| 15 | Tools/ | `gnome_settings_tool.h` | GNOME settings interface |
| 16 | Tools/ | `package_tool_shared.h` | Package struct + enum |
| 17 | Tools/ | `service_tool.h` | Service tool interface |
| 18 | Utils/ | `command_util.h` | Command utility interface |
| 19 | Utils/ | `file_util.h` | File utility interface |
| 20 | Utils/ | `format_util.h` | Format utility interface |

**Breakdown by subdirectory:** (root) (1), Info (11), Tools (5), Utils (3)

**Header-only files (no matching .cpp in shared):** `nexis-core_global.h`, `cpu_info.h`, `disk_info.h` (interface only — platform provides .cpp), `gpu_info.h`, `memory_info.h`, `network_info.h`, `system_info.h`, `thermal_info.h`, `apt_source_tool.h`, `gnome_settings_tool.h`, `service_tool.h`, `command_util.h`, `file_util.h`, `format_util.h`

**Q_OBJECT in core headers:** Only `process_info.h` contains Q_OBJECT. This is critical for AUTOMOC — the header must be listed in sources.

### 3.3 CORE_PLAT_SRCS (macOS) — macos/nexis-core/**/*.cpp (15 files)

| # | Subdirectory | File | Notes |
|---|-------------|------|-------|
| 1 | Info/ | `battery_info.cpp` | macOS IOKit battery |
| 2 | Info/ | `cpu_info.cpp` | macOS sysctl CPU |
| 3 | Info/ | `disk_health_info.cpp` | macOS diskutil + smartctl |
| 4 | Info/ | `disk_info_platform.cpp` | macOS disk enumeration |
| 5 | Info/ | `gpu_info.cpp` | macOS IOKit GPU |
| 6 | Info/ | `memory_info.cpp` | macOS mach VM stats |
| 7 | Info/ | `network_info.cpp` | macOS network interfaces |
| 8 | Info/ | `process_info.cpp` | macOS ps-based process list |
| 9 | Info/ | `system_info.cpp` | macOS sw_vers/sysctl |
| 10 | Info/ | `thermal_info.cpp` | macOS IOKit thermal |
| 11 | Tools/ | `apt_source_tool.cpp` | macOS Homebrew tap stubs |
| 12 | Tools/ | `gnome_settings_tool.cpp` | macOS no-op stubs |
| 13 | Tools/ | `package_tool.cpp` | macOS Homebrew + .app bundles |
| 14 | Tools/ | `service_tool.cpp` | macOS launchctl |
| 15 | Utils/ | `command_util_platform.cpp` | macOS command execution |

**Breakdown by subdirectory:** Info (10), Tools (4), Utils (1)

### 3.4 CORE_PLAT_HDRS (macOS) — macos/nexis-core/**/*.h (3 files)

| # | Subdirectory | File | Notes |
|---|-------------|------|-------|
| 1 | Tools/ | `gnome_settings_constants.h` | macOS GNOME constants (stubs) |
| 2 | Tools/ | `package_tool.h` | macOS PackageTool class |
| 3 | Utils/ | `brew_util.h` | Homebrew JSON parser (header-only, inline) |

### 3.5 CORE_PLAT_SRCS (Linux) — linux/nexis-core/**/*.cpp (15 files)

| # | Subdirectory | File | Notes |
|---|-------------|------|-------|
| 1 | Info/ | `battery_info.cpp` | Linux sysfs battery |
| 2 | Info/ | `cpu_info.cpp` | Linux /proc/cpuinfo |
| 3 | Info/ | `disk_health_info.cpp` | Linux sysfs + smartctl |
| 4 | Info/ | `disk_info_platform.cpp` | Linux /proc/mounts |
| 5 | Info/ | `gpu_info.cpp` | Linux sysfs/nvidia-smi |
| 6 | Info/ | `memory_info.cpp` | Linux /proc/meminfo |
| 7 | Info/ | `network_info.cpp` | Linux /sys/class/net |
| 8 | Info/ | `process_info.cpp` | Linux /proc-based process list |
| 9 | Info/ | `system_info.cpp` | Linux /etc/os-release |
| 10 | Info/ | `thermal_info.cpp` | Linux /sys/class/thermal |
| 11 | Tools/ | `apt_source_tool.cpp` | Linux APT/deb822 source parsing |
| 12 | Tools/ | `gnome_settings_tool.cpp` | Linux gsettings wrapper |
| 13 | Tools/ | `package_tool.cpp` | Linux APT/DNF/Pacman/etc. |
| 14 | Tools/ | `service_tool.cpp` | Linux systemctl |
| 15 | Utils/ | `command_util_platform.cpp` | Linux command execution |

### 3.6 CORE_PLAT_HDRS (Linux) — linux/nexis-core/**/*.h (2 files)

| # | Subdirectory | File | Notes |
|---|-------------|------|-------|
| 1 | Tools/ | `gnome_settings_constants.h` | Linux GNOME gsettings schema keys |
| 2 | Tools/ | `package_tool.h` | Linux PackageTool class |

### 3.7 GUI_SHARED_SRCS — shared/nexis/**/*.cpp (39 files)

| # | Subdirectory | File | Notes |
|---|-------------|------|-------|
| 1 | (root) | `app.cpp` | Main application window |
| 2 | (root) | `feedback.cpp` | Feedback dialog |
| 3 | (root) | `main.cpp` | Entry point |
| 4 | (root) | `signal_mapper.cpp` | Cross-component event bus |
| 5 | (root) | `sliding_stacked_widget.cpp` | Animated page transitions |
| 6 | Managers/ | `app_manager.cpp` | Theme/style manager |
| 7 | Managers/ | `cleaner_service.cpp` | Reusable scan/clean logic |
| 8 | Managers/ | `info_manager.cpp` | System info aggregator |
| 9 | Managers/ | `schedule_manager.cpp` | Scheduled cleaning manager |
| 10 | Managers/ | `setting_manager.cpp` | QSettings wrapper |
| 11 | Pages/AptSourceManager/ | `apt_source_edit.cpp` | APT source editor dialog |
| 12 | Pages/AptSourceManager/ | `apt_source_manager_page.cpp` | APT/Homebrew manager page |
| 13 | Pages/AptSourceManager/ | `apt_source_repository_item.cpp` | APT source list item widget |
| 14 | Pages/Dashboard/ | `circlebar.cpp` | Circular gauge widget |
| 15 | Pages/Dashboard/ | `dashboard_page.cpp` | Dashboard page |
| 16 | Pages/Dashboard/ | `linebar.cpp` | Line chart widget |
| 17 | Pages/Docker/ | `docker_page.cpp` | Docker management page |
| 18 | Pages/GnomeSettings/ | `gnome_appearance_tab.cpp` | GNOME appearance settings |
| 19 | Pages/GnomeSettings/ | `gnome_desktop_tab.cpp` | GNOME desktop settings |
| 20 | Pages/GnomeSettings/ | `gnome_mouse_tab.cpp` | GNOME mouse/touchpad settings |
| 21 | Pages/GnomeSettings/ | `gnome_settings_page.cpp` | GNOME settings page container |
| 22 | Pages/GnomeSettings/ | `gnome_wm_tab.cpp` | GNOME window manager settings |
| 23 | Pages/HardwareInfo/ | `hardware_info_page.cpp` | Hardware info page |
| 24 | Pages/Helpers/ | `helpers_page.cpp` | System helpers page |
| 25 | Pages/Helpers/ | `host_manage.cpp` | Hosts file editor |
| 26 | Pages/Processes/ | `processes_page.cpp` | Process manager page |
| 27 | Pages/Resources/ | `disk_usage_launcher_widget.cpp` | Disk analyzer launcher |
| 28 | Pages/Resources/ | `history_chart.cpp` | History chart widget |
| 29 | Pages/Resources/ | `resources_page.cpp` | Resources monitoring page |
| 30 | Pages/Search/ | `search_page.cpp` | File search page |
| 31 | Pages/Services/ | `service_item.cpp` | Service list item widget |
| 32 | Pages/Services/ | `services_page.cpp` | Services management page |
| 33 | Pages/Settings/ | `settings_page.cpp` | Settings page |
| 34 | Pages/StartupApps/ | `startup_app.cpp` | Startup app list item widget |
| 35 | Pages/StartupApps/ | `startup_apps_page.cpp` | Startup apps page |
| 36 | Pages/SystemCleaner/ | `byte_tree_widget.cpp` | File size tree widget |
| 37 | Pages/SystemCleaner/ | `schedule_editor_dialog.cpp` | Cleaning schedule editor |
| 38 | Pages/SystemCleaner/ | `system_cleaner_page.cpp` | System cleaner page |
| 39 | Pages/Uninstaller/ | `uninstaller_page.cpp` | Package uninstaller page |

**Breakdown by subdirectory:**
- (root): 5
- Managers/: 5
- Pages/AptSourceManager/: 3
- Pages/Dashboard/: 3
- Pages/Docker/: 1
- Pages/GnomeSettings/: 5
- Pages/HardwareInfo/: 1
- Pages/Helpers/: 2
- Pages/Processes/: 1
- Pages/Resources/: 3
- Pages/Search/: 1
- Pages/Services/: 2
- Pages/Settings/: 1
- Pages/StartupApps/: 2
- Pages/SystemCleaner/: 3
- Pages/Uninstaller/: 1

### 3.8 GUI_SHARED_HDRS — shared/nexis/**/*.h (43 files)

| # | Subdirectory | File | Notes |
|---|-------------|------|-------|
| 1 | (root) | `app.h` | Q_OBJECT |
| 2 | (root) | `dpi.h` | Header-only, no Q_OBJECT |
| 3 | (root) | `feedback.h` | Q_OBJECT |
| 4 | (root) | `nexis_roles.h` | Header-only enum, no Q_OBJECT |
| 5 | (root) | `signal_mapper.h` | Q_OBJECT |
| 6 | (root) | `sliding_stacked_widget.h` | Q_OBJECT |
| 7 | (root) | `utilities.h` | Header-only, no Q_OBJECT |
| 8 | Managers/ | `app_manager.h` | Q_OBJECT |
| 9 | Managers/ | `cleaner_service.h` | Q_OBJECT |
| 10 | Managers/ | `info_manager.h` | Q_OBJECT (implied by signal usage) |
| 11 | Managers/ | `schedule_manager.h` | Q_OBJECT |
| 12 | Managers/ | `setting_manager.h` | No Q_OBJECT |
| 13 | Managers/ | `tool_manager.h` | Header-only, no Q_OBJECT, no .cpp in shared |
| 14 | Pages/AptSourceManager/ | `apt_source_edit.h` | Q_OBJECT |
| 15 | Pages/AptSourceManager/ | `apt_source_manager_page.h` | Q_OBJECT |
| 16 | Pages/AptSourceManager/ | `apt_source_repository_item.h` | Q_OBJECT |
| 17 | Pages/Dashboard/ | `circlebar.h` | Q_OBJECT |
| 18 | Pages/Dashboard/ | `dashboard_page.h` | Q_OBJECT |
| 19 | Pages/Dashboard/ | `linebar.h` | Q_OBJECT |
| 20 | Pages/Docker/ | `docker_page.h` | Q_OBJECT |
| 21 | Pages/GnomeSettings/ | `gnome_appearance_tab.h` | Q_OBJECT |
| 22 | Pages/GnomeSettings/ | `gnome_desktop_tab.h` | Q_OBJECT |
| 23 | Pages/GnomeSettings/ | `gnome_mouse_tab.h` | Q_OBJECT |
| 24 | Pages/GnomeSettings/ | `gnome_settings_page.h` | Q_OBJECT |
| 25 | Pages/GnomeSettings/ | `gnome_wm_tab.h` | Q_OBJECT |
| 26 | Pages/HardwareInfo/ | `hardware_info_page.h` | Q_OBJECT |
| 27 | Pages/Helpers/ | `helpers_page.h` | Q_OBJECT |
| 28 | Pages/Helpers/ | `host_manage.h` | Q_OBJECT |
| 29 | Pages/Processes/ | `processes_page.h` | Q_OBJECT |
| 30 | Pages/Resources/ | `disk_usage_launcher_widget.h` | Q_OBJECT |
| 31 | Pages/Resources/ | `history_chart.h` | Q_OBJECT |
| 32 | Pages/Resources/ | `resources_page.h` | Q_OBJECT |
| 33 | Pages/Search/ | `search_page.h` | Q_OBJECT |
| 34 | Pages/Services/ | `service_item.h` | Q_OBJECT |
| 35 | Pages/Services/ | `services_page.h` | Q_OBJECT |
| 36 | Pages/Settings/ | `settings_page.h` | Q_OBJECT |
| 37 | Pages/StartupApps/ | `startup_app.h` | Q_OBJECT |
| 38 | Pages/StartupApps/ | `startup_app_edit.h` | Q_OBJECT |
| 39 | Pages/StartupApps/ | `startup_apps_page.h` | Q_OBJECT |
| 40 | Pages/SystemCleaner/ | `byte_tree_widget.h` | Q_OBJECT |
| 41 | Pages/SystemCleaner/ | `schedule_editor_dialog.h` | Q_OBJECT |
| 42 | Pages/SystemCleaner/ | `system_cleaner_page.h` | Q_OBJECT |
| 43 | Pages/Uninstaller/ | `uninstaller_page.h` | Q_OBJECT |

**Breakdown by subdirectory:**
- (root): 7
- Managers/: 6
- Pages/AptSourceManager/: 3
- Pages/Dashboard/: 3
- Pages/Docker/: 1
- Pages/GnomeSettings/: 5
- Pages/HardwareInfo/: 1
- Pages/Helpers/: 2
- Pages/Processes/: 1
- Pages/Resources/: 3
- Pages/Search/: 1
- Pages/Services/: 2
- Pages/Settings/: 1
- Pages/StartupApps/: 3
- Pages/SystemCleaner/: 3
- Pages/Uninstaller/: 1

**Header-only files (no matching .cpp in shared):** `dpi.h`, `nexis_roles.h`, `utilities.h`, `tool_manager.h`
**Note:** `tool_manager.h` is header-only in shared but has platform-specific `.cpp` files in both `macos/nexis/Managers/` and `linux/nexis/Managers/`. `startup_app_edit.h` likewise has its `.cpp` in the platform dirs only.

### 3.9 GUI_PLAT_SRCS (macOS) — macos/nexis/**/*.cpp (2 files)

| # | Subdirectory | File | Notes |
|---|-------------|------|-------|
| 1 | Managers/ | `tool_manager.cpp` | macOS ToolManager implementation |
| 2 | Pages/StartupApps/ | `startup_app_edit.cpp` | macOS startup app editor (launchd plist) |

### 3.10 GUI_PLAT_HDRS (macOS) — macos/nexis/**/*.h (0 files)

No platform-specific headers for the macOS GUI. The shared headers (`tool_manager.h`, `startup_app_edit.h`) use `#ifdef Q_OS_MACOS` for platform-specific declarations.

### 3.11 GUI_PLAT_SRCS (Linux) — linux/nexis/**/*.cpp (2 files)

| # | Subdirectory | File | Notes |
|---|-------------|------|-------|
| 1 | Managers/ | `tool_manager.cpp` | Linux ToolManager implementation |
| 2 | Pages/StartupApps/ | `startup_app_edit.cpp` | Linux startup app editor (.desktop files) |

### 3.12 GUI_PLAT_HDRS (Linux) — linux/nexis/**/*.h (0 files)

No platform-specific headers for the Linux GUI.

### 3.13 NEXIS_TRANSLATIONS — shared/translations/**/*.ts (34 files)

```
nexis_af.ts   nexis_ar.ts   nexis_ca-es.ts  nexis_cs.ts   nexis_da.ts
nexis_de.ts   nexis_el.ts   nexis_en.ts     nexis_es.ts   nexis_fi.ts
nexis_fr.ts   nexis_gl.ts   nexis_he.ts     nexis_hi.ts   nexis_hu.ts
nexis_it.ts   nexis_ja.ts   nexis_kn.ts     nexis_ko.ts   nexis_ml.ts
nexis_nl.ts   nexis_no.ts   nexis_oc.ts     nexis_pl.ts   nexis_pt.ts
nexis_ro.ts   nexis_ru.ts   nexis_sr.ts     nexis_sv.ts   nexis_tr.ts
nexis_ua.ts   nexis_vn.ts   nexis_zh-cn.ts  nexis_zh-tw.ts
```

---

## 4. File Count Summary

| Variable | Shared | macOS Platform | Linux Platform |
|----------|--------|---------------|----------------|
| CORE_*_SRCS | 14 | 15 | 15 |
| CORE_*_HDRS | 20 | 3 | 2 |
| GUI_*_SRCS | 39 | 2 | 2 |
| GUI_*_HDRS | 43 | 0 | 0 |
| **Target subtotals** | | | |
| nexis-core total | 34 | 18 | 17 |
| nexis total | 82 | 2 | 2 |

**Grand total source files:**
- nexis-core: 34 shared + 18 macOS platform (or 17 Linux) = **52 files (macOS) / 51 files (Linux)**
- nexis: 82 shared + 2 platform = **84 files per platform**
- Translations: **34 files**

---

## 5. Build Target Configuration Analysis

### 5.1 nexis-core (Static Library)

```cmake
add_library(nexis-core STATIC
  ${CORE_SHARED_SRCS} ${CORE_SHARED_HDRS}
  ${CORE_PLAT_SRCS}   ${CORE_PLAT_HDRS}
)
```

- Links: `Qt6::Core Qt6::Network` (line 61)
- On macOS also links: `IOKit` and `CoreFoundation` frameworks (lines 65-68)
- Compile defs: `NEXISCORE_LIBRARY QT_DEPRECATED_WARNINGS` (line 60)
- Headers are listed for AUTOMOC (only `process_info.h` has Q_OBJECT)
- Include directories enumerate specific subdirectories, not just the root (lines 49-58)

### 5.2 nexis (Executable)

```cmake
add_executable(nexis
  ${GUI_SHARED_SRCS}
  ${GUI_SHARED_HDRS}
  ${GUI_PLAT_SRCS}
  ${GUI_PLAT_HDRS}
  "${GUI_SHARED_DIR}/static.qrc"      # Explicit — not globbed
  ${QM_FILES}                          # From qt_create_translation
  $<$<PLATFORM_ID:Darwin>:${MACOS_ICON}>  # macOS icon
)
```

- Links: `nexis-core Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Charts Qt6::Svg Qt6::Concurrent` (lines 154-156)
- AUTOMOC, AUTOUIC, AUTORCC are all enabled (lines 16, 88-89)
- AUTOUIC search paths are explicitly listed (lines 92-108) for 14 directories
- 35 out of 43 shared headers have Q_OBJECT (critical for AUTOMOC)
- Include directories enumerate 20 specific paths (lines 127-151)

### 5.3 Non-Globbed Resources

The following are NOT globbed — they are explicitly referenced:
- `static.qrc` — explicit on line 121
- macOS icon — explicit on line 112-113
- `.desktop` file — explicit on line 198-199
- PNG icons — explicit loop on lines 203-209

The `.ui` files are NOT listed anywhere — they are found by `CMAKE_AUTOUIC` using `CMAKE_AUTOUIC_SEARCH_PATHS` (lines 92-108). AUTOUIC processes them automatically when it finds `#include "ui_*.h"` in source files.

---

## 6. Existing Build Infrastructure

### 6.1 No sources.cmake Files

There are **zero** `sources.cmake` files anywhere in the project. This is a fresh start — no existing conventions to follow or conflict with.

### 6.2 CXXBasics Build Utilities

The `shared/cmake/cxxbasics/` directory contains build acceleration utilities:
- `CXXBasics.cmake` — Entry point, includes other modules
- `DefaultSettings.cmake` — Reasonable CMake defaults
- `InitCXXBasics.cmake` — Variable initialization
- `UseFasterLinkers.cmake` — LLD/mold linker detection
- `UseCompilerCacheTool.cmake` — ccache/sccache detection
- `UseCCache.cmake` / `UseSCCache.cmake` — Cache tool implementations
- `GetTargetArch.cmake` — Architecture detection

None of these modules deal with source file enumeration. They focus purely on build performance and compiler settings.

---

## 7. Potential Surprises and Edge Cases

### 7.1 .DS_Store Files

macOS `.DS_Store` files exist in:
- `shared/nexis-core/`
- `shared/nexis/`
- `macos/nexis/`
- `macos/nexis/Pages/`
- `linux/nexis/`
- `linux/nexis/Pages/`
- `shared/cmake/`

These do NOT match `**.cpp` or `**.h` patterns, so they are safely ignored by the current globs. Not a concern for the replacement.

### 7.2 No Test Files

There are zero `test*.cpp`, `*_test.cpp`, `*.bak`, or `*.old` files anywhere in the source directories. The globs are not accidentally collecting any unwanted files today.

### 7.3 Header-only Files Critical for AUTOMOC

Some `.h` files have Q_OBJECT but no corresponding `.cpp` in the same source tree:
- `shared/nexis/Managers/tool_manager.h` — header-only in shared, .cpp in platform dirs
- `shared/nexis/Pages/StartupApps/startup_app_edit.h` — header-only in shared, .cpp in platform dirs

These MUST remain in the header lists so AUTOMOC can process them and generate `moc_tool_manager.cpp` / `moc_startup_app_edit.cpp`.

### 7.4 The `**` Glob Pattern

The CMakeLists.txt uses `**.cpp` and `**.h` patterns (e.g., `"${CORE_SHARED_DIR}/**.cpp"`). In CMake, `**` inside `GLOB_RECURSE` matches any number of path components, including zero. This is effectively the same as just using `*.cpp` with GLOB_RECURSE. The behavior is correct but the double-star notation is somewhat unusual in CMake (more common in shell globs).

### 7.5 Platform Symmetry

macOS and Linux platform directories are nearly symmetric:

| Component | macOS | Linux | Difference |
|-----------|-------|-------|-----------|
| Core .cpp | 15 | 15 | Same count, same files |
| Core .h | 3 | 2 | macOS has extra `brew_util.h` |
| GUI .cpp | 2 | 2 | Same count, same files |
| GUI .h | 0 | 0 | Same |

The only asymmetry is `macos/nexis-core/Utils/brew_util.h` (Homebrew utility), which has no Linux equivalent.

### 7.6 Translation Files — Whether to Also Replace

The translation GLOB_RECURSE (line 83) collects 34 `.ts` files from a flat directory (no subdirectories). The risk is lower than source globs because:
- Translation files change infrequently
- Adding a translation is rare (maybe once a year)
- The directory is flat — no subdirectory surprise risk

However, for consistency, it should also be replaced with an explicit list. The list is simple and easy to maintain.

---

## 8. Recommended Approach

### 8.1 Option A: Inline set() Lists (Recommended)

Replace each `file(GLOB_RECURSE ...)` with a `set()` block directly in `CMakeLists.txt`. The file counts are manageable:
- Core shared: 34 files
- Core platform: 17-18 files
- GUI shared: 82 files
- GUI platform: 2 files
- Translations: 34 files

Total: ~170 lines of explicit file lists. The current CMakeLists.txt is 211 lines. This would roughly double it to ~380 lines, which is still very manageable for a single-file build system.

**Pros:** Everything in one file, easy to audit, no includes to chase.
**Cons:** CMakeLists.txt gets longer.

### 8.2 Option B: Separate sources.cmake Include Files

Create `sources.cmake` files in each source directory:
- `shared/nexis-core/sources.cmake`
- `macos/nexis-core/sources.cmake`
- `linux/nexis-core/sources.cmake`
- `shared/nexis/sources.cmake`
- `macos/nexis/sources.cmake`
- `linux/nexis/sources.cmake`
- `shared/translations/sources.cmake`

Then `include()` them from CMakeLists.txt.

**Pros:** CMakeLists.txt stays short, source lists live near the files they describe.
**Cons:** 7 new files to maintain, developers must know to update the right `sources.cmake` when adding files.

### 8.3 Recommendation

**Option A (inline)** is preferred because:
1. The project has a single `CMakeLists.txt` — developers already look here for build config.
2. The total line count is still reasonable (~380 lines).
3. No new files to discover or forget about.
4. The architecture roadmap (FR-34) may restructure the directory layout later — fewer files to refactor.
5. A single grep can find any source file in the build system.

---

## 9. AUTOMOC Considerations

When switching to explicit lists, it is **essential** to include `.h` files in the target sources if they contain `Q_OBJECT`, `Q_GADGET`, or `Q_NAMESPACE` macros. CMake's AUTOMOC scans the listed sources for these macros and generates the corresponding `moc_*.cpp` files.

### Files requiring AUTOMOC processing:

**nexis-core target (1 header):**
- `shared/nexis-core/Info/process_info.h` — Q_OBJECT

**nexis target (35 headers):**
All 35 headers listed in section 3.8 with "Q_OBJECT" annotation must be in the source list.

If a header is accidentally omitted from the explicit list, the build will fail with linker errors like:
```
undefined reference to 'vtable for ClassName'
```

---

## 10. Build Verification Strategy

After replacing the globs, verification should include:
1. **Clean build on macOS:** `rm -rf build && cmake -B build && cmake --build build`
2. **Clean build on Linux (CI):** Same commands in CI pipeline
3. **Compare object files:** The set of `.o` files produced should be identical before and after
4. **AUTOMOC check:** All `moc_*.cpp` files should still be generated (check `build/nexis_autogen/` and `build/nexis-core_autogen/`)
5. **Incremental build test:** Touch a single `.cpp` file, verify only it recompiles
6. **New file test:** Add a dummy `.cpp` file to a source directory, verify it does NOT automatically appear in the build (this is the whole point of FR-31)

---

## 11. References

- **Architecture Review section 4 (CMake GLOB_RECURSE):** `docs/ARCHITECTURE_REVIEW.md` lines 315-334
- **Architecture Review recommendation 1A:** `docs/ARCHITECTURE_REVIEW.md` lines 419-443
- **Implementation Roadmap Phase 2:** `docs/IMPLEMENTATION_ROADMAP.md` lines 112-133
- **CMake documentation warning:** "We do not recommend using GLOB to collect a list of source files from your source tree." — [cmake.org/cmake/help/latest/command/file.html#glob-recurse](https://cmake.org/cmake/help/latest/command/file.html#glob-recurse)
- **Feature request:** `FEATURE_REQUESTS.md` FR-31
