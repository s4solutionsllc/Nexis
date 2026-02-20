# FR-41: CI Screenshot Regression Tests — Research

## Overview

FR-41 proposes automated screenshot capture and perceptual diff comparison in CI to catch visual regressions that unit tests cannot detect. This was motivated by BUG-30 (margin regressions across 10 `.ui` files caught only by manual QA) and BUG-40 (hardcoded colors invisible in dark mode, caught only by manual testing).

---

## Current CI Pipeline (`.github/workflows/build.yml`)

**3-platform matrix:**

| Runner | Platform | Architecture |
|--------|----------|-------------|
| `ubuntu-24.04` | Linux | x86_64 |
| `ubuntu-24.04-arm` | Linux | ARM64 |
| `macos-14` | macOS | ARM64 (Apple Silicon) |

**Build steps:** Checkout → Install deps → Configure (cmake) → Build → Test (ctest) → Upload artifacts

**Current test step (line 69-70):**
```yaml
- name: Test
  run: ctest --test-dir build --output-on-failure
```

**Linux CI dependencies (already installed):**
`cmake`, `g++`, `libgl1-mesa-dev`, `qt6-base-dev`, `qt6-charts-dev`, `qt6-svg-dev`, `qt6-tools-dev`, `qt6-l10n-tools`

**Not installed (needed for FR-41):**
- `xvfb` — X Virtual Framebuffer for headless GUI rendering on Linux
- `imagemagick` — `compare` tool for perceptual image diff

**macOS:** Has virtual Cocoa display on CI runners — no additional setup needed for GUI rendering.

---

## Current Test Infrastructure

**Framework:** Qt Test (QTest) + CTest, established in FR-33/FR-36.

**Test structure:**
```
tests/
  CMakeLists.txt          # add_nexis_test() macro, per-file executables
  utils/                  # FormatUtil, FileUtil, CommandUtil tests
  core/                   # DiskHealthInfo tests
  managers/               # ScheduleManager tests
  theme/                  # Theme token validation tests
```

**Current:** 6 CTest executables, 63 test methods, all passing.

**CMake macro pattern:**
```cmake
add_nexis_test(NAME FormatUtilTests SOURCES utils/test_format_util.cpp)
```

Each test uses `QTEST_MAIN(TestClass)` and includes its own `.moc` — one test class per executable.

**DI support (FR-35):** All 10 page constructors accept optional manager pointers (`nullptr` → `::ins()` fallback). This means screenshot tests could inject mock managers for reproducible hardware data.

---

## Page Construction & Layout

### 14 Pages (11 always-visible + 3 conditional)

**Always visible:**
1. Dashboard (`DashboardPage`)
2. Hardware Info (`HardwareInfoPage`)
3. Startup Apps (`StartupAppsPage`)
4. System Cleaner (`SystemCleanerPage`)
5. Search (`SearchPage`)
6. Services (`ServicesPage`)
7. Processes (`ProcessesPage`)
8. Uninstaller (`UninstallerPage`)
9. Resources (`ResourcesPage`)
10. Helpers (`HelpersPage`)
11. Settings (`SettingsPage`)

**Conditional:**
12. APT Source Manager / Homebrew (`APTSourceManagerPage`) — if APT or Homebrew detected
13. Docker (`DockerPage`) — if `docker` binary found
14. GNOME Settings (`GnomeSettingsPage`) — if `gsettings` available (Linux only)

### Conditional Widget Visibility Within Pages

Dashboard hides gauges when hardware is absent:
```cpp
if (im->hasGpu()) { ui->gpuContainer->show(); }
else { ui->gpuContainer->hide(); mGpuBar->hide(); }

if (im->hasBattery()) { ui->batteryContainer->show(); }
else { ui->batteryContainer->hide(); mBatteryBar->hide(); }

if (im->hasDiskHealth()) { ui->circleBarsLayout->addWidget(mDiskHealthBar); }
else { mDiskHealthBar->hide(); }
```

**Impact:** On CI (no real GPU, battery, etc.), many Dashboard gauges will be hidden. Reference images must match the CI hardware environment exactly.

### Initialization Order in `App::init()`

1. AppManager created
2. 11 base pages instantiated (no data yet)
3. Conditional pages checked and added
4. Pages added to `SlidingStackedWidget`
5. `DataRefreshService::ins()->start()` — fires immediate ticks, populating data
6. `AppManager::ins()->updateStylesheet()` — applies QSS theme
7. Start page displayed

**For screenshots:** Must be taken AFTER step 6 (theme applied) and ideally after step 5 (data populated).

### SlidingStackedWidget

Custom `QStackedWidget` subclass with slide animations. Pages are shown via `slideInIdx(index)`. For screenshot capture, need to set the current index and wait for any animation to complete (or bypass animations).

---

## Theme System

**Three themes:** Dark (`default`), Light (`light`), Auto (system preference)

