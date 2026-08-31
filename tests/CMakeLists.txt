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
add_nexis_test(NAME MenuBarFormatUtilTests SOURCES utils/test_menu_bar_format_util.cpp)
add_nexis_test(NAME HealthScoreInputsTests SOURCES utils/test_health_score_inputs.cpp)
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
# SSO-3470 (WI-05.b): exercises the real sudoExecWithStatus write paths in
# AptSourceToolLinux::changeSource (tee/rm) against a temp file, under the
# NEXIS_SUDO_BYPASS=1 seam. Includes apt_source_tool_linux.h directly, so
# gate to Linux like BatteryChargeThresholdTests below.
if(UNIX AND NOT APPLE)
  add_nexis_test(NAME AptSourceToolExecSeamTests SOURCES core/test_apt_source_tool_exec_seam.cpp)
endif()
add_nexis_test(NAME RepoKnowledgeBaseTests SOURCES core/test_repo_knowledge_base.cpp)
add_nexis_test(NAME CleanerMLParserTests SOURCES core/test_cleanerml_parser.cpp)
add_nexis_test(NAME RepoHealthCheckerTests SOURCES core/test_repo_health_checker.cpp LIBS Qt6::Network)
add_nexis_test(NAME RepoRepairEngineTests SOURCES core/test_repo_repair_engine.cpp LIBS Qt6::Network)
add_nexis_test(NAME FanInfoTests      SOURCES core/test_fan_info.cpp)
add_nexis_test(NAME ThermalInfoTests  SOURCES core/test_thermal_info.cpp)
add_nexis_test(NAME BatteryInfoTests  SOURCES core/test_battery_info.cpp)
add_nexis_test(NAME DiskInfoTests     SOURCES core/test_disk_info.cpp)

# FW-15 (SSO-3743): battery charge-threshold control is Linux-only — the
# header/source live under linux/nexis-core and are compiled into nexis-core
# only on Linux (see CMakeLists.txt CORE_PLAT_SRCS). The test links nexis-core
# and includes Info/battery_charge_threshold.h, so registering it on macOS
# breaks the build (header not found). Gate it to Linux like NetworkInfoTests.
if(UNIX AND NOT APPLE)
  add_nexis_test(NAME BatteryChargeThresholdTests
    SOURCES core/test_battery_charge_threshold.cpp)
endif()

# SSO-15374: NvmlProcessSampler/NvmlBackend live under linux/nexis-core and
# are compiled into nexis-core only on Linux (see CMakeLists.txt
# CORE_PLAT_SRCS). Gate to Linux like BatteryChargeThresholdTests above.
if(UNIX AND NOT APPLE)
  add_nexis_test(NAME NvmlProcessSamplerTests
    SOURCES core/test_nvml_process_sampler.cpp)
endif()
# SSO-15378: RAPL/powercap package power reader is Linux-only (see
# CMakeLists.txt CORE_PLAT_SRCS) — gate like BatteryChargeThresholdTests.
if(UNIX AND NOT APPLE)
  add_nexis_test(NAME RaplPowerInfoTests
    SOURCES core/test_rapl_power_info.cpp)
endif()
# SSO-15378: CpuCoreListModel is the virtualized backing store for the
# per-core detail view (cross-platform, pure QAbstractListModel logic — no
# AppManager/SignalMapper dependency, unlike the widgets that render it).
add_nexis_test(NAME CpuCoreListModelTests
  SOURCES
    core/test_cpu_core_list_model.cpp
    "${GUI_SHARED_DIR}/Pages/Helpers/cpu_core_list_model.cpp"
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Pages/Helpers"
)
add_nexis_test(NAME PowerProfileTests SOURCES core/test_power_profile_info.cpp)
# SSO-15386 / SSO-15373: centralized deny-list shared by the uninstaller and
# the orphan-leftover scanner. Lives in shared/nexis-core so it builds and
# runs on both platforms; platform-specific deny-list cases are #ifdef-gated
# inside the test file itself.
add_nexis_test(NAME LifecycleDenyListTests SOURCES core/test_lifecycle_deny_list.cpp)
# SSO-15566 / SSO-15373 CISO §4: pure decision logic for the running-process
# warn/quit gate ahead of trashApps() — platform-neutral, runs everywhere.
add_nexis_test(NAME RunningAppGateTests SOURCES core/test_running_app_gate.cpp)
# SSO-15386 / SSO-15373: append-only audit trail for lifecycle-manager
# deletions (uninstaller + orphan-leftover scanner). Lives in
# shared/nexis-core so it builds and runs on both platforms.
add_nexis_test(NAME LifecycleAuditLogTests SOURCES core/test_lifecycle_audit_log.cpp)
add_nexis_test(NAME PackageToolTests  SOURCES core/test_package_tool.cpp)
# SSO-3470 (WI-05.b): exercises the real (non-overridden) PackageTool
# runCommand/runSudoCommand seam against the migrated execWithStatus contract.
# Includes package_tool_linux.h directly, so gate to Linux like
# BatteryChargeThresholdTests above.
if(UNIX AND NOT APPLE)
  add_nexis_test(NAME PackageToolExecSeamTests SOURCES core/test_package_tool_exec_seam.cpp)
