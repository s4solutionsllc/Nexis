# BUG-74 Research: ScreenshotTests SEGFAULT

## Summary

The `ScreenshotTests` test binary (`test-ScreenshotTests`) crashes with SIGSEGV every run. Three compounding issues cause the failure:

1. **Missing `Q_INIT_RESOURCE(static)` in the test binary** — the root cause
2. **Broken reference screenshots** — generated with the same broken binary
3. **Image size mismatch** — display-dependent capture sizes vs. fixed-size references

## Issue 1: Qt Resources Not Loaded

### Mechanism

The Qt resource file `shared/nexis/static.qrc` is compiled into the `nexis-gui` static library:

- **CMakeLists.txt:437** — `"${GUI_SHARED_DIR}/static.qrc"` listed as a source for `nexis-gui`
- CMake/Qt's `AUTOMOC`/`AUTORCC` generates `build/nexis-gui_autogen/GHXYBZ7ZQ4/qrc_static.cpp` and compiles it to `qrc_static.cpp.o`
- The object exports `qInitResources_static` (confirmed via `nm`)

When a Qt `.qrc` is compiled into a **static library**, the resource registration symbols can be dead-stripped by the linker if no code in the consuming executable references them. The Qt documentation prescribes calling `Q_INIT_RESOURCE(resourceName)` in the consuming binary to force-link the symbols.

The real application does this correctly:

- **`shared/nexis/main.cpp:86`** — `Q_INIT_RESOURCE(static);` called before `QApplication` construction

The test binary does NOT:

- **`tests/screenshots/test_screenshots.cpp:267`** — `QTEST_MAIN(ScreenshotTests)` generates `main()` automatically; no `Q_INIT_RESOURCE` call anywhere
- **`tests/CMakeLists.txt:48-53`** — test links `nexis-gui Qt6::Test` but has no linker flags to prevent dead-stripping

### Evidence

- `nm build/output/test-ScreenshotTests | grep qInitResources` returns **nothing** — the symbol was stripped
- `nm build/CMakeFiles/nexis-gui.dir/nexis-gui_autogen/.../qrc_static.cpp.o` shows `qInitResources_static` is present in the object file
- Test output shows hundreds of `qt.svg: Cannot open file ':/static/...'` warnings for every SVG icon, theme image, and font
- Captured screenshots show a nearly black window with only text labels visible (no icons, no themed backgrounds, no styled widgets)

### Why BUG-48 didn't catch this

BUG-48 added `Q_INIT_RESOURCE(static)` to `main.cpp` for the main application. The screenshot test was added later (FR-41) as a separate executable and was never given the same treatment.

## Issue 2: Broken Reference Screenshots

The reference screenshots at `tests/reference_screenshots/macos/{dark,light}/` were generated using `NEXIS_GENERATE_REFS=1` with the broken test binary. They show the same black/empty window as the current captures. This means even after fixing resource loading, the test will produce 100% diffs until references are regenerated.

Both dark and light themes have 11 reference images each (dashboard, hardware_info, startup_apps, system_cleaner, search, services, processes, uninstaller, resources, helpers, settings).

## Issue 3: Image Size Mismatch

- **Reference images:** 2048×1536 pixels (generated on a different display or DPI setting)
- **Current captures:** 2584×1536 pixels (current display)

The `compareImages()` function at `test_screenshots.cpp:74-81` returns an immediate 100% failure when image sizes don't match. This is correct behavior — it prevents meaningless pixel comparisons — but it means the test is inherently display-dependent. The test calls `mApp->resize(1024, 768)` (logical pixels) but `mApp->grab()` captures at the device pixel ratio, so the actual pixel dimensions vary by display.

## Issue 4: SEGFAULT During Cleanup

### Crash details

```
Received signal 11 (SIGSEGV), code 2,
  at instruction address 0x0000000105a963c8,
  accessing address 0x0000000000000008
```

Address `0x8` is a null pointer + 8-byte struct member offset. The crash occurs during or after `cleanupTestCase()` which does `delete mApp` (line 252). The `App` destructor tears down a complex widget tree including:

- 16 page widgets (DashboardPage, ResourcesPage, etc.)
- Sidebar with sections, buttons, badge labels
- System tray icon and menu
- DataRefreshService timers
- Multiple singleton managers

With all resources missing, many of these widgets are partially initialized (no icons, no stylesheets, broken layouts). Some managers or child objects may be null pointers that are not checked during destruction.

### Likely crash path

The crash happens after the QVERIFY2 failure in `screenshotLightTheme()`. Qt Test's flow:
1. `screenshotDarkTheme()` — runs but reference is missing size-match, skips or fails
2. `screenshotLightTheme()` — fails on `dashboard` with 100% diff, `QVERIFY2` returns early
3. `cleanupTestCase()` — `delete mApp` triggers cascade of destructor calls on partially-initialized objects → SIGSEGV

This will likely resolve itself once resources load properly and the app initializes fully.

## Fix Strategy

### Required

1. **Add `Q_INIT_RESOURCE(static)` to the test** — either in `initTestCase()` (before `new App()`) or by replacing `QTEST_MAIN` with a custom `main()` that calls it before `QApplication` construction.

   The Qt docs recommend calling `Q_INIT_RESOURCE` **before** `QApplication` is created. Since `QTEST_MAIN` creates `QApplication` internally, the cleanest approach is to replace `QTEST_MAIN(ScreenshotTests)` with a hand-written `main()`:
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

2. **Regenerate reference screenshots** — run with `NEXIS_GENERATE_REFS=1` after the resource fix to capture correct references.

### Recommended

3. **Make captures DPI-independent** — either force the device pixel ratio (e.g., `QT_SCALE_FACTOR=1`) or normalize images to logical size before comparison, so the test works on any display.

4. **Improve size-mismatch handling** — instead of 100% failure, scale one image to match the other before comparing (or skip with a warning and regeneration hint).

## Files Involved

| File | Role |
|------|------|
| `tests/screenshots/test_screenshots.cpp` | Test source — needs `Q_INIT_RESOURCE`, custom `main()` |
| `tests/CMakeLists.txt:48-53` | Test build config |
| `shared/nexis/main.cpp:86` | Reference for correct `Q_INIT_RESOURCE` usage |
| `CMakeLists.txt:431-438` | `nexis-gui` static library definition with `static.qrc` |
| `tests/reference_screenshots/macos/` | 22 reference images that need regeneration |
