# FR-41: CI Screenshot Regression Tests — Implementation Plan

## Overview

Add automated screenshot capture and perceptual diff comparison to the CI pipeline. Each CI run will capture the 11 always-visible pages in both Dark and Light themes, compare them against committed reference images, and report visual regressions. This directly addresses BUG-30 (margin regressions) and BUG-40 (hardcoded colors invisible in dark mode).

**Scope:** 11 pages × 2 themes × 3 platforms = 66 reference images, one new test executable, CI workflow changes, and a reference image update script.

---

## Phase 1 — Screenshot Test Executable

Create the QTest-based executable that captures all pages in both themes and compares against reference images.

### Task 1.1 — Create screenshot test source file
- [ ] Create `tests/screenshots/test_screenshots.cpp`
- [ ] Use `QTEST_MAIN(ScreenshotTests)` pattern with `#include "test_screenshots.moc"`
- [ ] Define the `ScreenshotTests` QTest class with:
  - `initTestCase()` — creates `QApplication`, instantiates `App`, calls `App::init()`, waits for theme to apply
  - `cleanupTestCase()` — cleans up the App instance
  - `capturePage(int pageIndex, const QString &pageName)` — helper that uses `QWidget::grab()` to capture a page screenshot
  - `comparePage(const QString &pageName, const QString &theme)` — helper that loads the reference image and compares with captured
  - Theme iteration: for each page, capture in Dark then switch to Light and capture again

**Key implementation details:**
- Set window to fixed 1024×768 before capture: `app->resize(1024, 768)`
- Navigate pages using `SlidingStackedWidget::setCurrentWidget(page)` (no animation, instant)
- Process events after each page switch: `QApplication::processEvents()` and small `QTest::qWait(100)` for layout
- Switch themes programmatically:
  ```cpp
  SettingManager::ins()->setColorScheme("dark");
  AppManager::ins()->updateStylesheet();
  QApplication::processEvents();
  QTest::qWait(200); // allow repaint
  ```
- Save captured screenshots to `build/test_screenshots/{platform}/{theme}/{pagename}.png`

**Page name mapping (11 pages):**

| Index | Page Class | Screenshot Name |
|-------|-----------|----------------|
| 0 | DashboardPage | `dashboard` |
| 1 | HardwareInfoPage | `hardware_info` |
| 2 | StartupAppsPage | `startup_apps` |
| 3 | SystemCleanerPage | `system_cleaner` |
| 4 | SearchPage | `search` |
| 5 | ServicesPage | `services` |
| 6 | ProcessesPage | `processes` |
| 7 | UninstallerPage | `uninstaller` |
| 8 | ResourcesPage | `resources` |
| 9 | HelpersPage | `helpers` |
| 10 | SettingsPage | `settings` |

> Note: Indices 5–10 may shift if conditional pages (APT/Homebrew, Docker, GNOME Settings) are inserted before them. The test will use a fixed map of page class names to screenshot names, iterating `mSlidingStacked->widget(i)` and checking each widget's `metaObject()->className()` to determine the screenshot name. Conditional pages will be skipped.

**Acceptance criteria:**
- Test executable builds and runs on macOS locally
- Captures 22 PNG files (11 pages × 2 themes) to the output directory
- Each PNG is 1024×768 and contains rendered UI (not blank/black)

### Task 1.2 — Implement pixel comparison with tolerance
- [ ] Add `imagesMatch()` helper function in the test file:
  ```cpp
  struct CompareResult {
      bool passed;
      double diffPercent;
      int diffPixels;
      int totalPixels;
      QImage diffImage; // visual diff for debugging
  };

  CompareResult compareImages(const QImage &actual, const QImage &reference, double tolerancePercent);
  ```
- [ ] Compare logic: iterate all pixels, count mismatches, compute percentage
- [ ] Generate a diff image highlighting changed pixels in red (for CI artifact upload on failure)
- [ ] Default tolerance: 1.0% (configurable via environment variable `NEXIS_SCREENSHOT_TOLERANCE`)
- [ ] On mismatch: save the actual image, reference image, and diff image side-by-side to `build/test_screenshots/failures/`

