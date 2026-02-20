# Test sources are listed explicitly. When adding a test file, update this list.
#
# Each test file gets its own executable because QTEST_MAIN() generates main().
# Use the add_nexis_test() macro to register new tests.

macro(add_nexis_test)
  cmake_parse_arguments(ARG "" "NAME" "SOURCES;LIBS;INCLUDES" ${ARGN})
  add_executable(test-${ARG_NAME} ${ARG_SOURCES})
  target_link_libraries(test-${ARG_NAME} nexis-core Qt6::Test ${ARG_LIBS})
  target_compile_definitions(test-${ARG_NAME} PRIVATE
    PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
  )
  if(ARG_INCLUDES)
    target_include_directories(test-${ARG_NAME} PRIVATE ${ARG_INCLUDES})
  endif()
  add_test(NAME ${ARG_NAME} COMMAND test-${ARG_NAME})
endmacro()

set(GUI_SHARED_DIR "${CMAKE_SOURCE_DIR}/shared/nexis")

# ── Utility tests ──────────────────────────────────────────────────────────────
add_nexis_test(NAME FormatUtilTests   SOURCES utils/test_format_util.cpp)
add_nexis_test(NAME FileUtilTests     SOURCES utils/test_file_util.cpp)
add_nexis_test(NAME CommandUtilTests  SOURCES utils/test_command_util.cpp)

# ── Core library tests ─────────────────────────────────────────────────────────
add_nexis_test(NAME DiskHealthTests   SOURCES core/test_disk_health_info.cpp)

# ── Manager tests ──────────────────────────────────────────────────────────────
# ScheduleManager lives in the GUI target; we compile its sources directly
# along with SettingManager (its only dependency beyond nexis-core).
add_nexis_test(NAME ScheduleTests
  SOURCES
    managers/test_schedule_manager.cpp
    "${GUI_SHARED_DIR}/Managers/schedule_manager.cpp"
    "${GUI_SHARED_DIR}/Managers/setting_manager.cpp"
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Managers"
)

# ── Theme validation tests ─────────────────────────────────────────────────────
add_nexis_test(NAME ThemeTokenTests   SOURCES theme/test_theme_tokens.cpp)

# ── Screenshot regression tests (FR-41) ───────────────────────────────────────
# Links against the full nexis-gui library (not just nexis-core) because it
# instantiates the App window with all pages, managers, and themes.
add_executable(test-ScreenshotTests screenshots/test_screenshots.cpp)
target_link_libraries(test-ScreenshotTests nexis-gui Qt6::Test)
target_compile_definitions(test-ScreenshotTests PRIVATE
  PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
add_test(NAME ScreenshotTests COMMAND test-ScreenshotTests)
