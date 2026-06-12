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
add_nexis_test(NAME CommandUtilTests       SOURCES utils/test_command_util.cpp)
add_nexis_test(NAME CommandUtilAsyncTests  SOURCES utils/test_command_util_async.cpp LIBS Qt6::Concurrent)
add_nexis_test(NAME HeadlessUtilTests SOURCES utils/test_headless_util.cpp)
add_nexis_test(NAME DisplayServerUtilTests SOURCES utils/test_display_server_util.cpp)

# ── Core library tests ─────────────────────────────────────────────────────────
add_nexis_test(NAME DiskHealthTests   SOURCES core/test_disk_health_info.cpp)
add_nexis_test(NAME MemoryInfoTests   SOURCES core/test_memory_info.cpp)
add_nexis_test(NAME CpuInfoTests      SOURCES core/test_cpu_info.cpp)
add_nexis_test(NAME GpuInfoTests      SOURCES core/test_gpu_info.cpp)
add_nexis_test(NAME UpdateInfoTests
  SOURCES
    core/test_update_info.cpp
    "${CMAKE_SOURCE_DIR}/linux/nexis-core/Info/update_info.cpp"
  INCLUDES
    "${CMAKE_SOURCE_DIR}/linux/nexis-core"
    "${CMAKE_SOURCE_DIR}/linux/nexis-core/Info"
)
add_nexis_test(NAME AptSourceTests    SOURCES core/test_apt_source_tool.cpp)
add_nexis_test(NAME RepoKnowledgeBaseTests SOURCES core/test_repo_knowledge_base.cpp)
add_nexis_test(NAME RepoHealthCheckerTests SOURCES core/test_repo_health_checker.cpp LIBS Qt6::Network)
add_nexis_test(NAME RepoRepairEngineTests SOURCES core/test_repo_repair_engine.cpp LIBS Qt6::Network)
add_nexis_test(NAME FanInfoTests      SOURCES core/test_fan_info.cpp)
add_nexis_test(NAME ThermalInfoTests  SOURCES core/test_thermal_info.cpp)
add_nexis_test(NAME BatteryInfoTests  SOURCES core/test_battery_info.cpp)
add_nexis_test(NAME DiskInfoTests     SOURCES core/test_disk_info.cpp)
add_nexis_test(NAME PowerProfileTests SOURCES core/test_power_profile_info.cpp)
add_nexis_test(NAME PackageToolTests  SOURCES core/test_package_tool.cpp)
add_nexis_test(NAME DockerToolTests   SOURCES core/test_docker_tool.cpp)
# SSO-3383 / WI-21 / audit M2: off-thread updateProcesses() + publish pair.
add_nexis_test(NAME ProcessInfoTests  SOURCES core/test_process_info.cpp LIBS Qt6::Concurrent)

# SSO-3366: PackageToolMacOS::trashApps injection guard. PackageToolMacOS is
# only built into nexis-core on Darwin, so the test is registered only there.
if(APPLE)
  add_nexis_test(NAME PackageToolMacOSTests
    SOURCES core/test_package_tool_macos.cpp)

  # SSO-3385 (WI-23): proves the AppleSMC connection is opened exactly once
  # across the process even under concurrent callers. The test seam free
  # functions are defined in macos/nexis-core/Info/thermal_info.cpp and
  # only exist on macOS.
  add_nexis_test(NAME ThermalInfoMacOSTests
    SOURCES core/test_thermal_info_macos.cpp)
endif()

# FR-127: pure parser for /proc/<pid>/*. Compiled directly here so the test
# runs on any platform, even though the parser is only wired into the real
# nexis-core build on Linux.
add_nexis_test(NAME ProcInfoParserTests
  SOURCES
    core/test_proc_info_parser.cpp
    "${CMAKE_SOURCE_DIR}/linux/nexis-core/Info/proc_info_parser.cpp"
  INCLUDES
    "${CMAKE_SOURCE_DIR}/linux/nexis-core/Info"
)

# FW-11 (SSO-3739): pure parsers for systemd-oomd + cgroup v2 inputs.
# Compiled directly so the test runs cross-platform even though the producer
# (OomdInfoLinux) is only wired into the real nexis-core build on Linux.
add_nexis_test(NAME OomdInfoParserTests
  SOURCES
    core/test_oomd_info_parser.cpp
    "${CMAKE_SOURCE_DIR}/linux/nexis-core/Info/oomd_info_parser.cpp"
  INCLUDES
    "${CMAKE_SOURCE_DIR}/linux/nexis-core/Info"
    "${CMAKE_SOURCE_DIR}/shared/nexis-core/Info"
)