endif()
add_nexis_test(NAME DockerToolTests   SOURCES core/test_docker_tool.cpp)
# SSO-3383 / WI-21 / audit M2: off-thread updateProcesses() + publish pair.
add_nexis_test(NAME ProcessInfoTests  SOURCES core/test_process_info.cpp LIBS Qt6::Concurrent)

# SSO-3366: PackageToolMacOS::trashApps injection guard. PackageToolMacOS is
# only built into nexis-core on Darwin, so the test is registered only there.
if(APPLE)
  add_nexis_test(NAME PackageToolMacOSTests
    SOURCES core/test_package_tool_macos.cpp)

  # SSO-15390: pure Sparkle appcast XML parser — compiled cross-platform
  # because the parser has no macOS-only dependencies.
  add_nexis_test(NAME SparkleAppcastTests
    SOURCES
      core/test_sparkle_appcast.cpp
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info/sparkle_appcast_parser.cpp"
    INCLUDES
      "${CMAKE_SOURCE_DIR}/macos/nexis-core"
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info"
  )

  # SSO-15431: known-answer tests (RFC 8032 §7.1) for SparkleSignatureVerifier
  # — compiled with the vendored verify-only ed25519 subset. macOS-only
  # because the verifier itself is only built into nexis-core on Darwin.
  add_nexis_test(NAME SparkleSignatureVerifierTests
    SOURCES
      core/test_sparkle_signature_verifier.cpp
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info/sparkle_signature_verifier.cpp"
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info/vendor/ed25519/verify.c"
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info/vendor/ed25519/sha512.c"
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info/vendor/ed25519/ge.c"
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info/vendor/ed25519/sc.c"
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info/vendor/ed25519/fe.c"
    INCLUDES
      "${CMAKE_SOURCE_DIR}/macos/nexis-core"
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info"
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info/vendor/ed25519"
  )

  # SSO-17776 AC4 hard gate: SparkleUpdateInstaller::verifyAndInstall() is
  # the real call site wiring SparkleSignatureVerifier into an install
  # decision — this proves fail-closed AT THAT LAYER (spy launcher/revealer
  # invoked zero times for every non-Valid Result), not just inside the
  # verifier itself. Also covers the §8 format-scope gate (AC7), the
  # floor-version store (AC5), and https-only rejection (AC2). macOS-only
  # because the installer and its dependents are only built into nexis-core
  # on Darwin (see CORE_PLAT_SRCS above).
  add_nexis_test(NAME SparkleUpdateInstallerTests
    SOURCES
      core/test_sparkle_update_installer.cpp
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info/sparkle_signature_verifier.cpp"
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info/sparkle_update_installer.cpp"
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info/sparkle_embedded_version.cpp"
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info/sparkle_update_floor_store.cpp"
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info/sparkle_update_downloader.cpp"
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info/vendor/ed25519/verify.c"
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info/vendor/ed25519/sha512.c"
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info/vendor/ed25519/ge.c"
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info/vendor/ed25519/sc.c"
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info/vendor/ed25519/fe.c"
    INCLUDES
      "${CMAKE_SOURCE_DIR}/macos/nexis-core"
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info"
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Info/vendor/ed25519"
    LIBS
      Qt6::Network
  )

  # SSO-15384 / CISO §2: deny-list unit tests — run on macOS and also on any
  # platform where the shared deny-list is compiled in (it is platform-agnostic
  # code even though it lives in a macOS-gated CMake block here).
  add_nexis_test(NAME AppUninstallDenyListTests
    SOURCES
      core/test_app_uninstall_deny_list.cpp
    INCLUDES
      "${CMAKE_SOURCE_DIR}/shared/nexis-core"
      "${CMAKE_SOURCE_DIR}/shared/nexis-core/Tools"
  )

  # SSO-15387 / FW-18 (SSO-3746): leftover artifact scanner — bundle-id
  # matching + expanded fixture suite (product-name boundary, vendor dirs,
  # LaunchDaemons privilege boundary, true-negative set, precision/recall
  # harness).  QSKIP'd off-macOS.
  add_nexis_test(NAME AppLeftoversMacOSTests
    SOURCES
      core/test_app_leftovers_macos.cpp
    INCLUDES
      "${CMAKE_SOURCE_DIR}/macos/nexis-core"
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Tools"
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Utils"
  )

  # SSO-15386 T3 (SSO-15373 §5): multi-signal orphan-leftover scanner.
  # QSKIP'd off-macOS. Gate for SSO-15386 T3 merge acceptance.
  add_nexis_test(NAME OrphanLeftoverScannerMacOSTests
    SOURCES
      core/test_orphan_leftover_scanner_macos.cpp
    INCLUDES
      "${CMAKE_SOURCE_DIR}/macos/nexis-core"
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Tools"
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Utils"
  )

  # SSO-15567: incremental (async) crumbs scan — proves scanCrumbsUnderHome()
  # streams each match via itemFoundCb in discovery order, that cancellation
  # actually stops the walk early, and that the final result (set, sizes,
  # sort-by-size-desc order) is unaffected by streaming. QSKIP'd off-macOS.
  add_nexis_test(NAME CrumbsScannerMacOSTests
    SOURCES
      core/test_crumbs_scanner_macos.cpp
    INCLUDES
      "${CMAKE_SOURCE_DIR}/macos/nexis-core"
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Tools"
      "${CMAKE_SOURCE_DIR}/macos/nexis-core/Utils"
  )

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

