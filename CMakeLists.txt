cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(Nexis VERSION 2.0.2)

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
find_package(Qt6 COMPONENTS Core Gui Widgets Charts Svg Concurrent Network Test REQUIRED)

# Setting the minimum C++ standard
set(CMAKE_CXX_STANDARD           17)
set(CMAKE_CXX_EXTENSIONS         YES)
set(CMAKE_CXX_STANDARD_REQUIRED  YES)

# Platform directory: macos/ or linux/
if(APPLE)
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
  "${CORE_SHARED_DIR}/Info/disk_health_info_shared.cpp"
  "${CORE_SHARED_DIR}/Info/disk_info_shared.cpp"
  "${CORE_SHARED_DIR}/Info/gpu_info_shared.cpp"
  "${CORE_SHARED_DIR}/Info/memory_info_shared.cpp"
  "${CORE_SHARED_DIR}/Info/process.cpp"
  "${CORE_SHARED_DIR}/Info/process_info_shared.cpp"
  "${CORE_SHARED_DIR}/Info/system_info_shared.cpp"
  "${CORE_SHARED_DIR}/Tools/docker_tool.cpp"
  "${CORE_SHARED_DIR}/Tools/file_search_tool_shared.cpp"
  "${CORE_SHARED_DIR}/Tools/package_tool_shared.cpp"
  "${CORE_SHARED_DIR}/Tools/service_tool_shared.cpp"
  "${CORE_SHARED_DIR}/Utils/command_util_shared.cpp"
  "${CORE_SHARED_DIR}/Utils/file_util.cpp"
  "${CORE_SHARED_DIR}/Utils/format_util.cpp"
)

set(CORE_SHARED_HDRS
  "${CORE_SHARED_DIR}/nexis-core_global.h"
  "${CORE_SHARED_DIR}/Info/battery_info.h"
  "${CORE_SHARED_DIR}/Info/cpu_info.h"
  "${CORE_SHARED_DIR}/Info/disk_health_info.h"
  "${CORE_SHARED_DIR}/Info/disk_info.h"
  "${CORE_SHARED_DIR}/Info/gpu_info.h"
  "${CORE_SHARED_DIR}/Info/memory_info.h"
  "${CORE_SHARED_DIR}/Info/network_info.h"
  "${CORE_SHARED_DIR}/Info/process.h"
  "${CORE_SHARED_DIR}/Info/process_info.h"
  "${CORE_SHARED_DIR}/Info/startup_info.h"
  "${CORE_SHARED_DIR}/Info/system_info.h"
  "${CORE_SHARED_DIR}/Info/thermal_info.h"
  "${CORE_SHARED_DIR}/Tools/apt_source_tool.h"
  "${CORE_SHARED_DIR}/Tools/docker_tool.h"
  "${CORE_SHARED_DIR}/Tools/file_search_tool.h"
  "${CORE_SHARED_DIR}/Tools/gnome_settings_tool.h"
  "${CORE_SHARED_DIR}/Tools/package_tool_shared.h"
  "${CORE_SHARED_DIR}/Tools/service_tool.h"
  "${CORE_SHARED_DIR}/Utils/command_util.h"
  "${CORE_SHARED_DIR}/Utils/file_util.h"
  "${CORE_SHARED_DIR}/Utils/format_util.h"
)

