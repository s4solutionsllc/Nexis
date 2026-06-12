cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

# Pin the macOS deployment target so the shipped .app/.dmg floor is explicit and
# reproducible instead of floating to whatever host builds it (an unset target
# makes clang stamp minos = the build machine's macOS version). The real binding
# constraint is Homebrew Qt6, whose bottles are built for minos 14.0 — dyld
# refuses to load them below macOS 14, so Sonoma is the hard floor regardless of
# this setting. Keep README/website/cask in sync with this value. Must be set
# before project(); only consulted on Apple, ignored elsewhere.
if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Minimum macOS deployment version")
endif()

project(Nexis VERSION 2.5.0)

# Adding features(build cache + faster linkers) and reasonable defaults(Debug build by default)
include("${CMAKE_CURRENT_SOURCE_DIR}/shared/cmake/cxxbasics/CXXBasics.cmake")

# Setting a cleaner directory structure for the generated binaries
set(CMAKE_BINARY_DIR        "${CMAKE_BINARY_DIR}/output")
set(EXECUTABLE_OUTPUT_PATH  "${CMAKE_BINARY_DIR}/")
set(LIBRARY_OUTPUT_PATH     "${CMAKE_BINARY_DIR}/lib")

set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(SHARED_DIR   "${PROJECT_ROOT}/shared")

# Activating MOC and searching for the Qt6 dependencies
set(CMAKE_AUTOMOC ON)
# SSO-3399: Qt 6.4 is the floor — we rely on QFile::moveToTrash (6.3+),
# std::as_const usage in 6.x signal arg sets, and the Charts theme APIs
# present in 6.4. Pin the minimum explicitly so configure fails early on
# older Qt rather than producing cryptic build errors deep in moc/codegen.
find_package(Qt6 6.4 COMPONENTS Core Gui Widgets Charts Svg Concurrent Network Test REQUIRED)

# Setting the minimum C++ standard
set(CMAKE_CXX_STANDARD           17)
set(CMAKE_CXX_EXTENSIONS         YES)
set(CMAKE_CXX_STANDARD_REQUIRED  YES)