# SSO-15379: persistent `nethogs -t` streamer for per-process network I/O.
# Compiled directly so the parser test runs cross-platform even though
# nethogs_streamer.cpp is only wired into the real nexis-core build on
# Linux — mirrors the MacosParserTests / NettopStreamer pattern.
add_nexis_test(NAME NethogsStreamerTests
  SOURCES
    core/test_nethogs_streamer.cpp
    "${CMAKE_SOURCE_DIR}/linux/nexis-core/Info/nethogs_streamer.cpp"
  INCLUDES
    "${CMAKE_SOURCE_DIR}/linux/nexis-core/Info"
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
set(PACKAGE_TOOL_UNINSTALL_TEST_SRCS
  core/test_package_tool_uninstall.cpp
  "${CMAKE_SOURCE_DIR}/linux/nexis-core/Tools/package_tool.cpp"
  "${CMAKE_SOURCE_DIR}/linux/nexis-core/Tools/leftover_scanner_linux.cpp"
  "${CMAKE_SOURCE_DIR}/macos/nexis-core/Tools/package_tool.cpp"
  "${CMAKE_SOURCE_DIR}/macos/nexis-core/Tools/uninstall_audit_log.cpp"
  "${CMAKE_SOURCE_DIR}/macos/nexis-core/Utils/plist_util.cpp"
)
if(APPLE)
  # SSO-15566: PackageToolMacOS::quitApp() calls into the Objective-C++
  # NSRunningApplication bridge under Q_OS_MAC — only reachable (and only
  # needs linking) when this test target is actually built on Darwin. On
  # Linux, Q_OS_MAC is undefined so quitApp() never references the symbol.
  list(APPEND PACKAGE_TOOL_UNINSTALL_TEST_SRCS
    "${CMAKE_SOURCE_DIR}/macos/nexis-core/Tools/app_quit_helper.mm")
endif()
add_nexis_test(NAME PackageToolUninstallTests
  SOURCES ${PACKAGE_TOOL_UNINSTALL_TEST_SRCS}
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
    "${CMAKE_SOURCE_DIR}/linux/nexis-core/Tools/leftover_scanner_linux.cpp"
  INCLUDES
    "${CMAKE_SOURCE_DIR}/linux/nexis-core"
    "${CMAKE_SOURCE_DIR}/linux/nexis-core/Tools"
)

# SSO-15387: Linux leftover-scan heuristics fixture suite — XDG
# config/cache/data + autostart .desktop patterns across dpkg/rpm/Flatpak/Snap
# package types.  True-negative and precision/recall harness.  Platform-neutral
# (all paths synthetic); gate for SSO-15385 merge acceptance.
#
# SSO-15430 added a Linux-only PackageToolLinux::trashLeftovers() deny-list
# case (QSKIP'd off-Linux), hence the linux/nexis-core include dirs below.
add_nexis_test(NAME AppLeftoversLinuxTests
  SOURCES core/test_app_leftovers_linux.cpp
  INCLUDES
    "${CMAKE_SOURCE_DIR}/linux/nexis-core"
    "${CMAKE_SOURCE_DIR}/linux/nexis-core/Tools"
)

# SSO-15385: exercises the REAL LeftoverScannerLinux::scanLeftovers() (not a
# duplicated contract stub) against the SSO-15387 fixture scenarios, so a
# regression in the matching heuristic is caught even if the contract test
# above still passes. Compiles the platform source directly so it runs on any
# host, mirroring AptHistoryTests / PackageToolUninstallTests above.
add_nexis_test(NAME LeftoverScannerLinuxRealTests
  SOURCES
    core/test_leftover_scanner_linux_real.cpp
    "${CMAKE_SOURCE_DIR}/linux/nexis-core/Tools/leftover_scanner_linux.cpp"
  INCLUDES
    "${CMAKE_SOURCE_DIR}/linux/nexis-core"
    "${CMAKE_SOURCE_DIR}/linux/nexis-core/Tools"
)

# SSO-15387: orphan-scan heuristics fixture suite — leftovers with no
# corresponding installed package (Linux XDG + macOS Library).  Covers true
# orphans, near-miss shared-vendor dirs, and precision/recall harness.
# Platform-neutral.  Gate for SSO-15386 merge acceptance.
add_nexis_test(NAME OrphanScanFixtureTests
  SOURCES core/test_orphan_scan_fixtures.cpp)

# SSO-15428 (SSO-15373 §5): multi-signal orphan-leftover scanner — Linux.
# Tests PackageToolLinux::findOrphanLeftovers() via a testable subclass that
# injects a synthetic installed-name set and scans a QTemporaryDir.
# QSKIP'd off-Linux. Gate for SSO-15428 merge acceptance.
if(UNIX AND NOT APPLE)
  add_nexis_test(NAME OrphanLeftoverScannerLinuxTests
    SOURCES
      core/test_orphan_leftover_scanner_linux.cpp
      "${CMAKE_SOURCE_DIR}/linux/nexis-core/Tools/package_tool.cpp"
      "${CMAKE_SOURCE_DIR}/linux/nexis-core/Tools/leftover_scanner_linux.cpp"
    INCLUDES
      "${CMAKE_SOURCE_DIR}/linux/nexis-core"
      "${CMAKE_SOURCE_DIR}/linux/nexis-core/Tools"
  )
endif()

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

# SSO-23867: SnapshotManagerWidget's tmutil listlocalsnapshots parsing —
# pure/static, no live filesystem needed. Lives in the GUI target; compile
# its source + deps directly like FirewallTests above.
add_nexis_test(NAME SnapshotManagerWidgetTests
  SOURCES
    core/test_snapshot_manager_widget.cpp
    "${GUI_SHARED_DIR}/Pages/Helpers/snapshot_manager_widget.cpp"
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

# SSO-15396: Stacer settings importer — parses a real Stacer INI fixture and
# verifies key mapping, no-equivalent reporting, and apply().
add_nexis_test(NAME StacerImporterTests
  SOURCES
    managers/test_stacer_importer.cpp
    "${GUI_SHARED_DIR}/Utils/stacer_importer.cpp"
    "${GUI_SHARED_DIR}/Managers/setting_manager.cpp"
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Managers"
)

# GH#191: pure dashboard layout helper — grid constants, tier mapping, and
# v1->v2 layout migration. Compiled directly so it runs cross-platform without
# pulling in nexis-gui (DashboardPage builds its whole widget tree in init()).
add_nexis_test(NAME DashboardLayoutUtilTests
  SOURCES
    managers/test_dashboard_layout_util.cpp
    "${GUI_SHARED_DIR}/Pages/Dashboard/dashboard_layout_util.cpp"
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Pages/Dashboard"
)

# GH#191 follow-up: pure Drive tile sub-header/health text helper.
add_nexis_test(NAME DriveTileFormatTests
  SOURCES
    managers/test_drive_tile_format.cpp
    "${GUI_SHARED_DIR}/Pages/Dashboard/drive_tile_format.cpp"
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Pages/Dashboard"
)

# GH#214: fit-to-width sizing for the value centered in gauge/hybrid/ring/donut
# tiles. Needs Qt6::Gui for QFontMetrics (QTEST_MAIN spins up QGuiApplication).
add_nexis_test(NAME TileValueFitTests
  SOURCES
    managers/test_tile_value_fit.cpp
    "${GUI_SHARED_DIR}/Pages/Dashboard/tile_value_fit.cpp"
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Pages/Dashboard"
  LIBS Qt6::Gui
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

# FW-14 (SSO-3742): HealthReportManager — on-demand Markdown export and
# schedule CRUD. Links against nexis-gui for InfoManager, CleanerService,
# ScheduleManager, and SettingManager (same pattern as CleanerExclusionTests).
add_nexis_test(NAME HealthReportTests
  SOURCES
    managers/test_health_report.cpp
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Managers"
  LIBS
    nexis-gui Qt6::Widgets
)

# SSO-23896: pure tray-menu grouping helper — derives the tray's MONITOR
# (flat) / MANAGE / SYSTEM (submenus) structure from the same SidebarSection
# model the sidebar itself uses, so the two surfaces cannot drift. Compiled
# directly (like DashboardLayoutUtilTests) so it runs without pulling in
# nexis-gui; needs Qt6::Widgets for QPushButton.
add_nexis_test(NAME TrayMenuModelTests
  SOURCES
    managers/test_tray_menu_model.cpp
    "${GUI_SHARED_DIR}/Managers/tray_menu_model.cpp"
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Managers"
  LIBS
    Qt6::Widgets
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

# SSO-15381: FileShredderService — overwrite-then-unlink correctness (proven
# at the filesystem level via a hard link), recursive folder shredding +
# empty-dir cleanup, symlink safety, per-item failure accounting, and the
# contained/duplicate path dedup shared by preview and shred.
add_nexis_test(NAME FileShredderServiceTests
  SOURCES
    managers/test_file_shredder_service.cpp
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Services"
  LIBS
    nexis-gui Qt6::Widgets Qt6::Concurrent
)

# SSO-15382: WipeFreeSpaceService::headroomForVolume() safety-margin math.
add_nexis_test(NAME WipeFreeSpaceTests
  SOURCES
    managers/test_wipe_free_space.cpp
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

# SSO-15380: Trust & Safety shared component — scan/execute orchestration.
# Compiled directly into the test (like DirSizeScannerTests above) so it
# runs cross-platform without pulling in nexis-gui; the dialog half of the
# component is QWidget UI and is exercised via UAT instead.
add_nexis_test(NAME TrustSafetyRunnerTests
  SOURCES
    managers/test_trust_safety_runner.cpp
    "${GUI_SHARED_DIR}/Common/trust_safety_runner.cpp"
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Common"
  LIBS
    Qt6::Concurrent
)

# SSO-15481: Maintenance Wizard's Trust & Safety adoption — scan→item mapping
# and performItem dry-run parity, exercised in isolation (see file header).
# Separate executable from TrustSafetyRunnerTests: QTEST_MAIN() generates
# main(), and each test file needs its own per the convention above.
add_nexis_test(NAME MaintenanceWizardTrustSafetyTests
  SOURCES
    managers/test_maintenance_wizard_trust_safety.cpp
    "${GUI_SHARED_DIR}/Common/trust_safety_runner.cpp"
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Common"
  LIBS
    Qt6::Concurrent
)

# SSO-15480: SystemCleanerProvider — per-item selection and dry-run parity.
# system_cleaner_provider.cpp and trust_safety_runner.cpp are already compiled
# into nexis-gui (GUI_SHARED_SRCS). Adding them again to SOURCES would cause
# AUTOMOC to generate duplicate MOC symbols (TrustSafetyRunner, etc.) — one
# copy in the test target's mocs_compilation.cpp.o and another in
# libnexis-gui.a(mocs_compilation.cpp.o). Link nexis-gui instead (like
# CleanerServiceTests) so all three binaries — CleanerService, ToolManager,
# TrustSafetyRunner, SystemCleanerProvider — come from a single archive.
add_nexis_test(NAME SystemCleanerProviderTests
  SOURCES
    managers/test_system_cleaner_provider.cpp
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Common"
    "${GUI_SHARED_DIR}/Pages/SystemCleaner"
  LIBS
    Qt6::Concurrent
    nexis-gui Qt6::Widgets
)

# SSO-15429: OrphanLeftoversDialog's pure, static, non-widget entry points —
# buildRows() (CISO §5: every match starts unchecked, full signal set
# carried through) and confirmationSentence() (Design Anchor SSO-1768:
# one-sentence, size+count, trash-only copy). orphan_leftovers_dialog.cpp is
# already compiled into nexis-gui (GUI_SHARED_SRCS); link it rather than
# recompiling the source here, same reasoning as SystemCleanerProviderTests.
add_nexis_test(NAME OrphanLeftoversDialogTests
  SOURCES
    managers/test_orphan_leftovers_dialog.cpp
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Pages/Uninstaller"
  LIBS
    Qt6::Concurrent
    nexis-gui Qt6::Widgets
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