**Mechanism:** Single `style.qss` template with `@token` placeholders → replaced at runtime from `values.ini` → applied via `qApp->setStyleSheet()`.

**For screenshot tests:** Should capture in both Dark and Light themes to catch BUG-40-class regressions (hardcoded colors invisible in certain themes).

**Token validation (FR-32):** Already validates that all `@tokens` resolve and colors are valid hex. But this doesn't catch hardcoded inline `setStyleSheet()` calls — which is exactly what screenshot tests would catch.

---

## UI File Inventory

27 `.ui` files across 14 pages:

| Page | Count | Files |
|------|-------|-------|
| Dashboard | 3 | `dashboard_page.ui`, `circlebar.ui`, `linebar.ui` |
| Hardware Info | 1 | `hardware_info_page.ui` |
| Startup Apps | 3 | `startup_apps_page.ui`, `startup_app.ui`, `startup_app_edit.ui` |
| System Cleaner | 1 | `system_cleaner_page.ui` |
| Search | 1 | `search_page.ui` |
| Services | 2 | `services_page.ui`, `service_item.ui` |
| Processes | 1 | `processes_page.ui` |
| Uninstaller | 1 | `uninstallerpage.ui` |
| Resources | 2 | `resources_page.ui`, `history_chart.ui` |
| Helpers | 2 | `helpers_page.ui`, `host_manage.ui` |
| APT Source Mgr | 3 | `apt_source_manager_page.ui`, `apt_source_repository_item.ui`, `apt_source_edit.ui` |
| Docker | 1 | `docker_page.ui` |
| GNOME Settings | 5 | `gnome_settings_page.ui` + 4 tab `.ui` files |
| Settings | 1 | `settings_page.ui` |

---

## Headless Rendering

### Linux CI Runners (Headless — no X11 display)

**Option A: Xvfb (recommended)**
- Install: `sudo apt-get install -y xvfb`
- Usage: `xvfb-run -a ./test-executable`
- Provides full X11 rendering pipeline — most faithful to actual visual appearance
- Proven approach for Qt GUI testing in CI

**Option B: `QT_QPA_PLATFORM=offscreen`**
- No installation needed — set environment variable
- Minimal rendering — some QWidget features may not render correctly
- Faster but less faithful

**Option C: `QT_QPA_PLATFORM=minimal`**
- Even more stripped down than offscreen
- Not suitable for visual testing

**Recommendation:** Use Xvfb for faithful rendering.

### macOS CI Runners

Virtual Cocoa display available out of the box. No special setup needed. Qt 6 renders natively to Cocoa subsystem. `QWidget::grab()` works directly.

---

## Image Comparison Approaches

### ImageMagick `compare` (External tool)

```bash
compare -metric AE reference.png actual.png diff.png
# Returns number of differing pixels as exit code / output
# AE = Absolute Error (pixel count)

compare -metric PHASH reference.png actual.png diff.png
# PHASH = Perceptual Hash (more tolerant of subpixel differences)

compare -fuzz 5% reference.png actual.png diff.png
# Allows 5% color variance per pixel
```

**Pros:** Industry standard, configurable sensitivity, generates visual diff images
**Cons:** External dependency, must install in CI

### Qt-native `QImage::operator==` (Built-in)

```cpp
QImage reference("reference.png");
QImage actual = widget->grab().toImage();
QCOMPARE(actual, reference);
```

**Pros:** No external dependencies, integrates with QTest
**Cons:** Pixel-perfect only (fails on subpixel font rendering differences), no perceptual tolerance, no diff image output

### Hybrid approach

Use Qt `QImage` for capture, then a custom pixel-comparison function with configurable tolerance:

```cpp
bool imagesMatch(const QImage &a, const QImage &b, double tolerancePercent) {
    if (a.size() != b.size()) return false;
    int totalPixels = a.width() * a.height();
    int diffPixels = 0;
    for (int y = 0; y < a.height(); ++y)
        for (int x = 0; x < a.width(); ++x)
            if (a.pixel(x, y) != b.pixel(x, y)) ++diffPixels;
    return (double(diffPixels) / totalPixels * 100.0) <= tolerancePercent;
}
```

**Pros:** No external dependencies, configurable tolerance
**Cons:** No visual diff output, simple pixel comparison (no perceptual weighting)

---

## Reference Image Strategy

### Storage Options

1. **In-repo (tracked by git)** — Simple, but binary PNGs inflate repo size. ~14 pages × 2 themes × 2 platforms ≈ 56 images × ~100KB each ≈ 5-6 MB.
2. **Git LFS** — Keeps repo clone light; requires LFS setup.
3. **CI artifact download** — Capture baseline in a special "generate references" CI run; store as release artifact. Complex workflow.

**Recommendation:** In-repo in `tests/reference_screenshots/`. 5-6 MB is acceptable for a repo of this size. Update images by re-running a generation script and committing the new PNGs.

### Reference Image Sets

