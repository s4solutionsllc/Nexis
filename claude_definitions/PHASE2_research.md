# Phase 2: Build System Hardening — Research

**Tracking:** Architecture Review §1A
**Goal:** Replace `GLOB_RECURSE` with explicit source lists so file additions/removals are deterministic.

---

## Current State

### CMakeLists.txt GLOB_RECURSE Calls

The build system uses 5 `GLOB_RECURSE` calls (lines 37-40, 77-80, 83):

```cmake
# nexis-core (lines 37-40)
file(GLOB_RECURSE CORE_SHARED_SRCS "${CORE_SHARED_DIR}/**.cpp")
file(GLOB_RECURSE CORE_SHARED_HDRS "${CORE_SHARED_DIR}/**.h")
file(GLOB_RECURSE CORE_PLAT_SRCS   "${CORE_PLAT_DIR}/**.cpp")
file(GLOB_RECURSE CORE_PLAT_HDRS   "${CORE_PLAT_DIR}/**.h")

# nexis GUI (lines 77-80)
file(GLOB_RECURSE GUI_SHARED_SRCS "${GUI_SHARED_DIR}/**.cpp")
file(GLOB_RECURSE GUI_SHARED_HDRS "${GUI_SHARED_DIR}/**.h")
file(GLOB_RECURSE GUI_PLAT_SRCS   "${GUI_PLAT_DIR}/**.cpp")
file(GLOB_RECURSE GUI_PLAT_HDRS   "${GUI_PLAT_DIR}/**.h")

# Translations (line 83)
file(GLOB_RECURSE NEXIS_TRANSLATIONS "${SHARED_DIR}/translations/**.ts")
```

### Why This Is a Problem

- Adding/removing a `.cpp` file does **not** trigger CMake reconfiguration
- Developer must manually re-run `cmake -B build` for changes to take effect
- Stale object files can linger after file deletion
- CI builds may pass when local builds fail (or vice versa)
- CMake official docs explicitly warn against this pattern

---

## Complete Source File Inventory

### 1. CORE_SHARED_SRCS (14 files) — `shared/nexis-core/`

```
shared/nexis-core/Info/battery_info_shared.cpp
shared/nexis-core/Info/disk_health_info_shared.cpp
shared/nexis-core/Info/disk_info_shared.cpp
shared/nexis-core/Info/gpu_info_shared.cpp
shared/nexis-core/Info/memory_info_shared.cpp
shared/nexis-core/Info/process.cpp
shared/nexis-core/Info/process_info_shared.cpp
shared/nexis-core/Info/system_info_shared.cpp
shared/nexis-core/Tools/docker_tool.cpp
shared/nexis-core/Tools/package_tool_shared.cpp
shared/nexis-core/Tools/service_tool_shared.cpp
shared/nexis-core/Utils/command_util_shared.cpp
shared/nexis-core/Utils/file_util.cpp
shared/nexis-core/Utils/format_util.cpp
```

### 2. CORE_SHARED_HDRS (20 files) — `shared/nexis-core/`

```
shared/nexis-core/nexis-core_global.h
shared/nexis-core/Info/battery_info.h
shared/nexis-core/Info/cpu_info.h
shared/nexis-core/Info/disk_health_info.h
shared/nexis-core/Info/disk_info.h
shared/nexis-core/Info/gpu_info.h
shared/nexis-core/Info/memory_info.h
shared/nexis-core/Info/network_info.h
shared/nexis-core/Info/process.h
shared/nexis-core/Info/process_info.h
shared/nexis-core/Info/system_info.h
shared/nexis-core/Info/thermal_info.h
shared/nexis-core/Tools/apt_source_tool.h
shared/nexis-core/Tools/docker_tool.h
shared/nexis-core/Tools/gnome_settings_tool.h
shared/nexis-core/Tools/package_tool_shared.h
shared/nexis-core/Tools/service_tool.h
shared/nexis-core/Utils/command_util.h
shared/nexis-core/Utils/file_util.h
shared/nexis-core/Utils/format_util.h
```

### 3. CORE_PLAT_SRCS — Platform-specific core sources

**macOS (`macos/nexis-core/`) — 15 files:**
```
macos/nexis-core/Info/battery_info.cpp
macos/nexis-core/Info/cpu_info.cpp
macos/nexis-core/Info/disk_health_info.cpp
macos/nexis-core/Info/disk_info_platform.cpp
macos/nexis-core/Info/gpu_info.cpp
macos/nexis-core/Info/memory_info.cpp
macos/nexis-core/Info/network_info.cpp
macos/nexis-core/Info/process_info.cpp
macos/nexis-core/Info/system_info.cpp
macos/nexis-core/Info/thermal_info.cpp
macos/nexis-core/Tools/apt_source_tool.cpp
macos/nexis-core/Tools/gnome_settings_tool.cpp
macos/nexis-core/Tools/package_tool.cpp
macos/nexis-core/Tools/service_tool.cpp
macos/nexis-core/Utils/command_util_platform.cpp
```

