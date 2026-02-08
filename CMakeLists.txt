cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(Stacer VERSION 2.0.1)

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
find_package(Qt6 COMPONENTS Core Gui Widgets Charts Svg Concurrent REQUIRED)

# Setting the minimum C++ standard and passing the Qt-specific define
set(CMAKE_CXX_STANDARD           17)
set(CMAKE_CXX_EXTENSIONS         YES)
set(CMAKE_CXX_STANDARD_REQUIRED  YES)
add_definitions(-DQT_DEPRECATED_WARNINGS)

# Platform directory: macos/ or linux/
if(APPLE)
  set(PLATFORM_DIR "${PROJECT_ROOT}/macos")
else()
  set(PLATFORM_DIR "${PROJECT_ROOT}/linux")
endif()

# ============================================================================
# stacer-core (static library) — shared + platform sources
# ============================================================================
set(CORE_SHARED_DIR "${SHARED_DIR}/stacer-core")
set(CORE_PLAT_DIR   "${PLATFORM_DIR}/stacer-core")

# Include dirs: platform first (so platform headers shadow shared when both exist)
include_directories(
  "${CORE_PLAT_DIR}"
  "${CORE_PLAT_DIR}/Info"
  "${CORE_PLAT_DIR}/Tools"
  "${CORE_PLAT_DIR}/Utils"
  "${CORE_SHARED_DIR}"
  "${CORE_SHARED_DIR}/Info"
  "${CORE_SHARED_DIR}/Tools"
  "${CORE_SHARED_DIR}/Utils"
)

file(GLOB_RECURSE CORE_SHARED_SRCS "${CORE_SHARED_DIR}/**.cpp")
file(GLOB_RECURSE CORE_SHARED_HDRS "${CORE_SHARED_DIR}/**.h")
file(GLOB_RECURSE CORE_PLAT_SRCS   "${CORE_PLAT_DIR}/**.cpp")
file(GLOB_RECURSE CORE_PLAT_HDRS   "${CORE_PLAT_DIR}/**.h")

add_definitions(-DSTACERCORE_LIBRARY)

find_package(Qt6 COMPONENTS Core Network REQUIRED)

add_library(stacer-core STATIC
  ${CORE_SHARED_SRCS} ${CORE_SHARED_HDRS}
  ${CORE_PLAT_SRCS}   ${CORE_PLAT_HDRS}
)
target_link_libraries(stacer-core Qt6::Core Qt6::Network)

# macOS-specific frameworks needed for system info APIs (IOKit, CoreFoundation)
if(APPLE)
  find_library(IOKIT_FRAMEWORK IOKit)
  find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
  target_link_libraries(stacer-core ${IOKIT_FRAMEWORK} ${COREFOUNDATION_FRAMEWORK})
endif()

# ============================================================================
# stacer (executable) — shared + platform GUI sources
# ============================================================================
set(GUI_SHARED_DIR "${SHARED_DIR}/stacer")
set(GUI_PLAT_DIR   "${PLATFORM_DIR}/stacer")

# Include dirs (platform first, then shared)
include_directories(
  "${GUI_PLAT_DIR}"
  "${GUI_PLAT_DIR}/Managers"
  "${GUI_PLAT_DIR}/Pages/StartupApps"
  "${GUI_PLAT_DIR}/Pages/SystemCleaner"
  "${GUI_PLAT_DIR}/Pages/Search"
  "${GUI_PLAT_DIR}/Pages/Settings"
  "${GUI_SHARED_DIR}"
  "${GUI_SHARED_DIR}/Managers"
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
  "${CMAKE_BINARY_DIR}"
)

# Sources (include .h files for AUTOMOC to find Q_OBJECT classes)
file(GLOB_RECURSE GUI_SHARED_SRCS "${GUI_SHARED_DIR}/**.cpp")
file(GLOB_RECURSE GUI_SHARED_HDRS "${GUI_SHARED_DIR}/**.h")
file(GLOB_RECURSE GUI_PLAT_SRCS   "${GUI_PLAT_DIR}/**.cpp")

# Translations
file(GLOB_RECURSE STACER_TRANSLATIONS "${SHARED_DIR}/translations/**.ts")
find_package(Qt6 COMPONENTS LinguistTools)
qt_create_translation(QM_FILES STACER_TRANSLATIONS ${GUI_SHARED_SRCS} ${GUI_PLAT_SRCS})
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
)

add_executable(stacer
  ${GUI_SHARED_SRCS}
  ${GUI_SHARED_HDRS}
  ${GUI_PLAT_SRCS}
  "${GUI_SHARED_DIR}/static.qrc"
  ${QM_FILES}
)

target_link_libraries(stacer
  stacer-core Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Charts Qt6::Svg Qt6::Concurrent
)

# Strict aliasing fix for Qt + LTO
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"
   OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang"
   OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
  target_compile_options(stacer PRIVATE "-fno-strict-aliasing")
endif()

# ============================================================================
# Install rules
# ============================================================================
if(APPLE)
  # macOS: Build as a .app bundle
  set_target_properties(stacer PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.stacer.app"
    MACOSX_BUNDLE_BUNDLE_NAME "Stacer"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
  )

  install(
    TARGETS stacer
    CONFIGURATIONS Release RelWithDebInfo MinSizeRel
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
  )
else()
  # Linux: Install binary, .desktop file, and icon
  install(
    TARGETS stacer
    CONFIGURATIONS Release RelWithDebInfo MinSizeRel
    RUNTIME DESTINATION bin
  )

  install(
    FILES "${PLATFORM_DIR}/applications/stacer.desktop"
    DESTINATION share/applications
    CONFIGURATIONS Release RelWithDebInfo MinSizeRel
  )

  install(
    FILES "${GUI_SHARED_DIR}/static/logo.png"
    DESTINATION share/icons
    CONFIGURATIONS Release RelWithDebInfo MinSizeRel
    RENAME stacer.png
  )
endif()