Need separate sets per platform due to font rendering differences:

```
tests/reference_screenshots/
  linux/
    dark/
      dashboard.png
      hardware_info.png
      ...
    light/
      dashboard.png
      ...
  macos/
    dark/
      dashboard.png
      ...
    light/
      ...
```

### Generating References

A dedicated test mode or script that captures all pages and saves them as reference images. Run once per platform when the UI is known-good, then commit the PNGs.

---

## Design Decisions

### 1. Which pages to screenshot?

**Recommended:** All 11 always-visible pages in both Dark and Light themes. Skip conditional pages (Docker, GNOME Settings, APT/Homebrew) initially — they add platform variance without high regression risk.

### 2. Mock hardware or use real detection?

**Recommended:** Use real CI hardware detection (which means most optional gauges will be hidden). This matches the actual CI environment and avoids mock complexity. Reference images are captured in the same CI environment, so they'll match.

The alternative — mocking InfoManager to return fixture data — would give richer screenshots but requires significant setup and creates a test environment that doesn't match reality.

### 3. Window size?

**Recommended:** Fixed 1024×768 window size for all screenshots. Set programmatically before capture to ensure consistency.

### 4. Diff threshold?

**Recommended:** Start with 1% pixel difference tolerance (allows for minor subpixel font rendering variations). Adjust based on experience. Use ImageMagick `compare -fuzz 2% -metric AE` for the actual comparison.

### 5. CI integration?

**Recommended:**
- New CTest executable: `test-ScreenshotTests`
- On failure: upload diff images as CI artifact for manual review
- Initially informational (warnings only, don't fail the build) until reference images stabilize

### 6. Test execution approach?

**Option A: Full App instantiation** — Create a real `App` instance, navigate to each page, capture.
- Pros: Most realistic rendering (theme, data, layout all real)
- Cons: Heavy, slow, requires full app context, flaky if data varies

**Option B: Isolated page widgets** — Instantiate each page as a standalone `QWidget`, apply theme manually.
- Pros: Lightweight, faster, more isolated
- Cons: Missing app-level context (sidebar, title bar), may not match real rendering

**Option C: Headless app with page navigation** — Start the app headlessly, programmatically navigate to each page, capture.
- Pros: Realistic but controllable
- Cons: Requires app to support headless screenshot mode

**Recommended:** Option A or C for maximum fidelity. The `App::init()` path already constructs all pages — we can reuse it.

---

## Motivating Bugs

### BUG-30: Margin regressions across `.ui` files
- Phase 2 margin standardization changed values in 10 `.ui` files
- Regressions (clipped text, excessive whitespace) caught only by manual QA
- All changes were reverted
- Screenshot tests would have immediately flagged layout changes

### BUG-40: Hardcoded colors invisible in dark mode
- FR-16 (Scheduled Cleaning) widgets used inline `setStyleSheet("color: gray")` instead of `@token` system
- Text invisible in certain theme combinations
- Caught only by manual testing after feature was merged
- Dark/Light screenshot comparison would have immediately shown the issue

---

## Existing Code That Can Be Reused

- **`add_nexis_test()` CMake macro** — in `tests/CMakeLists.txt` — for registering the screenshot test executable
- **`AppManager::updateStylesheet()`** — for applying themes before capture
- **`SlidingStackedWidget::setCurrentIndex()`** — for navigating to each page
- **FR-35 DI constructors** — for potentially injecting controlled managers
- **`QWidget::grab()`** — Qt built-in for capturing widget to QPixmap
- **QTest framework** — `QTEST_MAIN`, `QVERIFY`, `QCOMPARE` for test assertions

---

## Risks & Mitigations

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| Font rendering varies between CI runs | Medium | Medium | Use `fuzz` tolerance in comparison |
| Reference images become stale quickly | High | Low | Provide easy update script; don't fail build initially |
| Xvfb rendering differs from real display | Low | Medium | Xvfb is well-proven for Qt CI testing |
| Conditional widgets change layout per run | Medium | High | Use real CI detection consistently; separate refs per platform |
| CI time increase | Medium | Low | Screenshot test adds ~10-15s per platform |
| Large binary files in repo | Low | Low | ~5-6 MB total; acceptable |

---

## Summary

FR-41 is technically feasible with the existing infrastructure. The main work involves:

1. **New test executable** — `tests/screenshots/test_screenshots.cpp` using QTest
2. **CI changes** — Add `xvfb` and `imagemagick` to Linux dependencies; wrap test step with `xvfb-run`
3. **Reference images** — Capture baseline PNGs for 11 pages × 2 themes × 2 platforms
4. **Comparison logic** — Either ImageMagick `compare` or custom Qt-native pixel diff with tolerance
5. **Update script** — Convenience script to regenerate reference images when UI intentionally changes

The feature directly addresses the gap identified by BUG-30 and BUG-40: catching visual regressions that pass all unit tests but break the user-visible UI.