**Linux (`linux/nexis-core/`) — 15 files:**
```
linux/nexis-core/Info/battery_info.cpp
linux/nexis-core/Info/cpu_info.cpp
linux/nexis-core/Info/disk_health_info.cpp
linux/nexis-core/Info/disk_info_platform.cpp
linux/nexis-core/Info/gpu_info.cpp
linux/nexis-core/Info/memory_info.cpp
linux/nexis-core/Info/network_info.cpp
linux/nexis-core/Info/process_info.cpp
linux/nexis-core/Info/system_info.cpp
linux/nexis-core/Info/thermal_info.cpp
linux/nexis-core/Tools/apt_source_tool.cpp
linux/nexis-core/Tools/gnome_settings_tool.cpp
linux/nexis-core/Tools/package_tool.cpp
linux/nexis-core/Tools/service_tool.cpp
linux/nexis-core/Utils/command_util_platform.cpp
```

### 4. CORE_PLAT_HDRS — Platform-specific core headers

**macOS (`macos/nexis-core/`) — 3 files:**
```
macos/nexis-core/Tools/gnome_settings_constants.h
macos/nexis-core/Tools/package_tool.h
macos/nexis-core/Utils/brew_util.h
```

**Linux (`linux/nexis-core/`) — 2 files:**
```
linux/nexis-core/Tools/gnome_settings_constants.h
linux/nexis-core/Tools/package_tool.h
```

### 5. GUI_SHARED_SRCS (39 files) — `shared/nexis/`

```
shared/nexis/app.cpp
shared/nexis/feedback.cpp
shared/nexis/main.cpp
shared/nexis/signal_mapper.cpp
shared/nexis/sliding_stacked_widget.cpp
shared/nexis/Managers/app_manager.cpp
shared/nexis/Managers/cleaner_service.cpp
shared/nexis/Managers/info_manager.cpp
shared/nexis/Managers/schedule_manager.cpp
shared/nexis/Managers/setting_manager.cpp
shared/nexis/Pages/AptSourceManager/apt_source_edit.cpp
shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp
shared/nexis/Pages/AptSourceManager/apt_source_repository_item.cpp
shared/nexis/Pages/Dashboard/circlebar.cpp
shared/nexis/Pages/Dashboard/dashboard_page.cpp
shared/nexis/Pages/Dashboard/linebar.cpp
shared/nexis/Pages/Docker/docker_page.cpp
shared/nexis/Pages/GnomeSettings/gnome_appearance_tab.cpp
shared/nexis/Pages/GnomeSettings/gnome_desktop_tab.cpp
shared/nexis/Pages/GnomeSettings/gnome_mouse_tab.cpp
shared/nexis/Pages/GnomeSettings/gnome_settings_page.cpp
shared/nexis/Pages/GnomeSettings/gnome_wm_tab.cpp
shared/nexis/Pages/Helpers/helpers_page.cpp
shared/nexis/Pages/Helpers/host_manage.cpp
shared/nexis/Pages/HardwareInfo/hardware_info_page.cpp
shared/nexis/Pages/Processes/processes_page.cpp
shared/nexis/Pages/Resources/disk_usage_launcher_widget.cpp
shared/nexis/Pages/Resources/history_chart.cpp
shared/nexis/Pages/Resources/resources_page.cpp
shared/nexis/Pages/Search/search_page.cpp
shared/nexis/Pages/Services/service_item.cpp
shared/nexis/Pages/Services/services_page.cpp
shared/nexis/Pages/Settings/settings_page.cpp
shared/nexis/Pages/StartupApps/startup_app.cpp
shared/nexis/Pages/StartupApps/startup_apps_page.cpp
shared/nexis/Pages/SystemCleaner/byte_tree_widget.cpp
shared/nexis/Pages/SystemCleaner/schedule_editor_dialog.cpp
shared/nexis/Pages/SystemCleaner/system_cleaner_page.cpp
shared/nexis/Pages/Uninstaller/uninstaller_page.cpp
```

### 6. GUI_SHARED_HDRS (43 files) — `shared/nexis/`