**Acceptance criteria:**
- `imagesMatch()` returns true for identical images
- Returns false with correct diff percentage for images with differences beyond tolerance
- Diff image clearly highlights changed pixels

### Task 1.3 — Platform detection for reference image paths
- [ ] Detect platform at runtime to select correct reference image directory:
  ```cpp
  QString platformDir() {
      #ifdef Q_OS_MACOS
      return "macos";
      #else
      return "linux";
      #endif
  }
  ```
- [ ] Reference images loaded from: `{PROJECT_SOURCE_DIR}/tests/reference_screenshots/{platform}/{theme}/{pagename}.png`
- [ ] The `PROJECT_SOURCE_DIR` compile definition is already provided by the `add_nexis_test()` macro
- [ ] If reference image does not exist, skip comparison and log a warning (don't fail — allows first-run generation)

**Acceptance criteria:**
- Test loads correct platform-specific reference images
- Missing reference images produce warnings, not failures

### Task 1.4 — Generate mode (reference image capture)
- [ ] Support a `--generate` command-line argument or `NEXIS_GENERATE_REFS=1` environment variable
- [ ] In generate mode: capture all pages and save directly to `tests/reference_screenshots/{platform}/{theme}/` (overwriting existing references)
- [ ] Skip comparison entirely in generate mode — just capture and save
- [ ] Print summary: "Generated 22 reference screenshots for {platform}"

**Acceptance criteria:**
- Running with `NEXIS_GENERATE_REFS=1` creates/overwrites reference images in the correct directory structure
- Reference images are valid PNGs at 1024×768

---

## Phase 2 — CMake Integration

### Task 2.1 — Register screenshot test in CMakeLists.txt
- [ ] Add new test registration in `tests/CMakeLists.txt`:
  ```cmake
  # ── Screenshot regression tests ─────────────────────────────────────────────
  add_nexis_test(NAME ScreenshotTests
    SOURCES
      screenshots/test_screenshots.cpp
      # Need to compile App and all page sources since we instantiate the full app
    LIBS
      nexis-app  # Link against the GUI target for App, all pages, managers
      Qt6::Widgets
      Qt6::Charts
      Qt6::Svg
    INCLUDES
      "${CMAKE_SOURCE_DIR}/shared/nexis"
      "${CMAKE_SOURCE_DIR}/shared/nexis/Pages"
  )
  ```
- [ ] Verify the `nexis-app` (or equivalent GUI library target) exposes the necessary symbols — if not, may need to compile page sources directly (like ScheduleTests does for its manager sources)
- [ ] Alternative: if the app is built as an executable (not a library), create a static library target in the main `CMakeLists.txt` containing all GUI sources, and link the screenshot test against it

**Acceptance criteria:**
- `cmake -B build` succeeds with the new test registered
- `cmake --build build` compiles the screenshot test executable
- `ctest --test-dir build` lists `ScreenshotTests` in the test list

### Task 2.2 — Create reference screenshot directory structure
- [ ] Create the directory tree:
  ```
  tests/reference_screenshots/
    linux/
      dark/.gitkeep
      light/.gitkeep
    macos/
      dark/.gitkeep
      light/.gitkeep
  ```
- [ ] Add `.gitkeep` files so the empty directories are tracked

**Acceptance criteria:**
- Directory structure exists and is committed
- `.gitkeep` files present in all leaf directories

---

## Phase 3 — CI Workflow Changes

### Task 3.1 — Add Xvfb and ImageMagick to Linux CI dependencies
- [ ] In `.github/workflows/build.yml`, add to the Linux dependency install step:
  ```yaml
  xvfb \
  imagemagick
  ```
  (These are appended to the existing `apt-get install` list)

**Acceptance criteria:**
- `xvfb` and `imagemagick` are installed on Linux CI runners
- Existing build/test steps still pass

### Task 3.2 — Wrap test step with Xvfb on Linux
- [ ] Modify the `Test` step in `build.yml` to use `xvfb-run` on Linux:
  ```yaml
  - name: Test
    run: |
      if [ "$RUNNER_OS" = "Linux" ]; then
        xvfb-run -a ctest --test-dir build --output-on-failure
      else
        ctest --test-dir build --output-on-failure
      fi
  ```
- [ ] The `-a` flag auto-selects an available display number, avoiding conflicts

**Acceptance criteria:**
- Existing unit tests still pass on all 3 platforms
- Screenshot test can render GUI widgets on Linux CI (headless)
- macOS tests run without Xvfb (not needed)

### Task 3.3 — Upload failure artifacts on screenshot mismatch
- [ ] Add a new step after `Test` that uploads diff images when screenshot tests fail:
  ```yaml
  - name: Upload screenshot diffs
    if: failure()
    uses: actions/upload-artifact@v4
    with:
      name: screenshot-diffs-${{ matrix.name }}
      path: build/test_screenshots/failures/
      retention-days: 14
      if-no-files-found: ignore
  ```

**Acceptance criteria:**
- On screenshot mismatch, diff images are uploaded as CI artifacts
- On success (no mismatches), the step is silently skipped
- Artifact contains actual/reference/diff images for failed comparisons

### Task 3.4 — Make screenshot tests non-blocking initially
- [ ] Set the screenshot test to not fail the CTest run by adding `PROPERTIES WILL_FAIL FALSE` or by using `continue-on-error: true` at the CI step level
- [ ] Alternative approach: separate the test step into two:
  ```yaml
  - name: Unit Tests
    run: |
      if [ "$RUNNER_OS" = "Linux" ]; then
        xvfb-run -a ctest --test-dir build --output-on-failure -E ScreenshotTests
      else
        ctest --test-dir build --output-on-failure -E ScreenshotTests
      fi

  - name: Screenshot Tests
    continue-on-error: true
    run: |
      if [ "$RUNNER_OS" = "Linux" ]; then
        xvfb-run -a ctest --test-dir build --output-on-failure -R ScreenshotTests
      else
        ctest --test-dir build --output-on-failure -R ScreenshotTests
      fi
  ```
- [ ] This keeps unit test failures blocking while screenshot failures are informational until references stabilize

**Acceptance criteria:**
- Unit test failures still block the CI build
- Screenshot test failures produce warnings and upload artifacts but do not block the build
- Once references are stable, the `continue-on-error` can be removed to make screenshot tests blocking

---

## Phase 4 — Generate Initial Reference Images

### Task 4.1 — Generate macOS reference images locally
- [ ] Build the project locally on macOS
- [ ] Run: `NEXIS_GENERATE_REFS=1 ./build/test-ScreenshotTests`
- [ ] Verify 22 images generated in `tests/reference_screenshots/macos/{dark,light}/`
- [ ] Visually inspect each screenshot to confirm correct rendering
- [ ] Commit the reference images

### Task 4.2 — Generate Linux reference images via CI
- [ ] Temporarily modify CI to run screenshot tests in generate mode:
  - Set `NEXIS_GENERATE_REFS=1` environment variable
  - Upload generated reference images as CI artifacts
- [ ] Download the Linux artifacts and commit them to `tests/reference_screenshots/linux/{dark,light}/`
- [ ] Revert the CI to comparison mode (remove `NEXIS_GENERATE_REFS=1`)

**Alternative approach:** If the Linux CI environment matches a local Linux setup (Ubuntu 24.04 + Qt 6), generate locally instead.

**Acceptance criteria:**
- 22 reference images exist for each platform (44 macOS + 22 Linux = 66 total, though Linux x86_64 and ARM64 may need separate sets)
- All images are 1024×768, non-blank, correctly themed
- Images are committed to the repository

### Task 4.3 — Validate Linux architecture consistency
- [ ] Determine if Linux x86_64 and ARM64 produce identical screenshots (same Qt rendering)
- [ ] If font/rendering differs between architectures, create separate reference sets:
  ```
  tests/reference_screenshots/
    linux-x86_64/
    linux-arm64/
    macos/
  ```
- [ ] Update platform detection in test code to distinguish architectures on Linux

**Acceptance criteria:**
- Reference images match the CI environment for each platform/architecture combination
- No false positives from architecture-specific rendering differences

---

## Phase 5 — Reference Image Update Script

### Task 5.1 — Create update script
- [ ] Create `scripts/update_screenshots.sh`:
  ```bash
  #!/bin/bash
  # Regenerate reference screenshots for the current platform.
  # Usage: ./scripts/update_screenshots.sh
  #
  # Must be run after building: cmake --build build

  set -euo pipefail

  NEXIS_GENERATE_REFS=1 ./build/test-ScreenshotTests

  echo "Reference screenshots updated. Review changes with:"
  echo "  git diff --stat tests/reference_screenshots/"
  echo ""
  echo "If the changes look correct, commit them:"
  echo "  git add tests/reference_screenshots/"
  echo "  git commit -m 'chore: update reference screenshots'"
  ```
- [ ] Make the script executable: `chmod +x scripts/update_screenshots.sh`
- [ ] For Linux with Xvfb: detect and use `xvfb-run` if no display is available

**Acceptance criteria:**
- Running `./scripts/update_screenshots.sh` regenerates all reference images for the current platform
- Script provides clear instructions for reviewing and committing changes

---

## Phase 6 — Documentation & Tracking Updates

### Task 6.1 — Update FEATURE_REQUESTS.md
- [ ] Mark FR-41 as `[x]` done with resolution summary

### Task 6.2 — Update docs/APPLICATION_OVERVIEW.md
- [ ] Add screenshot regression tests to the testing section
- [ ] Update test counts (executables and methods)
- [ ] Document the reference image directory structure

### Task 6.3 — Update docs/ARCHITECTURE_REVIEW.md
- [ ] Note the new CI testing capability under the testing section
- [ ] Update any test infrastructure descriptions

### Task 6.4 — Update IMPLEMENTATION_ROADMAP.md
- [ ] Mark relevant FR-41 tasks as complete in the roadmap

### Task 6.5 — Archive research and plan files
- [ ] Move `claude_definitions/FR-41_research.md` and `claude_definitions/FR-41_plan.md` to `claude_definitions/Archive/`

---

## Build Verification Steps

After each phase:
1. `cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6)` — configure
2. `cmake --build build -j$(sysctl -n hw.ncpu)` — build
3. `ctest --test-dir build --output-on-failure` — run all tests (existing + new)

After Phase 4:
4. `NEXIS_GENERATE_REFS=1 ./build/test-ScreenshotTests` — generate reference images
5. `./build/test-ScreenshotTests` — run comparison against generated references (should pass with 0% diff)

---

## Technical Decisions Summary

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Headless rendering (Linux) | Xvfb | Most faithful Qt rendering; proven for CI |
| Image comparison | Qt-native with tolerance | No external dependency for comparison logic; ImageMagick only as optional CI tool |
| Reference storage | In-repo (`tests/reference_screenshots/`) | Simple, ~5-6 MB acceptable |
| Window size | 1024×768 fixed | Consistent across runs |
| Diff tolerance | 1.0% (configurable) | Accommodates subpixel font rendering variance |
| Pages to test | 11 always-visible | Skip conditional pages for now (platform-dependent) |
| Theme coverage | Dark + Light | Catches BUG-40-class regressions |
| Initial CI mode | Non-blocking (continue-on-error) | Until references stabilize |
| Test approach | Full App instantiation | Most realistic rendering; reuses existing init path |

---

## Risk Mitigations

- **Font rendering variance:** 1% tolerance with configurable override via env var
- **Stale references:** Easy update script + non-blocking CI initially
- **Architecture differences:** Separate reference sets per platform/arch if needed
- **CI time impact:** ~10-15s per platform (minimal overhead)
- **Conditional page shifts:** Use class name mapping, not index-based page identification
