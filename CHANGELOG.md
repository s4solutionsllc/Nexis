# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **APT 3.1 transaction history surface (FW-07 / SSO-3735):** A new **APT History** tab on the Uninstaller page (Linux only) lists recent `apt` transactions parsed from `apt history-list`, with one-click **Undo Last Transaction** and per-row **Undo Selected** / **Rollback To Selected** wired to `apt history-undo` / `apt history-rollback` via the existing `pkexec`/sudo seam. A **Why?** / **Why Not?** panel surfaces `apt why` / `apt why-not` output for any package the user names. The whole surface is gated on `parseAptVersion(apt --version) ≥ 3.1.0`; on older APT the nav button hides and the page shows a "not available" notice, so Debian 12 / Ubuntu 24.04 users see no broken affordance. Pure parsers (`PackageTool::parseAptVersion`, `parseAptHistoryList`, `parseAptHistoryInfo`, `parseAptWhy`) live in `shared/nexis-core/Tools/package_tool_shared.cpp` and are fixture-tested in `AptHistoryTests` alongside argv-construction tests for the `apt history-list/-info/-undo/-rollback` and `apt why/why-not` invocations via the `runSudoCommand`/`runCommand` seam established in WI-33.
- **macOS Background Task Management (btm) view + guarded `resetbtm` (SSO-3738 / FW-10):** The Startup Apps page now appends a "BTM Records (N)" section on macOS that parses `sfltool dumpbtm` into per-record rows showing name, identifier, type (Login Item / Launch Agent / Launch Daemon / Helper Launcher / App Extension / MDM-Managed / Daemon), on/off state, and badges for orphan, duplicate, and Apple-managed entries — the diagnostic surface a user needs when the Tahoe 26.4 `backgroundtaskmanagementd` 400% CPU bug is biting and Login Items silently fail. A destructive-styled "Repair BTM…" header button opens a confirmation dialog enumerating the side effects (every Login Item / Launch Agent re-prompts on next login, previously-disabled items revert to default, MDM-managed items re-evaluate, daemons may briefly fail) and requires the user to type `RESET` before the action runs `sudo sfltool resetbtm`. The new `BtmParser` (`macos/nexis-core/Info/btm_parser.{h,cpp}`) is platform-pure so it runs under the FR-127 compile-source-into-test pattern; `tests/core/test_btm_parser.cpp` covers section/record parsing, disposition decoding, duplicate detection by identifier and executable path, and orphan detection with an injected file-exists predicate (the Tahoe-private `/System/Library/LaunchAngels` tree is tagged Apple-managed and never marked orphan even when SIP hides the binary). Read-only by design: Apple exposes no public toggle for the BTM database, so per-record enable/disable is intentionally out of scope (the SMAppService path stays per-app).
- **Vendor hwmon/WMI sensor surfaces on Linux (SSO-3744 / FW-16):** Linux kernel 7.0 broadened `/sys/class/hwmon` with new vendor surfaces — `asus-wmi` (fan/backlight/kbd), `hp-wmi` (Victus/Omen manual fan control), and `lenovo legion-laptop` (Legion extra HWMON sensors). The thermal hwmon walker now resolves those vendor `name` strings to user-facing labels (`ASUS`, `HP`, `Legion`, `IdeaPad`, plus `asus_wmi_sensors` / `asus-ec-sensors`) instead of falling through to the capitalize-first-letter fallback, so the hardware-info / thermal tile reads "Legion – GPU" rather than "Legion – Sensor 3". The discovery loop itself was lifted into `ThermalInfo::enumerateHwmonSensors(root)` so the same code can be driven by a `QTemporaryDir` fixture; `ThermalInfoLinux::discoverSensors()` is now a one-liner that calls it with `/sys/class/hwmon`. New `friendly_*` and `enumerate_*` slots in `tests/core/test_thermal_info.cpp` cover the vendor name mappings, multi-temp-per-device enumeration, the 200 °C threshold-sanity clamp, and an explicit "no regression on machines without vendor nodes" assertion against a coretemp+nvme-only fixture. Fan-control writes (HP/ASUS pkexec stretch) are intentionally out of scope here — only the read-side surfaces ship in this PR.
- **Wayland-only readiness audit + `DisplayServerUtil` helper (SSO-3729 / FW-02):** Ubuntu 26.04 ships GNOME 50, which removes the X11 session entirely from Mutter / GNOME Shell / GDM — the desktop is Wayland-only, with XWayland retained for legacy apps. Audited the full tree (`shared/`, `linux/`, `macos/`, `tests/`, `scripts/`, every `CMakeLists.txt`) for X11 assumptions: `XOpenDisplay` / `XTest` / `xcb` / `QX11Info` / `<X11/…>` / raw `DISPLAY` reads / direct X11 linkage. **No X11-only code paths exist** — Nexis already routes all windowing through Qt6 abstractions (`QScreen`, `QWidget::grab()`), links no X11 libraries, and runs as a native Wayland client under GNOME 50. The screenshot test harness (`tests/screenshots/`) captures via `QApplication::grab()` and so renders the same regardless of session. `scripts/update_screenshots.sh` now prefers Qt's offscreen QPA (`QT_QPA_PLATFORM=offscreen`) when no `DISPLAY`/`WAYLAND_DISPLAY` is present instead of requiring Xvfb, matching the offscreen seam main.cpp already uses for scheduled `--clean` runs (SSO-3368). Added `shared/nexis-core/Utils/display_server_util.{h,cpp}` as the single canonical detector so any future XWayland-gated feature has one place to ask "are we Wayland, X11, XWayland, offscreen, or other?" — pure `classify()` for tests, env-driven `detect()` for pre-`QGuiApplication` callers, and `isXWayland(platformName)` for GUI code that has `QGuiApplication::platformName()`. New `tests/utils/test_display_server_util.cpp` covers every `classify` / `detect` / `isXWayland` / `describeCurrent` branch (28 slots), driving the env path hermetically through `qputenv`/`qunsetenv`. `docs/APPLICATION_OVERVIEW.md` gained a "Linux display server (Wayland / X11)" section documenting that no Nexis feature requires XWayland today.
- **macOS parser fixture tests + uninstall command-construction tests (SSO-3396 / WI-33):** Closed an audit gap (T3) where every test fixture was Linux-shaped and macOS live-tool parsers had zero coverage. Captured representative `nettop` CSV, `diskutil list/info -plist`, and `sysctl kern.boottime` output under `tests/fixtures/macos/`, extracted the inline `parsePlist` from `disk_health_info.cpp` into a public `MacosPlistParser::parse()` helper, exposed `NettopStreamer::parseCsvLine()` and `BootAnalysisInfoMacOS::parseKernBoottime()` as pure static methods, and added `MacosParserTests` that exercises all three via the FR-127 compile-source-into-test pattern (so they run on Linux CI too). For the uninstall path, introduced a `runSudoCommand`/`runCommand` virtual seam on `PackageTool` (mirroring `TestableRepairEngine`) so the per-package-manager argv construction (apt-get purge vs remove, snap remove, dnf/yum/pacman/brew flags, snap-revision flag) and the macOS osascript shell escaping (`CommandUtil::buildMacOsSudoShellCommand`) are now deterministically tested in `PackageToolUninstallTests` — replacing the single test that previously `QSKIP`-ed when `pkexec` was absent. Note: the audit also referenced `ioreg`/`pmset` battery parsers, which do not exist in the codebase today — `battery_info_macos` reads IOKit directly via CoreFoundation, with no textual output to fixture.
- **Unit coverage for `CleanerService` destructive paths (SSO-3370):** New `tests/managers/test_cleaner_service.cpp` exercises `cleanFiles()` against a `QTemporaryDir` tree — excluded files **and excluded children of scanned directories** survive, `minFileAgeSecs` is honored at every depth, symlinks are removed-not-followed, bytes-freed accounting matches actual deletions, and the elevated branch routes through the test seam instead of real `rm`. `cleanTrash()` is covered via a `trashRoot()` seam pointed at a temp dir.
- **`SECURITY.md` security policy (SSO-3374):** Added a top-level `SECURITY.md` declaring supported versions (2.3.x), the `security@s4solutions.ai` private contact, support for GitHub private vulnerability reporting (GHSA), and the 7-day patch SLA from `RELEASE.md` §6. The §6 day-6–7 step that says "update `SECURITY.md`" now references a real file. Reporters who land on the repo's Security tab will get a private channel instead of defaulting to a public issue once the maintainer enables Private Vulnerability Reporting in repo settings.

### Security
- **Automated vulnerability detection: CodeQL + pinned linuxdeploy (SSO-3375 / WI-13, audit C3):** Added `.github/workflows/codeql.yml`, a CodeQL C/C++ scan that runs on every push and pull request against `native` and on a weekly cron. The job mirrors the Linux dependency list from `build.yml` and uses the `security-and-quality` query suite so SAST findings land in the repo Security tab and feed the 7-day CVE SLA in `RELEASE.md` §6. In `release.yml`, the `linuxdeploy` and `linuxdeploy-plugin-qt` AppImage downloads no longer follow the mutable `continuous` tag — they are pinned to specific upstream release tags and verified against committed SHA256 sums per architecture, so a backdoored `continuous` build cannot be pulled into a Nexis release. `RELEASE.md` §6 and `docs/MAINTAINER_SOP.md` §4 now enumerate the security-detection subscriptions (Qt `announce@`, GitHub Dependabot, CodeQL alerts, linuxdeploy release feeds) that have to stay live for the SLA to actually fire.
- **Defense-in-depth on shell-eval and root-rm paths (SSO-3399 / WI-36):** Removed shell-string interpolation from three audit-flagged paths even where the inputs are currently trusted, so future callers cannot accidentally widen the blast radius. (1) `PackageToolLinux::dpkgDryRunRemove` no longer wraps `apt-get remove --dry-run …` in `bash -c` with the package list interpolated; it now invokes `apt-get` directly with argv. (2) `DiskHealthInfoLinux::refreshHealthElevatedBatch` validates every device path against `^/dev/[A-Za-z0-9]+$` before splicing it into the `pkexec sh -c "…"` batch (kept as a batch so users see one auth prompt instead of one per drive). (3) `CleanerService::cleanFiles` adds the `--` end-of-options guard before passing paths to `rm -rf` and splits the work — user-owned paths are removed via `QFile::remove` with no elevation, only files actually owned by another uid go through `pkexec rm`, so a clean operation on cache directories no longer pops a pkexec prompt for paths the current user can delete itself.

### Changed
- **Split Homebrew off the APT-source abstraction (SSO-3390 / WI-28, audit A4):** The macOS Homebrew adapter had been mis-implementing `AptSourceTool` — packaging brew formulae/casks into `APTSource` records by overloading `.uri` for package names and `.suites` for "formula"/"cask" while leaving `changeSource`/`changeStatus` as no-ops. The shared APT page then branched around the mismatch with ~30 `Q_OS_MAC` directives. Introduced a platform-neutral `RepositoryTool` interface with a generic `Repository` value type; `AptSourceTool` now extends it on Linux with the APT-specific operations, and macOS gets a dedicated `HomebrewToolMacOS` plus a new `HomebrewPage` driven by `PackageTool` data. `APTSourceManagerPage` moved under `linux/nexis/Pages/AptSourceManager/` and dropped all of its `Q_OS_MAC` branches. Behaviour is unchanged on both platforms; this is the architectural foundation for the deb822 editor and APT-history features in the feature plan.
- **Maintenance bucket — unverified-low cleanup (SSO-3399 / WI-36):**
  - Memory: pruned dead-PID entries from the macOS `NettopStreamer` and Linux `NvidiaSmiPmonStreamer` `mLatest` maps each tick using the existing `activePids` / `activeGpuPids` snapshots (singleton/long-lived streamers used to retain entries for exited processes for the app's lifetime). `DashboardPage::checkUpdate` now `deleteLater()`s the `QNetworkReply` and `QNetworkAccessManager` after the GitHub releases poll completes (both used to leak for the page's lifetime, one per invocation).
  - Qt/UI: renamed `SystemCleanerPage::on_btnScan_clicked` → `onBtnScanSystemClicked` so Qt's `on_<objectName>_<signal>` auto-connect machinery doesn't keep emitting a missing-widget warning to the test log for the never-existing `btnScan` (the actual button is `btnScanSystem`, wired explicitly with `connect()`). Moved the residual `metric_tile_base.cpp` purple/lime swatches behind `@rangePurpleColor` / `@rangeLimeColor` theme tokens in both `default` and `light` `values.ini` (BUG-47).
  - Build/packaging: macOS `entitlements.plist` now grants no extra hardened-runtime entitlements — `allow-unsigned-executable-memory` (no JIT in Nexis) and `disable-library-validation` (release.yml re-signs every bundled Qt framework/plugin with the same Developer ID, and the app does no runtime `dlopen`/`QLibrary` loading) were both unneeded. CMake `install(…)` rules dropped the `CONFIGURATIONS Release RelWithDebInfo MinSizeRel` gate that silently made `cmake --install` a no-op on the default Debug build. Bumped the Qt6 floor to 6.4 explicitly in `find_package(Qt6 6.4 …)` so configure fails early on older Qt. The PPA `ppa.yml` `tar … || true` step was masking every tar failure; it now treats exit 1 as benign (the "file changed as we read it" case the inline comment already covered) and fails the job on exit 2+.
  - CI: dropped the workflow-default `contents: write` / `actions: write` from `release.yml` and granted both only to the `release` job — the build matrices now get a read-scoped `GITHUB_TOKEN`. The release step now generates and uploads a `SHA256SUMS-<tag>.txt` alongside every artifact (`.deb` × 4, `.AppImage` × 2, raw binaries × 2, `.dmg`) so downloaders can verify integrity end-to-end. `RELEASE.md` §0 step 4 no longer references the deleted `scripts/nexis_db.py` / `BUGS.md` flow — it now points to Paperclip as the system of record.
  - Tests: deleted dead `tests/test_nexis_db.py` (file kept referencing a tooling chain that has been removed; the historical plan document at `docs/superpowers/plans/2026-04-24-sqlite-tracking-index.md` remains as the implementation record). `tests/managers/test_cleaner_exclusions.cpp::initTestCase()` now calls `QStandardPaths::setTestModeEnabled(true)` so the persistence-roundtrip tests run in a sandboxed config location instead of overwriting the developer's real `settings.ini` on every `ctest` run.
  - Docs: aligned `CONTRIBUTING.md` §5 labels with the actual GitHub label set (`good first issue` instead of `good-first-issue`, dropped the never-created `translation` label and re-homed translation work under `enhancement` + the existing `needs-native-qa` review label, added the live `help wanted` / `question` labels). Reconciled `README.md` §"Maintenance & Releases" intro with `docs/MAINTAINER_SOP.md` — both now say "single named full-time maintainer (no capacity time-box)". De-duplicated the second `### Fixed` block under `[2.3.7]`. Replaced the dead `SSOA-5` link in `docs/MAINTAINER_SOP.md` §6 step 5 with a description of how to find the current tracking parent under the `SSO` project in Paperclip. Added a top-level `CODE_OF_CONDUCT.md` (Contributor Covenant 2.1) and `.github/PULL_REQUEST_TEMPLATE.md`.