```
shared/nexis/app.h
shared/nexis/dpi.h
shared/nexis/feedback.h
shared/nexis/nexis_roles.h
shared/nexis/signal_mapper.h
shared/nexis/sliding_stacked_widget.h
shared/nexis/utilities.h
shared/nexis/Managers/app_manager.h
shared/nexis/Managers/cleaner_service.h
shared/nexis/Managers/info_manager.h
shared/nexis/Managers/schedule_manager.h
shared/nexis/Managers/setting_manager.h
shared/nexis/Managers/tool_manager.h
shared/nexis/Pages/AptSourceManager/apt_source_edit.h
shared/nexis/Pages/AptSourceManager/apt_source_manager_page.h
shared/nexis/Pages/AptSourceManager/apt_source_repository_item.h
shared/nexis/Pages/Dashboard/circlebar.h
shared/nexis/Pages/Dashboard/dashboard_page.h
shared/nexis/Pages/Dashboard/linebar.h
shared/nexis/Pages/Docker/docker_page.h
shared/nexis/Pages/GnomeSettings/gnome_appearance_tab.h
shared/nexis/Pages/GnomeSettings/gnome_desktop_tab.h
shared/nexis/Pages/GnomeSettings/gnome_mouse_tab.h
shared/nexis/Pages/GnomeSettings/gnome_settings_page.h
shared/nexis/Pages/GnomeSettings/gnome_wm_tab.h
shared/nexis/Pages/Helpers/helpers_page.h
shared/nexis/Pages/Helpers/host_manage.h
shared/nexis/Pages/HardwareInfo/hardware_info_page.h
shared/nexis/Pages/Processes/processes_page.h
shared/nexis/Pages/Resources/disk_usage_launcher_widget.h
shared/nexis/Pages/Resources/history_chart.h
shared/nexis/Pages/Resources/resources_page.h
shared/nexis/Pages/Search/search_page.h
shared/nexis/Pages/Services/service_item.h
shared/nexis/Pages/Services/services_page.h
shared/nexis/Pages/Settings/settings_page.h
shared/nexis/Pages/StartupApps/startup_app.h
shared/nexis/Pages/StartupApps/startup_app_edit.h
shared/nexis/Pages/StartupApps/startup_apps_page.h
shared/nexis/Pages/SystemCleaner/byte_tree_widget.h
shared/nexis/Pages/SystemCleaner/schedule_editor_dialog.h
shared/nexis/Pages/SystemCleaner/system_cleaner_page.h
shared/nexis/Pages/Uninstaller/uninstaller_page.h
```

### 7. GUI_PLAT_SRCS — Platform-specific GUI sources

**macOS (`macos/nexis/`) — 2 files:**
```
macos/nexis/Managers/tool_manager.cpp
macos/nexis/Pages/StartupApps/startup_app_edit.cpp
```

**Linux (`linux/nexis/`) — 2 files:**
```
linux/nexis/Managers/tool_manager.cpp
linux/nexis/Pages/StartupApps/startup_app_edit.cpp
```

### 8. GUI_PLAT_HDRS — 0 files on both platforms

No platform-specific GUI headers exist on either macOS or Linux.

### 9. NEXIS_TRANSLATIONS (34 files) — `shared/translations/`

34 `.ts` files (nexis_af.ts through nexis_zh-tw.ts). These are stable — translation files rarely change. GLOB_RECURSE is less problematic here but should still be replaced for consistency.

---

## Design Decisions

### Approach: Inline `set()` lists vs. `sources.cmake` includes

**Option A — Inline `set()` in CMakeLists.txt:**
- Pros: Everything in one file, easy to find
- Cons: CMakeLists.txt will grow by ~150 lines (currently 211 lines)

**Option B — Separate `sources.cmake` include files:**
- Pros: CMakeLists.txt stays clean; source lists are isolated
- Cons: More files to manage; developer must know where to look

**Recommendation: Option A (inline).** The total source file count is ~170 files across all categories. With grouped `set()` blocks and comments, this adds ~150 lines to CMakeLists.txt (bringing it to ~360 lines). This is well within the manageable range for a single CMakeLists.txt. Everything is in one place — no hunting across files.

### Translation files — keep or convert?

The translations GLOB is lower risk since `.ts` files change infrequently. However, for consistency and to fully eliminate GLOB_RECURSE, we should convert it too.

### Platform-conditional source lists

The current structure uses `PLATFORM_DIR` which resolves to `macos/` or `linux/` at configure time. The explicit lists need to handle this:
- Use `if(APPLE) ... else() ... endif()` blocks for platform sources
- OR keep using `${PLATFORM_DIR}` variable (simpler, same behavior)

**Recommendation:** Keep using `${PLATFORM_DIR}` — it's already well-established and the path resolution happens at configure time, which is deterministic.

---

## Impact Analysis

- **Zero user-visible behavior change** — this is purely a build system improvement
- **No API/ABI changes** — same files compiled in same order
- **Risk: Missing a file** — mitigated by building immediately after the change and comparing the compiled objects
- **Future impact: Phase 5** (Platform Interface Formalization) will rename files — the explicit lists make this easier to track