# WI-33: macOS live-tool parsers (nettop CSV, diskutil plist, sysctl
# kern.boottime). Compiled directly here so the tests run on any platform
# even though the parsers are only wired into the real nexis-core build
# on macOS.
add_nexis_test(NAME MacosParserTests
  SOURCES
    core/test_macos_parsers.cpp
    "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info/nettop_streamer.cpp"
    "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info/macos_plist_parser.cpp"
    "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info/boot_analysis_info.cpp"
  INCLUDES
    "${CMAKE_SOURCE_DIR}/macos/nexis-core"
    "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info"
)

# SSO-3738 / FW-10: sfltool dumpbtm parser. Compiled directly here per FR-127
# so the tests run on any platform even though `sfltool` itself is macOS-only.
add_nexis_test(NAME BtmParserTests
  SOURCES
    core/test_btm_parser.cpp
    "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info/btm_parser.cpp"
  INCLUDES
    "${CMAKE_SOURCE_DIR}/macos/nexis-core"
    "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info"
)

# WI-33: PackageTool uninstall command-construction tests. Subclasses the
# Linux + macOS package tools and overrides the runSudoCommand/runCommand
# seam on the PackageTool base. Compiles both platform sources directly so
# we get coverage on any host — only the live commands themselves are
# platform-specific.
add_nexis_test(NAME PackageToolUninstallTests
  SOURCES
    core/test_package_tool_uninstall.cpp
    "${CMAKE_SOURCE_DIR}/linux/nexis-core/Tools/package_tool.cpp"
    "${CMAKE_SOURCE_DIR}/macos/nexis-core/Tools/package_tool.cpp"
    "${CMAKE_SOURCE_DIR}/macos/nexis-core/Utils/plist_util.cpp"
  INCLUDES
    "${CMAKE_SOURCE_DIR}/linux/nexis-core"
    "${CMAKE_SOURCE_DIR}/linux/nexis-core/Tools"
    "${CMAKE_SOURCE_DIR}/macos/nexis-core"
    "${CMAKE_SOURCE_DIR}/macos/nexis-core/Tools"
    "${CMAKE_SOURCE_DIR}/macos/nexis-core/Utils"
)

# FW-07 (SSO-3735): apt-3.1 transaction history + why parsers + the
# PackageToolLinux argv-construction seam for history-list / -info / -undo
# / -rollback and why / why-not. Compiles the Linux platform source directly
# so the test is platform-agnostic — only the live commands run on Linux.
add_nexis_test(NAME AptHistoryTests
  SOURCES
    core/test_apt_history.cpp
    "${CMAKE_SOURCE_DIR}/linux/nexis-core/Tools/package_tool.cpp"
  INCLUDES
    "${CMAKE_SOURCE_DIR}/linux/nexis-core"
    "${CMAKE_SOURCE_DIR}/linux/nexis-core/Tools"
)

# SSO-351: pure parser for /proc/net/route — compiled directly so the test
# runs on any platform.
if(UNIX AND NOT APPLE)
  add_nexis_test(NAME NetworkInfoTests
    SOURCES
      core/test_network_info.cpp
      "${CMAKE_SOURCE_DIR}/linux/nexis-core/Info/network_info.cpp"
    INCLUDES
      "${CMAKE_SOURCE_DIR}/linux/nexis-core/Info"
    LIBS
      Qt6::Network
  )
endif()

# NetworkDiagWidget lives in the GUI target; compile its source + deps directly.
add_nexis_test(NAME NetworkDiagTests
  SOURCES
    core/test_network_diag.cpp
    "${GUI_SHARED_DIR}/Pages/Helpers/network_diag_widget.cpp"
    "${GUI_SHARED_DIR}/Managers/app_manager.cpp"
    "${GUI_SHARED_DIR}/Managers/info_manager.cpp"
    "${GUI_SHARED_DIR}/Managers/setting_manager.cpp"
    "${GUI_SHARED_DIR}/signal_mapper.cpp"
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Pages/Helpers"
    "${GUI_SHARED_DIR}/Managers"
  LIBS
    Qt6::Widgets Qt6::Network
)

# OpenPortsWidget lives in the GUI target; compile its source + deps directly.
add_nexis_test(NAME OpenPortsTests
  SOURCES
    core/test_open_ports.cpp
    "${GUI_SHARED_DIR}/Pages/Helpers/open_ports_widget.cpp"
    "${GUI_SHARED_DIR}/Managers/app_manager.cpp"
    "${GUI_SHARED_DIR}/Managers/info_manager.cpp"
    "${GUI_SHARED_DIR}/Managers/setting_manager.cpp"
    "${GUI_SHARED_DIR}/signal_mapper.cpp"
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Pages/Helpers"
    "${GUI_SHARED_DIR}/Managers"
  LIBS
    Qt6::Widgets Qt6::Network
)