- **Shared UI pages now reach platform-specific Info through `InfoManager` (SSO-3389 / WI-27, audit A3):** `HardwareInfoPage`, `DashboardPage`, and `BootAnalysisPage` previously stack-constructed `SystemInfoLinux` / `SystemInfoMacOS`, `CpuInfoLinux` / `CpuInfoMacOS`, and `BootAnalysisInfoLinux` / `BootAnalysisInfoMacOS` behind `#ifdef Q_OS_*`. They now go through `InfoManager` getters (`getHostname()`, `getDistribution()`, `getCpuModel()`, `bootAnalysisInfo()`, …). `BootAnalysisInfo` and `StartupInfo` are wired through `InfoManager` for the first time. A new CI gate (`scripts/check-pages-no-platform-headers.sh`, run from `build.yml`) blocks any future include of `*_macos.h` / `*_linux.h` from `shared/nexis/Pages`. Behavior unchanged; cuts one of the 47 `Q_OS_*` edit sites the audit flagged for the next platform port.
- **Release workflow now runs unit tests on every release build (SSO-3395 / WI-32):** A directly-tagged hotfix could previously ship without the unit suite running, because the `build-linux` (AppImage + raw binary) and `build-macos` legs of `.github/workflows/release.yml` had no `ctest` step — tests only ran on `push`/PR via `build.yml`, and on the Plucky `.deb` job via `debian/rules override_dh_auto_test`. Added a `Unit Tests` step to the Linux and macOS release legs that mirrors `build.yml` (`xvfb-run -a ctest --test-dir build --output-on-failure -E ScreenshotTests` on Linux; plain `ctest … -E ScreenshotTests` on macOS). The `release` job already gates on every build leg succeeding, so a red unit suite now blocks publication of all release artifacts.
- **Homebrew/AUR/GitHub Release pipeline now safe for backports (SSO-3378 / audit B2+C2):** A failed DMG download (e.g. a 404 served by the GitHub CDN) used to be hashed and committed to the Homebrew tap — `brew install nexis` would then fail for everyone while the workflow stayed green. The cask updater now uses `curl -fsSL --retry 3` and sanity-checks the DMG (size + file-type) before computing the checksum, and refuses to commit a checksum from a bad download. Cutting a backport tag (for example a `v2.3.x` security patch after `v2.4.0`) no longer downgrades Homebrew/AUR users or steals the GitHub "Latest" label: the release workflow computes `make_latest` from the actual tag vs. current Latest (and uses `legacy` for backports), the Homebrew and AUR jobs read the currently published version and skip the bump when the new tag is older, and the "Delete pre-existing release for this tag" step only fires on genuine pre-rebrand Stacer releases so a failed re-run cannot leave a tag release-less. `RELEASE.md` §4 manual fallback and §6 (CVE/security-fix path) document the backport caveat and operator checklist.
- **Unified `CommandUtil` error-handling contract (SSO-3367, audit A1):** Three contradictory contracts coexisted in the exec layer — `CommandUtil::exec()` threw a raw `QString` on QProcess error, `execWithStatus()`/`execAsync()` returned `ExecResult`, and `sudoExec()` swallowed every failure into an empty string — making it impossible to tell from a call site whether a failure would throw, return, or vanish, and forcing a verify-after-write read on top of every elevated write in `FileUtil::writeRootFile` and the FR-81/FR-117/FR-118 helpers cards. All entry points now route through a single internal `runProcess()` helper that never throws and always populates an `ExecResult { output, error, exitCode, ok() }`. Two new authoritative APIs — `CommandUtil::execWithStatus(cmd, args, data, timeoutMs)` (stdin overload) and `CommandUtil::sudoExecWithStatus(cmd, args, data)` (with a `NEXIS_SUDO_BYPASS=1` testing seam) — let callers branch on `result.ok()` instead of relying on exceptions or empty-string heuristics. The legacy QString-returning wrappers are preserved for the ~130 existing call sites; they now log failures via `qCritical` instead of throwing or swallowing them. `FileUtil::writeRootFile` and `FileSearchService::moveToTrash`/`::deleteFile` are the first migrated call sites — `writeRootFile` drops its now-unnecessary read-back byte compare, and the SSO-3365 `try/catch (const QString&)` around the search-page file ops is removed. Tests in `tests/utils/test_command_util.cpp` assert no exception escapes any entry point and that exit codes plus stderr are captured. Per-subsystem migration of the remaining call sites is tracked under SSO-3367 child issues.
- **Screenshot regression comparator hardened (NEX-3382 / WI-20):** Replaced the previous whole-page pixel-equality + 5–10% per-page tolerances — simultaneously brittle (anti-aliasing / font drift flipped every page red) and insensitive (a real regression touching ≤10% of pixels passed) — with a mask + fuzz model. The comparator now walks each page widget tree, builds a `QRegion` mask from declared dynamic child classes (`HistoryChart`, `BarChartWidget`, `DashboardTileWrapper`, `MetricTileBase`, `NetworkTile`, `QAbstractItemView`) and named widgets (`systemSummary`), then per-channel-fuzz compares the remaining unmasked pixels under a tight 1% default threshold (overridable via `NEXIS_SCREENSHOT_TOLERANCE`); pixels whose R/G/B/A all differ by ≤ 8 counts (overridable via `NEXIS_SCREENSHOT_CHANNEL_FUZZ`) count as equal. Missing page class or reference PNG `QFAIL`s loudly instead of `qWarning() + continue` silent skips; wholly absent platform/theme baseline directories `QSKIP` with explicit "regenerate via scripts/update_screenshots.sh or the Regenerate Screenshot Baselines workflow" instructions, so the suite never vacuously passes. Seven new self-test slots (`selfTest_identicalImagesPass`, `selfTest_perChannelFuzzAccepted`, `selfTest_perChannelFuzzExceededFails`, `selfTest_unmaskedDifferenceDetected`, `selfTest_maskedDifferenceIgnored`, `selfTest_maskDoesNotHideAdjacentRegression`, `selfTest_sizeMismatchFails`) validate the comparator against synthetic images on every run, so a broken comparison routine can't ride alongside green real-page checks. Removed the empty `tests/reference_screenshots/linux/{dark,light}/.gitkeep` placeholders left over from NEX-3381 so the Linux baseline gap is structurally visible until baselines are regenerated and committed on a pinned x64 runner.

### Removed
- **Flatpak (Flathub) distribution channel (SSO-3376):** The Flatpak manifest at `linux/flatpak/` and the reviewer-justification document at `docs/flatpak-reviewer-justification.md` have been removed. The sandbox model is fundamentally incompatible with Nexis's system-maintenance feature set: under bubblewrap the host root mounts at `/run/host`, the sandbox `/usr` is the KDE runtime (no `systemctl`/`apt`/`pkexec`/`smartctl`/`fstrim`/`nvidia-smi`), `pkexec`'s setuid bit is inert, and the PID namespace hides host processes — so every privileged operation would fail unconditionally. Users continue to install Nexis on Linux via the supported native channels (`.deb` / PPA / AUR / AppImage); no user action is required. The user-facing System Cleaner category that removes unused Flatpak runtimes on the host is unaffected and remains available.

