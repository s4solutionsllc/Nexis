# Nexis — Feature Implementation Plan (Paperclip Epic)

> **Date:** 2026-06-10 · **Source of features:** [2026-06-10-feature-recommendations.md](2026-06-10-feature-recommendations.md) · **Codebase at:** v2.3.13
> **Purpose:** This is the feature-level (epic) issue body for Paperclip. Each **Feature Work Item (FW-NN)** below is a self-contained sub-issue spec written so a **Sonnet 4.6 engineer agent** can design, implement, test, and PR it. Work them in the numbered order — the phases follow the priority tiers from the recommendations doc (defensive correctness first, then offensive moat-wideners, then depth, then larger bets).
> **Companion:** audit findings are in [2026-06-10-audit.md](2026-06-10-audit.md); the audit remediation epic is in [2026-06-10-audit-remediation-plan.md](2026-06-10-audit-remediation-plan.md). **Several audit fixes are prerequisites** for features here (flagged inline) — notably WI-06 (headless `--clean`), WI-08 (cleaner test harness), WI-28 (RepositoryTool refactor).

---

## How to work a feature sub-issue (READ FIRST — applies to every FW)

You are picking up exactly one Feature Work Item. Features are creative work, so the discipline differs slightly from bug fixes:

1. **Brainstorm before building** (for any FW with UI or design choices — most of them). Invoke the `brainstorming` skill to nail down scope, UX, and edge cases before writing code. Capture the outcome in `backlog/<NEX-id>_research.md` and a `backlog/<NEX-id>_plan.md` with checkboxes (the project's `/bug-feature-workflow` Phase 1/2). For purely defensive/mechanical FWs (e.g. FW-03 `/run/media`), a short plan is enough.
2. **Branch.** `git checkout native && git pull`, then `git checkout -b claude/<NEX-id>-<short-slug>`. Never commit to `native`.
3. **Reuse before you build.** Each FW lists the **existing APIs and patterns** to build on (mapped from the codebase). Do not reimplement what exists — e.g. the cleaner already has an exclusion engine, trash path, and trend storage; `AptSourceTool` already has a deb822 parser. Verify the current state first; extend, don't duplicate.
4. **Follow the page-addition pattern** for any new sidebar page — see [Appendix A](#appendix-a--adding-a-new-page-shared-reference). For a sub-page/widget inside an existing page, follow that page's existing structure.
5. **Implement.** Match surrounding code style. Theme everything via tokens (no hardcoded hex — BUG-47); wrap user-facing strings in `tr()`; do blocking work off the UI thread (QtConcurrent + `invokeMethod` marshalling, or subscribe to `DataRefreshService`).
6. **Test.** Add tests per the FW's **Tests** section, using the project convention (`tests/<category>/test_<name>.cpp`, `QTEST_MAIN`, `#include "...moc"`, register in `tests/CMakeLists.txt`). Parsers and command-construction logic must have unit tests; for destructive features, test against `QTemporaryDir` with an injected elevation seam (see WI-08 pattern).
7. **Build & verify** (`/build-verify`):
   ```bash
   cmake --build build -j$(sysctl -n hw.ncpu)   # macOS
   cmake --build build -j$(nproc)               # Linux
   ctest --test-dir build --output-on-failure
   ```
8. **Cross-platform check** (`/platform-check`) for anything touching `shared/`/`linux/`/`macos/` or `#ifdef`. **Qt/UI check** (`/qt-ui-change`) for any widget/QSS/theme change — including regenerating screenshot baselines for new pages once WI-19/WI-20 land.
9. **Docs.** Update `CHANGELOG.md` (under `## [Unreleased]`, `### Added`), `docs/APPLICATION_OVERVIEW.md` (new feature/UI/page), and `docs/ARCHITECTURE_REVIEW.md` (new manager/signal/timer/page count) **before committing**.
10. **Commit** conventional, ≤72 chars: `feat(scope): description (NEX-XXX)` (+ `GH#NN` if applicable), with the `Co-Authored-By` trailer. **PR** with `gh pr create --fill`; report the URL; do not merge.

### Definition of Done (every FW)
- [ ] Scope brainstormed (UI features) and a plan checked off.
- [ ] Feature implemented, themed, translated, and async where it does I/O.
- [ ] Unit tests for parsers/command-construction/derive logic; UAT steps in `backlog/<NEX-id>_uat.md` for UI behavior.
- [ ] `ctest` green locally (paste summary). New page → screenshot baseline added (coordinate with WI-19).
- [ ] Cross-platform reasoning recorded; built on the platform(s) the feature targets.
- [ ] `CHANGELOG.md` + `APPLICATION_OVERVIEW.md` + `ARCHITECTURE_REVIEW.md` updated.
- [ ] PR opened, not merged.

### ⚠️ DECISION-REQUIRED features
FW-06 (Intel/Qt sunset — platform-expansion/strategy), FW-20 (menu-bar monitor — large new surface), and the snap-packaging stance in FW-21 are calls reserved for the maintainer/CEO per `docs/MAINTAINER_SOP.md`. The agent prepares options + a recommendation and **stops to ask** before committing to a direction.

### Legend
**Platform:** Both / Linux / macOS. **Type:** Defensive (react to a platform change or regress) / Offensive (new capability). **Tier:** 1 (defensive must-do) → 4 (larger bet), from the recommendations doc. **Rec ref:** the item id in `2026-06-10-feature-recommendations.md`.

---

# Phase 1 — Tier 1 defensive must-dos (ship with 26.04 + macOS 27 support)

These keep Nexis from silently breaking or regressing on the new OS releases. Mostly small, high-certainty.

## FW-01 — Update the APT source editor for deb822 + `Signed-By` keyrings
- **Platform:** Linux · **Type:** Defensive (+Offensive) · **Tier:** 1 · **Effort:** M · **Rec ref:** LX1 (correctness half)
- **Why now:** APT 3.1 (Ubuntu 26.04) made **deb822 `.sources` the default format** and **removed `apt-key`** — keys must live in `/etc/apt/keyrings/` or `/usr/share/keyrings/` referenced by `Signed-By:`. The current page will mis-handle a stock 26.04 `ubuntu.sources`.
- **Existing code to build on (verify current coverage first):** `shared/nexis-core/Tools/apt_source_tool.h` **already defines** `APTSource::Format {Legacy, Deb822}`, `signedByPath`, and static parsers `parseSourceListLine()` + `parseDeb822Stanza()`. `linux/nexis-core/Tools/apt_source_tool.cpp` has deb822-aware `changeSource()`. The UI is `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp`; dispatch via `ToolManager` APT-named methods (`tool_manager.cpp:~178–206`).
- **What to build:** Audit current deb822 read/write fidelity against a real 26.04 `ubuntu.sources` (multi-suite, `Signed-By`, `Architectures`, comments). Fix gaps: (1) correctly round-trip deb822 stanzas incl. `Signed-By` keyring paths; (2) write new sources as deb822 by default on systems where `.sources` is the norm; (3) manage `Signed-By` keyrings (point at a key file in `/etc/apt/keyrings`) **without `apt-key`**; (4) keep reading legacy `.list` for older distros. Surface a clear editor for URI/suites/components/signed-by.
- **Tests:** `tests/core/test_apt_source_tool.cpp` (extend) — fixture-driven round-trip of a 26.04 `ubuntu.sources` (parse → model → serialize → byte-stable), `Signed-By` preserved, multi-stanza files, and legacy `.list` still parsed. Use `tests/fixtures/apt/` (add deb822 fixtures).
- **Acceptance:** A stock Ubuntu 26.04 source list loads, edits, and saves without corruption; new repos written as deb822 with `Signed-By`; no `apt-key` invocation anywhere.
- **Docs:** `APPLICATION_OVERVIEW.md` (APT sources page).
- **Dependency:** lighter if WI-28 (RepositoryTool refactor) lands first, but can be done standalone against the current `AptSourceTool`.

## FW-02 — Wayland-only readiness audit & fixes
- **Platform:** Linux · **Type:** Defensive · **Tier:** 1 · **Effort:** S→M · **Rec ref:** LX3
- **Why now:** GNOME 50 (26.04) **removed the X11 session entirely** (Mutter/Shell/GDM); the desktop is Wayland-only (XWayland remains for apps). Any X11-specific path is dead on a stock 26.04 GNOME session.
- **What to build:** Grep the tree for X11 assumptions — `XOpenDisplay`, `XTest`, `xcb`, `QX11Info`, raw `DISPLAY` reads, screenshot capture via X11, window/screen enumeration that assumes X. Confirm the Qt6 app runs as a native Wayland client (test under a Wayland session). Fix or guard anything X11-only; ensure the screenshot **test harness** (`tests/screenshots/`) renders under Wayland/XWayland (coordinate with WI-19/WI-20). Document any feature that legitimately needs XWayland.
- **Tests:** N/A new logic typically; if you add a display-server detection helper, unit-test it. Manual UAT: launch under a Wayland GNOME 50 session (or a Wayland compositor) and verify all pages render and screenshots work.
- **Acceptance:** Nexis runs and all features work on a Wayland-only GNOME 50 session; no silent X11 dependency.
- **Docs:** `APPLICATION_OVERVIEW.md` platform-support note if any feature becomes XWayland-gated.

## FW-03 — Handle `/run/media` removable-media mount path
- **Platform:** Linux · **Type:** Defensive · **Tier:** 1 · **Effort:** S · **Rec ref:** LX8
- **Why now:** GNOME 50 mounts removable media under **`/run/media`** instead of `/media`. Any cleaner/disk code scanning `/media` misses removable drives on 26.04.
- **Existing code:** mount-point scanning lives in the disk info providers (`linux/nexis-core/Info/disk_info.cpp`) and any cleaner path enumeration. Grep for `"/media"`.
- **What to build:** Update mount-point scanning to include `/run/media/<user>` (keep `/media` for older distros). Prefer reading actual mounts (`/proc/mounts`) over hardcoded prefixes where feasible.
- **Tests:** `tests/core/test_disk_info.cpp` (or a fixture test) — feed a `/proc/mounts` fixture containing a `/run/media/...` entry and assert the drive is enumerated.
- **Acceptance:** Removable drives mounted under `/run/media` appear in disk views and are scannable.

## FW-04 — macOS 27: strip quarantine xattr from written launchd plists
- **Platform:** macOS · **Type:** Defensive · **Tier:** 1 · **Effort:** S · **Rec ref:** MX1
- **Why now:** macOS 27 (Golden Gate) **refuses to load launchd plists carrying the `com.apple.quarantine` xattr.** Nexis writes launchd plists for **startup-items management** and **scheduled cleaning agents** (`ScheduleManager::createLaunchdPlist`, `~/Library/LaunchAgents/com.nexis.clean.*.plist`). A plist inheriting quarantine silently fails to load.
- **Existing code:** `shared/nexis/Managers/schedule_manager.cpp` (`createLaunchdPlist`, launchd path ~242–260, ~329+); macOS startup-items writer (`macos/nexis/Pages/StartupApps/...` / `StartupInfo` macOS impl).
- **What to build:** After writing any `.plist`, strip the quarantine attribute (`xattr -d com.apple.quarantine <path>` via `CommandUtil`, or `removexattr` via the Obj-C++ bridge) before `launchctl load`. Verify the plist loads on macOS 27 beta if available.
- **Tests:** `tests/managers/` (macOS-gated) — write a plist, set a fake quarantine xattr, run the strip step, assert the xattr is gone. `QSKIP` off-macOS.
- **Acceptance:** Plists Nexis writes load on macOS 27; startup-items and scheduled cleaning work post-upgrade.
- **Dependency:** scheduled-clean reliability also needs WI-06; coordinate.

## FW-05 — macOS 27: handle cross-team container access denial in cache cleaning
- **Platform:** macOS · **Type:** Defensive · **Tier:** 1 · **Effort:** M · **Rec ref:** MX2
- **Why now:** macOS 27 **denies cross-team app-container access by default with no prompt** — scanning/deleting other apps' `~/Library/Containers` data silently fails unless the user enables access in Privacy & Security. Cache cleaning regresses silently.
- **Existing code:** `CleanerService` `APPLICATION_CACHES` category → `InfoManager::getAppCaches()` (macOS SystemInfo provider returns `~/Library/Caches`/`Containers` paths).
- **What to build:** Detect denied container access (a failed read/delete that maps to the permission, not a real error) and, instead of silently skipping, surface a clear, actionable message ("Grant Full Disk Access / enable in Privacy & Security") with a deep link to the relevant settings pane. Don't count un-cleanable bytes as freed. Verify actual macOS 27 behavior against the beta before finalizing the detection heuristic.
- **Tests:** `tests/managers/` (macOS-gated) — simulate a permission-denied path via a seam and assert the cleaner reports "access needed" rather than a generic failure or silent skip.
- **Acceptance:** On macOS 27, cache cleaning either works (with access granted) or clearly tells the user how to grant access — never silently no-ops.
- **Platform:** macOS; build/run on macOS (ideally a 27 beta VM).

## FW-06 — macOS Intel sunset & Qt-version strategy  ⚠️ DECISION-REQUIRED
- **Platform:** macOS · **Type:** Defensive (planning) · **Tier:** 1 · **Effort:** S (decision) + M (execution) · **Rec ref:** MX7
- **Why now:** macOS 26 is the **last Intel-supporting release**; macOS 27 is **Apple-silicon-only** and Rosetta is being phased out. Qt supports macOS 26 **from Qt 6.10** (backported to 6.8 LTS / 6.5 patches); Liquid Glass forced rendering changes (compat mode preserves pre-Tahoe look); Xcode 26 dropped the bundled Metal toolchain.
- **What to do:** The agent prepares a short decision memo (`backlog/<NEX-id>_research.md`): Intel-build sunset timeline (Intel matters only for ≤26 users), recommended Qt floor for the macOS build (≥6.8 latest-patch, ideally 6.10/6.11), and the keep-`.dmg`/avoid-`.pkg` recommendation (Tahoe skips first-run XProtect for notarized apps; 26.3 had `.pkg` Gatekeeper rejections). **Maintainer/CEO decides.** Then execute the chosen Qt floor in CMake (`find_package(Qt6 <version> REQUIRED)` — note the audit's "no minimum Qt version" low finding) and the CI/macОS build matrix; test the UI under Liquid Glass (Tahoe) + macOS 27's transparency slider.
- **Tests:** N/A logic; CI build must succeed on the chosen Qt. **Acceptance:** documented Intel/Qt decision; Qt floor enforced; UI verified under Liquid Glass.

---

# Phase 2 — Tier 2 offensive moat-wideners

The features that make Nexis the preeminent tool. Each is a real capability competitors have (or none have).

## FW-07 — APT transaction history: undo / rollback / why
- **Platform:** Linux · **Type:** Offensive · **Tier:** 2 · **Effort:** M · **Rec ref:** LX2
- **Why now:** APT 3.1 added dnf-style **`apt history-list/info/undo/redo/rollback`** and **`apt why`/`apt why-not`**. **No GUI surfaces these** — a genuinely novel feature on Ubuntu 26.04.
- **Existing code:** package layer (`PackageService`/`PackageTool`), `CommandUtil` for invocation, the uninstaller/sources pages for placement. Gate on APT 3.1 presence (parse `apt --version`).
- **What to build:** A transaction-history view (in the Uninstaller or APT-sources area) listing recent apt transactions (`apt history-list`/`info`), with one-click **undo of the last operation** (`apt history-undo` via the existing pkexec path) and a **"why is this installed?"** explainer (`apt why`/`why-not`). Parse the apt history/why output into a model. Degrade gracefully (hide the feature) when apt < 3.1.
- **Tests:** `tests/core/` — fixture tests parsing real `apt history-list`/`apt why` output into the model (capture fixtures from a 26.04 box). Command-construction test for the undo/rollback invocation (correct argv via a seam).
- **Acceptance:** On Ubuntu 26.04 the user can view apt history, undo the last transaction, and ask why a package is installed; feature hidden on older apt.
- **Docs:** `APPLICATION_OVERVIEW.md`.

## FW-08 — Duplicate & large-file finder
- **Platform:** Both · **Type:** Offensive · **Tier:** 2 · **Effort:** M · **Rec ref:** BP1
- **Why now:** the most-cited gap vs competitors (Czkawka, CleanMyMac X). Platform-neutral.
- **Existing code to reuse:** `CleanerService` (`shared/nexis/Managers/cleaner_service.{h,cpp}`) — its exclusion engine (`isExcluded`, `ExclusionEntry`), the `QFile::moveToTrash` path (used by `DOWNLOADS_AGED`), bytes-freed/category accounting, and the scan/clean async pattern. `FileUtil::getFileSize`. The QtConcurrent + `invokeMethod` marshalling pattern (SystemCleaner page).
- **What to build:** A new finder (sub-page under Cleaner, or a new sidebar page per [Appendix A](#appendix-a--adding-a-new-page-shared-reference)) that scans a chosen root for: **duplicate files** (size pre-filter → partial hash → full hash to confirm), **large files** (top-N by size), and **empty folders**. Results table with checkboxes, size column, and safe **delete-to-trash** reusing the cleaner's trash path + exclusion engine. Scan on a worker thread with progress + cancel. Never auto-select all duplicates (keep at least one).
- **Tests:** `tests/managers/test_duplicate_finder.cpp` (new) — against a `QTemporaryDir`: known duplicate sets are grouped correctly (incl. same-size-different-content not flagged), large-file ranking correct, empty-folder detection correct, exclusions honored, and the never-delete-last-copy rule holds. Use the elevation/trash seam to avoid real deletion.
- **Acceptance:** Finds true duplicates (content-verified, not just size), lets the user trash selected ones safely, honors exclusions, and never deletes the last copy of a set.
- **Dependency:** Land **after** WI-08 (cleaner test harness + exclusion-bypass fix) since this reuses that destructive infrastructure.
- **Docs:** `APPLICATION_OVERVIEW.md`, `ARCHITECTURE_REVIEW.md` (new manager/page).

## FW-09 — Built-in disk-space visualizer (treemap/sunburst)
- **Platform:** Both · **Type:** Offensive · **Tier:** 2 · **Effort:** M · **Rec ref:** BP3
- **Why now:** today Nexis only **launches** external tools; DaisyDisk's sunburst is a flagship paid-macOS feature. A built-in visualizer is a recurring request.
- **Existing code:** `shared/nexis/Pages/Resources/disk_usage_launcher_widget.{h,cpp}` currently detects/launches Baobab/Filelight/QDirStat/DaisyDisk/etc. Augment or add alongside it. `InfoManager::getDisks()` for volume roots.
- **What to build:** A recursive directory-size scan on a worker thread (with progress + cancel), rendered as an interactive **treemap** (`QGraphicsView`/`QPainter`; treemap is simpler than sunburst — pick one, treemap recommended) with drill-down, hover size labels, "reveal in file manager", and "move to trash" (reuse the cleaner trash path). Keep the external-tool launcher as a fallback/option. Theme via tokens.
- **Tests:** `tests/managers/test_dir_size_scanner.cpp` (new) — the size-aggregation logic against a `QTemporaryDir` tree (nested dirs, symlinks not double-counted, hidden files). UI rendering is UAT.
- **Acceptance:** Scans a directory tree, renders an interactive treemap with accurate sizes, supports drill-down and trash/reveal; works on both platforms.
- **Docs:** `APPLICATION_OVERVIEW.md`, `ARCHITECTURE_REVIEW.md`.

## FW-10 — macOS Background Task Management (btm) startup-items view
- **Platform:** macOS · **Type:** Offensive · **Tier:** 2 · **Effort:** M · **Rec ref:** MX3
- **Why now:** `sfltool dumpbtm` is the diagnostic for login items/background tasks, and Tahoe 26.4 has a known **`backgroundtaskmanagementd` 400% CPU** bug where login items fail. Nexis's startup-apps page can be the best GUI for this.
- **Existing code:** macOS startup-items implementation (`macos/nexis/Pages/StartupApps/...`, `StartupInfo` macOS provider), `CommandUtil` for `sfltool`. `SMAppService` is the blessed API.
- **What to build:** Parse `sfltool dumpbtm` to show **all** background-task-manager items (not just user login items), flag orphaned/duplicate entries, and offer `sfltool resetbtm` as a repair action (with a clear warning + confirmation). Ensure the scanner ignores/tolerates the new `/System/Library/LaunchAngels` directory (Apple-private). 
- **Tests:** `tests/core/` (macOS-gated, compile-source-into-test per FR-127) — fixture test parsing real `sfltool dumpbtm` output into the model (capture a fixture); assert orphan/duplicate detection. `QSKIP` off-macOS.
- **Acceptance:** macOS startup view lists all btm items with state, flags orphans, and offers a guarded `resetbtm`; doesn't choke on LaunchAngels.
- **Docs:** `APPLICATION_OVERVIEW.md`.

## FW-11 — cgroup v2 / systemd-oomd observability
- **Platform:** Linux · **Type:** Defensive (+Offensive) · **Tier:** 2 · **Effort:** M · **Rec ref:** LX4
- **Why now:** systemd 259 (26.04) **removed cgroup v1 entirely** (won't run on v1 hosts) — per-process accounting assuming v1 paths breaks; and **systemd-oomd exposes `OOMKills`/`ManagedOOMKills`** per unit. Kernel 7.0 added a generic FS-error reporting channel.
- **Existing code:** process/dashboard accounting (`linux/nexis-core/Info/process_info.cpp`, memory info), `DataRefreshService` for a new tick/signal, `CommandUtil` for `systemctl show`/`busctl`.
- **What to build:** (Defensive) Ensure any cgroup path reads target **v2 only** (`/sys/fs/cgroup/...` unified hierarchy). (Offensive) Surface **OOM-kill counts** and recent OOM events as a new dashboard signal + a "what got killed and why" panel — read oomd `OOMKills`/`ManagedOOMKills` properties (via `systemctl show`/D-Bus). The in-box Resources app doesn't show this.
- **Tests:** `tests/core/` — fixture tests parsing cgroup v2 accounting and `systemctl show`-style oomd property output into the model.
- **Acceptance:** Process/memory accounting works on cgroup-v2-only 26.04; OOM-kill events are surfaced in the UI.
- **Docs:** `APPLICATION_OVERVIEW.md`, `ARCHITECTURE_REVIEW.md` (new signal/tick).

---

# Phase 3 — Tier 3 depth & polish

## FW-12 — App-specific cleaning profiles
- **Platform:** Both · **Type:** Offensive · **Tier:** 3 · **Effort:** M→L · **Rec ref:** BP2
- **Why now:** BleachBit has 1,000+ profiles, CleanMyMac X has deep macOS cleaners; Nexis has 6 general categories — its biggest cleaning-depth weakness.
- **Existing code:** `CleanerService` category model + scan/clean + exclusions. The profile system layers on top as a **data-driven** category source.
- **What to build:** A declarative profile schema (JSON/INI: app name, glob patterns, age policy, safety class `safe|aggressive`) + a loader that feeds paths into the cleaner's scan as a new dynamic category group. Seed 20–30 profiles per platform (browsers, IDEs, Docker, package-manager caches, Xcode caches on macOS). Community can add profiles without C++ changes (ship a profiles dir + allow user profiles). Respect exclusions and the safe/aggressive distinction in the UI.
- **Tests:** `tests/managers/test_cleaning_profiles.cpp` — schema load/validate (incl. malformed profiles rejected), glob expansion against a `QTemporaryDir`, safe-vs-aggressive gating, exclusions honored.
- **Acceptance:** Profiles load from data, expand to real paths, appear as cleanable groups, and respect safety class + exclusions; adding a profile needs no recompile.
- **Dependency:** after WI-08 (cleaner test harness).
- **Docs:** `APPLICATION_OVERVIEW.md`; a `CONTRIBUTING`-style note on adding profiles.

## FW-13 — Software updater surfacing
- **Platform:** Both · **Type:** Offensive · **Tier:** 3 · **Effort:** M · **Rec ref:** BP4
- **Why now:** CleanMyMac X has it; Nexis already brokers the package managers.
- **Existing code (extend, don't rebuild):** `InfoManager` **already has** `checkForSystemUpdates() → UpdateCheckResult`, `updateSources()`, `hasUpdateSources()`. `PackageService`/`PackageTool` per-platform managers. On Linux this maps to `apt`/`snap`/`flatpak` upgradable lists; on macOS to `brew outdated` + `softwareupdate -l`.
- **What to build:** An "Updates" view aggregating outdated packages across the platform's managers (extend `checkForSystemUpdates`/add per-manager "outdated" queries), with per-item and one-click "upgrade all" using the existing privilege paths. Async fetch + progress.
- **Tests:** `tests/core/` — fixture tests parsing `apt list --upgradable`, `brew outdated --json`, `softwareupdate -l`, `flatpak remote-ls --updates` into a unified model. Command-construction test for the upgrade action.
- **Acceptance:** The view lists upgradable items across managers and can upgrade them; reuses existing elevation.
- **Docs:** `APPLICATION_OVERVIEW.md`.

## FW-14 — Health-report export (+ scheduled reports)
- **Platform:** Both · **Type:** Offensive · **Tier:** 3 · **Effort:** S→M · **Rec ref:** BP5
- **Why now:** differentiator vs single-purpose competitors; pairs with the existing dashboard + maintenance wizard.
- **Existing code:** `InfoManager` getters (CPU/mem/disk/battery/SMART `getDriveHealth()`/thermal/GPU), `CleanerService::scan()` for cleanable space, `ScheduleManager` for the scheduled variant (reuse the cron/systemd/launchd + `--clean`-style invocation pattern; add a `--report` mode mirroring WI-06's headless handling).
- **What to build:** "Export system health report" producing Markdown (and optionally PDF) summarizing CPU/mem/disk/battery/SMART/thermal + cleanable space + recent issues. Add an optional schedule (reuse `ScheduleManager`). For scheduled/headless generation, follow WI-06's offscreen/`QCoreApplication` handling.
- **Tests:** `tests/managers/test_health_report.cpp` — generate a report from a stubbed `InfoManager` and assert sections/values render; Markdown is well-formed.
- **Acceptance:** A health report exports on demand and (optionally) on schedule with accurate data.
- **Dependency:** scheduled variant needs WI-06.
- **Docs:** `APPLICATION_OVERVIEW.md`.

## FW-15 — Battery charge-threshold control (Linux)
- **Platform:** Linux · **Type:** Offensive · **Tier:** 3 · **Effort:** S→M · **Rec ref:** LX6
- **Why now:** GNOME 50's Power panel added firmware charge modes but the stock UI is limited and has a **known revert bug on some Dell XPS**. Thresholds live in `/sys/class/power_supply/BATx/charge_control_end_threshold`. Nexis already reads battery health.
- **Existing code:** battery info provider (`linux/nexis-core/Info/battery_info.cpp`, `InfoManager::getBatteryData()`), `CommandUtil::sudoExec` (pkexec) for the privileged sysfs write, the Helpers/tuning-card UI pattern.
- **What to build:** A charge-threshold control (slider/presets: Maximize / Preserve ~80%) that writes `charge_control_end_threshold` (and `_start_threshold` if present) via pkexec, with a read-back verify (the codebase already does verify-after-write). Gate on the sysfs node existing. Persist the preference and re-apply on boot if needed (or document the kernel/udev persistence).
- **Tests:** `tests/core/` — the threshold read/parse logic against a sysfs fixture; command-construction for the write via a seam.
- **Acceptance:** On hardware exposing the sysfs node, the user can set a charge threshold and it sticks (verified by read-back); hidden when unsupported.
- **Docs:** `APPLICATION_OVERVIEW.md` (Helpers/Power).

## FW-16 — New vendor hwmon/WMI sensor surfaces (Linux)
- **Platform:** Linux · **Type:** Offensive · **Tier:** 3 · **Effort:** S · **Rec ref:** LX7
- **Why now:** kernel 7.0 added **ASUS WMI** (fan/backlight/kbd), **HP WMI manual fan control** (Victus), **Lenovo WMI extra HWMON sensors** (Legion) — new readable hwmon nodes; the gaming-laptop audience overlaps with "system optimizer" users.
- **Existing code:** thermal/fan providers (`linux/nexis-core/Info/thermal_info.cpp`, fan sensors; `InfoManager::getThermalSensors()`/`getFanSensors()`), the hardware-info/dashboard sensor display.
- **What to build:** Extend hwmon enumeration to pick up the new vendor sensors when present (read `/sys/class/hwmon/*` with vendor names), and display them in the hardware-info/thermal views. Optionally expose HP/ASUS manual fan control where the node is writable (pkexec write) — scope fan control as a stretch.
- **Tests:** `tests/core/test_thermal_info.cpp` — hwmon fixture incl. vendor WMI sensor nodes; assert enumeration/labels.
- **Acceptance:** Vendor WMI sensors appear when present; no regression on machines without them.
- **Docs:** `APPLICATION_OVERVIEW.md` (hardware info).

## FW-17 — macOS deep-maintenance tasks (OnyX-style)
- **Platform:** macOS · **Type:** Offensive · **Tier:** 3 · **Effort:** M · **Rec ref:** MX5
- **Why now:** OnyX's signature tasks (Spotlight reindex, disk verification, rebuild Launch Services, flush caches, hidden Finder/Dock/Safari defaults) are absent and are exactly what macOS power users reach for.
- **Existing code:** the **maintenance-wizard UI pattern** (`shared/nexis/Pages/Dashboard/maintenance_wizard_dialog.cpp` — but heed WI-01's UAF fix first), `CommandUtil`/`sudoExec`, `defaults`/`mdutil`/`diskutil`/`lsregister` invocations.
- **What to build:** A "Maintenance" panel exposing safe, clearly-labeled tasks: Spotlight reindex (`mdutil -E`), verify disk (`diskutil verifyVolume`), rebuild Launch Services, flush relevant caches, and a few well-known hidden `defaults` toggles — each with a description and confirmation. Run async; surface output/status. Reuse a safe worker-lifetime pattern (don't repeat the WI-01 UAF).
- **Tests:** `tests/core/` (macOS-gated) — command-construction tests for each task (correct argv) via a seam; `QSKIP` off-macOS.
- **Acceptance:** Each maintenance task runs with clear labeling/confirmation and reports success/failure; no UAF on dialog close.
- **Dependency:** build on the WI-01 worker-lifetime fix.
- **Docs:** `APPLICATION_OVERVIEW.md`.

## FW-18 — Thorough single-app uninstall leftovers (macOS)
- **Platform:** macOS · **Type:** Offensive · **Tier:** 3 · **Effort:** M · **Rec ref:** MX6
- **Why now:** AppCleaner is noted as more thorough at finding an app's scattered support files. With the AppleScript-injection fix (WI-05b) landing in the uninstaller anyway, it's a natural time to deepen it.
- **Existing code:** macOS uninstaller path (`PackageToolMacOS::getInstalledApps`/`trashApps`, `macos/nexis-core/Tools/package_tool.cpp`), the cleaner trash path.
- **What to build:** When uninstalling a `.app`, resolve its bundle id and scan standard leftover locations (`~/Library/{Application Support,Caches,Preferences,Logs,Containers,Saved Application State}`, `LaunchAgents`) for matching artifacts (by bundle id and app name), present them with sizes, and offer removal-to-trash. Honor exclusions; never delete unrelated files (match on bundle id, not loose substrings).
- **Tests:** `tests/core/` (macOS-gated) — leftover-matching logic against a synthetic `~/Library`-shaped `QTemporaryDir`; assert only matching-bundle-id artifacts are found; `QSKIP` off-macOS.
- **Acceptance:** Uninstalling an app surfaces its real leftovers (matched by bundle id) for safe removal; no false positives on unrelated apps.
- **Dependency:** depends on WI-05b (AppleScript injection fix) being in the uninstaller.
- **Docs:** `APPLICATION_OVERVIEW.md`.

---

# Phase 4 — Tier 4 larger bets

## FW-19 — Services page: drop SysV assumptions; add soft-reboot / run0 awareness
- **Platform:** Linux · **Type:** Defensive (+Offensive) · **Tier:** 4 · **Effort:** M · **Rec ref:** LX5
- **Why now:** systemd **260 will remove SysV init-script compatibility** (259 is the last with it); systemd also gained **`soft-reboot`** (userspace-only restart) and **`run0`** (sudo alternative, `--empower`); 26.04 ships **sudo-rs**.
- **Existing code:** services page + `ServiceTool` (`shared/nexis/Pages/Services/...`, `service_tool.cpp`), `CommandUtil` elevation.
- **What to build:** (Defensive) Remove assumptions that `/etc/init.d` generators persist; rely on `systemctl` for unit enumeration/control. Validate elevation works under `run0`/`sudo-rs` environments. (Offensive) Optionally add a **soft-reboot** action (faster than full reboot after updates) with a clear explanation.
- **Tests:** `tests/core/` — unit-enumeration parsing fixtures (no SysV dependency); command-construction for soft-reboot via a seam.
- **Acceptance:** Services page works without SysV generators; elevation works under sudo-rs/run0; optional soft-reboot action functions.
- **Docs:** `APPLICATION_OVERVIEW.md`.

## FW-20 — macOS menu-bar live monitor (iStat-Menus-style)  ⚠️ DECISION-REQUIRED (scope/size)
- **Platform:** macOS · **Type:** Offensive · **Tier:** 4 · **Effort:** L · **Rec ref:** MX4
- **Why now:** the most-cited macOS monitoring gap vs iStat Menus (always-on menu-bar CPU/mem/net/temp). Tahoe added a System Settings **Menu Bar section** that OS-manages per-item visibility, so a new `NSStatusItem` integrates cleanly. Pairs with Nexis's existing kiosk mode as the "always visible" story.
- **What to build (after maintainer agrees on scope):** an optional menu-bar agent (`NSStatusItem` via Obj-C++ bridge) showing live CPU/mem/network/thermal, clicking through to the dashboard; subscribe to `DataRefreshService` for data. Significant new surface (status-item lifecycle, separate from the main window, launch-at-login interplay). Brainstorm scope first; consider an MVP (CPU + mem only) before full parity.
- **Tests:** data-formatting helpers unit-tested; the status-item lifecycle is UAT.
- **Acceptance:** an optional, themeable menu-bar monitor shows live stats and opens the dashboard; respects the macOS 26 Menu Bar visibility model.
- **Docs:** `APPLICATION_OVERVIEW.md`, `ARCHITECTURE_REVIEW.md`.

## FW-21 — Packaging-reality decisions: Flatpak + snap stance  ⚠️ DECISION-REQUIRED
- **Platform:** Linux · **Type:** Defensive · **Tier:** 4 (planning) · **Effort:** S (memo) · **Rec ref:** LX9 (+ audit H7/WI-14)
- **Why now:** 26.04's snap permission-prompting would collide with Nexis's broad system access if shipped as a strict snap (and has known 26.04.0 kernel-panic bugs); the **deb avoids prompting friction** and the **App Center now manages debs** (overlapping the uninstaller). Flatpak is fundamentally broken for this app as-is (audit H7).
- **What to do:** This is **the same decision space as audit WI-14** (Flatpak) plus the snap stance. Produce one memo recommending: deb as the primary Ubuntu channel; Flatpak either fixed via `flatpak-spawn --host` (WI-14 route A) or dropped (route B); snap not pursued as strict-confined (or designed for prompting if pursued). **Maintainer/CEO decides.** Execution of the Flatpak choice is WI-14; this FW is the strategic packaging recommendation.
- **Tests:** N/A. **Acceptance:** a recorded packaging-channel decision the maintainer signs off on.
- **Dependency:** coordinate with audit WI-14 (don't duplicate the Flatpak engineering work).

---

## Dependency & sequencing summary

- **Audit prerequisites:** WI-06 (headless `--clean`) gates the scheduled half of FW-14; WI-08 (cleaner test harness + exclusion-bypass fix) gates FW-08 and FW-12; WI-05b (AppleScript fix) gates FW-18; WI-01 (wizard UAF fix) should precede FW-17; WI-28 (RepositoryTool) eases but doesn't block FW-01.
- **Fully independent (parallelizable):** FW-02, FW-03, FW-04, FW-09, FW-11, FW-15, FW-16.
- **Decision-gated (don't build until the maintainer/CEO chooses):** FW-06, FW-20, FW-21 (and FW-21 coordinates with audit WI-14).
- **Natural pairings:** FW-01 + FW-07 (both APT/26.04 — ship together as the "Ubuntu 26.04 ready" story); FW-08 + FW-09 + FW-12 (all cleaner-adjacent); FW-04 + FW-05 + FW-06 (macOS 27 readiness bundle).
- **Launch framing (from the recommendations doc):** FW-01 + FW-07 + FW-11 = "Nexis 2.4 — Ubuntu 26.04 ready: the repo-management GUI Ubuntu removed, plus OOM/GPU observability the stock Resources app doesn't show." Fits the SOP's 4–6 week release cadence.

## Coverage check

Every recommendation maps to an FW: BP1→08, BP2→12, BP3→09, BP4→13, BP5→14; LX1→01, LX2→07, LX3→02, LX4→11, LX5→19, LX6→15, LX7→16, LX8→03, LX9→21; MX1→04, MX2→05, MX3→10, MX4→20, MX5→17, MX6→18, MX7→06. (Strategic "don't chase remote/Windows" guidance needs no FW.)

---

## Appendix A — Adding a new page (shared reference)

Several FWs add a sidebar page. The mechanism (mapped from `shared/nexis/app.cpp`, `nexis_page.h`, and `CMakeLists.txt`):

1. **Create the page class** under `shared/nexis/Pages/<Area>/<name>_page.{h,cpp}`, inheriting `NexisPage` (`shared/nexis/nexis_page.h`) — override `onPageActivated()`/`onPageDeactivated()` (subscribe/unsubscribe to `DataRefreshService` here to save CPU when hidden). Build UI programmatically (most pages) or with a `.ui` file. Reference `shared/nexis/Pages/Network/network_usage_page.{h,cpp}` as a clean programmatic example.
2. **Theme:** implement `refreshThemeColors()`, connect it to `SignalMapper::sigChangedAppTheme` in the constructor, and read colors via `AppManager::ins()->getStyleValues()->value("@token", "#fallback")`. Add any new tokens to **both** `shared/static/themes/default/style/values.ini` and the light theme (and the code-token test from WI-24). No hardcoded hex (BUG-47).
3. **Register in `app.cpp`:** `#include` the header; declare a member pointer; create a sidebar button in `buildSidebar()`; set its `tr()` text in `init()` (with `#ifdef Q_OS_MAC` if the title differs by platform); append a `PageSlot{title, factory-lambda, nullptr, {}}` to `mPageSlots`; add the button to `mListSidebarButtons`; connect its click to `navByTitle(tr("..."))`. Lazy construction happens via `ensurePageByTitle` → `ensurePage` (which re-emits the theme signal so the new page themes correctly).
4. **CMake:** add the `.cpp` to `GUI_SHARED_SRCS` and the `.h` to `GUI_SHARED_HDRS` (for AUTOMOC) in `CMakeLists.txt`; add the dir to `target_include_directories`; if you use a `.ui` file, add the dir to `CMAKE_AUTOUIC_SEARCH_PATHS`. Platform-specific impls go in `GUI_PLAT_SRCS`/`GUI_PLAT_HDRS` under the `if(APPLE)`/`else()` split.
5. **Async:** do I/O on a `QtConcurrent::run` worker and marshal results back with `QMetaObject::invokeMethod(this, ...)` (register custom types with `qRegisterMetaType` before connecting), or subscribe to `DataRefreshService` signals. Never block the UI thread (see audit WI-21).
6. **Screenshot baseline:** once WI-19/WI-20 land, add the new page to `tests/screenshots/` `kPageMap` and generate baselines for both themes.

## Appendix B — Platform facts & sources

Condensed from the recommendations doc's research appendix; full source URLs are there.

- **Ubuntu 26.04 / GNOME 50 / kernel 7.0 / systemd 259 / APT 3.1:** Wayland-only (X11 session removed); new "Resources" app with GPU/NPU monitoring; "Software & Updates" GUI removed from default install; removable media at `/run/media`; deb822 default + `apt-key` removed + `Signed-By` keyrings; `apt history`/`why`; cgroup v1 removed; oomd `OOMKills`/`ManagedOOMKills`; SysV compat last release; `soft-reboot`/`run0`; sudo-rs default; battery charge-mode UI (buggy on some XPS); vendor WMI hwmon sensors (ASUS/HP/Lenovo).
- **macOS 26 "Tahoe" / 27 "Golden Gate":** Tahoe last Intel release; 27 Apple-silicon-only; 27 refuses quarantine-xattr launchd plists; 27 denies cross-team container access silently; `sfltool dumpbtm` diagnostic + Tahoe 26.4 btm 400% CPU bug; `/System/Library/LaunchAngels`; Menu Bar section in System Settings; Qt supports macOS 26 from 6.10 (backported to 6.8 LTS); prefer notarized `.dmg` over `.pkg`.
- **Competitor gaps Nexis fills with these features:** duplicate finder (FW-08), cleaning profiles (FW-12), disk visualizer (FW-09), updater (FW-13), btm/maintenance (FW-10/17), uninstall thoroughness (FW-18), menu-bar monitor (FW-20). Nexis deliberately cedes remote/web monitoring and Windows.
