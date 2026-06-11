# Nexis — Audit Remediation Plan (Paperclip Epic)

> **Date:** 2026-06-10 · **Source of findings:** [2026-06-10-audit.md](2026-06-10-audit.md) · **Audited version:** v2.3.13
> **Purpose:** This is the feature-level (epic) issue body for Paperclip. Each **Work Item (WI-NN)** below is a self-contained sub-issue spec written so a **Sonnet 4.6 engineer agent** can implement, test, verify, and PR it without further context. Work them in the numbered order — the phases encode dependency and priority.
> **Audit recap:** 0 critical · 11 high · 29 medium · 2 confirmed low. 37 work items below cover every confirmed finding plus one cleanup bucket for the unverified-low tail.

---

## How to work a sub-issue (READ FIRST — applies to every WI)

You are picking up exactly one Work Item. Do **not** scope-creep into adjacent WIs; if you touch a shared file, make the minimal change for your WI and note the overlap in your PR.

### Workflow (per the project's `/bug-feature-workflow`)
1. **Branch.** `git checkout native && git pull`, then `git checkout -b claude/<NEX-id>-<short-slug>` (e.g. `claude/NEX-310-logprovider-cancel-npe`). Never commit to `native`.
2. **Reproduce/understand first.** Open the cited files at the cited lines. Confirm the mechanism described matches the current code (the audit was at v2.3.13 — if the line moved, find it). If the finding no longer reproduces, say so in the PR and stop.
3. **Implement** the fix exactly as scoped. Match surrounding code style; no gratuitous refactors.
4. **Test.** Add/extend tests per the **Tests** section. Test-adding convention (from `CLAUDE.md`):
   - File: `tests/<category>/test_<classname>.cpp` (categories: `utils/`, `core/`, `managers/`, `theme/`, `screenshots/`).
   - Use `QTEST_MAIN(TestClassName)` and `#include "test_<classname>.moc"` at the end.
   - Add the file to the source list in `tests/CMakeLists.txt`; register with `add_test(NAME <TestName> COMMAND nexis-tests)` (or the appropriate target).
5. **Build & verify** (`/build-verify`):
   ```bash
   # macOS
   cmake --build build -j$(sysctl -n hw.ncpu)
   # Linux
   cmake --build build -j$(nproc)
   # tests (both)
   ctest --test-dir build --output-on-failure
   ```
   For a clean rebuild use the commands in `CLAUDE.md` / project `Build` section. Do not claim success without pasting the relevant passing output into the PR.
6. **Cross-platform check** (`/platform-check`) if you touched anything under `shared/`, `linux/`, or `macos/`, or any `#ifdef`. State which platform(s) you built/ran on and reason about the other.
7. **Qt/UI check** (`/qt-ui-change`) if you touched widgets, QSS, themes, or `.ui` files.
8. **Docs.** Update the three living docs **before committing** when your change affects documented behavior (per `CLAUDE.md`):
   - `CHANGELOG.md` — add an entry under `## [Unreleased]` with the right `### Added`/`### Fixed`/`### Changed` subsection. Reference the issue (`NEX-XXX`) and any `GH#NN`.
   - `docs/APPLICATION_OVERVIEW.md` — if features/UI/platform support changed.
   - `docs/ARCHITECTURE_REVIEW.md` — if signals/singletons/timers/cross-component patterns changed.
9. **Commit.** Conventional commits, ≤72 chars: `type(scope): description (NEX-XXX)`. Add `GH#NN` if a GitHub issue exists. End commit body with the `Co-Authored-By` trailer per repo policy.
10. **PR.** `gh pr create --fill` (or `--title`/`--body`). Report the URL. **Do not merge** — the maintainer reviews.

### Definition of Done (every WI)
- [ ] Fix implemented and matches the WI scope.
- [ ] New/extended test(s) added that **fail before, pass after** (state this explicitly in the PR).
- [ ] `ctest` green locally (paste the summary line). If a pre-existing unrelated test is red (e.g. ScreenshotTests before WI-19), note it.
- [ ] Cross-platform reasoning recorded.
- [ ] `CHANGELOG.md` + affected docs updated.
- [ ] PR opened, not merged.

### ⚠️ DECISION-REQUIRED items
WI-14 (Flatpak fate), WI-31b (FUNDING.yml), and the SPDX choice in WI-11 are **product/strategy or legal calls** reserved for the maintainer/CEO per `docs/MAINTAINER_SOP.md`. For these, the agent prepares the engineering options and the mechanical change for the chosen option, then **stops and asks** rather than picking unilaterally.

### Conventions glossary
- **Severity** carried from the audit. **Effort:** S ≤ ½ day, M ≤ 2 days, L > 2 days. **Audit ref:** the finding ID in `2026-06-10-audit.md`.
- "exec seam" = the `TestableRepairEngine`-style override pattern already used in `tests/.../test_repo_repair_engine.cpp` to bypass `pkexec` in tests.

---

# Phase 1 — Crash-class bugs (highest priority; mostly independent, parallelizable)

These four are isolated, small, and each can take down the app. Plus the one security code-exec bug, which is cheap and serious.

## WI-01 — Fix use-after-free in Maintenance Wizard workers
- **Severity:** High · **Effort:** S · **Audit ref:** H1
- **Files:** `shared/nexis/Pages/Dashboard/maintenance_wizard_dialog.cpp` (lines ~142, 149, 156, 163, 324), `.../maintenance_wizard_dialog.h`, caller `shared/nexis/Pages/Dashboard/dashboard_page.cpp:~1928`.
- **Problem:** `runChecks()` (and `onCleanSafeItems()`) launch detached `QtConcurrent::run` lambdas capturing raw `this` that later call `QMetaObject::invokeMethod(this, ...)` and dereference members. The dialog is created `WA_DeleteOnClose` + `exec()`. Checks take 10–60 s; closing/Esc before they finish deletes the dialog while workers still hold `this` → UAF on the next `invokeMethod`/member access. No `QFuture` stored, no destructor, no `QPointer`.
- **Fix (pick one, prefer the context-object route):**
  - Connect worker completion via `QFutureWatcher` parented to the dialog, or use the `invokeMethod`/`connect` overload that takes the dialog as a **context object**, so queued delivery is dropped when the dialog dies; **and/or**
  - Capture a `QPointer<MaintenanceWizardDialog>` in each lambda and bail if null before touching members; **and**
  - Store the `QFuture`s as members and `waitForFinished()` in a new destructor as a backstop.
- **Tests:** Add `tests/managers/` or `tests/core/` coverage is hard for a modal dialog; instead add a focused test that constructs the dialog, starts the checks, deletes it immediately, and processes events — assert no crash (run under ASan if available locally). If a headless unit test is impractical, document why in the PR and add a manual UAT step to a `backlog/NEX-XXX_uat.md`. At minimum, build with `-fsanitize=address` locally and exercise the close-early path.
- **Acceptance:** Closing the System Checkup dialog mid-scan never crashes; no worker dereferences a deleted dialog.
- **Docs:** `ARCHITECTURE_REVIEW.md` if the worker-lifetime pattern is documented there.