if(APPLE)
  set(CORE_PLAT_SRCS
    "${CORE_PLAT_DIR}/Info/battery_info.cpp"
    "${CORE_PLAT_DIR}/Info/cpu_info.cpp"
    "${CORE_PLAT_DIR}/Info/disk_health_info.cpp"
    "${CORE_PLAT_DIR}/Info/disk_info_platform.cpp"
    "${CORE_PLAT_DIR}/Info/gpu_info.cpp"
    "${CORE_PLAT_DIR}/Info/memory_info.cpp"
    "${CORE_PLAT_DIR}/Info/network_info.cpp"
    "${CORE_PLAT_DIR}/Info/process_info.cpp"
    "${CORE_PLAT_DIR}/Info/startup_info.cpp"
    "${CORE_PLAT_DIR}/Info/system_info.cpp"
    "${CORE_PLAT_DIR}/Info/thermal_info.cpp"
    "${CORE_PLAT_DIR}/Tools/apt_source_tool.cpp"
    "${CORE_PLAT_DIR}/Tools/file_search_tool.cpp"
    "${CORE_PLAT_DIR}/Tools/gnome_settings_tool.cpp"
    "${CORE_PLAT_DIR}/Tools/package_tool.cpp"
    "${CORE_PLAT_DIR}/Tools/service_tool.cpp"
    "${CORE_PLAT_DIR}/Utils/command_util_platform.cpp"
  )
  set(CORE_PLAT_HDRS
    "${CORE_PLAT_DIR}/Info/battery_info_macos.h"
    "${CORE_PLAT_DIR}/Info/cpu_info_macos.h"
    "${CORE_PLAT_DIR}/Info/disk_health_info_macos.h"
    "${CORE_PLAT_DIR}/Info/disk_info_macos.h"
    "${CORE_PLAT_DIR}/Info/gpu_info_macos.h"
    "${CORE_PLAT_DIR}/Info/memory_info_macos.h"
    "${CORE_PLAT_DIR}/Info/network_info_macos.h"
    "${CORE_PLAT_DIR}/Info/process_info_macos.h"
    "${CORE_PLAT_DIR}/Info/startup_info_macos.h"
    "${CORE_PLAT_DIR}/Info/system_info_macos.h"
    "${CORE_PLAT_DIR}/Info/thermal_info_macos.h"
    "${CORE_PLAT_DIR}/Tools/apt_source_tool_macos.h"
    "${CORE_PLAT_DIR}/Tools/file_search_tool_macos.h"
    "${CORE_PLAT_DIR}/Tools/gnome_settings_constants.h"
    "${CORE_PLAT_DIR}/Tools/gnome_settings_tool_macos.h"
    "${CORE_PLAT_DIR}/Tools/package_tool_macos.h"
    "${CORE_PLAT_DIR}/Tools/service_tool_macos.h"
    "${CORE_PLAT_DIR}/Utils/brew_util.h"
  )