# FirewallWidget lives in the GUI target; compile its source + deps directly.
add_nexis_test(NAME FirewallTests
  SOURCES
    core/test_firewall.cpp
    "${GUI_SHARED_DIR}/Pages/Helpers/firewall_widget.cpp"
    "${GUI_SHARED_DIR}/Managers/app_manager.cpp"
    "${GUI_SHARED_DIR}/Managers/info_manager.cpp"
    "${GUI_SHARED_DIR}/Managers/setting_manager.cpp"
    "${GUI_SHARED_DIR}/signal_mapper.cpp"
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Pages/Helpers"
    "${GUI_SHARED_DIR}/Managers"
  LIBS
    Qt6::Widgets Qt6::Network
)

# ── Service tests ─────────────────────────────────────────────────────────────
# HostService lives in the GUI target; compile its source directly.
add_nexis_test(NAME HostServiceTests
  SOURCES
    core/test_host_service.cpp
    "${GUI_SHARED_DIR}/Services/host_service.cpp"
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Services"
  LIBS
    Qt6::Network
)

# ── Manager tests ──────────────────────────────────────────────────────────────
# ScheduleManager lives in the GUI target; we compile its sources directly
# along with SettingManager (its only dependency beyond nexis-core).
# SSO-3390 / WI-28: platform-neutral RepositoryTool contract — exercises
# AptSourceToolLinux on Linux and HomebrewToolMacOS on macOS via the same
# interface, with backend availability checks for hosts where neither is
# installed.
add_nexis_test(NAME RepositoryToolTests
  SOURCES managers/test_repository_tool.cpp)

# SSO-3731 (FW-04, MX1): MacOsXattrUtil::stripQuarantine() — strips the
# com.apple.quarantine xattr from a plist before `launchctl load` on
# macOS 27. The helper is linked from nexis-core; the test body QSKIPs
# off-macOS because the xattr surface is platform-specific.
add_nexis_test(NAME MacOsXattrUtilTests
  SOURCES managers/test_macos_xattr_util.cpp)

add_nexis_test(NAME ScheduleTests
  SOURCES
    managers/test_schedule_manager.cpp
    "${GUI_SHARED_DIR}/Managers/schedule_manager.cpp"
    "${GUI_SHARED_DIR}/Managers/setting_manager.cpp"
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Managers"
)

# SettingManager — start-page id migration (SSO-3388). Pure unit test: we
# only need SettingManager itself; QStandardPaths is redirected to a temp
# dir via setTestModeEnabled() in initTestCase.
add_nexis_test(NAME SettingManagerTests
  SOURCES
    managers/test_setting_manager.cpp
    "${GUI_SHARED_DIR}/Managers/setting_manager.cpp"
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Managers"
)

# SSO-3391 / WI-29: pin GnomeSettingsToolMacOS down as a hard no-op stub so
# it can never write into Apple preference domains. Linux compiles the same
# test source but skips the platform-specific assertions.
add_nexis_test(NAME GnomeSettingsToolTests
  SOURCES managers/test_gnome_settings_tool.cpp)

# CleanerService exclusion logic — links against nexis-gui because
# CleanerService::scan()/clean() reference InfoManager and ToolManager.
# Tests only exercise isExcluded(), load/save/add/removeExclusion().
add_nexis_test(NAME CleanerExclusionTests
  SOURCES
    managers/test_cleaner_exclusions.cpp
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Managers"
  LIBS
    nexis-gui Qt6::Widgets
)

# SSO-3389 / WI-27: InfoManager facade wiring for BootAnalysisInfo +
# StartupInfo. Links against nexis-gui because InfoManager and the providers
# it owns are compiled into the GUI target.
add_nexis_test(NAME InfoManagerWiringTests
  SOURCES
    managers/test_info_manager_wiring.cpp
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Managers"
  LIBS
    nexis-gui Qt6::Widgets
)

# WI-08 / audit H9: CleanerService destructive paths (cleanFiles, cleanTrash,
# elevated branch via virtual seam). Pinned to nexis-gui like the exclusion
# tests because the implementation lives in the GUI library.
add_nexis_test(NAME CleanerServiceTests
  SOURCES
    managers/test_cleaner_service.cpp
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Managers"
  LIBS
    nexis-gui Qt6::Widgets
)