## WI-02 — Fix null-pointer call in `LogProvider::cancel()`
- **Severity:** High · **Effort:** S · **Audit ref:** H2
- **Files:** `shared/nexis/Pages/SystemLogs/log_provider.cpp` (cancel ~37; onProcessFinished error path ~97–107 / ~235–246), `shared/nexis/Pages/SystemLogs/system_logs_page.cpp:~38`.
- **Problem:** `cancel()` does `mProcess->kill(); mProcess->waitForFinished(3000); mProcess->deleteLater();`. `waitForFinished()` emits `finished()` synchronously; the error path (CrashExit from `kill()`) calls `deleteLater()` and sets `mProcess = nullptr`. Back in `cancel()`, `mProcess->deleteLater()` is a call through null → UB/segfault. `~SystemLogsPage()` always calls `cancel()`, and macOS `log show --last 1h` runs for seconds, so quitting after opening System Logs crashes.
- **Fix:** Before `kill()`, `mProcess->disconnect(this)` (so the slot doesn't run and null the member), **or** take a local `QProcess* p = mProcess; mProcess = nullptr;` then operate on `p` and re-check for null after the wait. Ensure `deleteLater()` is called exactly once.
- **Tests:** `tests/managers/test_log_provider.cpp` — drive a long-running fake process (or inject a `QProcess` stand-in via a seam), call `cancel()` while running, assert no double-delete and no null deref (use a guarded process pointer / `QSignalSpy` on destroyed). Verify on the macOS path mentally; the bug is platform-independent.
- **Acceptance:** Opening System Logs then quitting immediately never crashes; `cancel()` is safe whether the process is running, finished, or already cleaned up.

## WI-03 — Add mutex (or publish-on-UI-thread) for `DiskHealthInfo::mDrives`
- **Severity:** High · **Effort:** M · **Audit ref:** H3
- **Files:** `shared/nexis-core/Info/disk_health_info_shared.cpp` (`getDrives` ~124–127), `linux/nexis-core/Info/disk_health_info.cpp` (`discoverDrives` ~17; `refreshHealthElevated` ~142–154), macOS equivalent in `macos/nexis-core/Info/disk_health_info.cpp`, header `…/disk_health_info.h:~90`, callers `shared/nexis/Managers/data_refresh_service.cpp:~367`, `hardware_info_page.cpp:~463/665/686`, `resources_page.cpp:~50`.
- **Problem:** `onSlowTick()` runs `discoverDiskHealth()/refreshDiskHealth()` on a `QtConcurrent` worker every 30 s; `discoverDrives()` does `mDrives.clear()` then per-drive `smartctl` + append (mutating for seconds). The UI thread concurrently copies `mDrives` via `getDrives()` and writes `mDrives[i]` in `refreshHealthElevated`. No mutex exists. Concurrent clear/copy of a `QList` is UB. Sibling streamers already mutex their state — follow that pattern.
- **Fix (prefer the publish pattern, matches `DiskInfo::setDisks`):** Have the worker build into a **local** `QList<Drive>` and publish it to `mDrives` on the UI thread via the existing `QMetaObject::invokeMethod` hop. If that's too invasive, add a `QMutex` (or `QReadWriteLock`) member to `DiskHealthInfo` and lock in every accessor that reads/writes `mDrives` (`discoverDrives`, `refreshHealth*`, `getDrives`, `hasDrives`). Apply on **both** Linux and macOS implementations.
- **Tests:** `tests/core/test_disk_health_info.cpp` — a stress test spawning a writer thread doing `discoverDrives()`-like clear+append against a reader thread calling `getDrives()` in a loop; assert no crash and consistent records (run under TSan/ASan locally if available). Keep iterations modest for CI.
- **Acceptance:** No data race on `mDrives`; `getDrives()` always returns a coherent snapshot; behavior unchanged for users.
- **Docs:** `ARCHITECTURE_REVIEW.md` threading/refresh section.

## WI-04 — Guard the UI-thread delete/trash path (tactical exec fix)
- **Severity:** High · **Effort:** S · **Audit ref:** H4/H5 (tactical half; strategic half is WI-05)
- **Files:** `shared/nexis/Services/file_search_service.cpp` (`moveToTrash` ~67–70, `deleteFile`/`removeForever` ~81–84), caller `shared/nexis/Pages/Search/search_page.cpp:~340/359`. Reference: `command_util_shared.cpp:~60–61` (throws `QString`), and `process_service.cpp:~21–33` (existing try/catch pattern).
- **Problem:** `CommandUtil::exec` throws a raw `QString` on any `QProcess` error incl. 30 s timeout. `FileSearchService::moveToTrash`/`removeForever` call `exec("mv"/"rm")` with no try/catch, invoked directly from a UI-thread context-menu slot. A large delete >30 s kills the child mid-op and the `QString` escapes the slot through the event loop → app terminates; UI also frozen meanwhile. (The same file's `search()` already does it right.)
- **Fix:** Wrap both calls in `try/catch` (catch `const QString&`), surface failure to `SearchPage` via the existing error/signal mechanism, and move the operations off the UI thread with `QtConcurrent` exactly like `search()` does. Raise or remove the 30 s timeout for bulk file ops (pass a larger timeout arg to `exec`/the async variant).
- **Tests:** `tests/managers/test_file_search_service.cpp` — call `moveToTrash`/`deleteFile` against a `QTemporaryDir`; assert success path works and that a forced exec failure is caught and reported (not thrown). Use a seam or a path guaranteed to fail (`/nonexistent`) for the error case.
- **Acceptance:** Deleting/trashing from Search never crashes the app or freezes the UI; failures show an error instead of terminating.
- **Dependency:** WI-05 may later convert these calls to the non-throwing API and remove the try/catch — that's fine; do the safe thing now.

## WI-05b — Fix AppleScript injection in macOS app trashing  *(promoted into Phase 1)*
- **Severity:** Medium (audit) — **treat as High-priority:** arbitrary code execution · **Effort:** S · **Audit ref:** S1
- **Files:** `macos/nexis-core/Tools/package_tool.cpp` (`trashApps` ~209–223; `scanAppDirectory` ~162–191).
- **Problem:** `trashApps()` interpolates an app-bundle path into `tell application "Finder" to delete POSIX file "%1"` via `.arg(path)` with no escaping, run through `osascript`. The path is an `.app` bundle absolute path discovered by scanning `/Applications` + `~/Applications`. A bundle whose name contains a double quote (legal in macOS filenames; a downloaded archive can ship any name) breaks out of the string literal and injects arbitrary AppleScript — which can `do shell script` — when the user clicks Uninstall. `QProcess::start` argv blocks *shell* injection but the whole `-e` payload is parsed as AppleScript.
- **Fix:** Stop interpolating paths into AppleScript source. Use `QFile::moveToTrash(path)` (already used in `cleaner_service.cpp`) — it calls `NSFileManager trashItemAtURL:` with no shell/AppleScript surface. If Finder semantics are genuinely required, bind the path through `osascript` argv (`on run argv ... item 1 of argv`), never string concatenation; additionally reject any path containing quotes/newlines.
- **Tests:** `tests/core/` (compile the macOS source into the test per the FR-127 pattern) — feed `trashApps` a path containing `"` and `;` and assert the resulting action targets exactly that literal path (no injection); on non-macOS, `QSKIP`. Prefer testing the path-handling/command-construction seam.
- **Acceptance:** Uninstalling an app whose name contains shell/AppleScript metacharacters trashes exactly that bundle and executes nothing else.
- **Platform:** macOS-only path; build/run on macOS.

---

# Phase 2 — Exec-layer contract (strategic; do after Phase 1)

## WI-05 — Standardize the `CommandUtil` error-handling contract
- **Severity:** Medium (root cause of H4/H5) · **Effort:** L · **Audit ref:** A1
- **Files:** `shared/nexis-core/Utils/command_util.h:~18`, `shared/nexis-core/Utils/command_util_shared.cpp` (`exec` ~51–61 throws; `execWithStatus`/`execAsync` ~66–98 return `ExecResult`), `macos/nexis-core/Utils/command_util_platform.cpp:~11–30` and Linux equivalent (`sudoExec` swallows errors → `""`). 67 `exec()` call sites repo-wide.
- **Problem:** Three contradictory contracts coexist: `exec()` throws `QString`; `execWithStatus()/execAsync()` return `ExecResult`; `sudoExec()` swallows all errors and returns empty string. Reviewers can't tell from a call site whether failure throws, returns, or vanishes — and `sudoExec` swallowing forced verify-after-write boilerplate (documented in `ARCHITECTURE_REVIEW.md`).
- **Fix (staged, do not big-bang all 67 sites in one PR):**
  1. Convert `sudoExec` to return `ExecResult` (or a `std::expected`-like status), updating its call sites to check status; remove the now-unneeded verify-after-write reads where the status is authoritative.
  2. Make `exec()` non-throwing by delegating to `execWithStatus()` and deprecate the throwing variant (or have it return `ExecResult`); migrate call sites incrementally.
  3. Use the existing `NEXIS_ASSERT_ASYNC_EXEC` / `auditUiThread` hook to find and stage the migration.
- **Tests:** `tests/utils/test_command_util.cpp` — assert `exec`/`sudoExec`/`execWithStatus` all report failure via the unified `ExecResult` (no exception escapes), exit codes/stderr are captured, and the success path is unchanged. Use the pkexec-bypass seam for `sudoExec`.
- **Acceptance:** One error contract across the exec layer; no path throws `QString`; WI-04's try/catch becomes redundant (remove it in this PR or note it for follow-up).
- **Docs:** `ARCHITECTURE_REVIEW.md` — rewrite the exec-layer + verify-after-write section.
- **Dependency:** Land **after** WI-04. Large surface area — consider splitting per-subsystem PRs under the same NEX sub-issue.

---

# Phase 3 — Scheduled-clean correctness (silent data behavior)

## WI-06 — Make headless `--clean` mode work without a display
- **Severity:** High · **Effort:** S · **Audit ref:** H6
- **Files:** `shared/nexis/main.cpp:~179` (QApplication construction), arg parsing ~199–215; generators `shared/nexis/Managers/schedule_manager.cpp` (cron ~570, systemd ExecStart ~492–497).
- **Problem:** `--clean`/`--check-threshold` run after `QApplication app(...)`, which aborts if no QPA platform loads. The generated cron line and systemd user unit set no `DISPLAY`/`WAYLAND_DISPLAY` and no `QT_QPA_PLATFORM=offscreen`. Under cron (always display-less) and `Persistent=true` boot catch-up runs, the binary dies during `QApplication` construction → scheduled clean silently never runs.
- **Fix:** Detect headless argv (`--clean`, `--check-threshold`) **before** constructing the app and `qputenv("QT_QPA_PLATFORM", "offscreen")` (only when not already set), or construct a `QCoreApplication` for those modes (dropping the tray toast). Verify the clean path doesn't need widgets. Optionally also set the env in the generated cron/systemd lines as belt-and-suspenders.
- **Tests:** `tests/managers/` — a test that runs the binary (or the clean entry function) in an env with `DISPLAY`/`WAYLAND_DISPLAY` unset and asserts it does not abort and performs the scheduled clean against a `QTemporaryDir`. If launching the binary is impractical in CI, test the "should I force offscreen?" decision function directly.
- **Acceptance:** A cron- or boot-triggered `--clean` runs to completion with no display present.
- **Docs:** `APPLICATION_OVERVIEW.md` scheduled-cleaning section.
- **Note:** This unlocks reliable scheduled cleaning and a future CLI mode (see audit note on extracting `CleanerService` into `nexis-core` — out of scope here, capture as a follow-up).

## WI-07 — Implement the every-N-days gate for systemd schedules
- **Severity:** Medium · **Effort:** S · **Audit ref:** A2
- **Files:** `shared/nexis/Managers/schedule_manager.cpp` (`createSystemdTimer` ~465–472), `shared/nexis/Managers/cleaner_service.cpp` (`cleanSchedule` ~459–521).
- **Problem:** `createSystemdTimer` maps `EveryNDays` to a **daily** `OnCalendar` with a comment promising an in-service condition check that was never implemented. `cleanSchedule` never compares `lastRun` against `everyNDays`, so on systemd (the default scheduler) an "every 7 days" config deletes the selected categories **every day**. Cron/launchd honor the interval correctly.
- **Fix:** In `cleanSchedule()`, before cleaning: if `frequency == EveryNDays` and `lastRun.isValid()` and `lastRun.daysTo(now) < everyNDays`, return early (and don't update `lastRun`). Persist/read `lastRun` correctly.
- **Tests:** `tests/managers/test_schedule_manager.cpp` (already has an injected-clock suite) — assert that with `everyNDays = 7` and `lastRun = now-3d` the clean is skipped, and with `lastRun = now-8d` it runs. Cover the boundary (`== N`).
- **Acceptance:** "Every N days" deletes on the correct cadence on systemd; cron/launchd behavior unchanged.
- **Docs:** `APPLICATION_OVERVIEW.md` scheduling section if it states cadence behavior.

---

# Phase 4 — Test (and fix) the destructive paths

## WI-08 — Cover `CleanerService` deletion + fix the exclusion-bypass bug
- **Severity:** High · **Effort:** M · **Audit ref:** H9
- **Files:** `shared/nexis/Managers/cleaner_service.cpp` (`scan` ~221–228 filters only top-level; `clean` ~303; `cleanFiles` ~408–433 incl. `sudoExec("rm","-rf",...)` ~431; `cleanTrash`), `tests/managers/test_cleaner_exclusions.cpp`, `tests/CMakeLists.txt:~154`.
- **Problem (two parts):**
  1. **Real bug:** `scan()` runs `isExcluded()` only on top-level entries, but `cleanFiles()` then **recursively deletes directory contents without re-checking exclusions** — so an excluded child inside a scanned directory is deleted anyway. The passing `isExcluded()` unit tests give false confidence.
  2. **Coverage gap:** `clean()`, `cleanFiles()` (incl. the elevated `rm -rf` branch), `cleanTrash()`, `minFileAgeSecs`, and the move-to-trash branch are untested.
- **Fix:**
  1. Re-check `isExcluded()` for each file during recursive deletion (or filter the recursive walk through the same exclusion predicate). Also add a `--` end-of-options guard to the `rm` argv, and reconsider whether elevation is needed for user-owned paths (audit S-low note) — at minimum keep behavior but make it testable.
  2. Introduce an overridable `removeElevated()` seam on `CleanerService` (mirror `TestableRepairEngine`'s pkexec bypass) so the elevated branch is testable without root.
- **Tests:** `tests/managers/test_cleaner_service.cpp` (new) — run `clean()`/`cleanFiles()` against a `QTemporaryDir` tree with categories stubbed to temp paths; assert: excluded files **and excluded children of scanned directories** survive; `minFileAgeSecs` cutoff honored; symlinks removed-not-followed; bytes-freed accounting matches; the elevated branch routes through the test seam (not real `rm`). Use `QStandardPaths::setTestModeEnabled(true)` to avoid touching the real config (see WI-37 note).
- **Acceptance:** Exclusions are honored at every depth; all deletion paths have coverage; the elevated branch is exercised via the seam.
- **Docs:** `CHANGELOG.md` (Fixed: exclusions could be bypassed for files inside scanned directories).
- **Note:** Land this **before** WI (BP1/BP2 feature work) that reuses the cleaner.

---

# Phase 5 — Supply chain & compliance (cheap, gate-keeping for distro)

## WI-09 — Pin third-party GitHub Actions to commit SHAs
- **Severity:** High · **Effort:** S · **Audit ref:** H8
- **Files:** all 8 workflows in `.github/workflows/` — priority offenders: `aur.yml:~47–55` (`KSXGitHub/github-actions-deploy-aur@v4.1.3` + `AUR_SSH_KEY`), `crowdin-sync.yml:~38–41`, `release.yml:~541` (`softprops/action-gh-release@v3`), `lupdate.yml:~28` (`EndBug/add-and-commit@v9`), `pages.yml:~26` (`withastro/action@v3`).
- **Problem:** Every action is pinned by mutable **tag**, not commit SHA, while some hold long-lived publishing secrets. A re-pointed tag can exfiltrate the AUR key (→ malicious PKGBUILD running on Arch users' machines) — the `tj-actions/changed-files` incident vector.
- **Fix:** Replace each third-party (and ideally `actions/*`) tag with the full commit SHA of the intended release, with a trailing `# vX.Y.Z` comment. Add `.github/dependabot.yml` with the `github-actions` ecosystem so pins are kept fresh (this also partially satisfies WI-13).
- **Tests:** N/A (CI config). Verify each workflow still parses (`actionlint` if available) and the SHAs correspond to the tagged releases.
- **Acceptance:** No third-party action references a mutable tag; Dependabot config present.
- **Docs:** none (optionally note in `RELEASE.md` that actions are SHA-pinned).

## WI-10 — Ship third-party font license texts; fix `debian/copyright`
- **Severity:** High · **Effort:** S · **Audit ref:** H11
- **Files:** `shared/nexis/static.qrc:~25–29`, `shared/nexis/main.cpp:~338–342`, `linux/debian/copyright:~5–8`. Fonts: Inter (OFL 1.1), JetBrains Mono (OFL 1.1), Ubuntu-R (UFL 1.0).
- **Problem:** Five bundled fonts ship in every binary with **no OFL/UFL license text anywhere** in the repo (OFL §2 and the UFL require the license to accompany copies), and `debian/copyright` declares `Files: * License: GPL-3.0`, misstating the fonts' licensing in every `.deb`/PPA upload (the UFL is a known Debian reject-trigger).
- **Fix:** Add `LICENSE-OFL.txt` (Inter + JetBrains Mono) and `LICENSE-UFL.txt` (Ubuntu) under e.g. `shared/nexis/static/font/`; create `THIRD_PARTY_LICENSES.md` at repo root referencing them; install them with packages (add to install rules and `debian/install`); add per-pattern `Files:` stanzas for the fonts in `debian/copyright` with the correct SPDX (`OFL-1.1`, `Ubuntu-font-1.0`).
- **Tests:** N/A. Verify `debian/copyright` parses (e.g. `dpkg-parsechangelog`/lintian if available) and the license files are present in the built `.deb`.
- **Acceptance:** OFL/UFL texts shipped and installed; `debian/copyright` accurately attributes the fonts.
- **Docs:** mention in `CHANGELOG.md` (Fixed: bundled font license compliance).

## WI-11 — Resolve the GPL version inconsistency  ⚠️ DECISION-REQUIRED (SPDX choice)
- **Severity:** Medium · **Effort:** S · **Audit ref:** D1
- **Files:** `LICENSE:~5`, `linux/debian/copyright:~8`, `linux/aur/PKGBUILD:~8`, `linux/metainfo/io.github.s4solutionsllc.Nexis.metainfo.xml:~6`, `docs/MAINTAINER_SOP.md` §2.
- **Problem:** `GPL-3.0-or-later` is claimed in metainfo/PKGBUILD/SOP, but the `LICENSE` preamble uses GPL-3.0-**only** wording ("version 3 of the License", no "or any later version") and `debian/copyright` says `GPL-3.0`. As a fork, outbound license can't exceed upstream Stacer's grant.
- **Fix:** **First verify upstream oguzhaninan/Stacer's actual grant** (only/or-later). Then the maintainer decides the single correct SPDX id. The agent: present the upstream finding + both options, get the decision, then align `LICENSE` preamble, `debian/copyright`, `PKGBUILD`, `metainfo.xml`, and the SOP to that one identifier.
- **Tests:** N/A. **Acceptance:** one consistent SPDX id everywhere, consistent with upstream.

## WI-12 — Add `SECURITY.md` and enable private vulnerability reporting
- **Severity:** Medium · **Effort:** S · **Audit ref:** D2
- **Files:** new `SECURITY.md` (root or `.github/`); cross-ref `RELEASE.md:~239–255`.
- **Problem:** RELEASE.md §6 commits to a 7-day CVE SLA and a private channel, and its day-6–7 step says "update `SECURITY.md`" — but no such file exists; GitHub shows no Security Policy and reporters default to public issues.
- **Fix:** Create `SECURITY.md` stating supported versions, the `security@s4solutions.ai` contact, GitHub private-vulnerability-reporting (GHSA) support, and the 7-day SLA. Ask the maintainer to enable Private Vulnerability Reporting in repo settings (note this in the PR — it's a settings toggle the agent can't flip).
- **Tests:** N/A. **Acceptance:** `SECURITY.md` present and consistent with RELEASE.md; the dead runbook step now references a real file.

## WI-13 — Add automated vulnerability detection (CodeQL + dependency scanning)
- **Severity:** Medium · **Effort:** S–M · **Audit ref:** C3
- **Files:** new `.github/workflows/codeql.yml`, `.github/dependabot.yml` (if not already added by WI-09), `release.yml:~103–104` (linuxdeploy from mutable `continuous` tag).
- **Problem:** No Dependabot, no CodeQL, no dependency/SAST scanning anywhere — the 7-day CVE SLA's detection half is unstaffed. `linuxdeploy` is pulled from mutable `continuous` tags at release time.
- **Fix:** Add a CodeQL workflow for C/C++ on push/PR to `native` (and PRs). Ensure `dependabot.yml` covers `github-actions` (and `pip` for any tooling). Pin `linuxdeploy`/`linuxdeploy-plugin-qt` downloads to a specific release with SHA256 verification. Document subscribing a monitored channel to Qt security advisories in `RELEASE.md`/`MAINTAINER_SOP.md`.
- **Tests:** N/A. Verify the CodeQL workflow runs to completion on a test PR.
- **Acceptance:** CodeQL + Dependabot active; release-time downloads pinned + checksum-verified.
- **Dependency:** coordinate `dependabot.yml` with WI-09 (one file).

---

# Phase 6 — Packaging truth

## WI-14 — Decide and fix Flatpak  ⚠️ DECISION-REQUIRED (platform-expansion call → CEO per SOP)
- **Severity:** High · **Effort:** M (route A) / S (route B) · **Audit ref:** H7
- **Files:** `linux/flatpak/io.github.s4solutionsllc.Nexis.yml:~55–71`, `docs/flatpak-reviewer-justification.md`, `linux/flatpak/README.md`, `linux/nexis-core/Utils/command_util_platform.cpp:~12`.
- **Problem:** The manifest and justification doc assume `--filesystem=host` puts host `/usr/bin` on the sandbox PATH — false. Host root is at `/run/host`; sandbox `/usr` is the KDE runtime lacking `systemctl/apt/pkexec/smartctl/fstrim/nvidia-smi`. `pkexec` setuid is inert in bubblewrap, so **every** privileged op fails; the PID namespace hides host processes. No `flatpak-spawn`/sandbox-detection code exists. As-is, a Flathub submission ships broken headline features while requesting the most invasive permissions.
- **Engineering options (agent prepares; maintainer/CEO chooses):**
  - **Route A — make Flatpak actually work:** add `--talk-name=org.freedesktop.Flatpak`; detect sandbox via `QFile::exists("/.flatpak-info")` and route `CommandUtil` through `flatpak-spawn --host`; rewrite paths for `/run/host`; rewrite the justification doc; run `linux/flatpak/README.md`'s privileged-operation checklist against a real build. Larger effort, lasting capability.
  - **Route B — drop the channel:** remove/retire the Flatpak manifest and mark it unsupported in docs; remove the broken justification doc.
- **Tests:** Route A — run the README checklist against a local flatpak build (manual UAT in `backlog/NEX-XXX_uat.md`). **Acceptance:** either the checklist passes against a real flatpak, or the channel is removed and docs say so.
- **Note:** Also fix the in-repo manifest being pinned five releases behind (`v2.3.8`) as part of whichever route.

## WI-15 — Default `CXXBASICS_USE_FASTER_LINKERS` OFF (fix GH#82 root cause in-tree)
- **Severity:** Medium · **Effort:** S · **Audit ref:** B1
- **Files:** `shared/cmake/cxxbasics/accelerators/UseFasterLinkers.cmake:~14–15/55/57`; redundant workarounds in `linux/aur/PKGBUILD:~31`, `linux/debian/rules`, `release.yml`.
- **Problem:** The module defaults **ON** and appends `-fuse-ld=lld` whenever lld is present. Any env with lld + `-flto` in CXXFLAGS (Fedora/openSUSE defaults, CachyOS, downstreams not using the exact PKGBUILD) reproduces GH#82's "undefined symbol: main". A dev-speed accelerator should never be on for release/packaged builds.
- **Fix:** Default `CXXBASICS_USE_FASTER_LINKERS` to **OFF** (or gate on `CMAKE_BUILD_TYPE=Debug` / explicit developer opt-in), **or** skip lld selection when `-flto` appears in `CMAKE_CXX_FLAGS`/`ENV{CXXFLAGS}`. Then remove the now-redundant per-channel `-DCXXBASICS_USE_FASTER_LINKERS=OFF` workaround in the PKGBUILD (keep `options=('!lto')` decision to the maintainer). Reference GH#88.
- **Tests:** N/A. Verify a clean Release build links on the default toolchain (and that opting in still selects lld for devs).
- **Acceptance:** Packaged/release builds don't force lld; GH#82 can't recur from the default config.
- **Docs:** `CHANGELOG.md` (Fixed, GH#82/GH#88).

## WI-16 — Harden the Homebrew cask updater + backport-safe release tagging
- **Severity:** Medium · **Effort:** S · **Audit ref:** B2 + C2 (same workflows; do together)
- **Files:** `.github/workflows/homebrew.yml:~31–35/51–59`, `.github/workflows/release.yml:~540–544/475–486`, `.github/workflows/aur.yml:~21–24`, `RELEASE.md:~170` (the `-f`-less curl), §6.
- **Problem:**
  - **B2:** `curl -sL` (no `--fail`) pipes whatever GitHub returns into `sha256sum`; a 404 body's checksum gets committed to the tap → `brew install nexis` fails for everyone, workflow stays green.
  - **C2:** `make_latest: true` is unconditional and Homebrew/AUR bump on any `v*` tag with no version comparison; a `v2.3.x` security backport after `v2.4.0` would mark the old line "Latest" and downgrade Homebrew/AUR users. The "delete pre-existing release" step can leave a tag release-less on a failed re-run.
- **Fix:** Use `curl -fsSL --retry 3` and sanity-check the DMG (`file nexis.dmg | grep -q zlib`, or a min-size check) before hashing — in both `homebrew.yml` and `RELEASE.md`. Compute `make_latest` by comparing the tag to the current latest release (or set `make_latest: legacy` for backports); gate `homebrew.yml`/`aur.yml` bumps on the tag being ≥ the currently published version. Document the backport caveat in `RELEASE.md` §6.
- **Tests:** N/A. **Acceptance:** A failed/404 download fails the step instead of bricking the cask; backport tags don't downgrade users or steal "Latest".

## WI-17 — Use the tag-derived version for the macOS bundle
- **Severity:** Medium · **Effort:** S · **Audit ref:** B3
- **Files:** `CMakeLists.txt` (`APP_VERSION` computed ~564–571; bundle props ~631–632 use `${PROJECT_VERSION}`; bundle id `io.github.s4solutionsllc.Nexis` ~629 — was the unowned identifier through SSO-3379), `RELEASE.md` §1 git-add step, `release.yml:~279`.
- **Problem:** `release.yml` passes `-DAPP_VERSION_OVERRIDE=<tag>` but the `.app` Info.plist `CFBundleVersion`/`CFBundleShortVersionString` use `${PROJECT_VERSION}` (hardcoded in `project(...)`), which the override doesn't touch; and RELEASE.md §1 omits `CMakeLists.txt` from the commit step. Following the runbook ships a DMG whose version lags the release. (Resolved in SSO-3379.) Bundle id was a domain the project doesn't own and inconsistent with `io.github.s4solutionsllc.Nexis`; migrated to the owned identifier in SSO-3487 — one-time identity reset, see CHANGELOG.
- **Fix:** Set `MACOSX_BUNDLE_BUNDLE_VERSION`/`MACOSX_BUNDLE_SHORT_VERSION_STRING` from `${APP_VERSION}` (the override-aware variable). Add `CMakeLists.txt` to the RELEASE.md §1 `git add` checklist. **Flag the bundle-id** to the maintainer (changing it after notarized releases is painful — decide deliberately; do not change unilaterally).
- **Tests:** N/A. Verify a build with `-DAPP_VERSION_OVERRIDE=9.9.9` produces an Info.plist showing `9.9.9`.
- **Acceptance:** macOS bundle version tracks the tag; runbook commits `CMakeLists.txt`.
- **Platform:** macOS build.

### B3 audit-clean exception — `com.nexis.clean.<id>` launchd labels (SSO-3494)

`shared/nexis/Managers/schedule_manager.cpp` emits per-schedule LaunchAgent `Label` values of the form `com.nexis.clean.<id>` (one job per user-configured scheduled clean). These share the unowned-domain prefix `com.nexis.` and will continue to show up under a `grep -r 'com\.nexis\.'` audit pass even though the `.app`'s `CFBundleIdentifier` is now `io.github.s4solutionsllc.Nexis` (SSO-3487).

**Decision (SSO-3494, NexisMaintainer 2026-06-11): leave as-is.**

Rationale:
- launchd `Label` is an internal job key, not signed/notarized identity material. The compliance/security argument that drove the bundle-id migration (SSO-3433/SSO-3487 — code signing, notarization, handler registration all anchor on the bundle id) does not apply here.
- Migrating the labels would orphan every user's existing scheduled cleans on the same upgrade train that already orphaned their `.app` — a second consecutive user-visible disruption with no offsetting identity gain. Concentrated cost falls on the users who actually use the feature.
- The hybrid route (new labels under the owned prefix, old labels untouched) adds a permanent forever-branch to `syncToOS`, `deleteLaunchdPlist`, and the `com.nexis.clean.*.plist` glob without fully resolving the grep hit (old schedules still emit `com.nexis.*` plists indefinitely).

Future audit passes should treat the `com.nexis.clean.*` matches in `schedule_manager.cpp` as a known exception, not a re-open of B3. If we ever introduce a separate, deliberate launchd-label rotation (e.g. user-driven schedule reset, major version bump with a planned migration window), that's the time to revisit — not as ambient cleanup.

---

# Phase 7 — Concurrency/responsiveness mediums + visual-regression hygiene

## WI-18 — Bounds-check CPU percent/load lists
- **Severity:** Medium · **Effort:** S · **Audit ref:** M1
- **Files:** `shared/nexis/Managers/data_refresh_service.cpp:~286–289` (`onFastTick` emit), `shared/nexis/Pages/Dashboard/dashboard_page.cpp:~560` (`.at(0)`), `shared/nexis/Pages/Resources/resources_page.cpp:~176/202` (`.at(j+1)`). Producers: `macos/.../cpu_info.cpp:~123`, Linux `/proc/stat` reader.
- **Problem:** `getCpuPercents` returns an empty list on `host_processor_info`/`/proc/stat` failure; `onFastTick` emits without an `isEmpty()` check; consumers index `.at(0)`/`.at(j+1)` → UB in release on a transient failure. (`loadAvgs.at(j)` is safe — always 3 elements.)
- **Fix:** Skip the emit in `DataRefreshService` when `percents.isEmpty()` (or size != expected), and/or bounds-check both consumers before indexing.
- **Tests:** `tests/managers/test_data_refresh_service.cpp` (or a consumer test) — feed an empty percents list and assert no out-of-range access / no emit. Inject via a stub `InfoManager`/seam.
- **Acceptance:** A transient CPU-read failure never crashes the dashboard tick.

## WI-19 — Regenerate screenshot baselines and re-enable the suite in CI
- **Severity:** High · **Effort:** S · **Audit ref:** H10 (+ Q4)
- **Files:** `tests/reference_screenshots/{macos,linux}/{dark,light}/`, `scripts/update_screenshots.sh`, `.github/workflows/build.yml:~75–81`, `tests/screenshots/test_screenshots.cpp` (NEXIS_GENERATE_REFS ~155–161), `docs/ARCHITECTURE_REVIEW.md:~475` (false "CI runs screenshot tests" claim).
- **Problem:** Dashboard dark+light ~17–19% mismatch is **stale baselines** (last regen 2026-04-24; six dashboard-visual commits since), not environment. The suite is excluded from CI (`-E ScreenshotTests`) on both platforms and red for ~4 months. It was excluded deliberately (commit `5c173c7` — hangs on ARM64 Linux runners) and was `continue-on-error` even when present.
- **Fix:** Visually confirm the current dashboard rendering is intended, then regenerate baselines (`scripts/update_screenshots.sh` / `NEXIS_GENERATE_REFS=1`) for both themes on macOS (and Linux if you can render deterministically). Re-enable the suite in `build.yml` as a **separate `continue-on-error` step** that uploads `build/tests/test_screenshots/failures/` as artifacts, with a comment + tracking ID explaining the ARM64 hang constraint (run on the platforms that don't hang). Fix the false claim in `ARCHITECTURE_REVIEW.md`. Add a RELEASE.md step requiring green baselines before tagging.
- **Tests:** the suite itself. **Acceptance:** `ctest -R ScreenshotTests` green locally; CI runs it non-blocking with artifacts uploaded; docs match reality.
- **Dependency:** Do **before** WI-20 (methodology) so baselines are sane first. Coordinate with any WI that changes page visuals (regenerate in the same PR).

## WI-20 — Make screenshot comparison robust (mask dynamic regions + fuzz; fail on missing)
- **Severity:** Medium · **Effort:** M · **Audit ref:** T2
- **Files:** `tests/screenshots/test_screenshots.cpp` (compare ~96–109; tolerances ~35/40–41/60; missing-page ~141; missing-ref ~164–166; font reg ~234), `tests/reference_screenshots/linux/*` (only `.gitkeep`).
- **Problem:** Exact pixel equality + 5–10% whole-page tolerances is simultaneously brittle (AA/font drift flips everything) and insensitive (a real regression touching ≤10% passes). Missing page class / missing reference = `qWarning` + `continue` (silent skip); Linux ref dirs are empty so the suite vacuously passes there.
- **Fix:** `QFAIL` when a mapped page or its reference PNG is missing. Replace giant tolerances with **masking of known-dynamic rectangles** (chart/process-list regions) plus a small per-channel fuzz (e.g. per-channel delta ≤ 8 counts as equal), dropping the threshold to ~0.5–1% everywhere. Either generate+commit Linux baselines (deterministic on a pinned runner) or delete the empty dirs so the gap is explicit.
- **Tests:** the suite validates itself; add a self-test that a deliberately altered region is detected while masked dynamic regions are ignored.
- **Acceptance:** Missing refs fail loudly; thresholds are tight; no vacuous passes.
- **Dependency:** after WI-19.

## WI-21 — Move synchronous fork/exec off the UI thread (process tick + SMART unlock)
- **Severity:** Medium · **Effort:** M · **Audit ref:** M2
- **Files:** `shared/nexis/Managers/data_refresh_service.cpp:~384–388` (`onProcessTick`), `macos/nexis-core/Info/process_info.cpp:~26` (`ps` + `waitForFinished(30000)`), `shared/nexis/Pages/HardwareInfo/hardware_info_page.cpp:~663–665` (`onUnlockSmartDrive`), `linux/nexis-core/Info/disk_health_info.cpp:~145`.
- **Problem:** `onProcessTick` runs `updateProcesses()` on the UI thread; on macOS that's a blocking `ps` once/second while the Processes page is open (any stall freezes the GUI up to 30 s). `onUnlockSmartDrive` runs `pkexec smartctl` on the UI thread; its 30 s timeout races the user typing the polkit password and silently fails the unlock if they're slow.
- **Fix:** Move `onProcessTick`'s `updateProcesses()` onto a `QtConcurrent` worker, marshalling results back via `invokeMethod` (same pattern as `onSlowTick`). Run the pkexec SMART unlock asynchronously with a much larger/infinite timeout. Watch for thread-safety of the touched providers (coordinate with WI-03/WI-24).
- **Tests:** hard to unit-test UI responsiveness; add a test that `updateProcesses` runs off-thread (assert the call doesn't block the calling thread, or verify via a seam). Document manual UAT (open Processes page; UI stays responsive; slow polkit entry still unlocks).
- **Acceptance:** Processes page never freezes the GUI; slow password entry still completes the unlock.

## WI-22 — Stream-parse macOS system logs instead of buffering the whole hour
- **Severity:** Medium · **Effort:** M · **Audit ref:** M3
- **Files:** `shared/nexis/Pages/SystemLogs/log_provider.cpp:~179–186/237/249/290–291`, `system_logs_page.cpp:~33`.
- **Problem:** macOS `LogProviderMacOS` runs `log show --style ndjson --last 1h` with no line/level cap and reads nothing until `finished()`; an hour of ndjson is hundreds of MB to >1 GB, copied twice and parsed on the main thread → memory spike + UI stall on a page that auto-fetches on open.
- **Fix:** Stream-parse incrementally from `readyReadStandardOutput`, accumulate up to `mMaxEntries` and then stop/kill the child; and/or shrink the default window (`--last 5m`, escalate on demand) and add `--level` filtering. Keep parsing off the main thread or chunked.
- **Tests:** `tests/managers/test_log_provider.cpp` — feed a large synthetic ndjson stream via a seam and assert memory stays bounded and only `mMaxEntries` are retained, parsing incrementally.
- **Acceptance:** Opening System Logs on a busy Mac doesn't spike memory or stall; entries cap correctly.
- **Platform:** macOS.

## WI-23 — Eliminate wizard/provider cross-thread races
- **Severity:** Medium · **Effort:** M · **Audit ref:** M4
- **Files:** `shared/nexis/Pages/Dashboard/maintenance_wizard_dialog.cpp:~163–204` (worker reads `getDisks()`/`getThermalTemperature(0)`), `shared/nexis-core/Info/disk_info.h:~27–31` (`setDisks` unsynchronized), `macos/nexis-core/Info/thermal_info.cpp:~52–74` (unguarded static `sConn` check-then-open), `data_refresh_service.cpp:~341–355` (medium tick reassigns disks / reads SMC).
- **Problem:** The wizard health-score worker reads `DiskInfo`/thermal providers on a worker thread while the medium tick reassigns `DiskInfo::disks` and reads SMC temps on the UI thread; no synchronization, plus a static `io_connect_t` check-then-open race that can double-open/leak the AppleSMC connection.
- **Fix:** Snapshot the needed data on the UI thread and capture by value in the worker lambda (preferred — avoids locking), **or** add a mutex to `DiskInfo::getDisks/setDisks`. Serialize SMC access with a `QMutex` around `smcReadKey` and `std::call_once` for `smcOpen`.
- **Tests:** `tests/core/` — concurrency stress on `DiskInfo` get/set; `std::call_once` ensures a single SMC open (assert via a counter seam). `QSKIP` SMC specifics off-macOS.
- **Acceptance:** No race on `DiskInfo`/SMC during a System Checkup run; SMC connection opened once.
- **Dependency:** related to WI-03/WI-21; coordinate the threading model.

---

# Phase 8 — Qt/UI mediums

## WI-24 — Add the missing `@networkDownloadColor` theme token (+ code-token test)
- **Severity:** Medium · **Effort:** S · **Audit ref:** Q1
- **Files:** `shared/nexis/Pages/Network/network_usage_page.cpp:~492/510`, `shared/nexis/static/themes/default/style/values.ini`, `themes/light/style/values.ini`, `tests/theme/test_theme_tokens.cpp`.
- **Problem:** The RX bar reads `@networkDownloadColor`, absent from both `values.ini`, so it always falls back to legacy Stacer blue `#5294e2` and ignores theme switching (TX uses the proper `@networkUploadColor`). The theme-token test only validates QSS tokens, not C++ `getStyleValues()` reads.
- **Fix:** Add `@networkDownloadColor` to **both** themes (pair it sensibly with `@networkUploadColor`). Extend `test_theme_tokens.cpp` to grep `shared/` C++ for `value("@…")`/`sv->value("@…")` literals and assert each exists in both themes.
- **Tests:** the extended `test_theme_tokens.cpp` (fails before — missing token — passes after).
- **Acceptance:** RX bar themes correctly in both themes and follows live theme switches; code-side tokens are guarded like QSS tokens.
- **Qt/UI:** run `/qt-ui-change`.

## WI-25 — Resolve the raw `@borderColor` token in the data-cap progress bar
- **Severity:** Medium · **Effort:** S · **Audit ref:** Q2
- **Files:** `shared/nexis/Pages/Network/network_usage_page.cpp:~472–475` (and the `refreshThemeColors` pattern ~491).
- **Problem:** `mCapBar`'s stylesheet contains the literal `background: @borderColor;`. Per-widget `setStyleSheet` strings aren't run through token substitution (only the global stylesheet is), so Qt drops the invalid declaration and the groove renders unthemed.
- **Fix:** Resolve the token first: `const QString border = sv->value("@borderColor", <fallback>).toString();` then `.arg(border, chunkColor)` into the stylesheet string — matching the surrounding code.
- **Tests:** extend `tests/theme/` or a `network_usage_page` test to assert no raw `@token` remains in any applied stylesheet (grep the generated string for `@`). The WI-24 token-grep test can cover this class.
- **Acceptance:** Cap bar groove themed in both themes; no raw tokens in per-widget stylesheets.
- **Qt/UI:** run `/qt-ui-change`.

## WI-26 — Persist start page as a stable id, not translated text
- **Severity:** Medium · **Effort:** S–M · **Audit ref:** Q3
- **Files:** `shared/nexis/Pages/Settings/settings_page.cpp:~136–141/293–296` (combo populate + persist), `shared/nexis/Managers/setting_manager.cpp:~54–61`, `shared/nexis/app.cpp:~657/879–887/941–948` (start-page resolution), platform title diff `app.cpp:~235–237`.
- **Problem:** The start-page setting stores the localized combo text and launch matches it against localized sidebar titles, falling back to Dashboard on mismatch. Changing the UI language silently resets the start page and the combo no longer reflects the saved choice. The page title also differs by platform (Applications vs Uninstaller).
- **Fix:** Store a stable untranslated page id via `QComboBox` item data (the pattern used two lines down for the color scheme: `addItem(tr("Auto"), "auto")`). Map ids → pages in `App` instead of matching tooltips/titles. Add a one-time migration that maps known legacy localized values (and the platform title variants) to ids; default id `"dashboard"`.
- **Tests:** `tests/managers/test_setting_manager.cpp` — round-trip the start-page id; assert a legacy localized value migrates to the right id; assert an unknown value defaults to dashboard.
- **Acceptance:** Start page survives language changes and is consistent cross-platform.
- **Docs:** `APPLICATION_OVERVIEW.md` settings section.

---

# Phase 9 — Architecture mediums (sequence carefully; larger surface)

## WI-27 — Route shared pages through `InfoManager`; ban platform headers in `shared/Pages`
- **Severity:** Medium · **Effort:** M · **Audit ref:** A3
- **Files:** `shared/nexis/Pages/HardwareInfo/hardware_info_page.cpp:~158–194`, `dashboard_page.cpp:~327–330/471–474`, `boot_analysis_page.cpp:~22–24`, `shared/nexis/Managers/info_manager.{h,cpp}` (add getters; wire `BootAnalysisInfo`/`StartupInfo`).
- **Problem:** Three pages stack-construct `…MacOS`/`…Linux` `Info` subclasses behind `#ifdef`s instead of using the `InfoManager` facade (`BootAnalysisInfo`/`StartupInfo` aren't wired through it at all). 47 `shared/` files carry `Q_OS_*` conditionals — each a site to edit to add a platform.
- **Fix:** Add the missing `InfoManager` getters and wire `BootAnalysisInfo`/`StartupInfo` through it (or a `createForPlatform()` factory like `LogProvider`). Migrate the three pages to use the facade. Add a CI grep (small script + step) forbidding `shared/nexis/Pages` from including `*_macos.h`/`*_linux.h`.
- **Tests:** the CI grep; build on both platforms to confirm the facade wiring compiles. Add a unit test that `InfoManager` returns a non-null provider for each newly-wired type.
- **Acceptance:** The three pages no longer include platform headers; the grep passes; behavior unchanged.
- **Docs:** `ARCHITECTURE_REVIEW.md` (facade/factory section).

## WI-28 — Split Homebrew off the APT-source abstraction (`RepositoryTool`)
- **Severity:** Medium · **Effort:** L · **Audit ref:** A4 (also enables feature LX1/LX2)
- **Files:** `macos/nexis-core/Tools/apt_source_tool.cpp:~11–57`, `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp` (32 `Q_OS_MAC` directives), `shared/nexis/Managers/tool_manager.cpp:~178–206`. Template: the `RepoHealthChecker` split.
- **Problem:** `AptSourceToolMacOS` pretends Homebrew packages are APT repos (`.uri`=package name, `.suites`="formula"/"cask", no-op `changeSource/changeStatus`); the shared page branches around the mismatch with 32 preprocessor directives. Largest unreconstructed Stacer architecture; the APT-named `ToolManager` methods no longer describe either platform.
- **Fix:** Introduce a platform-neutral `RepositoryTool` interface (model on `RepoHealthChecker`'s shared-types-header split). Either make Homebrew its own page backed by `PackageTool`/brew-tap data, or generalize the interface vocabulary away from APT specifics. Retire the `APTSource` field-overloading.
- **Tests:** `tests/managers/` — interface-level tests for both platform implementations against the new abstraction; ensure existing APT behavior is preserved on Linux.
- **Acceptance:** No `APTSource` field-overloading; the page's `Q_OS_MAC` directive count drops substantially; Linux APT + macOS Homebrew both work.
- **Docs:** `ARCHITECTURE_REVIEW.md`, `APPLICATION_OVERVIEW.md`.
- **Note:** This is the natural foundation for feature WI/LX1 (deb822 editor) and LX2 (APT history) — scope this PR to de-shoehorning only; build features on top later.

## WI-29 — Make `GnomeSettingsTool` Linux-only (remove dangerous dead macOS adapter)
- **Severity:** Medium · **Effort:** S · **Audit ref:** A5
- **Files:** `macos/nexis-core/Tools/gnome_settings_constants.h:~8–44`, `gnome_settings_tool.cpp`, `shared/nexis/Managers/tool_manager.cpp:~27`, `shared/nexis/app.cpp:~559–573`, `CMakeLists.txt:~304–308` (shared GnomeSettings sources compile on macOS).
- **Problem:** The GNOME Settings page is hidden on macOS, yet `ToolManager` still constructs `GnomeSettingsToolMacOS`, whose mapping table collapses 9 GNOME keys onto `AppleInterfaceStyle` and others onto dock `orientation`. If the `#ifdef` hide were removed, `isAvailable()` returns true and writes would corrupt `NSGlobalDomain`.
- **Fix:** Make `ToolManager::mGnomeSettings` Linux-only, or a null stub whose `isAvailable()` returns false on macOS; drop the macOS branch from `checkGnomeSettings()`. Note the constants header may still be needed to compile shared sources — neutralize the behavior (stub) rather than necessarily deleting files; verify the macOS build still links.
- **Tests:** `tests/managers/` — assert `GnomeSettings` is unavailable on macOS (`isAvailable()==false`) and that the tool performs no `defaults write`.
- **Acceptance:** No code path can write GNOME-mapped values into macOS `NSGlobalDomain`; macOS build still compiles.
- **Platform:** verify on macOS.

## WI-30 — Reconcile the living architecture/overview docs
- **Severity:** Medium · **Effort:** S–M · **Audit ref:** A6 (+ D3)
- **Files:** `docs/ARCHITECTURE_REVIEW.md` (LOC ~92; signals ~250–263/86/273; ~475 CI claim), `docs/APPLICATION_OVERVIEW.md` (settings paths ~774–777; DataRefreshService counts ~600; headers ~4).
- **Problem:** Material drift: "~6,000–7,000 LOC" (actual ≈48,700); lists removed `sigDashboardLayoutReset` and "9 signals" (now 10); wrong settings paths (claims plist/`.conf`, actual `AppConfigLocation/settings.ini`); wrong DataRefreshService timer/signal counts (5/15 vs "4/10"); stale "Last updated" headers (violate CLAUDE.md's own checklist); false "CI runs screenshot tests" claim (fixed in WI-19).
- **Fix:** One reconciliation pass: regenerate by-the-numbers sections from the tree (`cloc`, grep counts), fix the settings-path and signal/timer sections, refresh both "Last updated"/version headers, and replace duplicated hard counts with one canonical table referenced by both docs. Add a small CI check comparing the doc version header to `PROJECT_VERSION`.
- **Tests:** the CI header check. **Acceptance:** docs match the code; headers current; counts consistent between the two docs.
- **Dependency:** do the `~475` CI-claim fix in coordination with WI-19 (or after).

---

# Phase 10 — CI/test remaining

## WI-31 — Fix the lupdate normalization trigger
- **Severity:** Medium · **Effort:** S · **Audit ref:** C1
- **Files:** `.github/workflows/lupdate.yml:~3–6/28`, `.github/workflows/crowdin-sync.yml:~38–41`.
- **Problem:** `lupdate.yml` triggers only on push to `l10n_crowdin_translations`, but the only writer is `crowdin/github-action` using the default `GITHUB_TOKEN`, and GitHub suppresses workflow triggers for token-created pushes — so normalization never runs automatically; translation PRs land un-normalized.
- **Fix:** Run lupdate as a step **inside** `crowdin-sync.yml` (before/after the Crowdin action), **or** have Crowdin push with a PAT/GitHub App token so downstream workflows trigger. Add an explicit `permissions: contents: write` block to `lupdate.yml` regardless.
- **Tests:** N/A. **Acceptance:** lupdate normalization actually runs on the Crowdin sync; explicit permissions set.

## WI-31b — Reconcile `FUNDING.yml` with the no-monetization rule  ⚠️ DECISION-REQUIRED (CEO per SOP)
- **Severity:** Medium · **Effort:** S · **Audit ref:** D4
- **Files:** `.github/FUNDING.yml:~3`, `docs/MAINTAINER_SOP.md` §2/§5.
- **Problem:** `FUNDING.yml` enables a GitHub Sponsors button, while the SOP forbids monetization "Ever" and reserves sponsorship to the CEO with a recorded rationale. The two contradict each other publicly.
- **Fix (maintainer/CEO decides):** either delete `FUNDING.yml`, or record a CEO-approved exception in `MAINTAINER_SOP.md` §2 clarifying repo-level donation links are permitted while in-app monetization stays prohibited. Agent prepares both diffs and asks.

## WI-32 — Run tests in the release workflow
- **Severity:** Medium · **Effort:** S · **Audit ref:** T1
- **Files:** `.github/workflows/release.yml` (build-linux, build-linux-deb-plucky, build-macos jobs).
- **Problem:** No `ctest` step in the release jobs (the `.deb` jobs do run tests via `debian/rules override_dh_auto_test`, but AppImage, raw-binary, and macOS jobs run none); tests run only on push/PR. A directly-tagged hotfix ships untested.
- **Fix:** Add a test step to each release build job mirroring `build.yml` (`xvfb-run -a ctest --test-dir build --output-on-failure -E ScreenshotTests` on Linux; plain `ctest … -E ScreenshotTests` on macOS), **or** gate the Release workflow on a green Build run for the tagged commit (preferred — avoids rebuild). Coordinate the `-E ScreenshotTests` exclusion with WI-19.
- **Tests:** N/A. **Acceptance:** No release artifact publishes without the unit suite having passed on that commit.

## WI-33 — Add macOS parser fixtures + uninstall command-construction tests
- **Severity:** Medium · **Effort:** M · **Audit ref:** T3
- **Files:** `macos/nexis-core/Info/nettop_streamer.cpp:~91`, `macos/nexis-core/Info/disk_health_info.cpp:~14` (`parsePlist`), ioreg/pmset battery + boot-analysis parsers; `shared/nexis/Services/package_service.h:~25–28`; both `sudoExec` impls; `tests/fixtures/` (currently Linux-only), `tests/CMakeLists.txt:~52–61` (compile-source-into-test pattern).
- **Problem:** All fixtures are Linux-shaped; macOS live-tool parsers (`nettop` CSV, `parsePlist`, ioreg/pmset battery, boot-analysis) have zero fixture tests despite macOS being a release platform. `PackageService::uninstall*`/`removeOrphanPackages` and `sudoExec` are exercised only by one test that `QSKIP`s without `pkexec`.
- **Fix:** Capture real `nettop`/`ioreg`/`pmset`/`diskutil`-plist output as `tests/fixtures/macos/...` and test the parsers using the FR-127 compile-source-into-test pattern. For uninstall, test the command-construction layer (package-name escaping, purge flag) via a seam mirroring `TestableRepairEngine`.
- **Tests:** the new fixture + construction tests (this WI **is** the tests).
- **Acceptance:** macOS parsers and uninstall command construction have deterministic fixture coverage; tests run (not `QSKIP`) on macOS CI.

---

# Phase 11 — Low severity + cleanup

## WI-34 — Add build hardening flags for Release (Linux/macOS)
- **Severity:** Low · **Effort:** S · **Audit ref:** L1
- **Files:** top-level `CMakeLists.txt:~17`, `shared/cmake/`.
- **Problem:** Only the `.deb` channel gets `hardening=+all`; AppImage, raw binary, and macOS inherit toolchain defaults. The real gap vs Ubuntu's GCC defaults is full RELRO/BIND_NOW and explicitness.
- **Fix:** Add a Release-config hardening block for Linux GCC/Clang: `-fstack-protector-strong`, `-D_FORTIFY_SOURCE=2` (or 3), `-Wl,-z,relro,-z,now`, `CMAKE_POSITION_INDEPENDENT_CODE ON`. Distro channels layer their own harmlessly on top. Add equivalent macOS hardening where applicable.
- **Tests:** N/A. Verify a Release build still links on both platforms; spot-check with `checksec`/`hardening-check` on Linux if available.
- **Acceptance:** Flagship portable artifacts are hardened, not the least-hardened.

## WI-35 — Document/track the ScreenshotTests CI exclusion
- **Severity:** Low · **Effort:** S · **Audit ref:** L2
- **Note:** Largely subsumed by **WI-19** (which re-enables the suite as non-blocking with a tracking comment). If WI-19 is done, this WI is just verifying the `-E ScreenshotTests` flag in `build.yml` carries an explanatory comment + tracking ID, or a non-comparing smoke mode exists. Close as duplicate of WI-19 if fully covered.

## WI-36 — Unverified-low cleanup bucket
- **Severity:** Low · **Effort:** M (several small items) · **Audit ref:** *Unverified low* notes across all dimensions
- **Scope:** A grab-bag of small, individually-confirmable items the audit surfaced but did not run through verification. **The agent must re-confirm each against current code before fixing** (some may be stale). Treat as a checklist; one PR may cover several closely-related items, or split as sensible:
  - **Security:** apt dry-run `bash -c` package-name interpolation → use argv (`package_tool.cpp:~346`); smartctl/setcap `pkexec sh -c` device-path interpolation → quote + validate `^/dev/[A-Za-z0-9]+$` or per-device argv (`disk_health_info.cpp:~104`); cleaner `rm -rf` `--` guard + drop unnecessary root for user-owned files (`cleaner_service.cpp:~431`).
  - **Memory:** prune dead-PID entries in `nettop_streamer`/`nvidia_smi_pmon_streamer` `mLatest` maps; `reply->deleteLater()` in `DashboardPage::checkUpdate` (`dashboard_page.cpp:~527`).
  - **Qt/UI:** rename/fix `on_btnScan_clicked` auto-connect mismatch in SystemCleaner (`system_cleaner_page.h:~77` / `.cpp:~149–152`) — removes the test-log warning; remove residual hardcoded hex in `metric_tile_base.cpp:~89–90` (BUG-47); reconsider pervasive `Qt::NoFocus` blocking keyboard nav (`app.cpp:~75`) — scope as a separate a11y task if large.
  - **Build/packaging:** flatpak manifest pinned to `v2.3.8` (covered by WI-14); macOS entitlements broader than needed (`entitlements.plist:~6`); install rules no-op for default Debug build (`CMakeLists.txt:~636`); no minimum Qt6 version enforced; `ppa.yml` `|| true` masks tar failures (`ppa.yml:~38`).
  - **CI:** overly broad `GITHUB_TOKEN` scopes on build jobs (`release.yml:~8`); no SHA256SUMS/signatures on release artifacts (`release.yml:~547`); RELEASE.md references removed `scripts/nexis_db.py`/`BUGS.md` (`RELEASE.md:~28`).
  - **Tests:** delete dead `tests/test_nexis_db.py`; `setTestModeEnabled(true)` in `test_cleaner_exclusions.cpp` to stop writing real user config.
  - **Docs:** CONTRIBUTING label mismatches (`CONTRIBUTING.md:~123`); README vs SOP time-box contradiction (`README.md:~239`); CHANGELOG saturated with internal tracker IDs + duplicate `### Fixed` in [2.3.7]; no per-file copyright headers; dead SSOA link in SOP (`MAINTAINER_SOP.md:~140`); no `CODE_OF_CONDUCT.md` / PR template.
- **Tests:** per item (parser/seam tests where code changes; N/A for docs/CI).
- **Acceptance:** each addressed item re-confirmed, fixed, and tested; stale ones closed with a note.

---

## Dependency & parallelization summary

- **Fully parallel (no shared files):** WI-01, WI-02, WI-03, WI-05b(S1), WI-06, WI-07, WI-18, WI-24, WI-25, WI-29.
- **Sequential chains:**
  - WI-04 → WI-05 (tactical exec fix before the strategic refactor).
  - WI-19 → WI-20 (regenerate baselines before tightening methodology); both coordinate with WI-30 (doc claim) and WI-32 (`-E` flag).
  - WI-28 (RepositoryTool) is a prerequisite for the deb822/APT-history **features** (separate epic) — not for any other remediation WI.
  - WI-09 and WI-13 share `dependabot.yml` — do WI-09 first.
- **Threading cluster (coordinate to avoid conflicting edits):** WI-03, WI-21, WI-23 all touch disk-health/provider threading and `data_refresh_service.cpp`. Prefer landing WI-03 first, then WI-21/WI-23 rebased on it.
- **Decision-gated (don't auto-resolve):** WI-11 (SPDX), WI-14 (Flatpak), WI-17 (bundle id only), WI-31b (FUNDING).

## Coverage check

Every confirmed audit finding maps to a WI: H1→01, H2→02, H3→03, H4/H5→04(+05), H6→06, H7→14, H8→09, H9→08, H10→19, H11→10; S1→05b; M1→18, M2→21, M3→22, M4→23; Q1→24, Q2→25, Q3→26, Q4→19; A1→05, A2→07, A3→27, A4→28, A5→29, A6→30; B1→15, B2→16, B3→17; C1→31, C2→16, C3→13; T1→32, T2→20, T3→33, (T H10/H9 covered); D1→11, D2→12, D3→30, D4→31b; L1→34, L2→35; unverified-low→36.