### Fixed
- **Scheduled cleans and macOS startup items silently fail to load on macOS 27 (SSO-3731 / FW-04):** macOS 27 (Golden Gate) refuses to load launchd plists whose file carries a `com.apple.quarantine` extended attribute. Nexis writes plists for scheduled cleans (`~/Library/LaunchAgents/com.nexis.clean.*.plist` via `ScheduleManager::createLaunchdPlist`) and for user-added startup items (via `StartupAppEdit`); on hosts where the home volume's default-write-attributes carry quarantine (some MDM-managed volumes, restored-from-snapshot setups, `osascript` shell scripts that produced the file), the plist inherits the attribute and `launchctl load` silently no-ops after the macOS 27 upgrade. Both write paths now call a new `MacOsXattrUtil::stripQuarantine(path)` helper immediately after writing the plist and before `launchctl load`. The helper wraps the POSIX `removexattr(…, com.apple.quarantine, XATTR_NOFOLLOW)` call (no Obj-C bridge, no `xattr -d` subprocess) and treats `ENOATTR` as idempotent success. Non-macOS builds compile to a no-op. New `tests/managers/test_macos_xattr_util.cpp` writes a plist under `QTemporaryDir`, applies a synthetic quarantine attribute via `setxattr`, asserts the helper removes it, and `QSKIP`s off-macOS. Pair with audit WI-06 (scheduled-clean reliability).
- **Intel iGPU utilization read N/A under the `xe` kernel driver (SSO-3706 / GH#91):** `GpuInfoLinux::discoverGpus()` only probed the i915 sysfs paths (`gt_cur_freq_mhz` / `gt_max_freq_mhz`) for Intel GPUs. The newer `xe` driver (kernel 6.8+, e.g. Raptor Lake-P Iris Xe) does not expose those files and instead publishes per-tile, per-GT frequency under `device/tile<T>/gt<G>/freq0/{cur_freq,max_freq}`. The probe missed it, `sysfsLoadPath`/`queryCommand` stayed empty, and the Hardware Info page rendered Utilization as N/A. Intel discovery now probes xe first via a new `GpuInfo::findIntelXeFreqDir()` helper (returns the first `freq0/` dir that has both files; deterministic `tile0/gt0` order for multi-tile cards) and falls back to the existing i915 paths so users on i915 don't regress. The cur/max MHz proxy in `parseIntelFreqUtilization()` works unchanged for both drivers. New `tests/core/test_gpu_info.cpp` cases (`xeFreqDir_*`) cover the iGPU layout, the multi-tile pick order, a partially-wired tile, and the i915-only and missing-path fallbacks via `QTemporaryDir` fake sysfs trees.
- **Interactive controls were unreachable by keyboard (SSO-3502):** Nexis previously opted out of keyboard focus on roughly 60 interactive controls (sidebar nav buttons, page-header actions, system-cleaner category checkboxes, helpers buttons, dashboard tile actions, repo-detail action buttons, etc.) by calling `setFocusPolicy(Qt::NoFocus)`, which meant users on keyboard, screen reader, or switch control could not tab through them. Each call site has been audited and the `Qt::NoFocus` removed on every interactive widget; the four remaining sites are deliberate (read-only `NoSelection` data tables, plus the command-palette result list whose focus is forwarded from the search box via an event filter) and now carry inline rationale comments. A new `@focusRingColor` token in `values.ini` (dark + light) drives a visible focus ring through `:focus` selectors in `style.qss` for buttons, checkboxes/radio buttons, sliders, combo boxes, line/spin/plain-text edits, and tree/table/list views, with the sidebar reusing the existing 3px left-edge stripe so focus shows without layout shift. `tests/theme/test_focus_visible.cpp` pins the focus-ring contract so a future edit that strips the selectors or the `@focusRingColor` token fails CI. Follow-up: SSO-3502 swept `.cpp` declarations only; `<enum>Qt::NoFocus</enum>` entries in `.ui` files remain for a future sweep.
- **System Logs no longer spikes memory / stalls the UI on macOS (SSO-3384 / audit WI-22):** `LogProviderMacOS` used to run `log show --style ndjson --last 1h` and not parse anything until the child finished — an hour of ndjson on a busy Mac is hundreds of MB to >1 GB, copied twice and JSON-parsed on the main thread, which spiked memory and stalled a page that auto-fetches on open. The provider now streams stdout via `readyReadStandardOutput`, hands each chunk to a new `MacOsLogStreamParser` that parses complete ndjson lines on the fly and discards consumed bytes, and kills the child as soon as the configured entry cap (`mMaxEntries`, default 500) is reached so we don't pay for output we'd just throw away. The default time window also drops from `--last 1h` to `--last 5m`, and `--info` / `--debug` are only requested when the active severity filter would actually surface them. Linux (`journalctl`) is unaffected — it already had `--lines` and per-line streaming via its JSON output.
- **Start page reset when interface language changed (SSO-3388 / audit Q3):** The "Start page" setting persisted the *translated* combo text and the launch path matched it against the translated sidebar button tooltips. Changing the UI language meant the saved value no longer matched any sidebar title, so Nexis silently fell back to Dashboard, and on macOS — where the sidebar uses `tr("Applications")` but the settings combo offered `tr("Uninstaller")` — the start page never resolved correctly. Start page is now stored as a stable untranslated id (e.g. `dashboard`, `uninstaller`) via `QComboBox::itemData`, mapped to a page id in `App::pageTitleById()`, and the combo on macOS now shows "Applications" so it matches the sidebar. A one-time migration in `SettingManager::migrateStartPageId()` maps known legacy English combo values (and the macOS `Applications` sidebar tooltip variant) to ids; anything unrecognized defaults to `dashboard`. New `tests/managers/test_setting_manager.cpp` round-trips ids, exercises the legacy migration, and asserts unknown values default to dashboard.
- **Transient CPU read failures could crash Dashboard/Resources ticks (SSO-3380 / audit M1):** `CpuInfo::getCpuPercents()` returns an empty list when `host_processor_info` (macOS) or `/proc/stat` (Linux) fails to sample. `DataRefreshService::onFastTick` emitted `cpuUpdated` regardless, so `DashboardPage::onCpuUpdated`'s `percents.at(0)` and `ResourcesPage::onCpuUpdated`'s `percents.at(j+1)` were undefined behavior in release builds on a transient producer failure. `onFastTick` now gates the emit on `DataRefreshService::isCpuPayloadEmittable(percents)` (drops the tick instead of crashing), and both consumers carry a defensive bounds check so a future direct caller cannot reintroduce the UB.
- **Processes page could freeze the GUI for up to 30 seconds; "Unlock Drive" raced slow polkit password entry (SSO-3383 / audit M2):** `DataRefreshService::onProcessTick()` called `InfoManager::updateProcesses()` on the UI thread once per second whenever the Processes page was open. On macOS that synchronously forked `ps ax -weo` under `QProcess::waitForFinished(30000)`, so any stall on the `ps` side froze the whole GUI for up to the 30 s cap; on Linux the in-process `/proc` walk was less catastrophic but still ran on the UI thread. The SMART "Unlock Drive" button on the Hardware Info page had the same shape: `refreshHealthElevated()` ran `pkexec smartctl` (or `osascript`+`smartctl` on macOS) under the same 30 s cap, which raced the user typing the polkit/authorization password — slow entry timed out and the unlock silently failed. `onProcessTick()` now mirrors `onSlowTick()`: it dispatches a `QtConcurrent` worker that calls a new `ProcessInfo::collectProcesses()` (builds a local `QList<Process>`, never touches `processList`) and the UI thread publishes via `setProcessList()` from inside the `QMetaObject::invokeMethod` hop. A `QMutex` guards every `processList` read/write, and a second mutex serialises concurrent collect calls so the per-PID delta state stays coherent if the synchronous HTML-export path races the worker. The Hardware Info SMART unlock (single-drive and "Unlock All") now dispatches `refreshDiskHealthElevated[Batch]()` through `QtConcurrent::run` + `QFutureWatcher`, and the pkexec/osascript timeout is bumped to 5 minutes so a slow password entry still completes the unlock. Covered by `tests/core/test_process_info.cpp` (publish pair, off-thread worker, reader-not-blocked-by-collect, concurrent set/get stress).
- **System Cleaner could delete excluded files inside a scanned directory (SSO-3370 / audit H9):** `CleanerService::scan()` filtered the exclusion list only against top-level entries, but `cleanFiles()` then recursively removed each scanned directory's contents without re-checking the predicate. A file or sub-folder the user added to their exclusion list — e.g. `~/.cache/JetBrains/important.db` underneath a scanned `~/.cache` folder — was deleted anyway. Both `cleanFiles()` and the new recursive walker now apply `isExcluded()` and the `minFileAgeSecs` cutoff at every depth, so protected children survive even when their parent directory is the scan target. As a related fix the `minFileAgeSecs` top-level guard no longer skips directories whose own mtime is recent (a cache dir we just wrote to looked "recent" and the entire recursive walk was skipped, so aged children survived forever); the per-child cutoff inside the recursive walker is now the source of truth. The elevated `rm` branch also now passes `-rf --` (end-of-options guard) so a file literally named `-rf` cannot be reparsed as a flag, and routes through an overridable `removeElevated()` seam so the privileged branch is testable without root.
- **Default `CXXBASICS_USE_FASTER_LINKERS` to OFF in-tree, fixing GH#82 at the root (SSO-3377 / GH#82 / GH#88):** The in-tree linker accelerator (`shared/cmake/cxxbasics/accelerators/UseFasterLinkers.cmake`) defaulted **ON** and appended `-fuse-ld=lld` whenever LLD was present. Any environment with LLD and `-flto` in `CXXFLAGS` (Fedora/openSUSE defaults, CachyOS, downstreams not using our PKGBUILD) reproduced GH#82's "undefined symbol: main" — GCC emits slim LTO objects that LLD cannot resolve. A dev-speed accelerator should never be on for release or packaged builds. The default is now **OFF**; developers who want the LLD/gold speedup opt in with `-DCXXBASICS_USE_FASTER_LINKERS=ON`. The redundant `-DCXXBASICS_USE_FASTER_LINKERS=OFF` workaround was removed from `linux/aur/PKGBUILD` (the `options=('!lto')` decision stays with the maintainer per GH#82). The underlying GCC + LLD + LTO incompatibility tracked in GH#88 is unchanged; this change makes sure the default config can never recur it.
- **Bundled font license compliance (SSO-3372):** Nexis bundles Inter (OFL 1.1), JetBrains Mono (OFL 1.1), and Ubuntu-R (UFL 1.0), but shipped no accompanying license texts as required by OFL §2 and the UFL. Added `LICENSE-OFL.txt` and `LICENSE-UFL.txt` under `shared/nexis/static/font/`, created `THIRD_PARTY_LICENSES.md` at the repo root, and corrected `debian/copyright` to include per-pattern `Files:` stanzas with the accurate `OFL-1.1` and `Ubuntu-font-1.0` SPDX identifiers (previously all files were incorrectly declared as GPL-3.0 only). License files are installed to `/usr/share/doc/nexis/` in `.deb` packages.
- **macOS: notarization ticket now stapled to the `.app` bundle, not only the `.dmg` (SSO-3513):** The release workflow ran `xcrun stapler staple` on `nexis.dmg` only, so the `.app` extracted from the DMG (Homebrew cask, drag-to-`/Applications`) had no embedded ticket. `spctl --assess` still passed on online machines via Apple's CloudKit lookup, but a fresh or offline first-launch could see Gatekeeper reject the bundle. `.github/workflows/release.yml` now staples and validates `nexis.app` immediately after `status: Accepted` and before stapling the DMG, so the ticket travels with the bundle regardless of distribution channel. Found by [SSO-3493](/SSO/issues/SSO-3493) verification of v2.3.14.
- **macOS bundle version now tracks the released tag (SSO-3379 / audit B3):** Cutting a release tag that did not also touch `CMakeLists.txt` produced a notarized `.dmg` whose `Info.plist` `CFBundleVersion` / `CFBundleShortVersionString` reported the previous `project()` version, not the tag. `release.yml` already passes `-DAPP_VERSION_OVERRIDE=<tag>` into the macOS configure step (and the override-aware `APP_VERSION` is used everywhere else, e.g. the in-app About string), but the bundle properties were keyed off `${PROJECT_VERSION}` — the hardcoded value in `project(Nexis VERSION X.Y.Z)` at the top of `CMakeLists.txt` — which the override does not touch. The bundle properties now use `${APP_VERSION}`, so an override-driven release build always produces an `Info.plist` whose version matches the tag. `RELEASE.md` §1 also now lists `CMakeLists.txt` in the `git add` checklist and explains why local/non-override builds (untagged dev builds, distro rebuilds without the override) still need the `project()` version bumped at release time.
- **Network Usage cap-progress groove rendered unthemed (audit WI-25):** `NetworkUsagePage::refreshCapBar()` was building the cap progress bar's stylesheet with the literal token `background: @borderColor`, but per-widget `setStyleSheet()` strings do not pass through the global token-substitution pass (only `qApp->setStyleSheet()` does). Qt dropped the invalid declaration and the groove showed Qt's default palette color in both themes. The token is now resolved via `sv->value("@borderColor", "#e0e0e0")` and interpolated into the stylesheet, matching the surrounding `refreshThemeColors()` pattern. A new `tests/theme/test_theme_tokens.cpp::noRawTokensInPerWidgetStyleSheet` regression scans every per-widget `setStyleSheet()` call in `shared/` C++ and fails if any raw `@token` literal remains in the argument.
- **Removed dangerous dead macOS `GnomeSettingsTool` adapter (SSO-3391 / WI-29, audit A5):** The GNOME Settings page is hidden on macOS, but `ToolManager` still constructed a macOS adapter whose mapping table collapsed 9 GNOME interface keys onto `AppleInterfaceStyle` and several window-manager keys onto dock `orientation` — if the page-visibility guard had ever regressed, `isAvailable()` would have reported true and writes would have corrupted `NSGlobalDomain` and `com.apple.dock`. `GnomeSettingsToolMacOS` is now a hard no-op stub (`isAvailable()` returns false; every setter returns false without invoking `defaults`), `ToolManager::checkGnomeSettings()` short-circuits to false on macOS as defense in depth, and the macOS constants header is wiped to empty strings so a future regression would target an obviously-invalid domain rather than a real Apple preference. New unit tests in `tests/managers/test_gnome_settings_tool.cpp` pin the contract.
- **"Every N days" scheduled cleans ran every day on systemd (SSO-3369):** `ScheduleManager::createSystemdTimer()` mapped `EveryNDays` to a daily `OnCalendar` trigger with a stale comment promising an in-service condition check that was never wired up, so on Linux distros where systemd is the default scheduler (the common case), a schedule configured for "every 7 days" deleted the selected categories every day. cron and launchd were unaffected because they honor the interval natively. `CleanerService::cleanSchedule()` now consults `ScheduleManager::isEveryNDaysGateBlocked(frequency, everyNDays, lastRun, now)` before doing any work, skipping the run (and leaving `lastRun` untouched) when `lastRun.daysTo(now) < everyNDays`. Boundary (`daysTo == N`) runs.
- **lupdate normalization never ran on Crowdin syncs (NEX-WI-31):** `lupdate.yml` triggered only on push to `l10n_crowdin_translations`, but the Crowdin action pushes with the default `GITHUB_TOKEN`, and GitHub suppresses workflow triggers for token-authored pushes — so translation PRs landed un-normalized. Normalization now runs inline in `crowdin-sync.yml` immediately after the Crowdin action, checking out the localization branch, running `lupdate` over `shared/`/`macos/`/`linux/`, and committing the result back to `l10n_crowdin_translations` so the open PR picks it up. The standalone `lupdate.yml` is kept as a manual/edge-case backstop and now declares an explicit `permissions: contents: write` block.
- **Network Usage RX bar ignored the active theme (SSO-3386, audit WI-24):** The Network Usage page's per-interface RX/TX bar chart read the RX color from `@networkDownloadColor`, but that token was never defined in `values.ini` for either theme. `QSettings::value()` fell back to the hard-coded default `#5294e2` (the legacy Stacer blue), so the download bar stayed blue in both dark and light themes and did not update on live theme switches — TX worked because `@networkUploadColor` was already defined. Added `@networkDownloadColor=#26A69A` to both `shared/nexis/static/themes/default/style/values.ini` and `shared/nexis/static/themes/light/style/values.ini` (paired with the existing `@networkUploadColor` row) so the RX bar now follows the active theme. Extended `tests/theme/test_theme_tokens.cpp` to scan `shared/` C++ for `value("@…")` literals and assert every referenced token exists in both themes, closing the gap that previously only validated QSS tokens.

## [2.3.14] - 2026-06-11

### Changed
- **Screenshot regression tests re-enabled in CI as non-blocking (NEX-3381):** The `ScreenshotTests` suite — excluded from CI for ~4 months after commit `5c173c7` because it hangs indefinitely on ARM64 Linux runners under xvfb — now runs as a separate `continue-on-error` step in `.github/workflows/build.yml` on Linux x64 and macOS (the ARM64 Linux job is still skipped, with an inline tracking comment). The whole `build/tests/test_screenshots/` directory is uploaded as the `screenshot-diffs-*` artifact every run, so reviewers can see actual/reference/diff PNGs for any drift. A new `workflow_dispatch` workflow (`screenshot-baselines.yml`) regenerates baselines on the same platforms and uploads them as `reference-screenshots-{linux,macos}` artifacts for maintainer review before committing. `RELEASE.md` §0 now requires green baselines (or an explicit waiver) before tagging, and `docs/ARCHITECTURE_REVIEW.md` §3B has been corrected — the previous "CI runs screenshot tests" claim was stale.
- **macOS: one-time bundle identity reset to `io.github.s4solutionsllc.Nexis` (SSO-3487, audit B3):** The macOS app's `CFBundleIdentifier` migrates from the previous unowned `com.nexis.app` to the s4solutionsllc-owned `io.github.s4solutionsllc.Nexis`, matching the Flatpak app id at `linux/flatpak/io.github.s4solutionsllc.Nexis.yml`. **This is a one-time identity reset, not an in-place rename.** After upgrading, your existing installed `Nexis.app` becomes an orphaned shell that macOS still treats as a separate, older application: it will not auto-update, Login Items still pointing at it will keep launching the old binary, default-app handlers / "Open With" choices keyed to the old bundle id will no longer route to the new one, and per-app preferences keyed under the old identifier will **not** auto-migrate (your saved Settings, theme, autostart toggle, etc. will start fresh).
  **What you need to do once after upgrading:**
  1. Quit Nexis if it is running.
  2. Delete the old `Nexis.app` from `/Applications` (and any other location you put it — `~/Applications`, Desktop, etc.) before installing the new version, so macOS does not keep both copies.
  3. Install the new `Nexis.app` from the release DMG (or `brew uninstall --zap nexis && brew install --cask nexis` if you use the Homebrew cask).
  4. **System Settings → General → Login Items & Extensions:** remove the old Nexis entry if present. Re-enable autostart from Nexis **Settings → "Start on boot"** if you want it back.
  5. **System Settings → Default Apps** (or right-click → Get Info → Open With): re-pick Nexis for any file types you previously had it routed to.
  6. Your Settings preferences (theme, language, scheduled cleans, etc.) will reset to defaults on first launch — re-set them in **Settings**.

  Why we did this: `com.nexis.app` is a domain s4solutionsllc does not own; shipping signed/notarized builds under an unowned reverse-DNS identifier is a long-running supply-chain liability and was flagged in the 2026-06-10 audit (finding B3). The new identifier is owned and consistent with the Linux Flatpak app id. Sibling finding B3.1 (bundle version override) shipped in SSO-3379.
- **CI: all GitHub Actions pinned to commit SHAs (SSO-3371 / WI-09):** Every third-party action used in `.github/workflows/` (and the `actions/*` first-party actions) is now referenced by full 40-character commit SHA with a trailing `# vX.Y.Z` comment, replacing the previous mutable `@v6` / `@v3` tag refs. Closes the `tj-actions/changed-files`-style supply-chain hole where a re-pointed upstream tag could exfiltrate the AUR signing key, Crowdin token, or Homebrew tap PAT held by these workflows. Added `.github/dependabot.yml` with the `github-actions` ecosystem so the pinned SHAs are kept fresh via weekly Dependabot PRs.
- **License SPDX identifier reconciled to `GPL-3.0-only` across packaging and docs (SSO-3373):** The repository previously claimed `GPL-3.0-or-later` in `linux/aur/PKGBUILD`, `linux/aur/.SRCINFO`, `linux/metainfo/io.github.s4solutionsllc.Nexis.metainfo.xml`, `docs/MAINTAINER_SOP.md`, `RELEASE.md`, and `CLAUDE.md`, while the `LICENSE` preamble used GPL-3.0-only wording ("version 3 of the License", no "or any later version") and `debian/copyright` used the ambiguous short form `GPL-3.0`. Upstream Stacer ships the verbatim FSF GPL-3.0 text with no author-elected "or later" application notice, so a fork cannot enlarge that grant. All five packaging/doc locations and `debian/copyright` are now `GPL-3.0-only`; the `LICENSE` preamble was already correct and is unchanged.
- **Docs:** Reconciled `docs/ARCHITECTURE_REVIEW.md` and `docs/APPLICATION_OVERVIEW.md` against the live tree (SSO-3392 / WI-30). Consolidated by-the-numbers counts (LOC, page/service/signal/timer/test totals) into a single canonical table in `APPLICATION_OVERVIEW.md` and removed the duplicated hard counts that had drifted (notably the "~6,000–7,000 LOC" Scale line, the stale `sigDashboardLayoutReset` mention and "9 signals" / "13 info classes" / "4 timers / 10 signals" claims, the wrong `~/Library/Preferences/com.nexis.plist` / `~/.config/nexis/nexis.conf` settings paths, and the "CI runs screenshot tests" claim already invalidated by WI-19). Refreshed both `Last updated` / `Version` headers to match `PROJECT_VERSION`.
- **CI:** New `scripts/check_doc_versions.sh` runs on every build matrix leg and fails the workflow if the doc version header drifts from `CMakeLists.txt`'s `project(... VERSION ...)`.

### Fixed
- **Cross-thread races on `DiskInfo` and macOS AppleSMC during a System Checkup run (SSO-3385 / audit M4):** The Maintenance Wizard's health-score check ran on a `QtConcurrent` worker that called `InfoManager::getDisks()` and `getThermalTemperature(0)` directly, while `DataRefreshService::onMediumTick()` was republishing `DiskInfo::disks` via `setDisks()` and the macOS thermal provider was reading SMC keys — both on the UI thread. The QList move-assign vs. concurrent copy is undefined behaviour, and on macOS the unguarded `if (sConn) ...` check-then-`IOServiceOpen` could double-open and leak the AppleSMC connection. Adopted the "preferred" snapshot pattern from the audit: the wizard now reads `coreCount`, load averages, the disk list, thermal-availability flags, and the temperature on the UI thread and captures them by value into the worker lambda — no provider calls happen off-thread. On macOS, `smcOpen()` is wrapped in `std::call_once` (open exactly once for the process lifetime, with a test-only counter seam) and the whole get-info/read pair in `smcReadKey` runs under a process-wide `QMutex` so concurrent callers cannot interleave `IOConnectCallStructMethod` halves on the shared connection. Covered by new concurrency stress tests in `tests/core/test_disk_info.cpp` and a macOS-only call-once invariant test in `tests/core/test_thermal_info_macos.cpp`.
- **Scheduled cleans silently never ran on display-less sessions (SSO-3368, audit H6):** Cron-launched and boot-catch-up systemd-user `--clean` invocations aborted during `QApplication` construction because cron unsets `DISPLAY`/`WAYLAND_DISPLAY` and the generated unit files set no QPA platform. The binary now detects the `--clean` / `--check-threshold` flags before constructing `QApplication` and `qputenv`s `QT_QPA_PLATFORM=offscreen` when the user has not pinned a platform themselves. As belt-and-suspenders, the generated cron entry and systemd user-unit `ExecStart`/`Environment=` lines also set `QT_QPA_PLATFORM=offscreen` so scheduled cleans run to completion with no display present.
- **App could crash when deleting or trashing large search results (SSO-3365, audit H4):** `FileSearchService::moveToTrash` and `deleteFile` ran `CommandUtil::exec("mv"/"rm")` synchronously from the UI-thread context-menu slot. `CommandUtil::exec` throws a raw `QString` on any `QProcess` error — including the 30 s timeout that fires on bulk operations — and the exception escaped the slot through the event loop, aborting the app while the UI was already frozen waiting on the call. Both methods now dispatch on a worker thread (`QtConcurrent::run`) with a `try/catch (const QString&)` around the exec, report success/failure through a new `fileOperationFinished` signal, and raise the file-op timeout to five minutes. The Search page reacts to the signal: rows are removed only after the operation completes, and failures show an inline error instead of terminating.
- **Crash when closing the System Checkup dialog mid-scan (SSO-3362):** Closing the Maintenance Wizard while its four parallel `QtConcurrent::run` checks were still running could trigger a use-after-free — the dialog uses `WA_DeleteOnClose`, so pressing Esc or **Close** before the workers finished freed the dialog while detached lambdas were still about to call `QMetaObject::invokeMethod(this, …)` against deleted memory. Each worker now captures a `QPointer<MaintenanceWizardDialog>` instead of a raw `this`, dispatches results through the context-object + functor overload of `invokeMethod` (so queued slots are dropped when the dialog dies), and the dialog stores every `QFuture` as a member and `waitForFinished()`s them in a new destructor as a backstop against any worker that already passed the guard check.
- **Data race on disk-health cache (SSO-3364 / audit H3):** `DataRefreshService::onSlowTick()` ran `DiskHealthInfo::discoverDrives()` on a `QtConcurrent` worker every 30 s; the worker did `mDrives.clear()` + per-drive `smartctl` append for several seconds, while the UI thread concurrently copied `mDrives` via `getDrives()` (Hardware Info, Resources, Dashboard, Settings) and wrote `mDrives[i]` from `refreshHealthElevated()` on user "Unlock Drive" clicks. Concurrent `QList<DriveHealth>` clear/copy/index-write is undefined behaviour. Adopted the publish pattern (mirrors `DiskInfo::collectDiskInfo()`/`setDisks()`): the worker now builds a fresh `QList<DriveHealth>` into a local via the new `collectDriveHealth()` virtual and returns it; the UI thread publishes via the new `InfoManager::setDriveHealth()` from inside the `QMetaObject::invokeMethod` hop. As a defence-in-depth a `QMutex` also guards every `mDrives` read/write (`getDrives`, `hasDrives`, `setDrives`, both `refreshHealthElevated*` paths). pkexec/sudo invocations run outside the lock so a slow elevation prompt does not block UI snapshots. Applied symmetrically on Linux and macOS.
- **Quitting Nexis right after opening System Logs could crash (SSO-3363 / audit H2):** `LogProvider::cancel()` did `mProcess->kill(); mProcess->waitForFinished(3000); mProcess->deleteLater();`, but `waitForFinished()` processes events on the calling thread, so `kill()`'s CrashExit delivered `finished()` synchronously. The connected error path nulled `mProcess` and called `deleteLater()` on it, and the next line in `cancel()` then dereferenced null. The macOS path was the easiest to hit because `log show --last 1h` runs for several seconds, so closing the window while the fetch was still in flight reproduced the segfault reliably. `cancel()` now disconnects the slot, copies the pointer to a local, and nulls the member before touching the QProcess, so the QProcess is destroyed exactly once and `cancel()` is safe whether the process is running, finished, or already cleaned up. Covered by `tests/managers/test_log_provider.cpp`.
- **`MAINTAINER_SOP.md` §2 contradicted `.github/FUNDING.yml`:** The SOP's "no monetization, ever" rule was being read as a blanket ban that also covered the repo-level GitHub Sponsors button enabled in `.github/FUNDING.yml`, even though that file had been live since the start of the project as a CEO-approved external donation surface. §2 rule 2 now states the scope explicitly: in-app monetization (paid tiers, donation walls in the app, sponsor-only features, telemetry-for-revenue) remains prohibited *Ever*, while passive external donation links at the repo level (`FUNDING.yml`, GitHub Sponsors, Open Collective, etc.) are permitted, with the active `.github/FUNDING.yml` itself serving as the CEO-approved record for this exception. No code or runtime behavior changes — governance documentation only.

### Security
- **Release builds now ship explicit hardening flags on Linux + macOS (SSO-3397 / WI-34, audit L1):** Previously only the `.deb` channel got `DEB_BUILD_MAINT_OPTIONS = hardening=+all` via `linux/debian/rules`; the AppImage, raw tarball, and macOS `.app` inherited whatever the toolchain happened to default to. The top-level `CMakeLists.txt` now sets a Release-config hardening baseline for every GCC/Clang/AppleClang build of `nexis`, `nexis-core`, `nexis-gui`, and the test binaries: `-fstack-protector-strong`, `-D_FORTIFY_SOURCE=2` (with a `-U_FORTIFY_SOURCE` first so distros that already inject it don't warn), and `CMAKE_POSITION_INDEPENDENT_CODE=ON`. On Linux the executable is also linked with `-Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -pie` for full RELRO + immediate symbol binding + non-executable stack + PIE; on macOS the link line gets `-Wl,-bind_at_load` (the macOS equivalent of `-z,now`). Distro channels' own hardening (Debian's `dpkg-buildflags`, Arch's `LDFLAGS`) layers harmlessly on top. Guarded by a new `NEXIS_ENABLE_HARDENING` CMake option (default `ON`) so downstream packagers can opt out without patching the file. Spot-check with `checksec`/`hardening-check` on the resulting binary.
- **macOS: AppleScript injection in app uninstaller (SSO-3366, audit S1):** `PackageToolMacOS::trashApps` interpolated each `.app` bundle path into a `tell application "Finder" to delete POSIX file "%1"` AppleScript source string and ran it through `osascript -e`. Bundle names containing a double quote (legal on macOS — anything downloaded from the web can ship one) terminated the string literal and let attacker-controlled AppleScript run when the user clicked Uninstall, including `do shell script` for arbitrary code execution. The trashing path now uses `QFile::moveToTrash` (`NSFileManager::trashItemAtURL:` on macOS), which takes an `NSURL` and has no shell or AppleScript parsing surface — bundle-name metacharacters are treated as path data, not code.

## [2.3.13] - 2026-06-05

### Fixed
- **AUR install still failed on CachyOS / Arch with `undefined symbol: main` (GH#82):** The v2.3.12 fix only disabled the test suite, which moved the same link failure from the test binaries onto the `nexis` executable itself. The real cause is GCC LTO objects being linked by LLD: on distros that enable LTO by default (CachyOS), GCC emits slim LTO objects, and LLD cannot resolve symbols defined inside them — the lone real-object reference into LTO code (`Scrt1.o`'s `_start → main`) surfaces as an undefined `main`, which is why *every* executable failed and only on `main`. The build also force-selected LLD via the in-tree `CXXBASICS_USE_FASTER_LINKERS` helper, so the LLD half was self-inflicted. The PKGBUILD now sets `options=('!lto')` (no `-flto` injection) and builds with `-DCXXBASICS_USE_FASTER_LINKERS=OFF` (system default linker), making the packaged build a reproducible default-toolchain build that links cleanly. The underlying GCC 16 + LLD + LTO toolchain incompatibility remains tracked in GH#88.

## [2.3.12] - 2026-06-04

### Fixed
- **Changing the interface language had no visible effect:** Selecting a different language in Settings only saved the preference — the translator is installed once at startup and was never swapped, and no page implements live `retranslateUi()`, so the UI stayed in the previous language with no indication why. Nexis now tells the user the change takes effect after a restart and offers to relaunch immediately. (The relaunch uses `$APPIMAGE` when running from an AppImage so it re-launches the bundle rather than the transient mount path.)
- **Several UI strings were not translated in non-English locales (GH#87):** Size unit labels in the Search page (Bytes / Kibibytes / Mebibytes / Gibibytes), titlebar action names in GNOME Settings, and health score labels (Excellent / Good / Poor) were bypassing the translation system. These strings now go through `tr()` and will be picked up by lupdate on the next translation update cycle.
- **Translations did not load in development builds (GH#85):** Running `build/output/nexis` in a non-English locale always showed English because `.qm` files compiled to `build/` rather than `build/output/translations/` where the binary searches. A `POST_BUILD` step now copies the compiled `.qm` files next to the binary so in-tree runs respect the selected language. Installed packages (AppImage, `.deb`, AUR) were unaffected.
- **AUR build failed on Arch Linux / CachyOS and other Arch derivatives (GH#82):** `paru -S nexis` and `yay -S nexis` aborted with `undefined symbol: main` when building the test suite on GCC 16 + LLD toolchains. The PKGBUILD now passes `-DBUILD_TESTING=OFF` so tests are not compiled during install. The underlying GCC 16 + LLD linker incompatibility is tracked in GH#88.

## [2.3.11] - 2026-06-04

### Fixed
- **Translations still missing from all packages — the actual root cause (GH#75):** The 2.3.10 fix added a CMake install rule and corrected the runtime load path, but translations remained broken because no `.qm` files were ever compiled in the first place. The `qt_create_translation` call was passing the bare token `NEXIS_TRANSLATIONS` instead of the expanded `${NEXIS_TRANSLATIONS}` list, so none of the arguments ended in `.ts` and lrelease was never invoked — `QM_FILES` came out empty, leaving the install rule with nothing to stage. Switched to `qt_add_translation(QM_FILES ${NEXIS_TRANSLATIONS})`, which compiles the committed `.ts` files into `.qm` via lrelease only. (Using `qt_add_translation` rather than `qt_create_translation` also keeps the build from running lupdate over the sources and rewriting the tracked `.ts` files — source-string extraction is owned by the separate `lupdate.yml` workflow.) All 34 locales now compile and install to `share/nexis/translations/`.

## [2.3.10] - 2026-06-03

### Fixed
- **Incorrect update count due to phased APT packages (GH#76):** Nexis was counting packages that APT has deferred via phased rollout as available updates, causing the count to disagree with Update Manager and `apt` itself. Phased packages appear in `apt list --upgradable` output annotated with `[phased X%]`; these lines are now excluded from the count. Also resolves the related symptom where the update notification banner appeared only once: with phased packages no longer inflating the count, it correctly drops to zero when no real updates exist, restoring the 0→N transition that triggers the notification.
- **Dashboard default layout overlap in bottom-right corner (GH#77):** On new installations with all four sensor types present (GPU, thermal, battery, fan), the default layout generator placed the health tile at column 4 — past the grid boundary — causing it to be clamped to column 3 and overlap the fan tile. The health tile is now anchored to a fixed position (row 1, col 3) and sensor tiles fill columns 0–2, wrapping to the next row if all four are present.
- **Translation files missing from AppImage and .deb packages (GH#75):** All non-English locales were silently broken in distributed packages — the compiled `.qm` files were generated during the build but never included in the install step, so `cmake --install` (used by both the AppImage and .deb workflows) left them behind. Added a CMake install rule that stages all `nexis_*.qm` files into `share/nexis/translations/`. Also fixed the runtime translation load path in `AppManager`: the app was searching `<binaryDir>/translations/` (correct for dev builds only), but installed layouts place the files at `<binaryDir>/../share/nexis/translations/`. The loader now tries the FHS-installed path first, with the beside-binary path as a fallback for development builds.

## [2.3.9] - 2026-06-01

### Fixed
- **GPU gauge shows N/A on NVIDIA cards (GH#72):** `discoverGpus()` was storing the sysfs PCI bus ID (`"0000:07:00.0"`) in the utilization query field, but the update path called `.toInt()` on it expecting a numeric nvidia-smi device index — always failing, leaving utilization at -1 (N/A) indefinitely. Fixed by querying `nvidia-smi --query-gpu=index,pci.bus_id` once at startup to map each GPU's PCI address to its nvidia-smi index, with normalization to reconcile sysfs (`0000:07:00.0`) and nvidia-smi (`00000000:07:00.0`) formats. GPU name detection was unaffected.

## [2.3.8] - 2026-05-29

### Fixed
- **Multi-battery support (GH#65):** Nexis now detects and displays all batteries on systems with more than one (e.g. Dell Rugged 14 with BAT0 + BAT1). The Hardware Info page shows each battery in its own labeled section with full individual data. The Dashboard tile and live battery signal show an aggregated view: capacities are summed, charge% is weighted by max capacity, and the system is shown as charging if any battery is charging. Single-battery systems are unaffected.
- **Orphaned Packages view shows safety information (GH#67):** The Orphaned Packages tab now displays a 4-column table (Package, Size, Installed, Reverse Deps) instead of a flat list. On APT systems, each package shows whether it was manually or automatically installed (`apt-mark showauto`) and how many installed packages depend on it (`apt-cache rdepends --installed`). The Reverse Deps column is color-coded: green = safe to remove, amber = manually installed (review first), red = has dependents (do not remove). Pacman and DNF systems show "—" in the new columns; enrichment for those package managers is a future follow-up.
- **Disk analyzer tools now detected when installed via Flatpak (GH#64):** Baobab, Filelight, and QDirStat are now correctly detected whether installed natively (apt/dnf/pacman) or via Flatpak. A `findDesktopApp()` helper tries the native binary name, then the Flatpak app-ID wrapper in `$PATH`, then a direct filesystem probe of the standard Flatpak export paths — fixing detection when Nexis is launched as an AppImage in a non-login shell where Flatpak's exports directory is absent from `$PATH`. When a Flatpak-installed tool is detected, it is launched via `flatpak run <app-id>` rather than the missing native binary name.
- **System Logs severity filter now re-fetches from journalctl (GH#62):** Selecting "Error && Above" (or any non-All filter) now passes `--priority=0..N` to journalctl so the returned entries are guaranteed to match the chosen level. Previously the filter operated client-side on a truncated dataset, meaning no errors appeared if the last N journal lines happened to be INFO/DEBUG.
- **Update checker locale-safe on non-English systems (GH#61):** `checkApt`, `checkSnap`, and `checkFlatpak` now force `LANG=C` when invoking package-manager commands, preventing localized output from breaking string-based parsing. `checkSnap` also guards against malformed lines with fewer than 2 columns. `checkFlatpak` uses `--columns=application,version` for a stable tab-separated format and reads the version from column index 1 (was 2).

## [2.3.7] - 2026-05-17

### Added
- **Start minimized in system tray (SSO-354 / GH#54):** New "Start minimized in system tray" toggle under Settings → General. When enabled, Nexis launches with no main window — only the tray icon is visible — regardless of how it was started (autostart entry, launcher click, terminal). The tray icon's activate handler restores the window as usual. Equivalent to passing `--hide` on every launch; useful when Nexis is in your login autostart list and you don't want it to pop on top of other apps. Off by default.
- **Flatpak packaging (SSO-93):** Flatpak manifest added at `linux/flatpak/io.github.s4solutionsllc.Nexis.yml` for Flathub submission. Uses the KDE Qt6 SDK (`org.kde.Platform//6.7`), `--filesystem=host` for broad system access (required for /proc, /sys, hardware sensors, APT sources, and host tool invocations), and `org.freedesktop.PolicyKit1` D-Bus access for privileged operations. Reviewer justification at `docs/flatpak-reviewer-justification.md`.
- **Unit tests for `FileUtil::writeRootFile()` (SSO-353 / GH#50):** Two new test cases in `tests/utils/test_file_util.cpp` close the coverage gap on the privileged-write helper — `writeRootFile_returnsFalseOnNonLinux` verifies the macOS/non-Linux branch is a no-op returning `false`, and `writeRootFile_invalidPath` verifies the Linux branch fails gracefully (returns `false`, does not crash) when handed a path under a non-existent directory. Both tests `QSKIP` on the wrong platform so `ctest -R FileUtil` passes on Linux and macOS without root.
- **Select All / Clear All on System Cleaner (GH#55 / SSO-355):** Restored the bulk-toggle affordance that was dropped in the FR-130 card redesign. New button in the System Cleaner footer flips every category checkbox in one click and switches its label to **Clear All** once everything is selected.

### Fixed
- **System Cleaner cards overflowed in small windows (GH#55 / SSO-355):** The FR-130 card grid was added to a `QScrollArea` but the layout gave it no stretch and no minimum height, so the page fought for its 1025×736 design size and clipped at smaller resolutions. The scroll area now stretches to fill the available vertical space and compresses to a small minimum, engaging its vertical scrollbar instead of overflowing the page.
- **Window size & position not remembered (GH#55 / SSO-355):** Nexis re-centered itself at the default 1025×736 on every relaunch because no `saveGeometry()` / `restoreGeometry()` calls existed for the main window. `App::closeEvent` now persists `QMainWindow::saveGeometry()` and `saveState()` to `SettingManager`, and `App::init` restores them on launch (including from the minimize-to-tray "ignored close" path, where Qt does not deliver a second `closeEvent` if the user later quits from the tray).
- **Toggle indicators invisible on Linux Mint 22 and other Ubuntu 24.04 derivatives (SSO-381 / GH#42):** Toggle indicators no longer disappear on Linux Mint 22 ("Zena") and other Ubuntu-24.04 derivatives that omit Qt6 SVG plugins from their default seed. The `.deb` now explicitly depends on `libqt6svg6` (which pulls the `imageformats/libqsvg.so` plugin), `qt6-qpa-plugins`, and a Qt platform theme (`qt6-gtk-platformtheme | qt6ct`). As a runtime safety net, the binary detects whether the SVG image plugin is loadable via `QImageReader::supportedImageFormats()` and falls back to PNG siblings for the `QCheckBox::indicator` images (`checkbox`, `un-checkbox`, `circle-checked`, `circle-unchecked`) when it is not, logging a single warning. PNG indicator assets are now bundled in `static.qrc` so the fallback works without any optional Qt6 packages installed.
- **Linux Wi-Fi not detected in Network Usage (SSO-351 / GH#43):** Wi-Fi interfaces (`wlp*`, `wlan0`) were missing from the Network Usage interface selector and never accrued RX/TX stats, even when they were the only active connection. The Linux `NetworkInfo` constructor cached the first non-loopback up+running interface exactly once at startup, which let docker0 / virbr0 / a stale ethernet entry shadow the real default. The implementation now re-enumerates active interfaces on every sample tick, picks the default from `/proc/net/route` (with a flag-based fallback), and emits a per-interface RX/TX snapshot so `NetUsageTracker` records traffic for every up+running interface. macOS uses the same per-interface enumeration via `getifaddrs()` for parity.

## [2.3.6] - 2026-05-11

### Fixed
- **Process page search returns no results (NEX-279):** Typing in the Search field on the Processes page now correctly shows matching processes. The previous implementation converted user input through `QRegularExpression::wildcardToRegularExpression()`, which anchors plain text into an exact-match pattern (e.g. `chrome` → `^(?:chrome)$`), so nothing ever matched. Replaced with `setFilterFixedString()` for case-insensitive substring matching. Clearing the field now restores the full process list automatically.
- **App shows wrong version number (NEX-280):** The application title and Settings page could display a version two patch releases behind the installed package (e.g. 2.3.3 when 2.3.5 was installed). The root cause was that `CMakeLists.txt` was not updated during the 2.3.5 release, and the `debian/rules` override mechanism was only active when an environment variable was explicitly set — which Launchpad PPA builds do not do. `CMakeLists.txt` is now kept in sync with the release version, and `debian/rules` auto-derives the version from `debian/changelog` so the two can never drift again.

## [2.3.5] - 2026-05-06

### Fixed
- **APT Repository Manager: false "unreachable" for Release-only repos (NEX-276):** Repositories that serve `Release`/`Release.gpg` but not `InRelease` (e.g. Vivaldi) were incorrectly flagged as "Repository unreachable." APT itself handles this format without issue. The root cause was that Qt's network layer returns a typed error for HTTP 404 rather than an HTTP status code, and Nexis was not checking the status code first. Fixed by prioritising the HTTP status — any response from the server is now correctly treated as reachable regardless of the status code.

## [2.3.3] - 2026-04-24

### Added
- **Restore individual dashboard tiles (FR-132):** "Add Tile ▾" button appears in the Edit Mode toolbar whenever at least one tile has been hidden. Clicking it opens a dropdown listing each hidden tile by name; selecting one restores it to the first available grid cell. All other tile customizations (colors, styles, sizes) are preserved. The button hides once all tiles are visible.

## [2.3.2] - 2026-04-24

### Fixed
- **Scrollable sidebar nav (BUG-134):** The sidebar navigation no longer squishes or clips buttons on short windows or small screens (e.g. 1366×768 with fractional DPI). Nav sections are now wrapped in a `QScrollArea` between the pinned logo row and pinned version/feedback footer. A thin 4px scrollbar appears only when content overflows. Badges (System Cleaner, APT updates) correctly hide when their button is scrolled out of view.

## [2.3.1] - 2026-04-23

### Changed
- **Helpers page two-section layout (FR-131):** The Helpers header bar is now split into a **TOOLS** section (tab-style nav buttons) and a **MAINTENANCE** section (clickable cards with title + description). Maintenance cards trigger existing confirm-dialog actions. macOS shows 4 cards (Flush DNS, Rebuild Spotlight, Verify Disk, Rebuild Launch Services); Linux shows 1 (Flush DNS).

## [2.3.0] - 2026-04-23

### Added
- **CPU Pressure Stall chart (FR-124, Linux):** New "CPU Pressure Stall (some)" HistoryChart on the Resources page — three series tracking avg10, avg60, and avg300 stall percentages sourced from `/proc/pressure/cpu`. Created only when the PSI file is present (kernel 4.20+); zero cost when hidden via DataRefreshService subscription gating.
- **HTML system report export (FR-126):** New "Export as HTML…" button on the Hardware Info page. Generates a self-contained HTML file (inline CSS) containing a live system snapshot (CPU %, memory, GPU, battery), all hardware tables, top-10 processes by CPU at export time, and pending update count. Default filename: `nexis-report-YYYY-MM-DD.html`.
- **Wake-on-LAN helper (FR-120):** New Helpers-page card on both platforms. "Discover Hosts" reads the ARP cache (`/proc/net/arp` on Linux, `arp -a` on macOS) on a worker thread and populates a table with IP, MAC, hostname, and an editable friendly-name column. "Wake" sends a standard 102-byte UDP magic packet to broadcast port 9 — no root needed. Friendly names persist in settings as a JSON map keyed by MAC address.
- **Quick Actions tray submenu (FR-125):** New "Quick Actions" submenu in the system tray right-click menu. Contains: Open Command Palette, Run System Cleaner Scan (navigates to the cleaner and starts a full scan via new `SystemCleanerPage::quickScan()`), and a Power Profile submenu (Linux only, checked state refreshed on each open).
- **Swappiness tuning widget (FR-81, Linux):** New Helpers-page card surfaces `/proc/sys/vm/swappiness` with three presets (Desktop 60, Performance 10, Low-RAM 80) plus a Custom slider. Shows current swap usage alongside. "Persist across reboots" checkbox writes `/etc/sysctl.d/99-nexis-swappiness.conf`. All writes read back to verify.
- **CPU turbo + frequency tuning (FR-117, Linux):** New Helpers-page card reads and writes `/sys/devices/system/cpu/cpuN/cpufreq/*` plus `intel_pstate/no_turbo` and `cpufreq/boost`. Turbo toggle, min/max frequency sliders (linked so min ≤ max), whole-CPU governor combo, and an expandable "Show per-core governors" grid for per-core overrides. Optional persist-on-launch via four new settings; skips re-apply when the current state already matches. Disabled with a notice when power-profiles-daemon owns the backend.
- **SSD TRIM scheduler widget (FR-118):** New Helpers-page card. Linux: enable/disable `fstrim.timer` toggle, last-run / next-run timestamps parsed from `systemctl list-timers`, "Run TRIM now" button firing `fstrim -av` and capturing the per-mount output. Button hidden when `fstrim` isn't installed. macOS: read-only status parsed from `diskutil info -plist /` (with `system_profiler SPNVMeDataType` fallback) — APFS manages TRIM automatically.
- **Per-process GPU% and VRAM columns (FR-115, Linux):** Two new columns on the Processes page — hidden by default, unhide via the header right-click menu. Collection is gated on column visibility (zero cost when hidden). Intel/AMD path walks `/proc/<pid>/fdinfo/*` directly, deduping by `drm-client-id` and summing engine nanoseconds for the `%GPU` delta. NVIDIA path uses a new `NvidiaSmiPmonStreamer` — one persistent `nvidia-smi pmon` child plus one persistent `nvidia-smi --query-compute-apps` child — so no forks per tick. macOS renders em-dash for now (no supported public API; tracked as FR-128).
- **Pin processes + threshold alerts (FR-116):** Right-click any row in the Processes table for "Pin / Unpin", "Set Alert…" and "End Process". Pinned rows stay at the top regardless of sort column or direction (new `PinSortFilterProxyModel` subclass). Threshold dialog sets per-process CPU% and/or memory (MB/GB) triggers; tray notifications fire with per-(name, metric) hysteresis and aggregate across all PIDs sharing a name ("chrome (12 processes) exceeded 4.0 GB"). State persists via new `ProcessPrefsManager` singleton.
- **Listening-port audit (FR-121):** The Open Ports & Connections widget gains a **Path** column and flags binaries whose path isn't under a trusted prefix (platform defaults + user extras via new `TrustedBinderPrefixes` setting) — warning-colored Process + Path cells with a ⚠ glyph on the Process cell. On macOS a new "Verify Signatures" button lazily runs `codesign -dv` per unique path on a worker thread and re-renders unsigned binaries with destructive color. Path resolution via `readlink /proc/<pid>/exe` on Linux, `proc_pidpath` on macOS.
- **Pre-clean snapshots (FR-112):** Optional "Create restore point before cleaning" toggle under Settings → Scheduled Cleaning. On Linux, runs `timeshift --create` via `pkexec`; on macOS, runs `tmutil localsnapshot` (no elevation prompt). Default **off** — users opt in. The toggle is hidden entirely when the platform tool isn't installed. Snapshot failure never blocks the clean. Applies across the Maintenance Wizard, scheduled cleans, and the interactive System Cleaner page.
- **Downloads auto-cleanup (FR-113):** New "Old Downloads" category in the Schedule Editor. When included in a schedule, Nexis moves files older than a configurable age (default 30 days) from a configurable folder (default: platform Downloads dir) to the Trash — recoverable, not `rm -rf`'d. Path and age controls live under Settings → Scheduled Cleaning. Respects existing cleaner exclusions. Answers the most-upvoted open Stacer feature request (#286).
- **Per-category scan trend (FR-114):** Every System Cleaner scan persists per-category byte totals (rolling 20-sample window). Each category now shows a current-size label + a 60×16 sparkline below the checkbox; tooltip reports the delta vs the previous scan. Turns one-shot cleans into informed decisions — users can see which categories are actually worth cleaning.
- **macOS app crumbs scanner (FR-123):** After uninstalling a `.app` or Homebrew cask via the Uninstaller, Nexis scans `~/Library/{Preferences,Application Support,Caches,Saved Application State,Containers,Logs}` for files matching the app's bundle identifier and offers a review-then-delete dialog. All rows pre-selected; user unchecks what they want to keep, then Delete Selected moves the rest to the Trash via `QFile::moveToTrash`.

### Changed
- **System Cleaner redesign (FR-130):** Replaced the icon-grid category selector with a two-column card layout matching the Nexis design system. Each category is now a titled card with a subtitle showing relevant paths, a size label populated after scanning, and a checkbox with a highlighted border when selected. A persistent footer shows the estimated total recoverable space and a "Clean selected" button — no need to navigate away from the category view. "View scan results →" still provides access to the detailed file tree for power users. Scan/clean state is maintained on page 0 via retained result lists, so cleaning from the card view works without visiting the tree.
- **Linux process-info overhaul (FR-127):** Replaced the per-tick `ps ax -weo ...` fork in `ProcessInfoLinux::updateProcesses` with a direct `/proc` walk. Eliminates ~10–20 ms of subprocess overhead per Processes-page refresh. Constructor caches `sysconf(_SC_CLK_TCK)`, `_SC_PAGESIZE`, `/proc/stat btime`, and `/proc/meminfo MemTotal`. Username/group lookups memoise `getpwuid_r`/`getgrgid_r` results. Parsing was extracted into a pure `ProcInfoParser` module with 22 fixture-backed tests covering `comm`-with-parens, kernel threads, and the boot-time/meminfo/uptime readers.
- **Linux GPU polling goes fork-free (FR-106 Step C):** Replaced Bundle B's per-tick `nvidia-smi` fork with a persistent `nvidia-smi -l 1` child via `NvidiaSmiStreamer`. Zero forks per tick in steady state. Mirrors the macOS `NettopStreamer` pattern introduced in FR-102. `NvidiaSmiCache` namespace API is unchanged so `GpuInfoLinux` and `FanInfoLinux` call sites didn't move.

## [2.2.17] - 2026-04-21

### Changed
- **Runtime performance (Bundle B — FR-99 through FR-109):** 11 coordinated changes that make Nexis measurably lighter to leave open all day and smoother while navigating. Headline wins: no more per-tick `nvidia-smi` / `nettop` / `lscpu` forks, no `/proc/<pid>/io` or `QStorageInfo` work on the UI thread, no sampling at all for pages that aren't visible.
  - **`CommandUtil::execAsync()` + UI-thread audit (FR-99):** New `QFuture<ExecResult> execAsync(...)` helper so future work can run subprocess calls off the main thread. Debug-build assert gated on `NEXIS_ASSERT_ASYNC_EXEC` catches regressions.
  - **Sparkline `replace()` (FR-107):** Dashboard tiles now update their sparkline series in one call per tick instead of 60 append calls — eliminates ~300 chart relayouts/sec across the 5 MetricTile/HybridTile instances.
  - **Temp & fan at 5 s cadence (FR-104):** Thermal and fan readings moved from the 1 Hz fast tick to the 5 Hz medium tick. Thermals move on minute scales; the high-frequency polling was wasted work.
  - **sysfs-first CPU freq on Linux (FR-100):** `CpuInfoLinux::getAvgClock()` now reads `/sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq` first, falls through to `/proc/cpuinfo` then `lscpu`. Capability flags cached so dead paths aren't re-probed. Replaces a bash+lscpu fork per second on Linux.
  - **Three micro-optimisations (FR-109):** `CommandUtil::isExecutable()` now memoises PATH lookups in a thread-safe `QHash`. `DiskHealthInfoMacOS::discoverDrives()` dedupes by (model, size) *before* the `smartctl` fork — three out of four wasted forks eliminated on Apple Silicon. `main.cpp`'s `messageHandler` keeps a single log file handle open (mutex-guarded) instead of opening/closing per message; also now installed for the GUI path so normal runs actually produce a log file.
  - **`mountedVolumes()` off the UI thread (FR-101):** The 5 s disk-usage tick now runs `QStorageInfo::mountedVolumes()` on a QtConcurrent worker with a re-entrancy guard. No more stutter when a slow NFS/SMB mount is asleep.
  - **Batched `nvidia-smi` (FR-106):** New `NvidiaSmiCache` runs one combined `nvidia-smi --query-gpu=index,utilization.gpu,fan.speed` per tick covering every device. `GpuInfoLinux::updateGpuInfo` and `FanInfoLinux::readNvidiaSpeed` both read from the shared snapshot. Collapses 2-4 forks/sec to 1 on Linux+NVIDIA.
  - **Skip disk/net I/O when cols hidden (FR-108):** `ProcessInfo::setCollectDiskIO()` / `setCollectNetIO()` gate the per-PID I/O collection. `ProcessesPage` drives them from the current column visibility. By default all four I/O columns are hidden, so out-of-the-box users pay no cost: no `/proc/<pid>/io` walk on Linux, no `nettop` fork on macOS.
  - **Subscriber-gated DataRefreshService (FR-103):** New `Signal` enum + `subscribe/unsubscribe/hasSubscribers` API. Dashboard and Resources register for the signals they render in `onPageActivated` and release in `onPageDeactivated`. `onFastTick` / `onMediumTick` skip entire sample blocks when nothing is interested — the `nvidia-smi`, `QStorageInfo` walk, and GPU/memory/network samples all silence when the user is on Settings or Processes.
  - **Low-power cadence tiers (FR-105):** New `PowerMode { Normal, Battery, Unfocused }` on `DataRefreshService`. `App::changeEvent` emits `sigAppFocusChanged` on `QEvent::WindowActivate/WindowDeactivate`. Intervals: Normal 1/5/30 s, Battery 2/10/60 s, Unfocused 5/30/60 s. Unfocused wins over Battery when both apply.
  - **Persistent `nettop` streamer on macOS (FR-102, macOS half):** New `NettopStreamer` owns one long-lived `nettop -P -d -s1` `QProcess` and harvests deltas from its streaming stdout. Starts only when Net Down/s or Net Up/s columns are visible; tears down otherwise. Eliminates the 50-150 ms per-tick fork cost on macOS. The Linux half (replacing `ps ax` with a direct `/proc` walk) is split out as FR-127 for a follow-up — FR-108 already killed the expensive `/proc/<pid>/io` loop on Linux, so most of that side's savings have already landed.

## [2.2.16] - 2026-04-20

### Changed
- **Cold launch performance (FR-96, FR-97, FR-98 — Bundle A):** App launches faster and uses less idle RAM, especially on systems with external/spinning drives. Three coordinated changes:
  - **Lazy page construction (FR-97):** Only the Dashboard is built at launch. The 12 other sidebar pages (Hardware Info, Resources, System Cleaner, Disk Tools, Search, Processes, Services, Startup Apps, Uninstaller/Applications, Helpers, System Logs, Settings, plus conditional APT/Homebrew, Docker, GNOME Settings) now build on first navigation via a new `PageSlot` factory registry. Pages you never visit don't consume memory or run their initialization work. `App::ensureAllPages()` helper added for the screenshot test harness.
  - **Async disk-health discovery (FR-96):** `DiskHealthInfo::discoverDrives()` no longer runs synchronously inside the `InfoManager` singleton constructor. Discovery is driven off the main thread by `DataRefreshService::onSlowTick()` — `diskutil`, `smartctl`, and sysfs reads happen on a `QtConcurrent` worker while the main window paints. Removes 300-1500 ms of blocking I/O from the critical path on macOS (much more with spinning rust). Three one-shot `hasDiskHealth()` readers now re-evaluate on the first `diskHealthUpdated` emission: `ResourcesPage` creates the disk-temperature chart lazily, `SettingsPage` unhides the disk-health-alert checkbox, and `DashboardPage` updates the Health-Score `smart` component availability.
  - **Deferred Hardware Info populate (FR-98):** `HardwareInfoPage::init()` (sysctl, SMART, fan, battery I/O) now runs on first `showEvent()` rather than at page construction — zero cost until the user navigates to the page.

## [2.2.15] - 2026-04-17

### Fixed
- **Sidebar update badge alignment (BUG-109):** The green "available updates" counter badge (and the System Cleaner size badge) now re-snap to their sidebar buttons when the main window is resized. Previously these absolutely-positioned badges only repositioned on count changes, sidebar collapse, and section toggles — a window resize left them pinned at stale coordinates until another trigger fired. `App` now overrides `resizeEvent()` to call the existing `repositionBadges()` helper.

## [2.2.14] - 2026-04-17

### Changed
- **GPU selector UX (FR-95):** The Dashboard GPU tile no longer displays a permanent dropdown below the gauge. On multi-GPU systems a gear icon appears on the tile; clicking it opens a popup menu of available GPUs, and selecting one immediately dismisses the menu. The tile's subtitle continues to show the active GPU name. Behaviour unifies with the existing disk/temp/fan selectors and preserves all persistence (`SettingManager::setGpuDeviceId`), subtitle updates, sparkline reset on switch (BUG-71 guard), kiosk mode, and tile-style-change handling.
- **Tile gear icon placement (FR-95):** The shared `MetricTileBase::repositionGearButton()` now anchors the gear icon to the top-left, immediately to the right of the tile's title label, on all tiles with selectors (GPU, Disk, Temp, Fan) instead of the top-right corner. Position is measured via `QFontMetrics::horizontalAdvance()` on the title text so it stays tight to the label regardless of tile width.

## [2.2.13] - 2026-04-15

### Fixed
- **Compact window layout (FR-94):** Resolved window-too-large issues on small high-DPI screens (e.g., 2160×1440 12-inch laptops). System Cleaner Scan button is now visible without resizing or dragging. Changes: reduced System Cleaner top spacer (100 px → 20 px) and pre-scan fixed spacer (30 px → 10 px); scan results Size column is always visible via proportional column sizing (no more hard-coded 600 px filename column); empty-state placeholder height reduced from 200 px to 100 px on Startup Apps, Services, APT Repository Manager, and Uninstaller; sidebar button height reduced from 36 px to 32 px; window minimum size enforced at 700×480 px.
- **Responsive row wrapping (BUG-108):** Three pages now dynamically reflow their controls at narrow window widths. Disk Tools "Large & Old Files" filter row splits into two rows (Size + Age / Match + Scan) when page width drops below 720 px. Search Advanced Search pane switches from a two-column grid to a stacked single-column layout below 560 px. Helpers navigation button row wraps into two rows when all buttons no longer fit on one line; navigation buttons also enforce a minimum width so text is never clipped.

## [2.2.12] - 2026-04-08

### Changed
- **QSS migration (FR-89):** Migrated 13 remaining inline `setStyleSheet()` calls in repo detail panel and network diagnostics widget to property-driven QSS selectors. All widget colors now come from theme tokens via the global stylesheet.
- **Attribution:** Added explicit copyright lines for Oğuzhan İnan (original Stacer, 2017-2020) and S4 Solutions, LLC (Nexis fork, 2025-2026) to `LICENSE`. Reworded the homepage footer to lead with S4 Solutions maintenance while preserving Stacer attribution.

## [2.2.11] - 2026-04-01

### Added
- **Homebrew Cask (FR-92):** macOS users can now install via `brew tap s4solutionsllc/nexis && brew install --cask nexis`. Custom tap auto-updates on new releases via GitHub Actions.
- **macOS code signing and notarization (FR-93):** macOS .dmg releases are now signed with a Developer ID certificate and notarized by Apple. The app opens without Gatekeeper warnings on first launch.

## [2.2.10] - 2026-04-01

### Fixed
- **PPA (FR-90):** Fixed Jammy (22.04) PPA build by injecting `libqt6opengl6-dev` via per-series workflow patch. The Debian alternatives approach didn't work because apt short-circuits when `qt6-base-dev` is already in the dep list.

## [2.2.9] - 2026-03-31

### Fixed
- **PPA (FR-90):** Fixed Jammy (22.04) PPA build failure by adding missing `libqt6opengl6-dev` build dependency. Qt6OpenGL dev headers are a separate package on Jammy but bundled in `qt6-base-dev` on Noble/Questing.

## [2.2.8] - 2026-03-30

### Added
- **Website "Release Notes" modal (FR-91):** Replaced the "View on GitHub" button on the website landing page with a "Release Notes" button. Clicking it opens a modal that displays all new features and bug fixes from the latest release, parsed from CHANGELOG.md at build time. Uses native `<dialog>` element with dark theme styling, click-outside-to-close, and fallback message if changelog is unavailable.
- **PPA repository (FR-90):** Ubuntu users can now install via `sudo add-apt-repository ppa:s4solutionsllc/nexis && sudo apt install nexis` with automatic updates. Supports Ubuntu 22.04 (Jammy), 24.04 (Noble), and 25.04 (Plucky) on x86_64 and ARM64.

## [2.2.3] - 2026-03-26

### Added
- **Ask Claude.ai button (FR-90):** Replaced "Search Online" button in APT Repository Manager with an "Ask Claude.ai" button that opens a pre-filled Claude.ai query for repository troubleshooting.

### Fixed
- **BUG-104:** APT health checker now handles flat repo URL formats and no longer reports false "unreachable" status for path-based repositories.
- **BUG-105:** Skipped suite mismatch check on third-party repos that use non-standard suite naming.
- **BUG-106:** DEB822 multi-suite stanzas are now expanded into separate entries for correct per-suite health checking.

## [2.2.2] - 2026-03-25

### Fixed
- **BUG-103:** Fixed schedule indicator rendering as a white square on initial System Cleaner page load.
- Reduced System Cleaner category icons from 64px to 48px and scaled labels/checkboxes to match.
- Moved Select All toggle directly below the Scan button for better discoverability.
- Added missing QSS styles for Docker tree widgets.
- Fixed QSS audit issues: missing styles, orphaned selectors, and hardcoded colors.

### Changed
- **FR-88:** Migrated inline styles to central QSS in four phases — helper widgets, static inline styles, semi-dynamic property selectors, and cleanup of empty methods and unused includes.

## [2.2.1] - 2026-03-25

### Added
- **APT Repository Health Dashboard (FR-87):** Added a health dashboard to the APT Repository Manager page with status indicators, detail panel, repo descriptions from a built-in knowledge base, and macOS Homebrew health checking. Includes repair actions: disable/enable/remove sources, duplicate removal, legacy-to-deb822 conversion, and connection diagnosis with inline results.

### Fixed
- Fixed detail panel gray background and text padding in repo health dashboard.
- Normalized health checker cache keys, added dynamic_cast safety, and implemented macOS tap/pinned checks.

## [2.1.19] - 2026-03-18

### Fixed
- **BUG-101:** Injected tag version into binary at build time to fix version display in release builds.

## [2.1.18] - 2026-03-18

### Added
- **FR-86:** Added Unlock All and Make Permanent buttons for SMART disk health elevation.

### Fixed
- **BUG-98:** Added smartctl Unlock button to Hardware Info and fixed false health verdicts for locked drives.
- **BUG-99:** Detected system icon theme name for AppImage tray icon rendering.
- **BUG-100:** Batched SMART elevation prompts into a single dialog and fixed unlock crash from concurrent pkexec calls.
- **BUG-101:** Replaced unstyled QToolButtons with QPushButton on Hardware Info page.
- **BUG-102:** Added color token to Download button for correct light mode rendering.

## [2.1.17] - 2026-03-18

### Fixed
- **BUG-97:** Added system icon theme paths for AppImage environments so tray icon renders correctly.

## [2.1.16] - 2026-03-17

### Added
- **Maintenance wizard (FR-83):** Added a guided system checkup wizard accessible from the Dashboard.
- **System theme tray icon (FR-86):** Added a "System Theme" option for the tray icon that uses the desktop environment's native icon.

### Fixed
- Used `utilities-system-monitor` icon name for system theme tray icon compatibility.

## [2.1.15] - 2026-03-11

### Added
- **GPU diagnostics (FR-84, FR-85):** Detected simple-framebuffer GPUs and added a diagnostic report for systems without dedicated GPU drivers.

## [2.1.14] - 2026-03-10

### Added
- **Cleaner exclusion rules (FR-18):** Added configurable exclusion rules for the System Cleaner with a floating gear button overlay.
- **Firewall status widget (FR-68):** Added a firewall status card to the Helpers page showing UFW/PF state.
- **Open ports viewer (FR-66):** Added an open ports and active connections viewer to the Helpers page.
- **Network diagnostics panel (FR-82):** Added a network diagnostics panel to the Helpers page.
- **Snap/Flatpak cleanup (FR-79, FR-80):** Added Snap and Flatpak cache cleanup categories and orphan package detection to System Cleaner and Uninstaller.
- **CPU governor switcher (FR-70):** Added a CPU governor and power profile switcher to the Power page.

## [2.1.13] - 2026-03-07

### Added
- **Theme-aware splashscreen (FR-78):** Splashscreen now adapts to the system color scheme.
- **Expanded unit tests (FR-76):** Added unit test coverage for critical-risk code paths.

### Fixed
- **BUG-76:** Used DRM card order and improved multi-GPU display sorting.
- **BUG-82:** Removed 3 unused SignalMapper signals and associated dead code.
- **BUG-83:** Connected remaining hardcoded hex colors to the theme token system.
- **BUG-86:** Included Core sources in `qt_create_translation()` for complete i18n coverage.
- **BUG-90:** Standardized error handling and added logging to previously silent failures.
- **BUG-91:** Removed dead ioreg code block in macOS system_info.cpp.
- **BUG-92:** Parented QSystemTrayIcon to qApp for proper cleanup on exit.
- **BUG-93:** Checked `isSymLink()` before `isDir()` in cleaner to prevent symlink traversal data loss.
- **BUG-94:** Emptied Linux trash directories instead of deleting them (preserving Trash structure).
- **BUG-95:** Stripped inline `#` comments when parsing host file entries.
- Removed 9 unused legacy resource files from QRC.
- Removed orphaned upstream resource files.

### Changed
- **FR-77:** Deduplicated dashboard tile subclasses via MetricTileBase helper methods.

## [2.1.12] - 2026-03-05

### Fixed
- **BUG-80:** Fixed update check regex to support multi-digit version numbers.

## [2.1.11] - 2026-03-05

### Fixed
- **BUG-76:** Sorted GPUs by PCI bus address instead of DRM card number for consistent ordering.
- **BUG-79:** Added QSS theme rules for Disk Tools display widgets.

## [2.1.10] - 2026-03-03

### Added
- **Hide footer option (FR-75):** Added a setting to hide the Dashboard footer info bar.

### Fixed
- **BUG-75:** Added Ubuntu 25.04+ .deb build to fix t64 dependency issue.
- **BUG-77:** Compensated for QSS padding in Hardware Info table column widths.
- **BUG-78:** Repositioned sidebar badges after animation completes instead of before.

## [2.1.9] - 2026-02-27

### Added
- **Disk Tools page (FR-62, FR-63):** Added a new Disk Tools page with large file finder and duplicate file finder using a 3-stage hash pipeline with cancellation support.
- **System Log Viewer (FR-71):** Added a System Logs page with filtering and platform-specific log provider backends (journalctl on Linux, log show on macOS).
- **Health Score tile (FR-73):** Added a composite health score tile to the Dashboard combining CPU, memory, disk, and temperature metrics with weighted scoring.
- **Collapsible sidebar groups (FR-74):** Added collapsible navigation groups (MONITOR/MANAGE/SYSTEM) to the sidebar.

### Fixed
- **BUG-72:** Eliminated subprocess blocking and reduced allocation churn; gated page updates on visibility to reduce idle CPU and memory usage.
- **BUG-73:** Corrected sidebar badge positioning with `mapTo` coordinate conversion.
- **BUG-74:** Resolved ScreenshotTests SEGFAULT and broken screenshot references.
- Fixed health score color mapping thresholds and disk score weighting by partition size.
- Fixed sidebar to show icon-only buttons in collapsed mode and hide dividers.

## [2.1.8] - 2026-02-25

### Added
- **Available Updates section (FR-60):** Moved software updates from Dashboard tile to a dedicated tree view section on the APT/Homebrew page with sidebar badge indicator.
- **Browser Privacy cleanup (FR-64):** Added a Browser Privacy category to System Cleaner for clearing browser caches and data.
- **macOS maintenance actions (FR-69):** Added macOS-specific maintenance actions to the Helpers page (rebuild Spotlight, flush DNS, repair permissions).

### Fixed
- **BUG-71:** Cleared sparkline history when switching GPU, temperature, or fan sensors.
- Matched updates section styling (title padding, tree header height, margins) with the package tree.

## [2.1.7] - 2026-02-25

### Added
- **Fan speed monitoring (FR-56):** Added fan speed monitoring with FANS dashboard tile, sensor selection, Hardware Info fans section, and platform-specific backends (hwmon sysfs on Linux, SMC on macOS).
- **Memory pressure visualization (FR-57):** Added memory pressure breakdown visualization to the Dashboard.
- **Per-process I/O columns (FR-58, FR-59):** Added Disk Read/s, Disk Write/s, Net Down/s, and Net Up/s columns to the Processes page with platform-specific collection.
- **Broken Symlinks cleaner (FR-61):** Added a Broken Symlinks category to System Cleaner.
- **DNS cache flush (FR-65):** Added a one-click DNS cache flush button to the Helpers page.
- **Export System Report (FR-72):** Added an Export System Report button to the Hardware Info page.

### Fixed
- **BUG-70:** Added dedicated `fanUpdated` signal and fallback detection paths for fan sensors.
- Hidden GNOME Settings page on macOS; renamed to "System Preferences" on macOS.

## [2.1.6] - 2026-02-24

### Fixed
- **BUG-69:** Added sensor and device subtitles to GPU and Temp dashboard tiles; fixed macOS GPU iteration order.
- Fixed CI retry logic for transient macOS brew install GHCR failures.

## [2.1.5] - 2026-02-24

### Added
- **Dashboard edit mode (FR-50, FR-51):** Added drag-and-drop tile repositioning, resize handles, edit mode toggle with toolbar, and kiosk mode mutual exclusion. Dashboard layout persisted to settings. Reset Layout button in Settings page.
- **Selectable widget styles (FR-53):** Added per-tile widget style selector (sparkline, speedometer, gauge, VU meter, hybrid) with theme-compliant rendering.
- **Remove widgets (FR-54):** Added ability to remove dashboard tiles in edit mode with empty tile slot support and occupancy grid.
- **Per-widget colors (FR-55):** Added per-widget color customization with color range presets for speedometer and VU meter styles.
- **Minimize to tray (FR-52):** Added a minimize-to-tray option in Settings; macOS hides dock icon when minimized.
- **Tray icon style selector (FR-48):** Added 3 monochrome tray icon SVG variants with a style combo box and live preview in Settings.

### Fixed
- **BUG-44:** Redesigned Settings page with grouped QGroupBox sections.
- **BUG-62:** Dashboard supports empty tile slots with proper resize and drag-drop handling for multi-cell tiles.
- **BUG-63:** Dashboard floating buttons re-raised after z-order disruptions.
- **BUG-64:** Synced QPalette with theme tokens and fixed Settings page QSS cascade; forced Fusion style for QComboBox popup theming on macOS.
- **BUG-65:** Converted schedule indicator to floating overlay on System Cleaner page.
- **BUG-66, BUG-67:** Improved text centering and font sizing in speedometer, gauge, and hybrid tiles.
- **BUG-68:** Reserved space for speedometer tick labels to prevent clipping.

## [2.0.2] - 2026-02-22

_This release was tagged after v2.1.3 in the repository history; it consolidates the full UI redesign, architecture overhaul, and all fixes from the v1.x and early v2.1.x line into a single rebased release. See [2.0.0] and [2.0.1] for the detailed breakdown of those changes._

### Added
- **Tray icon style selector (FR-48):** Added 3 monochrome tray icon SVG variants (light, dark, colored) with a style combo box and live preview in Settings.

## [2.1.3] - 2026-02-13

### Added
- **Dev Tool Caches category (FR-03):** Added Electron app caches and dev tool paths to System Cleaner, with sort dropdown label fixes (**BUG-19**) and consistent back button icon (**BUG-20**).
- **Disk Usage Analyzer launcher (FR-23):** Replaced the File System pie chart with an external disk analyzer tool launcher on the Resources page.
- **Configurable disk analyzer (FR-24):** Added a Settings combobox to choose the preferred disk usage analysis tool.

### Fixed
- **BUG-05:** Background threads now wait for completion on exit via `waitForDone()`.
- **BUG-21:** Applied theme styling to the Homebrew tree view.

## [2.1.2] - 2026-02-11

### Fixed
- **BUG-02:** System Cleaner now empties directory contents instead of deleting entire directories.
- **BUG-18:** Settings version label uses dynamic version from CMake instead of a hardcoded string.

## [2.1.1] - 2026-02-11

### Fixed
- **BUG-17:** Replaced defunct feedback form with GitHub Issues launcher.
- Updated splashscreen reference to version-independent filename.

## [2.1.0] - 2026-02-10

### Added
- **macOS platform support:** Added macOS support via `#ifdef` abstraction layer with unified cross-platform CMake build. Includes macOS thermal sensors, CPU speed, dashboard layout, theme system, app icon, and `.app` bundle packaging.
- **GPU monitoring (FR-11):** Added Dashboard CircleBar and Resources history chart for GPU utilization with multi-GPU selector.
- **macOS .app bundle uninstaller (FR-21):** Scans `/Applications`, parses `Info.plist`, moves to Trash.
- **Homebrew page rework (FR-22):** Grouped Formula/Cask sections with multi-select tree widget.

### Fixed
- **BUG-12:** Added icon fallback for System Cleaner on macOS.
- **BUG-13:** Fixed macOS sidebar icons, Homebrew integration, System Cleaner category icons (Adwaita SVGs), and NVIDIA GPU utilization reading.
- **BUG-14:** Fixed NVIDIA GPU utilization using PCI bus ID instead of DRM card index.
- **BUG-15, BUG-16:** Fixed Uninstaller Homebrew paths and added package descriptions via JSON API on macOS.
- Fixed macOS release re-signing of app bundle after `macdeployqt`.
- Derived app version from `git describe` at build time.

## [2.0.1] - 2026-02-22

### Fixed
- **Release workflow:** Delete pre-existing GitHub releases for inherited upstream tags before creating new Nexis releases. Mark new releases as latest.

## [2.0.0] - 2026-02-22

### Added — Complete UI Redesign
- **Bento dashboard with collapsible sidebar (FR-42):** Complete visual overhaul replacing CircleBar/LineBar gauges with MetricTile widgets featuring sparkline charts, progress bars, and trend indicators. Collapsible sidebar with grouped sections (MONITOR/MANAGE/SYSTEM), smooth 250ms animation, icon-rail at 64px, Ctrl+B shortcut. Gradient sidebar logo (wordmark expanded, lettermark collapsed). Command Palette (Ctrl+K) for fuzzy navigation and actions. System summary card on dashboard. Footer status bar with version and refresh rate.
- **HeroCard widget (FR-43):** Combined CPU + Memory tile with vertical divider. Each half is a MetricTile in Hero display mode with sparkline history.
- **DiskTile with donut chart (FR-43, FR-44):** Custom-painted donut chart replacing sparkline MetricTile for disk usage. Drive health badge with verdict and numeric percentage (e.g., "Apple SSD: Good (92%)").
- **NetworkTile (FR-44):** Two-row layout with Download and Upload labels each paired with a separate sparkline chart, horizontal divider, and active interface name.
- **Pixel-perfect mockup alignment (FR-44):** 29 individual fixes aligning the implementation to approved SVG mockups, including MetricTile display modes, sidebar polish, and grid reorganization.
- **Disk tile gear icon selector (FR-45):** Gear icon in top-right corner of DiskTile (visible when 2+ disks detected) opens dropdown to switch displayed disk; selection persisted.
- **Temperature tile gear icon selector (FR-46):** Replaced QComboBox with gear icon + QMenu dropdown matching DiskTile pattern. Gear button added to shared MetricTile widget (opt-in, hidden by default).
- **Kiosk mode UI controls (FR-30):** Three new entry/exit methods for kiosk mode: checkable tray menu action, floating fullscreen/collapse toggle button on Dashboard, and transient "Press ESC to exit" overlay with fade animation.

### Added — Architecture Overhaul
- **Service layer abstraction (FR-42):** 7 new domain services (StartupService, FileSearchService, HostService, ProcessService, SystemServiceManager, DockerService, PackageService) separating business logic from UI. NexisPage base class with `onPageActivated()`/`onPageDeactivated()` lifecycle hooks. All `QtConcurrent::run` and `CommandUtil::exec` calls removed from pages.
- **Abstract base classes (FR-34):** Pure virtual interfaces for all 10 Info classes and 4 Tool classes. Platform implementations become named subclasses (e.g., `CpuInfoLinux`, `CpuInfoMacOS`). Managers use `std::unique_ptr<Interface>` with compile-time factory construction.
- **Dependency injection (FR-35):** Optional `nullptr`-default manager pointer parameters on all 10 page constructors. Constructor initializers use ternary fallback. All `::ins()` calls in page bodies replaced with member variable access.
- **Centralized DataRefreshService (FR-37):** Replaced ~25 per-page QTimers with a single service owning 4 QTimers (1s/5s/30s/configurable). 10 typed data-change signals. Pages subscribe as reactive consumers. Pause/resume on app minimize (kiosk mode overrides pause).
- **QSS token validation (FR-32):** Runtime validation that all `@tokens` in `style.qss` have corresponding values in `values.ini`, and that color values are valid hex format.
- **Explicit CMake source lists (FR-31):** Confirmed all CMakeLists.txt files use explicit `set()` source lists — no `file(GLOB_RECURSE ...)` calls.

### Added — Testing & CI
- **Qt Test infrastructure (FR-33):** Test directory, CMake test target, CTest integration, CI test step.
- **Unit test suite (FR-36):** 63 tests across 6 executables covering FormatUtil, FileUtil, CommandUtil, DiskHealthInfo, ScheduleManager, and theme token validation.
- **CI screenshot regression tests (FR-41):** Automated screenshot capture of each page across themes with perceptual diff comparison.

### Added — Visual Consistency
- **Bundled fonts and font picker (FR-38):** Bundled Inter, JetBrains Mono, and Ubuntu fonts. `@fontFamily` QSS token with user-configurable font picker on Settings page.
- **SVG-only icons (FR-25):** Removed all `QIcon::fromTheme()` calls (except one legitimate dynamic app-icon lookup). Created 4 disk tool SVGs. Sidebar icons always use bundled SVGs.
- **Removed hardcoded Ubuntu font (FR-26):** Stripped all `<family>Ubuntu</family>` blocks from `.ui` files. Font family controlled by QSS token and font picker.

### Changed
- **Disk Health tile removed (FR-47):** Standalone Disk Health MetricTile removed from Dashboard grid. Health information now shown as badges on the DiskTile with numeric health percentage.
- **Quick Actions bar removed:** Replaced with expanded full-width system summary card.
- **Refined theme colors:** Dark theme uses deep charcoal base (`#1A1C22`) with warm orange accent (`#FF6B1A`). Light theme uses warm cream base (`#F5F0EB`). 24 additional theme tokens added for full coverage.

### Fixed
- **BUG-42:** GNOME Settings "Mouse & Touchpad" tab button renders correctly (escaped ampersand).
- **BUG-43:** Host Manager security and data integrity — 7 fixes including `sudo tee` instead of predictable temp files, error handling, input validation (IPv4/IPv6, RFC 1123 hostnames), backup before save, confirmation dialog with change summary.
- **BUG-45:** Kiosk mode toggle button icons changed from gray to Nexis orange.
- **BUG-46:** Kiosk mode "Press ESC" overlay centered on screen geometry instead of stacked widget.
- **BUG-47:** Theme switching now fully applies to all widgets — 24 new theme tokens, `refreshThemeColors()` methods on all tile widgets, zero hardcoded colors in C++.
- **BUG-48:** Qt resources now load correctly (`Q_INIT_RESOURCE(static)` added to main.cpp).
- **BUG-49:** QSS token replacement sorted by descending length to prevent substring collisions.
- **BUG-50:** Command palette "Toggle Theme" uses correct `getColorScheme()`/`setColorScheme()` methods.
- **BUG-51:** Disk tile percentage text uses theme color instead of system palette.
- **BUG-52:** Sidebar toggle changed from QPushButton to QToolButton for correct icon rendering on macOS.
- **BUG-53:** Duplicate drive health entries on macOS fixed — disk image filter and model-based deduplication.
- **BUG-54:** GNOME Settings Appearance tab now uses QGroupBox containers matching other tabs.
- **BUG-55:** Dashboard card borders and shadows improved — increased border contrast and shadow depth.
- **BUG-56:** Sidebar items correctly left-aligned after expanding from collapsed state (child widget re-polish).
- **BUG-57:** Disk selector filters virtual filesystems, snapshots, loopbacks, and hidden macOS system volumes.
- **BUG-58:** Search page button icon path corrected from `.png` to `.svg`.
- **BUG-59:** Removed upstream Stacer "BETA version" label from Search page.
- **BUG-60:** Dashboard system summary shows correct RAM (lazy-updated on first memory callback).
- **BUG-61:** Disk tile health badge shows only the selected disk's health with volume-to-physical-drive matching.
- Fixed double-free crash in `DiskTile::clearDriveHealth()` (QLayout inherits QLayoutItem — `takeAt()` returns the layout itself).
- Settings page: Disk Health Alert repositioned next to Disk Analyzer (from isolated column).

## [1.2.0] - 2026-02-18

### Added
- **deb822 APT source file support (FR-01):** Dual-format parsing for modern `.sources` (deb822) and legacy `.list` files. Stanza-aware editing preserves field order, comments, and multi-line `Signed-By` values. Format-preserving writes. Renamed `distribution` → `suites` to match APT terminology. Edit dialog uses structured objects instead of format-specific strings. UX improvements: selection cleared after operations, search filter preserved, button feedback during add.
- **Docker management page (FR-20):** New sidebar page for managing Docker images, containers, and volumes. Three-tab interface with grouped tree views, batch remove/prune operations with confirmation dialogs, container start/stop, search filtering, lazy tab loading, and daemon status detection. Conditionally shown when Docker is installed.
- **APT-RPM support (FR-09):** Added support for APT-RPM package management (ALT Linux, PCLinuxOS, Vine Linux). Dynamic `binaryType()`/`sourceType()` helpers replace hardcoded `"deb"`/`"deb-src"` strings. `apt-repo` support for repository add/remove with fallback. RPM-format placeholder text in the APT Source Manager page.
- **Scheduled/automated cleaning (FR-16):** Settings page section with quick-setup toggle for weekly cleaning, custom schedule management (create/edit/delete via dialog), cleaning history viewer, threshold-based junk notifications, and post-clean tray notifications. Full schedule persistence via JSON with ScheduleManager and CleanerService backend.
- **Battery & disk health monitoring (FR-29):** Three-phase implementation — battery health CircleBar on Dashboard with capacity degradation alerts, SMART-based disk health monitoring via `smartctl`/IOKit with temperature and wear tracking, and Resources page integration with disk temperature history chart.
- **Startup Apps improvements (FR-10):** Search/filter bar, inline editor fields, and application icons in the startup app list.
- **ARM64 Linux CI (FR-06):** GitHub Actions workflow for ARM64 Linux builds and release artifacts.

### Fixed
- **BUG-06:** Large `/etc/hosts` files no longer freeze the UI at startup (deferred loading, batched model population, pre-compiled regex, incremental updates).
- **BUG-07:** HiDPI/4K scaling via `Dpi::scale()` utility, `@dpN` QSS tokens, and relaxed `.ui` size constraints.
- **BUG-08:** Wayland compatibility — guarded `primaryScreen()` null dereference, use `requestActivate()` for `xdg-activation` protocol.
- **BUG-10:** System Cleaner memory leak fixed — concurrent worker guards, proper destructor cleanup, scan result list clearing, eliminated redundant directory traversals.
- **BUG-39:** `getDesktopValue()` no longer truncates Exec lines containing `=` (env variables).
- **BUG-40:** FR-16 UI regressions — repositioned Scheduled Cleaning section above footer, replaced all hardcoded colors with QSS theme tokens, added dialog-title labels, primary/danger button styling, drop shadows, and focus ring fix.
- **BUG-41:** Manage Schedules dialog scroll area viewport now renders correctly in dark mode via transparent stylesheet pattern.
- Added generic `QDialog` and `QDialog QLabel` QSS rules for consistent dark mode dialog theming.
- Added `QCheckBox:focus` QSS rule to suppress platform-default purple focus ring.

## [1.1.2] - 2025-02-16

### Added
- **Hardware Info page (FR-12):** New sidebar tab between Dashboard and Startup Apps displaying System, Processor, Graphics, and Memory details. Platform-specific cache size retrieval (sysctl on macOS, sysfs on Linux).
- **Dashboard kiosk mode (FR-28):** F11 toggles fullscreen dashboard-only mode (hides sidebar and page title). ESC exits. State persisted across sessions. Temperature sensor and GPU device selections remembered.
- **Crowdin translations:** New translations via GitHub Action integration.

### Fixed
- **BUG-33:** Uninstaller "Purge" checkbox text now visible in dark mode (added `QCheckBox { color }` rule).
- **BUG-34:** Settings page author link changed from hardcoded blue to Nexis accent orange (`#E95420`).
- **BUG-35:** Removed non-functional "Donate" button from Settings page.
- **BUG-36:** System Cleaner "Total Size" label now visible in dark mode.
- **BUG-37:** System Cleaner `scanLoading.gif` animation now plays reliably; fixed QMovie memory leak on theme change.
- **BUG-38:** Hardware Info table rows now legible in dark mode; removed broken alternating row colours, added opaque item backgrounds.
- Hardware Info QGroupBox containers now size correctly (moved padding from QSS to layout margins).
- Hardware Info tables no longer show unnecessary scrollbars.

### Changed
- Dashboard system info panel removed (replaced by Hardware Info page).
- Dashboard temperature/GPU/network modules now fill full width in normal and kiosk modes.
- Hardware Info page simplified to 4 sections (System, Processor, Graphics, Memory); Storage, Network, and Thermal sections removed due to sizing limitations.

## [1.1.0] - 2025-01-16

### Added
- **GPU monitoring (FR-11):** Dashboard CircleBar and Resources history chart for GPU utilization. Multi-GPU selector. Linux: AMD sysfs, NVIDIA nvidia-smi, Intel frequency ratio. macOS: IOKit IOAccelerator.
- **Hardware info research (FR-12):** Initial research and planning for hardware info tab.
- **Disk Usage Analyzer launcher (FR-23):** Resources page launcher for Baobab, Filelight, GrandPerspective, etc.
- **Configurable disk analyzer (FR-24):** Settings combobox to choose preferred disk usage tool.
- **GNOME Settings dropdowns (FR-27):** Replaced theme/font text fields with populated dropdowns.
- **Homebrew page tree widget (FR-22):** Grouped Formula/Cask sections with multi-select.
- **macOS .app bundle uninstaller (FR-21):** Scan /Applications, parse Info.plist, move to Trash.
- **Purge option in uninstaller (FR-19):** APT `purge` mode via checkbox.
- **Autostart delay option (FR-15):** Configurable delay for startup apps.
- **Expanded cache cleaning (FR-03):** Electron app caches and dev tool paths.
- **Single-instance enforcement (FR-02):** QLockFile prevents duplicate launches.
- **SVG logo and tray icon (FR-07):** Redesigned in SVG for all themes.
- **Crowdin translation integration (FR-08):** Automated PR workflows for 34 languages.
- **Pip cache cleaning (FR-17):** PIP_CACHE_DIR env var support.

### Fixed
- **BUG-01:** Swapped memory variables in `/proc/meminfo` parsing.
- **BUG-02:** System Cleaner no longer deletes entire cache directories (empties contents instead).
- **BUG-03:** Single-instance enforcement via QLockFile.
- **BUG-04:** CPU speed now reads from sysfs cpufreq as fallback.
- **BUG-05:** Background threads cleaned up on exit via `waitForDone()`.
- **BUG-09:** `LC_ALL=C` for system command parsing on non-English locales.
- **BUG-11:** macOS crash on launch from double CFRelease in GPU detection.
- **BUG-12:** Missing icon fallback for System Cleaner on macOS.
- **BUG-13:** Sidebar icons now use Adwaita theme on macOS via Homebrew icon paths.
- **BUG-14:** NVIDIA GPU utilization uses PCI bus ID instead of DRM card index.
- **BUG-15:** Uninstaller finds Homebrew via absolute paths on macOS.
- **BUG-16:** Uninstaller shows descriptions for Homebrew packages via JSON API.
- **BUG-17:** Feedback form replaced with GitHub Issues launcher (removed defunct Heroku endpoint).
- **BUG-18:** Settings version label now dynamic from CMake `PROJECT_VERSION`.
- **BUG-19:** System Cleaner sort dropdown labels now include direction (A-Z, Z-A, etc.).
- **BUG-20:** System Cleaner back button uses consistent SVG icon.
- **BUG-21:** Homebrew tree view respects dark theme via object name.
- **BUG-22:** Uninstaller/Homebrew tree views show expand/collapse chevrons.
- **BUG-23:** Uninstaller/Homebrew tree views use table-style layout matching System Cleaner.
- **BUG-24:** YUM/DNF cache paths fixed (was returning Pacman paths).
- **BUG-25:** CircleBar potential double-delete of QChart fixed.
- **BUG-26:** DiskInfo changed to value semantics (Rule of Three violation fixed).
- **BUG-27:** Linux `/proc/meminfo` bounds checking added.
- **BUG-28:** CPU core count changed from `quint8` to `int` (overflow at 256 threads).
- **BUG-29:** `toLong()` changed to `toLongLong()` for 64-bit safety.
- **BUG-30:** Reverted Phase 2 margin changes to restore original layouts.
- **BUG-31:** GNOME Settings now shows inline errors and reverts widgets on `gsettings set` failure.
- **BUG-32:** GNOME Settings speed sliders debounced with 200ms timer.
- Linux `.desktop` file: added `StartupWMClass` for GNOME X11 icon matching.
- Linux: hicolor icons installed, desktop filename set, AppImage icon path fixed.
- Build: CMake `PROJECT_VERSION` used instead of `git describe` for app version.