else()
  set(CORE_PLAT_SRCS
    "${CORE_PLAT_DIR}/Info/battery_info.cpp"
    "${CORE_PLAT_DIR}/Info/cpu_info.cpp"
    "${CORE_PLAT_DIR}/Info/disk_health_info.cpp"
    "${CORE_PLAT_DIR}/Info/disk_info_platform.cpp"
    "${CORE_PLAT_DIR}/Info/gpu_info.cpp"
    "${CORE_PLAT_DIR}/Info/memory_info.cpp"
    "${CORE_PLAT_DIR}/Info/network_info.cpp"
    "${CORE_PLAT_DIR}/Info/process_info.cpp"
    "${CORE_PLAT_DIR}/Info/startup_info.cpp"
    "${CORE_PLAT_DIR}/Info/system_info.cpp"
    "${CORE_PLAT_DIR}/Info/thermal_info.cpp"
    "${CORE_PLAT_DIR}/Tools/apt_source_tool.cpp"
    "${CORE_PLAT_DIR}/Tools/file_search_tool.cpp"
    "${CORE_PLAT_DIR}/Tools/gnome_settings_tool.cpp"
    "${CORE_PLAT_DIR}/Tools/package_tool.cpp"
    "${CORE_PLAT_DIR}/Tools/service_tool.cpp"
    "${CORE_PLAT_DIR}/Utils/command_util_platform.cpp"
  )
  set(CORE_PLAT_HDRS
    "${CORE_PLAT_DIR}/Info/battery_info_linux.h"
    "${CORE_PLAT_DIR}/Info/cpu_info_linux.h"
    "${CORE_PLAT_DIR}/Info/disk_health_info_linux.h"
    "${CORE_PLAT_DIR}/Info/disk_info_linux.h"
    "${CORE_PLAT_DIR}/Info/gpu_info_linux.h"
    "${CORE_PLAT_DIR}/Info/memory_info_linux.h"
    "${CORE_PLAT_DIR}/Info/network_info_linux.h"
    "${CORE_PLAT_DIR}/Info/process_info_linux.h"
    "${CORE_PLAT_DIR}/Info/startup_info_linux.h"
    "${CORE_PLAT_DIR}/Info/system_info_linux.h"
    "${CORE_PLAT_DIR}/Info/thermal_info_linux.h"
    "${CORE_PLAT_DIR}/Tools/apt_source_tool_linux.h"
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
target_link_libraries(nexis-core Qt6::Core Qt6::Network)

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
  "${GUI_SHARED_DIR}/Managers/data_refresh_service.cpp"
  "${GUI_SHARED_DIR}/Managers/info_manager.cpp"
  "${GUI_SHARED_DIR}/Managers/tool_manager.cpp"
  "${GUI_SHARED_DIR}/Managers/schedule_manager.cpp"
  "${GUI_SHARED_DIR}/Managers/setting_manager.cpp"
  "${GUI_SHARED_DIR}/Services/file_search_service.cpp"
  "${GUI_SHARED_DIR}/Services/host_service.cpp"
  "${GUI_SHARED_DIR}/Services/process_service.cpp"
  "${GUI_SHARED_DIR}/Services/startup_service.cpp"
  "${GUI_SHARED_DIR}/Services/system_service_manager.cpp"
  "${GUI_SHARED_DIR}/Services/docker_service.cpp"
  "${GUI_SHARED_DIR}/Services/package_service.cpp"
  "${GUI_SHARED_DIR}/Pages/AptSourceManager/apt_source_edit.cpp"
  "${GUI_SHARED_DIR}/Pages/AptSourceManager/apt_source_manager_page.cpp"
  "${GUI_SHARED_DIR}/Pages/AptSourceManager/apt_source_repository_item.cpp"
  "${GUI_SHARED_DIR}/Pages/Dashboard/circlebar.cpp"
  "${GUI_SHARED_DIR}/Pages/Dashboard/dashboard_page.cpp"
  "${GUI_SHARED_DIR}/Pages/Dashboard/linebar.cpp"
  "${GUI_SHARED_DIR}/Pages/Dashboard/metric_tile.cpp"
  "${GUI_SHARED_DIR}/Pages/Dashboard/network_tile.cpp"
  "${GUI_SHARED_DIR}/Pages/Dashboard/disk_tile.cpp"
  "${GUI_SHARED_DIR}/command_palette.cpp"
  "${GUI_SHARED_DIR}/Pages/Docker/docker_page.cpp"
  "${GUI_SHARED_DIR}/Pages/GnomeSettings/gnome_appearance_tab.cpp"
  "${GUI_SHARED_DIR}/Pages/GnomeSettings/gnome_desktop_tab.cpp"
  "${GUI_SHARED_DIR}/Pages/GnomeSettings/gnome_mouse_tab.cpp"
  "${GUI_SHARED_DIR}/Pages/GnomeSettings/gnome_settings_page.cpp"
  "${GUI_SHARED_DIR}/Pages/GnomeSettings/gnome_wm_tab.cpp"
  "${GUI_SHARED_DIR}/Pages/Helpers/helpers_page.cpp"
  "${GUI_SHARED_DIR}/Pages/Helpers/host_manage.cpp"
  "${GUI_SHARED_DIR}/Pages/HardwareInfo/hardware_info_page.cpp"
  "${GUI_SHARED_DIR}/Pages/Processes/processes_page.cpp"
  "${GUI_SHARED_DIR}/Pages/Resources/disk_usage_launcher_widget.cpp"
  "${GUI_SHARED_DIR}/Pages/Resources/history_chart.cpp"
  "${GUI_SHARED_DIR}/Pages/Resources/resources_page.cpp"
  "${GUI_SHARED_DIR}/Pages/Search/search_page.cpp"
  "${GUI_SHARED_DIR}/Pages/Services/service_item.cpp"
  "${GUI_SHARED_DIR}/Pages/Services/services_page.cpp"
  "${GUI_SHARED_DIR}/Pages/Settings/settings_page.cpp"
  "${GUI_SHARED_DIR}/Pages/StartupApps/startup_app.cpp"
  "${GUI_SHARED_DIR}/Pages/StartupApps/startup_apps_page.cpp"
  "${GUI_SHARED_DIR}/Pages/SystemCleaner/byte_tree_widget.cpp"
  "${GUI_SHARED_DIR}/Pages/SystemCleaner/schedule_editor_dialog.cpp"
  "${GUI_SHARED_DIR}/Pages/SystemCleaner/system_cleaner_page.cpp"
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
  "${GUI_SHARED_DIR}/Managers/data_refresh_service.h"
  "${GUI_SHARED_DIR}/Managers/info_manager.h"
  "${GUI_SHARED_DIR}/Managers/schedule_manager.h"
  "${GUI_SHARED_DIR}/Managers/setting_manager.h"
  "${GUI_SHARED_DIR}/Managers/tool_manager.h"
  "${GUI_SHARED_DIR}/Services/file_search_service.h"
  "${GUI_SHARED_DIR}/Services/host_service.h"
  "${GUI_SHARED_DIR}/Services/process_service.h"
  "${GUI_SHARED_DIR}/Services/startup_service.h"
  "${GUI_SHARED_DIR}/Services/system_service_manager.h"
  "${GUI_SHARED_DIR}/Services/docker_service.h"
  "${GUI_SHARED_DIR}/Services/package_service.h"
  "${GUI_SHARED_DIR}/Pages/AptSourceManager/apt_source_edit.h"
  "${GUI_SHARED_DIR}/Pages/AptSourceManager/apt_source_manager_page.h"
  "${GUI_SHARED_DIR}/Pages/AptSourceManager/apt_source_repository_item.h"
  "${GUI_SHARED_DIR}/Pages/Dashboard/circlebar.h"
  "${GUI_SHARED_DIR}/Pages/Dashboard/dashboard_page.h"
  "${GUI_SHARED_DIR}/Pages/Dashboard/linebar.h"
  "${GUI_SHARED_DIR}/Pages/Dashboard/metric_tile.h"
  "${GUI_SHARED_DIR}/Pages/Dashboard/network_tile.h"
  "${GUI_SHARED_DIR}/Pages/Dashboard/disk_tile.h"
  "${GUI_SHARED_DIR}/command_palette.h"
  "${GUI_SHARED_DIR}/Pages/Docker/docker_page.h"
  "${GUI_SHARED_DIR}/Pages/GnomeSettings/gnome_appearance_tab.h"
  "${GUI_SHARED_DIR}/Pages/GnomeSettings/gnome_desktop_tab.h"
  "${GUI_SHARED_DIR}/Pages/GnomeSettings/gnome_mouse_tab.h"
  "${GUI_SHARED_DIR}/Pages/GnomeSettings/gnome_settings_page.h"
  "${GUI_SHARED_DIR}/Pages/GnomeSettings/gnome_wm_tab.h"
  "${GUI_SHARED_DIR}/Pages/Helpers/helpers_page.h"
  "${GUI_SHARED_DIR}/Pages/Helpers/host_manage.h"
  "${GUI_SHARED_DIR}/Pages/HardwareInfo/hardware_info_page.h"
  "${GUI_SHARED_DIR}/Pages/Processes/processes_page.h"
  "${GUI_SHARED_DIR}/Pages/Resources/disk_usage_launcher_widget.h"
  "${GUI_SHARED_DIR}/Pages/Resources/history_chart.h"
  "${GUI_SHARED_DIR}/Pages/Resources/resources_page.h"
  "${GUI_SHARED_DIR}/Pages/Search/search_page.h"
  "${GUI_SHARED_DIR}/Pages/Services/service_item.h"
  "${GUI_SHARED_DIR}/Pages/Services/services_page.h"
  "${GUI_SHARED_DIR}/Pages/Settings/settings_page.h"
  "${GUI_SHARED_DIR}/Pages/StartupApps/startup_app.h"
  "${GUI_SHARED_DIR}/Pages/StartupApps/startup_app_edit.h"
  "${GUI_SHARED_DIR}/Pages/StartupApps/startup_apps_page.h"
  "${GUI_SHARED_DIR}/Pages/SystemCleaner/byte_tree_widget.h"
  "${GUI_SHARED_DIR}/Pages/SystemCleaner/schedule_editor_dialog.h"
  "${GUI_SHARED_DIR}/Pages/SystemCleaner/system_cleaner_page.h"
  "${GUI_SHARED_DIR}/Pages/Uninstaller/uninstaller_page.h"
)

