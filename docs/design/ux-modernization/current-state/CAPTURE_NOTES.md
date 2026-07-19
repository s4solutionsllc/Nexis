# Current-State Capture Notes

Evidence pack for the UX modernization effort (see
`docs/superpowers/specs/2026-07-13-ux-modernization-design.md` and
`docs/superpowers/plans/2026-07-13-ux-modernization-planning.md`). This file
tracks "before" screenshot coverage across all 19 tracked rows (18 prototype
pages + the Dashboard reference) × platform.

## Coverage table

| # | Page | macOS | Linux |
|---|------|-------|-------|
| 1 | Dashboard (reference) | captured — `macos/{dark,light}/dashboard.png` (harness) | captured — `linux/{dark,light}/dashboard.png` (harness) |
| 2 | Processes | captured — `macos/{dark,light}/processes.png` (harness) | captured — `linux/{dark,light}/processes.png` (harness) |
| 3 | Search | captured — `macos/{dark,light}/search.png` (harness) | captured — `linux/{dark,light}/search.png` (harness) |
| 4 | System Logs | captured — `macos/{dark,light}/system_logs.png` (manual) | not-captured (Linux): outside the harness's 12 pages, and the capture host is headless (offscreen QPA, no interactive session) so no manual capture is possible. Page is cross-platform (`shared/nexis/Pages`); prototype derives from the macOS capture of the same shared page. |
| 5 | Boot Analysis | captured — `macos/{dark,light}/boot_analysis.png` (manual) | not-captured (Linux): outside the harness's 12 pages; headless host, no manual capture possible. Cross-platform page; prototype derives from the macOS capture. |
| 6 | Hardware Info | captured — `macos/{dark,light}/hardware_info.png` (harness) | captured — `linux/{dark,light}/hardware_info.png` (harness) |
| 7 | Uninstaller (sidebar label: "Applications") | captured — `macos/{dark,light}/uninstaller.png` (harness) | captured — `linux/{dark,light}/uninstaller.png` (harness) |
| 8 | Services | captured — `macos/{dark,light}/services.png` (harness) | captured — `linux/{dark,light}/services.png` (harness) |
| 9 | Startup Apps | captured — `macos/{dark,light}/startup_apps.png` (harness) | captured — `linux/{dark,light}/startup_apps.png` (harness) |
| 10 | APT Source Manager | not-applicable (Linux-only page; the macOS build compiles `HomebrewPage` in this sidebar slot instead — see row 15) | captured — `linux/{dark,light}/apt_source_manager.png` (harness, round 2 — SSO-14981) |
| 11 | System Cleaner | captured — `macos/{dark,light}/system_cleaner.png` (harness) | captured — `linux/{dark,light}/system_cleaner.png` (harness) |
| 12 | Disk Tools | captured — `macos/{dark,light}/disk_tools.png` (manual) | not-captured (Linux): outside the harness's 12 pages; headless host, no manual capture possible. Cross-platform page; prototype derives from the macOS capture. |
| 13 | Docker | not-capturable (macOS): Docker is not installed on the capture machine, so `ToolManager::checkDocker()` returns false and `app.cpp` hides the sidebar button at runtime — the page is compiled but not reachable via UI. Re-capture once Docker is available, or capture on a host that has it installed. | captured — `linux/{dark,light}/docker.png` (harness, round 2 — SSO-14981). **Note:** the GitHub Actions runner already has a live Docker daemon (v28.0.4) running as part of its standard image — installing the `docker` CLI to satisfy `ToolManager::checkDocker()` connected to it, so the capture shows a fully populated, real state: 7 images (the runner's own tooling/workflow images, e.g. `ghcr.io/github/gh-aw-firewall/*`), Images/Containers/Volumes tabs, and a "Docker daemon: Running" status bar — not an empty state. |
| 14 | Helpers | captured — `macos/{dark,light}/helpers.png` (harness) | captured — `linux/{dark,light}/helpers.png` (harness) |
| 15 | Homebrew | captured — `macos/{dark,light}/homebrew.png` (manual) | not-applicable (macOS-only page; the Linux build compiles `APTSourceManagerPage` in this sidebar slot instead — see row 10) |
| 16 | Resources | captured — `macos/{dark,light}/resources.png` (harness) | captured — `linux/{dark,light}/resources.png` (harness) |
| 17 | Network Usage | captured — `macos/{dark,light}/network_usage.png` (harness) | captured — `linux/{dark,light}/network_usage.png` (harness) |
| 18 | Settings | captured — `macos/{dark,light}/settings.png` (harness) | captured — `linux/{dark,light}/settings.png` (harness) |
| 19 | GNOME Settings | not-applicable (Linux-only page; does not exist in the macOS build) | captured — `linux/{dark,light}/gnome_settings.png` (harness, round 2 — SSO-14981) |

macOS coverage: 16 of 18 prototype pages captured in both themes, plus the
Dashboard reference (17/19 rows populated). The 2 gaps are both
platform/runtime scoping, not capture failures: Docker (not installed on this
machine) and GNOME Settings (Linux-only, doesn't exist on macOS).

Linux coverage: 14 of 18 prototype pages captured in both themes, plus the
Dashboard reference (15/19 rows populated). The remaining 3 Linux gaps
(`boot_analysis`, `disk_tools`, `system_logs`) are harness/host scoping, not
capture failures — outside the ScreenshotTests harness's page map, and the
Linux capture host is a headless server (offscreen QPA) where manual
UI-driven capture is impossible. `docker`, `apt_source_manager`, and
`gnome_settings` were added to the harness's `kPageMap` and captured in round
2 (SSO-14981) — see "Round-2 capture: APT Source Manager, Docker, GNOME
Settings (SSO-14981)" below.

### Pages with no live capture anywhere (final capture-gap log)

All three pages that previously had no live capture on either platform
(`docker`, `apt_source_manager`, `gnome_settings`) now have a Linux capture
as of round 2 (SSO-14981) — see rows 10, 13, 19 in the Coverage table above
and the dedicated section below. `docker` still has no macOS capture (Docker
not installed on the macOS capture machine → sidebar hidden), but the Linux
capture is a live current-state pixel source, so this row is no longer a
"no capture anywhere" gap.

Pages with a macOS capture but no Linux capture (`boot_analysis`,
`disk_tools`, `system_logs`) are cross-platform `shared/nexis/Pages` code —
prototypes for these derive from the macOS capture of the same shared page.

## Capture inventory

`docs/design/ux-modernization/current-state/macos/{dark,light}/` — 16 PNGs
per theme, 32 total:

- 12 from the `ScreenshotTests` harness (`tests/screenshots/test_screenshots.cpp`,
  `kPageMap`): `dashboard`, `hardware_info`, `startup_apps`, `system_cleaner`,
  `search`, `services`, `processes`, `uninstaller`, `resources`, `helpers`,
  `network_usage`, `settings`.
- 4 from manual capture (harness doesn't cover these pages): `boot_analysis`,
  `disk_tools`, `system_logs`, `homebrew`.

Window framing: harness PNGs are native `QPixmap::grab()` output at the
harness's `App::resize(1024, 768)` (some are saved at 2x, e.g.
2048×1536, since `grab()` captures at the display's device pixel ratio).
Manual PNGs are cropped from full-screen `screencapture` output to the Nexis
window's content bounds (see "Manual capture method" below); window was
resized to ~885×785 logical points (see gotcha #3), not exactly 1024×768.

`docs/design/ux-modernization/current-state/linux/{dark,light}/` — 15 PNGs
per theme, 30 total: the same 12 from the original `ScreenshotTests` harness
`kPageMap` (listed above), plus 3 from round 2 (SSO-14981):
`apt_source_manager`, `docker`, `gnome_settings` — see "Round-2 capture" below
for their capture environment. The original 12 were captured as follows:

- Host: Luke's homelab (`media`, Ryzen 7 5700X, **Ubuntu 26.04 LTS** — note:
  the machine has been upgraded past 24.04), headless.
- Toolchain: GCC 15.2.0, CMake 4.2.3, distro Qt 6.10.2
  (`qt6-base-dev`/`qt6-charts-dev`/`qt6-svg-dev`/`qt6-tools-dev` were already
  installed — no packages were added).
- Method: throwaway clone at `/tmp/nexis-ux` of branch
  `claude/ux-modernization-spec`, `cmake --build build --target
  test-ScreenshotTests`, then `./scripts/update_screenshots.sh` — which is
  **generate mode** (`NEXIS_GENERATE_REFS=1`), not compare mode, so the stale
  `dashboard` baseline gotcha (#2 below) does not apply on this path. The
  script auto-selected `QT_QPA_PLATFORM=offscreen` (no
  DISPLAY/WAYLAND_DISPLAY on the host; SSO-3729/FW-02). All PNGs are exactly
  1024×768 at 1x device pixel ratio (offscreen QPA has no HiDPI scaling),
  unlike the 2x macOS harness output.
- The clone was deleted from the homelab after `scp`; nothing was committed
  from that machine and no `tests/reference_screenshots/` files in THIS repo
  were touched.

## Round-2 capture: APT Source Manager, Docker, GNOME Settings (SSO-14981)

These 3 Linux-only pages were deferred from round 1 (no live capture existed
anywhere for them — see the now-resolved "Pages with no live capture
anywhere" note above). Round 2 added them to the shared `kPageMap` in
`tests/screenshots/test_screenshots.cpp` (behind `#ifdef Q_OS_LINUX`, since
none of the three exist on macOS) and captured them via the
`screenshot-baselines.yml` CI workflow (`ubuntu:26.04` container +
`xvfb`/offscreen QPA), the same harness used for the original 12 Linux pages,
run in the same `NEXIS_GENERATE_REFS=1` generate mode via
`scripts/update_screenshots.sh`.

Each page is runtime-gated by a `ToolManager` check in `app.cpp`
(`checkSourceRepository()` / `checkDocker()` / `checkGnomeSettings()`) that
must pass for the page to be registered in `mSlidingStacked` at all — no live
GNOME session or running Docker daemon is required for the page itself to
construct and render, only the relevant CLI tool / schema. The CI job
installs `docker.io`, `libglib2.0-bin`, and `gsettings-desktop-schemas` so
all three checks pass on the container.

**Gotcha #2 accounted for.** Gotcha #2 (below) is specific to *compare mode*:
`QVERIFY2` aborts `captureAndCompare()`'s whole loop on the first mismatch
against a stale baseline. `scripts/update_screenshots.sh` runs in *generate*
mode (`NEXIS_GENERATE_REFS=1`), which has no baseline comparison and no
`QVERIFY2` abort path, so appending these 3 pages to the end of the shared
`kPageMap` could not truncate the other 12 pages' captures the way a
compare-mode run could. This was confirmed empirically, not just by reading
the code: CI run
[29695778365](https://github.com/s4solutionsllc/Nexis/actions/runs/29695778365)
completed successfully and produced all 15 pages' PNGs (both themes) in a
single pass — no isolated per-page run was needed for generate mode. (An
isolated-per-page harness variant was prototyped separately on
`claude/SSO-14981-round2-capture` for the case where a *future* page's
runtime check might crash/hang rather than cleanly fail — see that branch's
`NEXIS_SCREENSHOT_ONLY` gate if that scenario needs handling later — but it
was not required to get a clean capture here.)

**Gotcha #5 applies.** Same as the other 11 non-Dashboard Linux harness
captures, all 3 of these pages are captured via
`QStackedWidget::setCurrentWidget()`, not a real sidebar click, so their
committed PNGs show a stale "Dashboard" sidebar highlight. Consumers must
render the correct highlight (APT Source Manager / Docker / GNOME Settings
respectively) rather than copying the highlight shown in the PNG.

**Note — Docker daemon is live, not empty.** The `docker.io` package was
installed in the CI container only to satisfy `checkDocker()`'s CLI-presence
check; the assumption going in was that no daemon would be running and the
page would show an empty state. That assumption was wrong and was corrected
after actually inspecting the committed PNGs: GitHub Actions runners already
ship a live Docker daemon as part of the standard runner image (used by the
runner's own containerized steps), and `checkDocker()` connects to it like
any other client. `docker.png` therefore shows a fully populated, real
state — Docker daemon: Running (v28.0.4), 7 images including the runner's
own tooling/workflow images (e.g. `ghcr.io/github/gh-aw-firewall/agent`,
`ghcr.io/dependabot/dependabot-updater`) — not an empty daemon-not-running
placeholder. This is still an honest current-state capture (real pixels from
a real run), just not the empty state originally assumed; it's arguably more
useful for round-2 mockup work since it exercises the populated table/tabs
UI rather than an empty-state placeholder.

## Gotchas encountered (for whoever runs Task 2+ or re-runs this capture)

1. **Harness output path differs from the brief.** The brief's Step 1
   commands say output lands in `build/test_screenshots/macos/{dark,light}/`.
   The actual `ScreenshotTests::initTestCase()` sets
   `mOutDir = QDir::currentPath() + "/test_screenshots/" + mPlatform` where
   `QDir::currentPath()` is ctest's working directory
   (`build/tests/` for this CMake layout), so the real path is
   `build/tests/test_screenshots/macos/{dark,light}/`. Verify with
   `find build -type d -iname test_screenshots` if it moves again.

2. **A stale `dashboard` baseline silently truncates the capture set.**
   `captureAndCompare()` in `tests/screenshots/test_screenshots.cpp` loops
   over all 12 `kPageMap` entries, but on the first `QVERIFY2(cmp.passed, ...)`
   failure, Qt Test's macro does an early `return` out of
   `captureAndCompare()` — which aborts the whole loop, not just that one
   page. `dashboard` is first in `kPageMap` and its committed baseline is
   currently stale (differs by ~28-32% of unmasked pixels), so a normal
   `ctest -R ScreenshotTests` run only ever produces `dashboard.png` and
   silently skips the other 11 pages — with exit code 1, which `|| true`
   masks. **Workaround used here:** re-ran with
   `NEXIS_SCREENSHOT_TOLERANCE=100 ctest --test-dir build -R ScreenshotTests`.
   This only raises the pass/fail threshold read by `captureAndCompare()`
   (`qgetenv("NEXIS_SCREENSHOT_TOLERANCE")`) — it does not touch
   `tests/reference_screenshots/` or any file under `tests/`. All 12 pages
   compared "pass" under the relaxed tolerance and the loop ran to
   completion, producing all 12 PNGs per theme. `git status --short --
   shared/ linux/ macos/ tests/` was empty before and after. This is a real
   test-harness bug worth fixing separately (the `dashboard` baseline is
   stale and the abort-on-first-failure behavior means CI would have the
   same 1-page blind spot) — flagged, not fixed, per this task's
   read-only-on-`tests/` constraint.

3. **Manual captures required computer-use, not AppleScript/System Events.**
   `osascript -e 'tell application "System Events" to ...'` fails with
   "not allowed assistive access" (-1728) in this environment — Terminal
   has no Accessibility permission, and granting it is a system-settings
   change outside this task's scope. `screencapture -x` (full-screen, no
   accessibility needed) does work, but the physical desktop has the Claude
   Code chat window itself floating above Nexis at a fixed screen region
   (real x >= ~1800px on this 2880×1864 display), which a plain full-screen
   capture would include. Worked around by: (a) using the `computer-use`
   MCP tool for clicking/navigation/theme-switching (its screenshots
   compositor-filter to only the granted app, so it was used for driving
   the UI), (b) resizing/moving the Nexis window via `computer-use` drags to
   ~885×785 logical points positioned entirely left of the Claude window's
   region, then (c) `screencapture -x` for the actual pixel source (full
   desktop, Nexis window now unobstructed) followed by a Python/Pillow crop
   to the window's exact content bounds (crop box `(20, 120, 1790, 1690)`
   in real/physical pixels at 2x scale — i.e. logical `(10, 60, 895, 845)`).
   `computer-use`'s own `screenshot`/`zoom` `save_to_disk` output was not
   reachable from the Bash tool's filesystem (searched `/private/tmp`,
   `/var/folders`, `~/Library/Caches`, Spotlight — no hits), so it could
   only be used for on-screen verification/navigation, not as the PNG
   source.

4. **System Logs table background observation — transiently observed but not
   reproduced.** During the manual capture session, the log-entry table view
   appeared to have a dark background after switching to Light theme. However,
   this was NOT reproduced in the committed capture (`macos/light/system_logs.png`);
   pixel sampling confirms the table background is pure white. Unverified — do not act
   on this observation without reproducing it independently; excluded from
   prototype modernization rationale.

5. **Sidebar highlight artifact in harness-generated captures.** The 11
   harness-generated non-Dashboard macOS captures (all pages except dashboard,
   boot_analysis, disk_tools, system_logs, homebrew) have a stale sidebar
   highlight: it incorrectly remains on "Dashboard" because the screenshot
   harness switches pages via `QStackedWidget::setCurrentWidget()` rather than
   a real sidebar click. The 4 manually captured pages (boot_analysis,
   disk_tools, system_logs, homebrew) show the correct sidebar highlight for
   their respective pages. Consumers of these captures (prototype/mockup tasks)
   must render the CORRECT sidebar highlight for each page when using these
   images — do not copy the stale "Dashboard" highlight shown in the committed
   PNGs. **This applies equally to the 11 non-Dashboard Linux harness captures**
   (`linux/{dark,light}/*.png` except `dashboard.png`) — same harness, same
   `QStackedWidget::setCurrentWidget()` page-switching mechanism.

## Theme switching method

Settings → Appearance → Color Scheme dropdown (Auto / Light / Dark) was used
directly rather than editing `~/Library/Preferences/nexis/settings.ini`
between launches — the app was already open and driving the dropdown via
`computer-use` was simpler than relaunching. Confirmed via
`SettingManager::setColorScheme()` call path (dropdown → `AppManager::
updateStylesheet()`), same as the harness. Setting was restored to `Auto`
(its original value) before quitting the app at the end of this task.

## Toolchain constraint & render regeneration process (SSO-14459)

**Investigated 2026-07-18.** NexisMaintainer's execution sandbox cannot build
Nexis or capture its own screenshots:

- No `cmake`, `g++`, or `qmake`/`qmake6` on `PATH`, and no Qt6 packages
  visible to `pkg-config --list-all`.
- No root/`sudo`; `apt-get install` fails with "Permission denied" on the
  dpkg lock, so the agent cannot self-provision the missing toolchain either.
- This is a sandbox/workspace-image constraint (Paperclip adapter config),
  not something fixable from within a Nexis PR — provisioning a build
  toolchain would require a platform-level change to the execution-workspace
  image, outside NexisMaintainer's write access. Filed as infra follow-up if
  someone with adapter-config access wants to pursue it; not re-attempted
  here per-epic-item.

**Resolution: renders are captured by the maintainer during UAT, not
regenerated by the implementing agent.** This is already the de facto
pattern — round 2's Battery card (SSO-14298) shipped with the maintainer
supplying `current-state/macos/{dark,light}/hardware_info_battery.png` via
manual capture (see gotcha #3 above) — this section makes it the documented
default for every future `docs/design/ux-modernization/mockups/renders/*.png`
regeneration across the SSO-13723 epic, not a one-off:

1. The implementing agent (whoever ships the design-system PR) does not
   attempt to build Nexis or produce renders itself.
2. The implementing agent's PR description / Paperclip comment must include
   exact regeneration instructions for the maintainer: which page(s)
   changed, which theme(s), and — if the crop differs from the standard
   window bounds in "Manual capture method" (gotcha #3) above (logical
   `(10, 60, 895, 845)` / physical `(20, 120, 1790, 1690)` at 2x) — the
   specific crop box and window size to use.
3. The maintainer captures the render(s) as part of their normal UAT pass
   (they already run the built app interactively to sign off on each page),
   following the "Manual capture method" in gotcha #3: `computer-use`-driven
   navigation/theme-switch, `screencapture -x` for the pixel source, then a
   crop to content bounds.
4. The maintainer commits the resulting PNG(s) directly to
   `docs/design/ux-modernization/mockups/renders/`, referencing the
   design-system issue in the commit message.
5. If the maintainer's platform can't reach a given theme/page combination
   (e.g. a Linux-only page on a macOS UAT host), log the gap using the same
   `not-captured (reason)` convention as the Coverage table above, rather
   than leaving the render silently stale.

This closes the process gap flagged in SSO-14459 for the whole SSO-13723
UX-modernization epic — future epic page items should follow steps 1-5
above instead of assuming the implementing agent can self-capture renders.
