
cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(stacer)

set(MANAGERS_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/Managers")
set(PAGES_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/Pages")

include_directories(
  "${PROJECT_ROOT}/stacer-core"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${MANAGERS_DIR}"
  "${PAGES_DIR}/Dashboard"
  "${PAGES_DIR}/Processes"
  "${PAGES_DIR}/Resources"
  "${PAGES_DIR}/Services"
  "${PAGES_DIR}/Settings"
  "${PAGES_DIR}/StartupApps"
  "${PAGES_DIR}/SystemCleaner"
  "${PAGES_DIR}/Uninstaller"
  "${PAGES_DIR}/GnomeSettings"
  "${CMAKE_CURRENT_BINARY_DIR}" # Necessary for CMake 3.7 and older
)

# Sources
file(GLOB_RECURSE ${PROJECT_NAME}_srcs         "${CMAKE_CURRENT_SOURCE_DIR}/**.cpp")
file(GLOB_RECURSE ${PROJECT_NAME}_translations "${PROJECT_ROOT}/translations/**.ts")

# Translations
find_package(Qt6 COMPONENTS LinguistTools)
qt_create_translation(QM_FILES  ${PROJECT_NAME}_translations  ${${PROJECT_NAME}_srcs})
set_directory_properties(PROPERTIES  ADDITIONAL_MAKE_CLEAN_FILES  "${QM_FILES}")

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_executable(${PROJECT_NAME}
  ${${PROJECT_NAME}_srcs}
  "${CMAKE_CURRENT_SOURCE_DIR}/static.qrc"
  ${QM_FILES}
)

target_link_libraries(${PROJECT_NAME}
  stacer-core  Qt6::Core  Qt6::Gui  Qt6::Widgets  Qt6::Charts  Qt6::Svg  Qt6::Concurrent
)

# Strict aliasing and LTO interact poorly with Qt's internal type-punning.
# -fno-strict-aliasing is needed to prevent miscompilation under LTO.
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  target_compile_options(${PROJECT_NAME}  PRIVATE  "-fno-strict-aliasing")
endif()

if(APPLE)
  # macOS: Build as a .app bundle
  set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.stacer.app"
    MACOSX_BUNDLE_BUNDLE_NAME "Stacer"
    MACOSX_BUNDLE_BUNDLE_VERSION "2.0.1"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "2.0.1"
  )

  install(
    TARGETS  ${PROJECT_NAME}
    CONFIGURATIONS Release RelWithDebInfo MinSizeRel
    BUNDLE  DESTINATION  .
    RUNTIME  DESTINATION  bin
  )
else()
  install(
    TARGETS  ${PROJECT_NAME}
    CONFIGURATIONS Release RelWithDebInfo MinSizeRel # Not allowing to install an unoptimized build
    RUNTIME  DESTINATION  bin
  )

  install(
    FILES           "${PROJECT_ROOT}/applications/stacer.desktop"
    DESTINATION     share/applications
    CONFIGURATIONS  Release RelWithDebInfo MinSizeRel
  )

  install(
    FILES           "${PROJECT_ROOT}/stacer/static/logo.png"
    DESTINATION     share/icons
    CONFIGURATIONS  Release RelWithDebInfo MinSizeRel
    RENAME          stacer.png
  )
endif()