if(APPLE)
  set(GUI_PLAT_SRCS
    "${GUI_PLAT_DIR}/Pages/StartupApps/startup_app_edit.cpp"
  )
  set(GUI_PLAT_HDRS)
else()
  set(GUI_PLAT_SRCS
    "${GUI_PLAT_DIR}/Pages/StartupApps/startup_app_edit.cpp"
  )
  set(GUI_PLAT_HDRS)
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
find_package(Qt6 COMPONENTS LinguistTools)
qt_create_translation(QM_FILES NEXIS_TRANSLATIONS ${GUI_SHARED_SRCS} ${GUI_PLAT_SRCS})
set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${QM_FILES}")

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Tell AUTOUIC to search shared UI file directories
set(CMAKE_AUTOUIC_SEARCH_PATHS
  "${GUI_SHARED_DIR}"
  "${GUI_SHARED_DIR}/Pages/Dashboard"
  "${GUI_SHARED_DIR}/Pages/Processes"
  "${GUI_SHARED_DIR}/Pages/Resources"
  "${GUI_SHARED_DIR}/Pages/Services"
  "${GUI_SHARED_DIR}/Pages/Settings"
  "${GUI_SHARED_DIR}/Pages/StartupApps"
  "${GUI_SHARED_DIR}/Pages/SystemCleaner"
  "${GUI_SHARED_DIR}/Pages/Uninstaller"
  "${GUI_SHARED_DIR}/Pages/Helpers"
  "${GUI_SHARED_DIR}/Pages/AptSourceManager"
  "${GUI_SHARED_DIR}/Pages/GnomeSettings"
  "${GUI_SHARED_DIR}/Pages/Search"
  "${GUI_SHARED_DIR}/Pages/HardwareInfo"
  "${GUI_SHARED_DIR}/Pages/Docker"
)

