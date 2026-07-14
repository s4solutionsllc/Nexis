# Current-State Capture Notes

Evidence pack for the UX modernization effort (see
`docs/superpowers/specs/2026-07-13-ux-modernization-design.md` and
`docs/superpowers/plans/2026-07-13-ux-modernization-planning.md`). This file
tracks "before" screenshot coverage across all 19 tracked rows (18 prototype
pages + the Dashboard reference) × platform.

## Coverage table

| # | Page | macOS | Linux |
|---|------|-------|-------|
| 1 | Dashboard (reference) | captured — `macos/{dark,light}/dashboard.png` (harness) | pending-linux |
| 2 | Processes | captured — `macos/{dark,light}/processes.png` (harness) | pending-linux |
| 3 | Search | captured — `macos/{dark,light}/search.png` (harness) | pending-linux |
| 4 | System Logs | captured — `macos/{dark,light}/system_logs.png` (manual) | pending-linux |
| 5 | Boot Analysis | captured — `macos/{dark,light}/boot_analysis.png` (manual) | pending-linux |
| 6 | Hardware Info | captured — `macos/{dark,light}/hardware_info.png` (harness) | pending-linux |
| 7 | Uninstaller (sidebar label: "Applications") | captured — `macos/{dark,light}/uninstaller.png` (harness) | pending-linux |
| 8 | Services | captured — `macos/{dark,light}/services.png` (harness) | pending-linux |
| 9 | Startup Apps | captured — `macos/{dark,light}/startup_apps.png` (harness) | pending-linux |
| 10 | APT Source Manager | not-applicable (Linux-only page; the macOS build compiles `HomebrewPage` in this sidebar slot instead — see row 15) | pending-linux |
| 11 | System Cleaner | captured — `macos/{dark,light}/system_cleaner.png` (harness) | pending-linux |
| 12 | Disk Tools | captured — `macos/{dark,light}/disk_tools.png` (manual) | pending-linux |
| 13 | Docker | not-capturable (macOS): Docker is not installed on the capture machine, so `ToolManager::checkDocker()` returns false and `app.cpp` hides the sidebar button at runtime — the page is compiled but not reachable via UI. Re-capture once Docker is available, or capture on a host that has it installed. | pending-linux |
| 14 | Helpers | captured — `macos/{dark,light}/helpers.png` (harness) | pending-linux |
| 15 | Homebrew | captured — `macos/{dark,light}/homebrew.png` (manual) | not-applicable (macOS-only page; the Linux build compiles `APTSourceManagerPage` in this sidebar slot instead — see row 10) |
| 16 | Resources | captured — `macos/{dark,light}/resources.png` (harness) | pending-linux |
| 17 | Network Usage | captured — `macos/{dark,light}/network_usage.png` (harness) | pending-linux |
| 18 | Settings | captured — `macos/{dark,light}/settings.png` (harness) | pending-linux |
| 19 | GNOME Settings | not-applicable (Linux-only page; does not exist in the macOS build) | pending-linux |

macOS coverage: 16 of 18 prototype pages captured in both themes, plus the
Dashboard reference (17/19 rows populated). The 2 gaps are both
platform/runtime scoping, not capture failures: Docker (not installed on this
machine) and GNOME Settings (Linux-only, doesn't exist on macOS).

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

4. **System Logs table keeps a dark background in Light theme.** Observed,
   not fixed: on the System Logs page, the log-entry table view still
   renders with a dark background/row colors after switching Color Scheme
   to Light (visible in `macos/light/system_logs.png`), while the rest of
   the page chrome (toolbar, dropdowns) is light. Logged here as a candidate
   real UX finding for the modernization pass, not addressed in this
   capture-only task.

## Theme switching method

Settings → Appearance → Color Scheme dropdown (Auto / Light / Dark) was used
directly rather than editing `~/Library/Preferences/nexis/settings.ini`
between launches — the app was already open and driving the dropdown via
`computer-use` was simpler than relaunching. Confirmed via
`SettingManager::setColorScheme()` call path (dropdown → `AppManager::
updateStylesheet()`), same as the harness. Setting was restored to `Auto`
(its original value) before quitting the app at the end of this task.