# ============================================================================
# Release build hardening (Linux + macOS) — SSO-3397 / audit WI-34
# ============================================================================
# Distribution packagers (e.g. the .deb channel via linux/debian/rules) layer
# their own `DEB_BUILD_MAINT_OPTIONS = hardening=+all` on top. The AppImage,
# raw tarball, and macOS .app otherwise inherit toolchain defaults, so we set
# an explicit baseline here that matches what hardened distros expect:
#   - -fstack-protector-strong   stack-canary on functions with risky frames
#   - -D_FORTIFY_SOURCE=2        runtime checks for unsafe libc calls (needs -O>=1)
#   - -Wl,-z,relro,-z,now        full RELRO + immediate symbol binding (Linux)
#   - -Wl,-z,noexecstack         non-executable stack (Linux)
#   - -pie                       position-independent executable (Linux)
#   - -Wl,-bind_at_load          immediate symbol binding (macOS equivalent of -z,now)
#   - CMAKE_POSITION_INDEPENDENT_CODE ON   -fPIE for the executable and -fPIC for libs
option(NEXIS_ENABLE_HARDENING "Enable build hardening flags for Release builds" ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NEXIS_ENABLE_HARDENING
   AND (CMAKE_BUILD_TYPE STREQUAL "Release"
        OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"
        OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
   AND (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
        OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
        OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"))

  # -U then -D so a distro that already injects -D_FORTIFY_SOURCE doesn't warn
  # about redefinition (Debian's dpkg-buildflags sets =2, so this is a no-op
  # in that channel; for AppImage/tarball/macOS it provides the floor).
  add_compile_options(-fstack-protector-strong -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2)

  if(APPLE)
    add_link_options(-Wl,-bind_at_load)
  elseif(UNIX)
    add_link_options(-Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -pie)
  endif()

  message(STATUS "Nexis: build hardening enabled for ${CMAKE_BUILD_TYPE} (${CMAKE_CXX_COMPILER_ID})")
endif()

# Platform directory: macos/ or linux/
if(APPLE)
  enable_language(OBJCXX)
  set(PLATFORM_DIR "${PROJECT_ROOT}/macos")
else()
  set(PLATFORM_DIR "${PROJECT_ROOT}/linux")
endif()

# ============================================================================
# nexis-core (static library) — shared + platform sources
# ============================================================================
set(CORE_SHARED_DIR "${SHARED_DIR}/nexis-core")
set(CORE_PLAT_DIR   "${PLATFORM_DIR}/nexis-core")

# Source files are listed explicitly (not globbed). When adding or removing
# a .cpp or .h file, update the appropriate set() block below.

set(CORE_SHARED_SRCS
  "${CORE_SHARED_DIR}/Info/battery_info_shared.cpp"
  "${CORE_SHARED_DIR}/Info/cpu_info_shared.cpp"
  "${CORE_SHARED_DIR}/Info/disk_health_info_shared.cpp"
  "${CORE_SHARED_DIR}/Info/disk_info_shared.cpp"
  "${CORE_SHARED_DIR}/Info/fan_info_shared.cpp"
  "${CORE_SHARED_DIR}/Info/gpu_info_shared.cpp"
  "${CORE_SHARED_DIR}/Info/memory_info_shared.cpp"
  "${CORE_SHARED_DIR}/Info/power_profile_info_shared.cpp"
  "${CORE_SHARED_DIR}/Info/process.cpp"
  "${CORE_SHARED_DIR}/Info/process_info_shared.cpp"
  "${CORE_SHARED_DIR}/Info/system_info_shared.cpp"
  "${CORE_SHARED_DIR}/Info/thermal_info_shared.cpp"
  "${CORE_SHARED_DIR}/Tools/apt_source_tool_shared.cpp"
  "${CORE_SHARED_DIR}/Tools/repo_knowledge_base.cpp"
  "${CORE_SHARED_DIR}/Tools/repo_health_checker.cpp"
  "${CORE_SHARED_DIR}/Tools/repo_repair_engine.cpp"
  "${CORE_SHARED_DIR}/Tools/docker_tool.cpp"
  "${CORE_SHARED_DIR}/Tools/file_search_tool_shared.cpp"
  "${CORE_SHARED_DIR}/Tools/package_tool_shared.cpp"
  "${CORE_SHARED_DIR}/Tools/service_tool_shared.cpp"
  "${CORE_SHARED_DIR}/Utils/command_util_shared.cpp"
  "${CORE_SHARED_DIR}/Utils/display_server_util.cpp"
  "${CORE_SHARED_DIR}/Utils/file_util.cpp"
  "${CORE_SHARED_DIR}/Utils/format_util.cpp"
  "${CORE_SHARED_DIR}/Utils/headless_util.cpp"
  "${CORE_SHARED_DIR}/Utils/macos_xattr_util.cpp"
)

set(CORE_SHARED_HDRS
  "${CORE_SHARED_DIR}/nexis-core_global.h"
  "${CORE_SHARED_DIR}/Info/battery_info.h"
  "${CORE_SHARED_DIR}/Info/cpu_info.h"
  "${CORE_SHARED_DIR}/Info/disk_health_info.h"
  "${CORE_SHARED_DIR}/Info/disk_info.h"
  "${CORE_SHARED_DIR}/Info/gpu_info.h"
  "${CORE_SHARED_DIR}/Info/memory_info.h"
  "${CORE_SHARED_DIR}/Info/power_profile_info.h"
  "${CORE_SHARED_DIR}/Info/network_info.h"
  "${CORE_SHARED_DIR}/Info/oomd_snapshot.h"
  "${CORE_SHARED_DIR}/Info/process.h"
  "${CORE_SHARED_DIR}/Info/process_info.h"
  "${CORE_SHARED_DIR}/Info/boot_analysis_info.h"
  "${CORE_SHARED_DIR}/Info/startup_info.h"
  "${CORE_SHARED_DIR}/Info/system_info.h"
  "${CORE_SHARED_DIR}/Info/thermal_info.h"
  "${CORE_SHARED_DIR}/Info/fan_info.h"
  "${CORE_SHARED_DIR}/Info/update_info.h"
  "${CORE_SHARED_DIR}/Tools/apt_source_tool.h"
  "${CORE_SHARED_DIR}/Tools/repository_tool.h"
  "${CORE_SHARED_DIR}/Tools/repo_health_types.h"
  "${CORE_SHARED_DIR}/Tools/repo_knowledge_base.h"
  "${CORE_SHARED_DIR}/Tools/repo_health_checker.h"
  "${CORE_SHARED_DIR}/Tools/repo_repair_engine.h"
  "${CORE_SHARED_DIR}/Tools/docker_tool.h"
  "${CORE_SHARED_DIR}/Tools/file_search_tool.h"
  "${CORE_SHARED_DIR}/Tools/gnome_settings_tool.h"
  "${CORE_SHARED_DIR}/Tools/package_tool_shared.h"
  "${CORE_SHARED_DIR}/Tools/service_tool.h"
  "${CORE_SHARED_DIR}/Utils/command_util.h"
  "${CORE_SHARED_DIR}/Utils/display_server_util.h"
  "${CORE_SHARED_DIR}/Utils/file_util.h"
  "${CORE_SHARED_DIR}/Utils/format_util.h"
  "${CORE_SHARED_DIR}/Utils/headless_util.h"
  "${CORE_SHARED_DIR}/Utils/macos_xattr_util.h"
)

if(APPLE)
  set(CORE_PLAT_SRCS
    "${CORE_PLAT_DIR}/Info/battery_info.cpp"
    "${CORE_PLAT_DIR}/Info/cpu_info.cpp"
    "${CORE_PLAT_DIR}/Info/disk_health_info.cpp"
    "${CORE_PLAT_DIR}/Info/disk_info_platform.cpp"
    "${CORE_PLAT_DIR}/Info/gpu_info.cpp"
    "${CORE_PLAT_DIR}/Info/macos_plist_parser.cpp"
    "${CORE_PLAT_DIR}/Info/memory_info.cpp"
    "${CORE_PLAT_DIR}/Info/network_info.cpp"
    "${CORE_PLAT_DIR}/Info/nettop_streamer.cpp"
    "${CORE_PLAT_DIR}/Info/power_profile_info.cpp"
    "${CORE_PLAT_DIR}/Info/process_info.cpp"
    "${CORE_PLAT_DIR}/Info/boot_analysis_info.cpp"
    "${CORE_PLAT_DIR}/Info/btm_parser.cpp"
    "${CORE_PLAT_DIR}/Info/startup_info.cpp"
    "${CORE_PLAT_DIR}/Info/system_info.cpp"
    "${CORE_PLAT_DIR}/Info/thermal_info.cpp"
    "${CORE_PLAT_DIR}/Info/fan_info.cpp"
    "${CORE_PLAT_DIR}/Info/update_info.cpp"
    "${CORE_PLAT_DIR}/Tools/homebrew_tool.cpp"
    "${CORE_PLAT_DIR}/Tools/crumbs_scanner.cpp"
    "${CORE_PLAT_DIR}/Tools/file_search_tool.cpp"
    "${CORE_PLAT_DIR}/Tools/gnome_settings_tool.cpp"
    "${CORE_PLAT_DIR}/Tools/package_tool.cpp"
    "${CORE_PLAT_DIR}/Tools/repo_health_checker.cpp"
    "${CORE_PLAT_DIR}/Tools/repo_repair_engine.cpp"
    "${CORE_PLAT_DIR}/Tools/service_tool.cpp"
    "${CORE_PLAT_DIR}/Utils/command_util_platform.cpp"
    "${CORE_PLAT_DIR}/Utils/plist_util.cpp"
  )
  set(CORE_PLAT_HDRS
    "${CORE_PLAT_DIR}/Info/battery_info_macos.h"
    "${CORE_PLAT_DIR}/Info/cpu_info_macos.h"
    "${CORE_PLAT_DIR}/Info/disk_health_info_macos.h"
    "${CORE_PLAT_DIR}/Info/disk_info_macos.h"
    "${CORE_PLAT_DIR}/Info/gpu_info_macos.h"
    "${CORE_PLAT_DIR}/Info/macos_plist_parser.h"
    "${CORE_PLAT_DIR}/Info/memory_info_macos.h"
    "${CORE_PLAT_DIR}/Info/network_info_macos.h"
    "${CORE_PLAT_DIR}/Info/nettop_streamer.h"
    "${CORE_PLAT_DIR}/Info/power_profile_info_macos.h"
    "${CORE_PLAT_DIR}/Info/process_info_macos.h"
    "${CORE_PLAT_DIR}/Info/boot_analysis_info_macos.h"
    "${CORE_PLAT_DIR}/Info/btm_parser.h"
    "${CORE_PLAT_DIR}/Info/startup_info_macos.h"
    "${CORE_PLAT_DIR}/Info/system_info_macos.h"
    "${CORE_PLAT_DIR}/Info/thermal_info_macos.h"
    "${CORE_PLAT_DIR}/Info/fan_info_macos.h"
    "${CORE_PLAT_DIR}/Info/update_info_macos.h"
    "${CORE_PLAT_DIR}/Tools/homebrew_tool_macos.h"
    "${CORE_PLAT_DIR}/Tools/crumbs_scanner.h"
    "${CORE_PLAT_DIR}/Tools/file_search_tool_macos.h"
    "${CORE_PLAT_DIR}/Tools/gnome_settings_constants.h"
    "${CORE_PLAT_DIR}/Tools/gnome_settings_tool_macos.h"
    "${CORE_PLAT_DIR}/Tools/package_tool_macos.h"
    "${CORE_PLAT_DIR}/Tools/repo_health_checker_macos.h"
    "${CORE_PLAT_DIR}/Tools/repo_repair_engine_macos.h"
    "${CORE_PLAT_DIR}/Tools/service_tool_macos.h"
    "${CORE_PLAT_DIR}/Utils/brew_util.h"
    "${CORE_PLAT_DIR}/Utils/plist_util.h"
  )
else()
  set(CORE_PLAT_SRCS
    "${CORE_PLAT_DIR}/Info/battery_info.cpp"
    "${CORE_PLAT_DIR}/Info/battery_charge_threshold.cpp"
    "${CORE_PLAT_DIR}/Info/cpu_info.cpp"
    "${CORE_PLAT_DIR}/Info/cpu_tuning.cpp"
    "${CORE_PLAT_DIR}/Info/disk_health_info.cpp"
    "${CORE_PLAT_DIR}/Info/disk_info_platform.cpp"
    "${CORE_PLAT_DIR}/Info/gpu_info.cpp"
    "${CORE_PLAT_DIR}/Info/memory_info.cpp"
    "${CORE_PLAT_DIR}/Info/network_info.cpp"
    "${CORE_PLAT_DIR}/Info/oomd_info.cpp"
    "${CORE_PLAT_DIR}/Info/oomd_info_parser.cpp"
    "${CORE_PLAT_DIR}/Info/nvidia_smi_cache.cpp"
    "${CORE_PLAT_DIR}/Info/nvidia_smi_pmon_streamer.cpp"
    "${CORE_PLAT_DIR}/Info/nvidia_smi_streamer.cpp"
    "${CORE_PLAT_DIR}/Info/power_profile_info.cpp"
    "${CORE_PLAT_DIR}/Info/psi_info.cpp"
    "${CORE_PLAT_DIR}/Info/proc_info_parser.cpp"
    "${CORE_PLAT_DIR}/Info/process_info.cpp"
    "${CORE_PLAT_DIR}/Info/boot_analysis_info.cpp"
    "${CORE_PLAT_DIR}/Info/startup_info.cpp"
    "${CORE_PLAT_DIR}/Info/system_info.cpp"
    "${CORE_PLAT_DIR}/Info/thermal_info.cpp"
    "${CORE_PLAT_DIR}/Info/fan_info.cpp"
    "${CORE_PLAT_DIR}/Info/update_info.cpp"
    "${CORE_PLAT_DIR}/Tools/apt_source_tool.cpp"
    "${CORE_PLAT_DIR}/Tools/repo_health_checker.cpp"
    "${CORE_PLAT_DIR}/Tools/repo_repair_engine.cpp"
    "${CORE_PLAT_DIR}/Tools/file_search_tool.cpp"
    "${CORE_PLAT_DIR}/Tools/gnome_settings_tool.cpp"
    "${CORE_PLAT_DIR}/Tools/package_tool.cpp"
    "${CORE_PLAT_DIR}/Tools/service_tool.cpp"
    "${CORE_PLAT_DIR}/Utils/command_util_platform.cpp"
  )
  set(CORE_PLAT_HDRS
    "${CORE_PLAT_DIR}/Info/battery_info_linux.h"
    "${CORE_PLAT_DIR}/Info/battery_charge_threshold.h"
    "${CORE_PLAT_DIR}/Info/cpu_info_linux.h"
    "${CORE_PLAT_DIR}/Info/cpu_tuning.h"
    "${CORE_PLAT_DIR}/Info/disk_health_info_linux.h"
    "${CORE_PLAT_DIR}/Info/disk_info_linux.h"
    "${CORE_PLAT_DIR}/Info/gpu_info_linux.h"
    "${CORE_PLAT_DIR}/Info/memory_info_linux.h"
    "${CORE_PLAT_DIR}/Info/network_info_linux.h"
    "${CORE_PLAT_DIR}/Info/oomd_info_linux.h"
    "${CORE_PLAT_DIR}/Info/oomd_info_parser.h"
    "${CORE_PLAT_DIR}/Info/nvidia_smi_cache.h"
    "${CORE_PLAT_DIR}/Info/nvidia_smi_pmon_streamer.h"
    "${CORE_PLAT_DIR}/Info/nvidia_smi_streamer.h"
    "${CORE_PLAT_DIR}/Info/proc_info_parser.h"
    "${CORE_PLAT_DIR}/Info/psi_info.h"
    "${CORE_PLAT_DIR}/Info/process_info_linux.h"
    "${CORE_PLAT_DIR}/Info/boot_analysis_info_linux.h"
    "${CORE_PLAT_DIR}/Info/startup_info_linux.h"
    "${CORE_PLAT_DIR}/Info/system_info_linux.h"
    "${CORE_PLAT_DIR}/Info/thermal_info_linux.h"
    "${CORE_PLAT_DIR}/Info/fan_info_linux.h"
    "${CORE_PLAT_DIR}/Info/update_info_linux.h"
    "${CORE_PLAT_DIR}/Tools/apt_source_tool_linux.h"
    "${CORE_PLAT_DIR}/Tools/repo_health_checker_linux.h"
    "${CORE_PLAT_DIR}/Tools/repo_repair_engine_linux.h"
    "${CORE_PLAT_DIR}/Tools/file_search_tool_linux.h"
    "${CORE_PLAT_DIR}/Tools/gnome_settings_constants.h"
    "${CORE_PLAT_DIR}/Tools/gnome_settings_tool_linux.h"
    "${CORE_PLAT_DIR}/Tools/package_tool_linux.h"
    "${CORE_PLAT_DIR}/Tools/service_tool_linux.h"
  )
endif()

add_library(nexis-core STATIC
  ${CORE_SHARED_SRCS} ${CORE_SHARED_HDRS}
  ${CORE_PLAT_SRCS}   ${CORE_PLAT_HDRS}
)

# Include dirs: platform first (so platform headers shadow shared when both exist).
# PUBLIC so that consumers (nexis executable) inherit these paths.
target_include_directories(nexis-core PUBLIC
  "${CORE_PLAT_DIR}"
  "${CORE_PLAT_DIR}/Info"
  "${CORE_PLAT_DIR}/Tools"
  "${CORE_PLAT_DIR}/Utils"
  "${CORE_SHARED_DIR}"
  "${CORE_SHARED_DIR}/Info"
  "${CORE_SHARED_DIR}/Tools"
  "${CORE_SHARED_DIR}/Utils"
)

target_compile_definitions(nexis-core PRIVATE NEXISCORE_LIBRARY QT_DEPRECATED_WARNINGS)
target_link_libraries(nexis-core Qt6::Core Qt6::Network Qt6::Concurrent)

# macOS-specific frameworks needed for system info APIs (IOKit, CoreFoundation)
if(APPLE)
  find_library(IOKIT_FRAMEWORK IOKit)
  find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
  target_link_libraries(nexis-core ${IOKIT_FRAMEWORK} ${COREFOUNDATION_FRAMEWORK})
endif()

# ============================================================================
# nexis-gui (static library) — shared + platform GUI sources (no main.cpp)
# Used by the nexis executable and by screenshot regression tests (FR-41).
# ============================================================================
set(GUI_SHARED_DIR "${SHARED_DIR}/nexis")
set(GUI_PLAT_DIR   "${PLATFORM_DIR}/nexis")

# Sources — include .h files for AUTOMOC to find Q_OBJECT classes.
# When adding or removing a source file, update the appropriate set() block.

set(GUI_SHARED_SRCS
  "${GUI_SHARED_DIR}/app.cpp"
  "${GUI_SHARED_DIR}/feedback.cpp"
  "${GUI_SHARED_DIR}/signal_mapper.cpp"
  "${GUI_SHARED_DIR}/sliding_stacked_widget.cpp"
  "${GUI_SHARED_DIR}/Managers/app_manager.cpp"
  "${GUI_SHARED_DIR}/Managers/cleaner_service.cpp"
  "${GUI_SHARED_DIR}/Managers/cleaning_profiles_service.cpp"
  "${GUI_SHARED_DIR}/Managers/data_refresh_service.cpp"
  "${GUI_SHARED_DIR}/Managers/dir_size_scanner.cpp"
  "${GUI_SHARED_DIR}/Managers/info_manager.cpp"
  "${GUI_SHARED_DIR}/Managers/tool_manager.cpp"
  "${GUI_SHARED_DIR}/Managers/schedule_manager.cpp"
  "${GUI_SHARED_DIR}/Managers/setting_manager.cpp"
  "${GUI_SHARED_DIR}/Services/file_search_service.cpp"
  "${GUI_SHARED_DIR}/Services/duplicate_finder_service.cpp"
  "${GUI_SHARED_DIR}/Services/host_service.cpp"
  "${GUI_SHARED_DIR}/Services/process_service.cpp"
  "${GUI_SHARED_DIR}/Services/startup_service.cpp"
  "${GUI_SHARED_DIR}/Services/system_service_manager.cpp"
  "${GUI_SHARED_DIR}/Services/docker_service.cpp"
  "${GUI_SHARED_DIR}/Services/package_service.cpp"
  "${GUI_SHARED_DIR}/Services/snapshot_service.cpp"
  "${GUI_SHARED_DIR}/Managers/process_prefs_manager.cpp"
  # APT-specific GUI components live under linux/ only — they're built into
  # GUI_PLAT_SRCS on Linux. On macOS the Homebrew page replaces them.
  "${GUI_SHARED_DIR}/Pages/Dashboard/circlebar.cpp"
  "${GUI_SHARED_DIR}/Pages/Dashboard/dashboard_page.cpp"
  "${GUI_SHARED_DIR}/Pages/Dashboard/linebar.cpp"
  "${GUI_SHARED_DIR}/Pages/Dashboard/metric_tile_base.cpp"
  "${GUI_SHARED_DIR}/Pages/Dashboard/metric_tile.cpp"
  "${GUI_SHARED_DIR}/Pages/Dashboard/network_tile.cpp"
  "${GUI_SHARED_DIR}/Pages/Dashboard/disk_tile.cpp"
  "${GUI_SHARED_DIR}/Pages/Dashboard/gauge_tile.cpp"
  "${GUI_SHARED_DIR}/Pages/Dashboard/ring_tile.cpp"
  "${GUI_SHARED_DIR}/Pages/Dashboard/hybrid_tile.cpp"
  "${GUI_SHARED_DIR}/Pages/Dashboard/speedometer_tile.cpp"
  "${GUI_SHARED_DIR}/Pages/Dashboard/vumeter_tile.cpp"
  "${GUI_SHARED_DIR}/Pages/Dashboard/health_score_calculator.cpp"
  "${GUI_SHARED_DIR}/Pages/Dashboard/health_score_tile.cpp"
  "${GUI_SHARED_DIR}/Pages/Dashboard/dashboard_tile_wrapper.cpp"
  "${GUI_SHARED_DIR}/Pages/Dashboard/maintenance_wizard_dialog.cpp"
  "${GUI_SHARED_DIR}/command_palette.cpp"
  "${GUI_SHARED_DIR}/Pages/Docker/docker_page.cpp"
  "${GUI_SHARED_DIR}/Pages/GnomeSettings/gnome_appearance_tab.cpp"
  "${GUI_SHARED_DIR}/Pages/GnomeSettings/gnome_desktop_tab.cpp"
  "${GUI_SHARED_DIR}/Pages/GnomeSettings/gnome_mouse_tab.cpp"
  "${GUI_SHARED_DIR}/Pages/GnomeSettings/gnome_settings_page.cpp"
  "${GUI_SHARED_DIR}/Pages/GnomeSettings/gnome_wm_tab.cpp"
  "${GUI_SHARED_DIR}/Pages/Helpers/helpers_page.cpp"
  "${GUI_SHARED_DIR}/Pages/Helpers/host_manage.cpp"
  "${GUI_SHARED_DIR}/Pages/Helpers/network_diag_widget.cpp"
  "${GUI_SHARED_DIR}/Pages/Helpers/open_ports_widget.cpp"
  "${GUI_SHARED_DIR}/Pages/Helpers/firewall_widget.cpp"
  "${GUI_SHARED_DIR}/Pages/Helpers/swappiness_widget.cpp"
  "${GUI_SHARED_DIR}/Pages/Helpers/cpu_tuning_widget.cpp"
  "${GUI_SHARED_DIR}/Pages/Helpers/battery_charge_threshold_widget.cpp"
  "${GUI_SHARED_DIR}/Pages/Helpers/oom_kills_widget.cpp"
  "${GUI_SHARED_DIR}/Pages/Helpers/trim_widget.cpp"
  "${GUI_SHARED_DIR}/Pages/Helpers/wol_widget.cpp"
  "${GUI_SHARED_DIR}/Pages/HardwareInfo/hardware_info_page.cpp"
  "${GUI_SHARED_DIR}/Pages/Processes/pin_sort_filter_proxy_model.cpp"
  "${GUI_SHARED_DIR}/Pages/Processes/process_alert_dialog.cpp"
  "${GUI_SHARED_DIR}/Pages/Processes/processes_page.cpp"
  "${GUI_SHARED_DIR}/Pages/Resources/disk_treemap_dialog.cpp"
  "${GUI_SHARED_DIR}/Pages/Resources/disk_usage_launcher_widget.cpp"
  "${GUI_SHARED_DIR}/Pages/Resources/history_chart.cpp"
  "${GUI_SHARED_DIR}/Pages/Resources/treemap_view.cpp"
  "${GUI_SHARED_DIR}/Pages/Resources/resources_page.cpp"
  "${GUI_SHARED_DIR}/Pages/Search/search_page.cpp"
  "${GUI_SHARED_DIR}/Pages/Services/service_item.cpp"
  "${GUI_SHARED_DIR}/Pages/Services/services_page.cpp"
  "${GUI_SHARED_DIR}/Pages/Settings/settings_page.cpp"
  "${GUI_SHARED_DIR}/Pages/BootAnalysis/boot_analysis_page.cpp"
  "${GUI_SHARED_DIR}/Pages/Network/network_usage_page.cpp"
  "${GUI_SHARED_DIR}/Pages/Network/net_usage_tracker.cpp"
  "${GUI_SHARED_DIR}/Pages/StartupApps/startup_app.cpp"
  "${GUI_SHARED_DIR}/Pages/StartupApps/startup_apps_page.cpp"
  "${GUI_SHARED_DIR}/Pages/SystemCleaner/byte_tree_widget.cpp"
  "${GUI_SHARED_DIR}/Pages/SystemCleaner/exclusion_manager_dialog.cpp"
  "${GUI_SHARED_DIR}/Pages/SystemCleaner/schedule_editor_dialog.cpp"
  "${GUI_SHARED_DIR}/Pages/SystemCleaner/system_cleaner_page.cpp"
  "${GUI_SHARED_DIR}/Pages/DiskTools/disk_tools_page.cpp"
  "${GUI_SHARED_DIR}/Pages/SystemLogs/log_provider.cpp"
  "${GUI_SHARED_DIR}/Pages/SystemLogs/system_logs_page.cpp"
  "${GUI_SHARED_DIR}/Pages/Uninstaller/uninstaller_page.cpp"
)

set(GUI_SHARED_HDRS
  "${GUI_SHARED_DIR}/app.h"
  "${GUI_SHARED_DIR}/dpi.h"
  "${GUI_SHARED_DIR}/feedback.h"
  "${GUI_SHARED_DIR}/nexis_page.h"
  "${GUI_SHARED_DIR}/nexis_roles.h"
  "${GUI_SHARED_DIR}/signal_mapper.h"
  "${GUI_SHARED_DIR}/sliding_stacked_widget.h"
  "${GUI_SHARED_DIR}/utilities.h"
  "${GUI_SHARED_DIR}/Managers/app_manager.h"
  "${GUI_SHARED_DIR}/Managers/cleaner_service.h"
  "${GUI_SHARED_DIR}/Managers/cleaning_profiles_service.h"
  "${GUI_SHARED_DIR}/Managers/data_refresh_service.h"
  "${GUI_SHARED_DIR}/Managers/dir_size_scanner.h"
  "${GUI_SHARED_DIR}/Managers/info_manager.h"
  "${GUI_SHARED_DIR}/Managers/schedule_manager.h"
  "${GUI_SHARED_DIR}/Managers/setting_manager.h"
  "${GUI_SHARED_DIR}/Managers/tool_manager.h"
  "${GUI_SHARED_DIR}/Services/file_search_service.h"
  "${GUI_SHARED_DIR}/Services/duplicate_finder_service.h"
  "${GUI_SHARED_DIR}/Services/host_service.h"
  "${GUI_SHARED_DIR}/Services/process_service.h"
  "${GUI_SHARED_DIR}/Services/startup_service.h"
  "${GUI_SHARED_DIR}/Services/system_service_manager.h"
  "${GUI_SHARED_DIR}/Services/docker_service.h"
  "${GUI_SHARED_DIR}/Services/package_service.h"
  "${GUI_SHARED_DIR}/Services/snapshot_service.h"
  "${GUI_SHARED_DIR}/Managers/process_prefs_manager.h"
  # APT-specific GUI headers live under linux/ only (see GUI_PLAT_HDRS).
  "${GUI_SHARED_DIR}/Pages/Dashboard/circlebar.h"
  "${GUI_SHARED_DIR}/Pages/Dashboard/dashboard_page.h"
  "${GUI_SHARED_DIR}/Pages/Dashboard/linebar.h"
  "${GUI_SHARED_DIR}/Pages/Dashboard/metric_tile_base.h"
  "${GUI_SHARED_DIR}/Pages/Dashboard/metric_tile.h"
  "${GUI_SHARED_DIR}/Pages/Dashboard/network_tile.h"
  "${GUI_SHARED_DIR}/Pages/Dashboard/disk_tile.h"
  "${GUI_SHARED_DIR}/Pages/Dashboard/gauge_tile.h"
  "${GUI_SHARED_DIR}/Pages/Dashboard/ring_tile.h"
  "${GUI_SHARED_DIR}/Pages/Dashboard/hybrid_tile.h"
  "${GUI_SHARED_DIR}/Pages/Dashboard/speedometer_tile.h"
  "${GUI_SHARED_DIR}/Pages/Dashboard/vumeter_tile.h"
  "${GUI_SHARED_DIR}/Pages/Dashboard/health_score_calculator.h"
  "${GUI_SHARED_DIR}/Pages/Dashboard/health_score_tile.h"
  "${GUI_SHARED_DIR}/Pages/Dashboard/dashboard_tile_wrapper.h"
  "${GUI_SHARED_DIR}/command_palette.h"
  "${GUI_SHARED_DIR}/Pages/Docker/docker_page.h"
  "${GUI_SHARED_DIR}/Pages/GnomeSettings/gnome_appearance_tab.h"
  "${GUI_SHARED_DIR}/Pages/GnomeSettings/gnome_desktop_tab.h"
  "${GUI_SHARED_DIR}/Pages/GnomeSettings/gnome_mouse_tab.h"
  "${GUI_SHARED_DIR}/Pages/GnomeSettings/gnome_settings_page.h"
  "${GUI_SHARED_DIR}/Pages/GnomeSettings/gnome_wm_tab.h"
  "${GUI_SHARED_DIR}/Pages/Helpers/helpers_page.h"
  "${GUI_SHARED_DIR}/Pages/Helpers/host_manage.h"
  "${GUI_SHARED_DIR}/Pages/Helpers/wol_widget.h"
  "${GUI_SHARED_DIR}/Pages/HardwareInfo/hardware_info_page.h"
  "${GUI_SHARED_DIR}/Pages/Processes/pin_sort_filter_proxy_model.h"
  "${GUI_SHARED_DIR}/Pages/Processes/process_alert_dialog.h"
  "${GUI_SHARED_DIR}/Pages/Processes/processes_page.h"
  "${GUI_SHARED_DIR}/Pages/Resources/disk_treemap_dialog.h"
  "${GUI_SHARED_DIR}/Pages/Resources/disk_usage_launcher_widget.h"
  "${GUI_SHARED_DIR}/Pages/Resources/history_chart.h"
  "${GUI_SHARED_DIR}/Pages/Resources/treemap_view.h"
  "${GUI_SHARED_DIR}/Pages/Resources/resources_page.h"
  "${GUI_SHARED_DIR}/Pages/Search/search_page.h"
  "${GUI_SHARED_DIR}/Pages/Services/service_item.h"
  "${GUI_SHARED_DIR}/Pages/Services/services_page.h"
  "${GUI_SHARED_DIR}/Pages/Settings/settings_page.h"
  "${GUI_SHARED_DIR}/Pages/BootAnalysis/boot_analysis_page.h"
  "${GUI_SHARED_DIR}/Pages/Network/network_usage_page.h"
  "${GUI_SHARED_DIR}/Pages/Network/net_usage_tracker.h"
  "${GUI_SHARED_DIR}/Pages/StartupApps/startup_app.h"
  "${GUI_SHARED_DIR}/Pages/StartupApps/startup_app_edit.h"
  "${GUI_SHARED_DIR}/Pages/StartupApps/startup_apps_page.h"
  "${GUI_SHARED_DIR}/Pages/SystemCleaner/byte_tree_widget.h"
  "${GUI_SHARED_DIR}/Pages/SystemCleaner/exclusion_manager_dialog.h"
  "${GUI_SHARED_DIR}/Pages/SystemCleaner/schedule_editor_dialog.h"
  "${GUI_SHARED_DIR}/Pages/SystemCleaner/system_cleaner_page.h"
  "${GUI_SHARED_DIR}/Pages/DiskTools/disk_tools_page.h"
  "${GUI_SHARED_DIR}/Pages/SystemLogs/log_provider.h"
  "${GUI_SHARED_DIR}/Pages/SystemLogs/system_logs_page.h"
  "${GUI_SHARED_DIR}/Pages/Uninstaller/uninstaller_page.h"
)

if(APPLE)
  set(GUI_PLAT_SRCS
    "${GUI_PLAT_DIR}/Pages/StartupApps/startup_app_edit.cpp"
    "${GUI_SHARED_DIR}/Pages/StartupApps/btm_reset_dialog.cpp"
    "${GUI_SHARED_DIR}/Pages/StartupApps/btm_row.cpp"
    "${GUI_PLAT_DIR}/Pages/Uninstaller/crumbs_review_dialog.cpp"
    "${GUI_PLAT_DIR}/Pages/Homebrew/homebrew_page.cpp"
    "${GUI_PLAT_DIR}/macos_dock_helper.mm"
  )
  set(GUI_PLAT_HDRS
    "${GUI_SHARED_DIR}/Pages/StartupApps/btm_reset_dialog.h"
    "${GUI_SHARED_DIR}/Pages/StartupApps/btm_row.h"
    "${GUI_PLAT_DIR}/Pages/Uninstaller/crumbs_review_dialog.h"
    "${GUI_PLAT_DIR}/Pages/Homebrew/homebrew_page.h"
    "${GUI_PLAT_DIR}/macos_dock_helper.h"
  )
else()
  set(GUI_PLAT_SRCS
    "${GUI_PLAT_DIR}/Pages/StartupApps/startup_app_edit.cpp"
    "${GUI_PLAT_DIR}/Pages/AptSourceManager/apt_source_edit.cpp"
    "${GUI_PLAT_DIR}/Pages/AptSourceManager/apt_source_manager_page.cpp"
    "${GUI_PLAT_DIR}/Pages/AptSourceManager/apt_source_repository_item.cpp"
    "${GUI_PLAT_DIR}/Pages/AptSourceManager/repo_detail_panel.cpp"
  )
  set(GUI_PLAT_HDRS
    "${GUI_PLAT_DIR}/Pages/AptSourceManager/apt_source_edit.h"
    "${GUI_PLAT_DIR}/Pages/AptSourceManager/apt_source_manager_page.h"
    "${GUI_PLAT_DIR}/Pages/AptSourceManager/apt_source_repository_item.h"
    "${GUI_PLAT_DIR}/Pages/AptSourceManager/repo_detail_panel.h"
  )
endif()

# Translations
set(NEXIS_TRANSLATIONS
  "${SHARED_DIR}/translations/nexis_af.ts"
  "${SHARED_DIR}/translations/nexis_ar.ts"
  "${SHARED_DIR}/translations/nexis_ca-es.ts"
  "${SHARED_DIR}/translations/nexis_cs.ts"
  "${SHARED_DIR}/translations/nexis_da.ts"
  "${SHARED_DIR}/translations/nexis_de.ts"
  "${SHARED_DIR}/translations/nexis_el.ts"
  "${SHARED_DIR}/translations/nexis_en.ts"
  "${SHARED_DIR}/translations/nexis_es.ts"
  "${SHARED_DIR}/translations/nexis_fi.ts"
  "${SHARED_DIR}/translations/nexis_fr.ts"
  "${SHARED_DIR}/translations/nexis_gl.ts"
  "${SHARED_DIR}/translations/nexis_he.ts"
  "${SHARED_DIR}/translations/nexis_hi.ts"
  "${SHARED_DIR}/translations/nexis_hu.ts"
  "${SHARED_DIR}/translations/nexis_it.ts"
  "${SHARED_DIR}/translations/nexis_ja.ts"
  "${SHARED_DIR}/translations/nexis_kn.ts"
  "${SHARED_DIR}/translations/nexis_ko.ts"
  "${SHARED_DIR}/translations/nexis_ml.ts"
  "${SHARED_DIR}/translations/nexis_nl.ts"
  "${SHARED_DIR}/translations/nexis_no.ts"
  "${SHARED_DIR}/translations/nexis_oc.ts"
  "${SHARED_DIR}/translations/nexis_pl.ts"
  "${SHARED_DIR}/translations/nexis_pt.ts"
  "${SHARED_DIR}/translations/nexis_ro.ts"
  "${SHARED_DIR}/translations/nexis_ru.ts"
  "${SHARED_DIR}/translations/nexis_sr.ts"
  "${SHARED_DIR}/translations/nexis_sv.ts"
  "${SHARED_DIR}/translations/nexis_tr.ts"
  "${SHARED_DIR}/translations/nexis_ua.ts"
  "${SHARED_DIR}/translations/nexis_vn.ts"
  "${SHARED_DIR}/translations/nexis_zh-cn.ts"
  "${SHARED_DIR}/translations/nexis_zh-tw.ts"
)
find_package(Qt6 COMPONENTS LinguistTools REQUIRED)
# Compile the committed .ts files into .qm. Use qt_add_translation (lrelease
# only) rather than qt_create_translation: source-string extraction via lupdate
# is handled by the dedicated lupdate.yml workflow, so the build must not
# regenerate the tracked .ts files. (Also note: qt_create_translation only
# emits .qm for arguments ending in .ts — passing the bare variable name
# NEXIS_TRANSLATIONS instead of ${NEXIS_TRANSLATIONS} silently produced zero
# .qm files, which is why translations were missing from every package.)
qt_add_translation(QM_FILES ${NEXIS_TRANSLATIONS})
set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${QM_FILES}")

install(
  FILES ${QM_FILES}
  DESTINATION share/nexis/translations
  CONFIGURATIONS Release RelWithDebInfo MinSizeRel
)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Tell AUTOUIC to search shared UI file directories
set(CMAKE_AUTOUIC_SEARCH_PATHS
  "${GUI_SHARED_DIR}"
  "${GUI_SHARED_DIR}/Pages/BootAnalysis"
  "${GUI_SHARED_DIR}/Pages/Network"
  "${GUI_SHARED_DIR}/Pages/Dashboard"
  "${GUI_SHARED_DIR}/Pages/Processes"
  "${GUI_SHARED_DIR}/Pages/Resources"
  "${GUI_SHARED_DIR}/Pages/Services"
  "${GUI_SHARED_DIR}/Pages/Settings"
  "${GUI_SHARED_DIR}/Pages/StartupApps"
  "${GUI_SHARED_DIR}/Pages/SystemCleaner"
  "${GUI_SHARED_DIR}/Pages/DiskTools"
  "${GUI_SHARED_DIR}/Pages/Uninstaller"
  "${GUI_SHARED_DIR}/Pages/Helpers"
  "${GUI_SHARED_DIR}/Pages/GnomeSettings"
  "${GUI_SHARED_DIR}/Pages/Search"
  "${GUI_SHARED_DIR}/Pages/HardwareInfo"
  "${GUI_SHARED_DIR}/Pages/Docker"
)
if(NOT APPLE)
  list(APPEND CMAKE_AUTOUIC_SEARCH_PATHS "${GUI_PLAT_DIR}/Pages/AptSourceManager")
endif()

# --- nexis-gui static library (everything except main.cpp) ---
add_library(nexis-gui STATIC
  ${GUI_SHARED_SRCS}
  ${GUI_SHARED_HDRS}
  ${GUI_PLAT_SRCS}
  ${GUI_PLAT_HDRS}
  "${GUI_SHARED_DIR}/static.qrc"
  "${GUI_SHARED_DIR}/cleaning_profiles.qrc"
)

# Include dirs are PUBLIC so that consumers (nexis executable, screenshot tests)
# inherit them without re-listing.
target_include_directories(nexis-gui PUBLIC
  "${GUI_PLAT_DIR}"
  "${GUI_PLAT_DIR}/Managers"
  "${GUI_PLAT_DIR}/Pages/StartupApps"
  "${GUI_PLAT_DIR}/Pages/SystemCleaner"
  "${GUI_PLAT_DIR}/Pages/Search"
  "${GUI_PLAT_DIR}/Pages/Settings"
  "${GUI_PLAT_DIR}/Pages/Uninstaller"
  "${GUI_SHARED_DIR}"
  "${GUI_SHARED_DIR}/Managers"
  "${GUI_SHARED_DIR}/Services"
  "${GUI_SHARED_DIR}/Pages/BootAnalysis"
  "${GUI_SHARED_DIR}/Pages/Network"
  "${GUI_SHARED_DIR}/Pages/Dashboard"
  "${GUI_SHARED_DIR}/Pages/Processes"
  "${GUI_SHARED_DIR}/Pages/Resources"
  "${GUI_SHARED_DIR}/Pages/Services"
  "${GUI_SHARED_DIR}/Pages/Settings"
  "${GUI_SHARED_DIR}/Pages/StartupApps"
  "${GUI_SHARED_DIR}/Pages/SystemCleaner"
  "${GUI_SHARED_DIR}/Pages/DiskTools"
  "${GUI_SHARED_DIR}/Pages/Uninstaller"
  "${GUI_SHARED_DIR}/Pages/Helpers"
  "${GUI_SHARED_DIR}/Pages/GnomeSettings"
  "${GUI_SHARED_DIR}/Pages/Search"
  "${GUI_SHARED_DIR}/Pages/HardwareInfo"
  "${GUI_SHARED_DIR}/Pages/Docker"
  "${CMAKE_BINARY_DIR}"
)
if(APPLE)
  target_include_directories(nexis-gui PUBLIC "${GUI_PLAT_DIR}/Pages/Homebrew")
else()
  target_include_directories(nexis-gui PUBLIC "${GUI_PLAT_DIR}/Pages/AptSourceManager")
endif()

if(APP_VERSION_OVERRIDE AND NOT APP_VERSION_OVERRIDE STREQUAL "")
  set(APP_VERSION "${APP_VERSION_OVERRIDE}")
else()
  set(APP_VERSION "${PROJECT_VERSION}")
endif()
message(STATUS "App version: ${APP_VERSION}")

target_compile_definitions(nexis-gui PUBLIC QT_DEPRECATED_WARNINGS "APP_VERSION=\"${APP_VERSION}\"")
target_link_libraries(nexis-gui PUBLIC
  nexis-core Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Charts Qt6::Svg Qt6::Concurrent
)

if(APPLE)
  find_library(APPKIT_FRAMEWORK AppKit)
  target_link_libraries(nexis-gui PUBLIC ${APPKIT_FRAMEWORK})
endif()

# Strict aliasing fix for Qt + LTO
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"
   OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang"
   OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
  target_compile_options(nexis-gui PRIVATE "-fno-strict-aliasing")
endif()

# --- nexis executable (thin wrapper: just main.cpp + translations + icon) ---

# macOS app icon
if(APPLE)
  set(MACOS_ICON "${CMAKE_SOURCE_DIR}/macos/nexis.icns")
  set_source_files_properties("${MACOS_ICON}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
endif()

add_executable(nexis
  "${GUI_SHARED_DIR}/main.cpp"
  ${QM_FILES}
  $<$<PLATFORM_ID:Darwin>:${MACOS_ICON}>
)

target_link_libraries(nexis nexis-gui)

# Stage .qm files next to the binary for dev/in-tree runs.
# cmake --install handles packages; this covers build-tree execution where
# applicationDirPath()/translations would otherwise never exist.
add_custom_command(TARGET nexis POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${EXECUTABLE_OUTPUT_PATH}translations"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${QM_FILES} "${EXECUTABLE_OUTPUT_PATH}translations/"
    COMMENT "Staging .qm files for in-tree runs"
)

# ============================================================================
# Tests (Qt Test + CTest)
# ============================================================================
option(BUILD_TESTING "Build the test suite" ON)
if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()

# ============================================================================
# Install rules
# ============================================================================
# SSO-3399: install rules used to be CONFIGURATIONS-gated to Release/
# RelWithDebInfo/MinSizeRel, which made `cmake --install` a silent no-op on
# the default Debug build (CXXBasics defaults CMAKE_BUILD_TYPE to Debug).
# Distros and the release workflow both pass an explicit Release type, so
# we don't need that gate — drop it so Debug installs also work.
if(APPLE)
  # macOS: Build as a .app bundle
  set_target_properties(nexis PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.s4solutionsllc.Nexis"
    MACOSX_BUNDLE_BUNDLE_NAME "Nexis"
    # APP_VERSION respects -DAPP_VERSION_OVERRIDE from release.yml; PROJECT_VERSION does not.
    MACOSX_BUNDLE_BUNDLE_VERSION "${APP_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${APP_VERSION}"
    MACOSX_BUNDLE_ICON_FILE "nexis"
  )

  install(
    TARGETS nexis
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
  )
else()
  # Linux: Install binary, .desktop file, and icon
  install(
    TARGETS nexis
    RUNTIME DESTINATION bin
  )

  install(
    FILES "${PLATFORM_DIR}/applications/nexis.desktop"
    DESTINATION share/applications
  )

  foreach(_size 16x16 32x32 48x48 64x64 128x128 256x256)
    install(
      FILES "${CMAKE_SOURCE_DIR}/icons/hicolor/${_size}/apps/nexis.png"
      DESTINATION "share/icons/hicolor/${_size}/apps"
    )
  endforeach()

  install(
    FILES "${PLATFORM_DIR}/metainfo/io.github.s4solutionsllc.Nexis.metainfo.xml"
    DESTINATION share/metainfo
  )

  install(
    FILES
      "${SHARED_DIR}/nexis/static/font/LICENSE-OFL.txt"
      "${SHARED_DIR}/nexis/static/font/LICENSE-UFL.txt"
    DESTINATION share/doc/nexis
    CONFIGURATIONS Release RelWithDebInfo MinSizeRel
  )
endif()