# --- nexis-gui static library (everything except main.cpp) ---
add_library(nexis-gui STATIC
  ${GUI_SHARED_SRCS}
  ${GUI_SHARED_HDRS}
  ${GUI_PLAT_SRCS}
  ${GUI_PLAT_HDRS}
  "${GUI_SHARED_DIR}/static.qrc"
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
  "${GUI_SHARED_DIR}"
  "${GUI_SHARED_DIR}/Managers"
  "${GUI_SHARED_DIR}/Services"
  "${GUI_SHARED_DIR}/Pages/Dashboard"
  "${GUI_SHARED_DIR}/Pages/Processes"
  "${GUI_SHARED_DIR}/Pages/Resources"
  "${GUI_SHARED_DIR}/Pages/Services"
  "${GUI_SHARED_DIR}/Pages/Settings"
  "${GUI_SHARED_DIR}/Pages/StartupApps"
  "${GUI_SHARED_DIR}/Pages/SystemCleaner"
  "${GUI_SHARED_DIR}/Pages/Uninstaller"
  "${GUI_SHARED_DIR}/Pages/Helpers"
  "${GUI_SHARED_DIR}/Pages/AptSourceManager"
  "${GUI_SHARED_DIR}/Pages/GnomeSettings"
  "${GUI_SHARED_DIR}/Pages/Search"
  "${GUI_SHARED_DIR}/Pages/HardwareInfo"
  "${GUI_SHARED_DIR}/Pages/Docker"
  "${CMAKE_BINARY_DIR}"
)

set(APP_VERSION "${PROJECT_VERSION}")
message(STATUS "App version: ${APP_VERSION}")

target_compile_definitions(nexis-gui PUBLIC QT_DEPRECATED_WARNINGS "APP_VERSION=\"${APP_VERSION}\"")
target_link_libraries(nexis-gui PUBLIC
  nexis-core Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Charts Qt6::Svg Qt6::Concurrent
)

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
if(APPLE)
  # macOS: Build as a .app bundle
  set_target_properties(nexis PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.nexis.app"
    MACOSX_BUNDLE_BUNDLE_NAME "Nexis"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_ICON_FILE "nexis"
  )

  install(
    TARGETS nexis
    CONFIGURATIONS Release RelWithDebInfo MinSizeRel
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
  )
else()
  # Linux: Install binary, .desktop file, and icon
  install(
    TARGETS nexis
    CONFIGURATIONS Release RelWithDebInfo MinSizeRel
    RUNTIME DESTINATION bin
  )

  install(
    FILES "${PLATFORM_DIR}/applications/nexis.desktop"
    DESTINATION share/applications
    CONFIGURATIONS Release RelWithDebInfo MinSizeRel
  )

  foreach(_size 16x16 32x32 48x48 64x64 128x128 256x256)
    install(
      FILES "${CMAKE_SOURCE_DIR}/icons/hicolor/${_size}/apps/nexis.png"
      DESTINATION "share/icons/hicolor/${_size}/apps"
      CONFIGURATIONS Release RelWithDebInfo MinSizeRel
    )
  endforeach()
endif()
