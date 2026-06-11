# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed
- **Crash when closing the System Checkup dialog mid-scan (SSO-3362):** Closing the Maintenance Wizard while its four parallel `QtConcurrent::run` checks were still running could trigger a use-after-free — the dialog uses `WA_DeleteOnClose`, so pressing Esc or **Close** before the workers finished freed the dialog while detached lambdas were still about to call `QMetaObject::invokeMethod(this, …)` against deleted memory. Each worker now captures a `QPointer<MaintenanceWizardDialog>` instead of a raw `this`, dispatches results through the context-object + functor overload of `invokeMethod` (so queued slots are dropped when the dialog dies), and the dialog stores every `QFuture` as a member and `waitForFinished()`s them in a new destructor as a backstop against any worker that already passed the guard check.
- **Data race on disk-health cache (SSO-3364 / audit H3):** `DataRefreshService::onSlowTick()` ran `DiskHealthInfo::discoverDrives()` on a `QtConcurrent` worker every 30 s; the worker did `mDrives.clear()` + per-drive `smartctl` append for several seconds, while the UI thread concurrently copied `mDrives` via `getDrives()` (Hardware Info, Resources, Dashboard, Settings) and wrote `mDrives[i]` from `refreshHealthElevated()` on user "Unlock Drive" clicks. Concurrent `QList<DriveHealth>` clear/copy/index-write is undefined behaviour. Adopted the publish pattern (mirrors `DiskInfo::collectDiskInfo()`/`setDisks()`): the worker now builds a fresh `QList<DriveHealth>` into a local via the new `collectDriveHealth()` virtual and returns it; the UI thread publishes via the new `InfoManager::setDriveHealth()` from inside the `QMetaObject::invokeMethod` hop. As a defence-in-depth a `QMutex` also guards every `mDrives` read/write (`getDrives`, `hasDrives`, `setDrives`, both `refreshHealthElevated*` paths). pkexec/sudo invocations run outside the lock so a slow elevation prompt does not block UI snapshots. Applied symmetrically on Linux and macOS.
- **Quitting Nexis right after opening System Logs could crash (SSO-3363 / audit H2):** `LogProvider::cancel()` did `mProcess->kill(); mProcess->waitForFinished(3000); mProcess->deleteLater();`, but `waitForFinished()` processes events on the calling thread, so `kill()`'s CrashExit delivered `finished()` synchronously. The connected error path nulled `mProcess` and called `deleteLater()` on it, and the next line in `cancel()` then dereferenced null. The macOS path was the easiest to hit because `log show --last 1h` runs for several seconds, so closing the window while the fetch was still in flight reproduced the segfault reliably. `cancel()` now disconnects the slot, copies the pointer to a local, and nulls the member before touching the QProcess, so the QProcess is destroyed exactly once and `cancel()` is safe whether the process is running, finished, or already cleaned up. Covered by `tests/managers/test_log_provider.cpp`.
- **`MAINTAINER_SOP.md` §2 contradicted `.github/FUNDING.yml`:** The SOP's "no monetization, ever" rule was being read as a blanket ban that also covered the repo-level GitHub Sponsors button enabled in `.github/FUNDING.yml`, even though that file had been live since the start of the project as a CEO-approved external donation surface. §2 rule 2 now states the scope explicitly: in-app monetization (paid tiers, donation walls in the app, sponsor-only features, telemetry-for-revenue) remains prohibited *Ever*, while passive external donation links at the repo level (`FUNDING.yml`, GitHub Sponsors, Open Collective, etc.) are permitted, with the active `.github/FUNDING.yml` itself serving as the CEO-approved record for this exception. No code or runtime behavior changes — governance documentation only.
### Changed
- **License SPDX identifier reconciled to `GPL-3.0-only` across packaging and docs (SSO-3373):** The repository previously claimed `GPL-3.0-or-later` in `linux/aur/PKGBUILD`, `linux/aur/.SRCINFO`, `linux/metainfo/io.github.s4solutionsllc.Nexis.metainfo.xml`, `docs/MAINTAINER_SOP.md`, `RELEASE.md`, and `CLAUDE.md`, while the `LICENSE` preamble used GPL-3.0-only wording ("version 3 of the License", no "or any later version") and `debian/copyright` used the ambiguous short form `GPL-3.0`. Upstream Stacer ships the verbatim FSF GPL-3.0 text with no author-elected "or later" application notice, so a fork cannot enlarge that grant. All five packaging/doc locations and `debian/copyright` are now `GPL-3.0-only`; the `LICENSE` preamble was already correct and is unchanged.
- **Docs:** Reconciled `docs/ARCHITECTURE_REVIEW.md` and `docs/APPLICATION_OVERVIEW.md` against the live tree (SSO-3392 / WI-30). Consolidated by-the-numbers counts (LOC, page/service/signal/timer/test totals) into a single canonical table in `APPLICATION_OVERVIEW.md` and removed the duplicated hard counts that had drifted (notably the "~6,000–7,000 LOC" Scale line, the stale `sigDashboardLayoutReset` mention and "9 signals" / "13 info classes" / "4 timers / 10 signals" claims, the wrong `~/Library/Preferences/com.nexis.plist` / `~/.config/nexis/nexis.conf` settings paths, and the "CI runs screenshot tests" claim already invalidated by WI-19). Refreshed both `Last updated` / `Version` headers to match `PROJECT_VERSION`.
- **CI:** New `scripts/check_doc_versions.sh` runs on every build matrix leg and fails the workflow if the doc version header drifts from `CMakeLists.txt`'s `project(... VERSION ...)`.

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

### Fixed
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