# SSO-3732 / FW-05: CleanerService macOS 27 cross-team container access-denial
# surface — pinned to nexis-gui for the same reason as CleanerServiceTests.
# The implementation lives in the GUI library (not nexis-core) so the test
# links against it; the Q_OS_MACOS gate inside the source file controls
# whether the real assertions run or a single QSKIP placeholder reports the
# platform mismatch on non-macOS hosts.
add_nexis_test(NAME CleanerAccessNeededTests
  SOURCES
    managers/test_cleaner_access_needed.cpp
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Managers"
  LIBS
    nexis-gui Qt6::Widgets
)

# FW-12: data-driven cleaning profiles. Loader and glob expansion are
# pure (no InfoManager/ToolManager touch), but the service lives in the
# GUI library alongside CleanerService, so we link nexis-gui here too.
add_nexis_test(NAME CleaningProfilesTests
  SOURCES
    managers/test_cleaning_profiles.cpp
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Managers"
  LIBS
    nexis-gui Qt6::Widgets
)

# SSO-3380 / WI-18: DataRefreshService::isCpuPayloadEmittable() guards
# the cpuUpdated emit against an empty producer payload. Links against
# nexis-gui because the predicate lives in the GUI-side service.
add_nexis_test(NAME DataRefreshServiceTests
  SOURCES
    managers/test_data_refresh_service.cpp
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Managers"
  LIBS
    nexis-gui Qt6::Widgets
)

# SSO-3365: FileSearchService — guard the UI-thread delete/trash path. Links
# against nexis-gui because FileSearchService lives in the GUI library.
add_nexis_test(NAME FileSearchServiceTests
  SOURCES
    managers/test_file_search_service.cpp
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Services"
  LIBS
    nexis-gui Qt6::Widgets Qt6::Concurrent
)

# SSO-3736 / FW-08: DuplicateFinderService — duplicate grouping (content-
# verified), top-N large-file ranking, empty-folder detection, exclusion
# engine compliance, and the never-delete-last-copy invariant. Builds on
# the WI-08 trash/elevation seam pattern, so the destructive paths are
# exercised without touching the user's real trash.
add_nexis_test(NAME DuplicateFinderTests
  SOURCES
    managers/test_duplicate_finder.cpp
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Services"
    "${GUI_SHARED_DIR}/Managers"
  LIBS
    nexis-gui Qt6::Widgets Qt6::Concurrent
)

# SSO-381: AppManager::applyIndicatorPngFallback() — the PNG fallback
# substitution we run when the Qt SVG image plugin is missing (Linux Mint 22
# "Zena" and other minimal Ubuntu-24.04 derivatives). Links against nexis-gui
# both for the AppManager symbol and to pull in the static.qrc resources so we
# can assert the PNG siblings ship inside the binary.
add_nexis_test(NAME IndicatorFallbackTests
  SOURCES
    managers/test_indicator_fallback.cpp
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Managers"
  LIBS
    nexis-gui Qt6::Widgets
)

# SSO-3737 / FW-09: DirSizeScanner — pure directory-size aggregation that
# powers the built-in treemap visualizer. Compiled directly into the test so
# it runs cross-platform without pulling in nexis-gui.
add_nexis_test(NAME DirSizeScannerTests
  SOURCES
    managers/test_dir_size_scanner.cpp
    "${GUI_SHARED_DIR}/Managers/dir_size_scanner.cpp"
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Managers"
  LIBS
    Qt6::Concurrent
)

# SSO-3363 / audit H2: LogProvider::cancel() null-deref defence and
# SSO-3384 / WI-22: MacOsLogStreamParser incremental ndjson parsing.
# Compiles log_provider.cpp directly so the test runs cross-platform without
# pulling in nexis-gui or the platform-specific log backends.
add_nexis_test(NAME LogProviderTests
  SOURCES
    managers/test_log_provider.cpp
    "${GUI_SHARED_DIR}/Pages/SystemLogs/log_provider.cpp"
  INCLUDES
    "${GUI_SHARED_DIR}"
)

# ── Theme validation tests ─────────────────────────────────────────────────────
add_nexis_test(NAME ThemeTokenTests   SOURCES theme/test_theme_tokens.cpp)
add_nexis_test(NAME FocusVisibleTests SOURCES theme/test_focus_visible.cpp)

# ── Screenshot regression tests (FR-41) ───────────────────────────────────────
# Links against the full nexis-gui library (not just nexis-core) because it
# instantiates the App window with all pages, managers, and themes.
add_executable(test-ScreenshotTests screenshots/test_screenshots.cpp)
target_link_libraries(test-ScreenshotTests nexis-gui Qt6::Test)
target_compile_definitions(test-ScreenshotTests PRIVATE
  PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
add_test(NAME ScreenshotTests COMMAND test-ScreenshotTests)
set_tests_properties(ScreenshotTests PROPERTIES ENVIRONMENT "QT_SCALE_FACTOR=1")
