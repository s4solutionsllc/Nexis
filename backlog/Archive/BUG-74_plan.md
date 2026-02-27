# BUG-74 Plan: Fix ScreenshotTests SEGFAULT

## Task 1: Add `Q_INIT_RESOURCE(static)` to the test binary

- [x]**1a.** In `tests/screenshots/test_screenshots.cpp`, replace the `QTEST_MAIN(ScreenshotTests)` macro (line 267) with a hand-written `main()` that calls `Q_INIT_RESOURCE(static)` before creating `QApplication`, then sets the Fusion style and executes the test:
  ```cpp
  int main(int argc, char *argv[])
  {
      Q_INIT_RESOURCE(static);
      QApplication app(argc, argv);
      app.setStyle(QStyleFactory::create("Fusion"));
      ScreenshotTests tc;
      return QTest::qExec(&tc, argc, argv);
  }
  ```
- [x]**1b.** Add `#include <QStyleFactory>` to the includes if not already present.
- [x]**1c.** Build incrementally and verify the test binary starts without `Cannot open file ':/static/...'` warnings.

**Acceptance:** Running the test shows the app with full theming (icons, backgrounds, styled widgets) instead of a black window. Resource warnings gone.

## Task 2: Force DPI-independent captures

- [x]**2a.** Set `QT_SCALE_FACTOR=1` in the test's CMake definition via `set_tests_properties(ScreenshotTests PROPERTIES ENVIRONMENT "QT_SCALE_FACTOR=1")` in `tests/CMakeLists.txt`. This forces 1x rendering regardless of display, making captures reproducible across machines.

**Acceptance:** Captured screenshots have consistent dimensions regardless of display DPI.

## Task 3: Regenerate reference screenshots

- [x]**3a.** Run the test with `NEXIS_GENERATE_REFS=1 QT_SCALE_FACTOR=1` to generate correct reference images.
- [x]**3b.** Verify the generated references show a fully-rendered app (not a black window).
- [x]**3c.** Copy/commit the new references into `tests/reference_screenshots/macos/`.

**Acceptance:** Reference images show correct themed UI for all 11 pages in both dark and light themes.

## Task 4: Verify full test pass

- [x]**4a.** Run `ctest --test-dir build --output-on-failure -R ScreenshotTests` and confirm it passes (no SEGFAULT, no mismatches beyond tolerance).
- [x]**4b.** Run full test suite `ctest --test-dir build --output-on-failure` and confirm 7/7 pass.

**Acceptance:** All tests pass. No SEGFAULT.

## Task 5: Finalize

- [x]**5a.** Update `BUGS.md` — mark BUG-74 as `[x]` with resolution notes.
- [x]**5b.** Commit and push.
- [x]**5c.** Move `backlog/BUG-74_research.md` and `backlog/BUG-74_plan.md` to `backlog/Archive/`.
