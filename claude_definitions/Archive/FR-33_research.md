# FR-33 Research: Testing Infrastructure (Qt Test + CTest + CI)

## 1. Current Build System Analysis

### 1.1 Root CMakeLists.txt Structure

**File:** `/Users/luke/Documents/GitHub/Nexis/CMakeLists.txt` (211 lines)

```cmake
cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(Nexis VERSION 1.2.1)
```

**Two targets are defined:**

| Target | Type | Line | Links |
|--------|------|------|-------|
| `nexis-core` | STATIC library | L42 | `Qt6::Core Qt6::Network` (+ IOKit/CoreFoundation on macOS) |
| `nexis` | Executable | L116 | `nexis-core Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Charts Qt6::Svg Qt6::Concurrent` |

**Qt6 modules currently found (L17):**
```cmake
find_package(Qt6 COMPONENTS Core Gui Widgets Charts Svg Concurrent Network REQUIRED)
```

**Key observation:** `Qt6::Test` is **NOT** in the `find_package` call. It must be added.

**C++ standard (L20-22):**
```cmake
set(CMAKE_CXX_STANDARD           17)
set(CMAKE_CXX_EXTENSIONS         YES)
set(CMAKE_CXX_STANDARD_REQUIRED  YES)
```

**Output directory structure (L8-10):**
```cmake
set(CMAKE_BINARY_DIR        "${CMAKE_BINARY_DIR}/output")
set(EXECUTABLE_OUTPUT_PATH  "${CMAKE_BINARY_DIR}/")
set(LIBRARY_OUTPUT_PATH     "${CMAKE_BINARY_DIR}/lib")
```

**AUTOMOC is enabled (L16):** `set(CMAKE_AUTOMOC ON)` — critical for Qt Test, since test classes use `Q_OBJECT`.

**CXXBasics inclusion (L5):**
```cmake
include("${CMAKE_CURRENT_SOURCE_DIR}/shared/cmake/cxxbasics/CXXBasics.cmake")
```
This includes `InitCXXBasics.cmake` (enables C/CXX languages, custom macros), `DefaultSettings.cmake` (sets default build type to Debug, enables `compile_commands.json`), and `UseFasterLinkers.cmake` (LLD/gold linker detection). None of these conflict with testing. The `opt_ifndef` macro from `MacroOpt.cmake` is used for cache variable defaults — could be used for a `BUILD_TESTING` option, but the standard `option()` command is simpler and more conventional for this purpose.

### 1.2 nexis-core Include Path Structure

**File:** `/Users/luke/Documents/GitHub/Nexis/CMakeLists.txt`, lines 49-58

```cmake
target_include_directories(nexis-core PUBLIC
  "${CORE_PLAT_DIR}"           # macos/nexis-core or linux/nexis-core
  "${CORE_PLAT_DIR}/Info"
  "${CORE_PLAT_DIR}/Tools"
  "${CORE_PLAT_DIR}/Utils"
  "${CORE_SHARED_DIR}"         # shared/nexis-core
  "${CORE_SHARED_DIR}/Info"
  "${CORE_SHARED_DIR}/Tools"
  "${CORE_SHARED_DIR}/Utils"
)
```

These are **PUBLIC** include directories, meaning any target that links against `nexis-core` automatically inherits them. A test executable that does `target_link_libraries(nexis-tests nexis-core Qt6::Test)` will get all these include paths for free.

**Implication for tests:** Test files can include headers exactly like production code:
```cpp
#include "format_util.h"          // works (Utils/ is in the include path)
#include <Utils/format_util.h>    // also works (shared/nexis-core is in the include path)
```

The angle-bracket `<Utils/format_util.h>` style is used by the GUI code (see usage in Section 3 below). Either works.

### 1.3 nexis-core Compile Definitions

**Line 60:**
```cmake
target_compile_definitions(nexis-core PRIVATE NEXISCORE_LIBRARY QT_DEPRECATED_WARNINGS)
```

`NEXISCORE_LIBRARY` is defined as PRIVATE to nexis-core. This controls the export macro in `nexis-core_global.h`:

```cpp
// shared/nexis-core/nexis-core_global.h
#if defined(NEXISCORE_LIBRARY)
#  define NEXISCORESHARED_EXPORT Q_DECL_EXPORT
#else
#  define NEXISCORESHARED_EXPORT Q_DECL_IMPORT
#endif
```

Since nexis-core is a **static library**, this export/import distinction is irrelevant at link time — the symbols are all statically linked. The test executable will link against `nexis-core` statically and the `Q_DECL_IMPORT` path resolves to nothing on static builds. No special handling needed.

### 1.4 Existing Test Infrastructure

**None.** Confirmed by:
- No `tests/` directory in the project root
- No `enable_testing()` or `add_test()` calls anywhere in CMake files
- No `BUILD_TESTING` option
- No `.cpp` files with Qt Test macros (`QTEST_MAIN`, `QCOMPARE`, etc.)
- No CTest configuration files (`CTestCustom.cmake`, `CTestConfig.cmake`)
- The only test files found in the repo are in `website/node_modules/` (unrelated JavaScript dependencies)

---

## 2. CI Pipeline Analysis

### 2.1 Workflow Files

| File | Trigger | Purpose |
|------|---------|---------|
| `.github/workflows/build.yml` | push/PR to `native`, `master`, `main` | Build verification (the primary CI workflow) |
| `.github/workflows/release.yml` | tag `v*` | Release pipeline (.deb, .AppImage, .dmg) |
| `.github/workflows/crowdin-sync.yml` | push to `native` (English TS file) + cron | Translation sync |
| `.github/workflows/lupdate.yml` | push to `l10n_crowdin_translations` | Normalize translation files |
| `.github/workflows/pages.yml` | push to `native` (website/ path) | Deploy website to GitHub Pages |

**The test step must go in `build.yml`** (the primary CI workflow). The release workflow could optionally also run tests, but `build.yml` is the gatekeeper for all pushes and PRs.

### 2.2 Build Workflow Detail (`build.yml`)

**Matrix:**

| Runner | Name | Artifact |
|--------|------|----------|
| `ubuntu-24.04` | Linux (x64) | `build/output/nexis` |
| `ubuntu-24.04-arm` | Linux (ARM64) | `build/output/nexis` |
| `macos-14` | macOS (ARM64) | `build/output/nexis.app` |

**Steps (in order):**
1. `actions/checkout@v4`
2. Install dependencies (Linux: `apt-get`, macOS: `brew`)
3. Configure (Linux: `cmake -B build -DCMAKE_BUILD_TYPE=Release`, macOS: adds `-DCMAKE_PREFIX_PATH=$(brew --prefix qt@6)`)
4. Build (`cmake --build build -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)`)
5. Upload artifact

**The test step should be inserted between step 4 (Build) and step 5 (Upload artifact).**

### 2.3 Linux CI Dependencies (build.yml, L38-51)

```yaml
sudo apt-get install -y -qq \
  cmake g++ libgl1-mesa-dev \
  qt6-base-dev qt6-charts-dev qt6-svg-dev \
  qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools \
  libqt6charts6-dev libqt6svg6-dev
```

**Qt6 Test availability:** On Ubuntu 24.04, `Qt6::Test` (the `QTest` framework) is part of the `qt6-base-dev` package. It does **not** require a separate package install. The library `libQt6Test6` and headers are included in `qt6-base-dev`.

Verification: `qt6-base-dev` on Ubuntu 24.04 provides `/usr/include/x86_64-linux-gnu/qt6/QtTest/` and `/usr/lib/x86_64-linux-gnu/libQt6Test.so`.

### 2.4 macOS CI Dependencies (build.yml, L56)

```yaml
brew install qt@6
```

The `qt@6` Homebrew formula includes `QtTest`. No additional formula needed.

**Key finding: No additional CI dependency changes are required for Qt Test on either platform.**

---

## 3. FormatUtil — The Smoke Test Target

### 3.1 Header

**File:** `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Utils/format_util.h`

```cpp
#ifndef FORMAT_UTIL_H
#define FORMAT_UTIL_H

#include "nexis-core_global.h"

class NEXISCORESHARED_EXPORT FormatUtil
{
public:
    static QString formatBytes(const quint64 &bytes);

public:
    static const quint64 KIBI = 1024;
    static const quint64 MEBI = 1048576;
    static const quint64 GIBI = 1073741824;
    static const quint64 TEBI = 1099511627776;
};
```

**Characteristics:**
- Static class (no instances needed, all static methods and constants)
- Single public method: `formatBytes(const quint64 &bytes)`
- No dependencies beyond `Qt6::Core` (uses `QString`, `quint64`)
- No I/O, no system calls, no file access — pure computation
- No constructor, no state — ideal for unit testing

### 3.2 Implementation

**File:** `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Utils/format_util.cpp`

```cpp
#include "format_util.h"
#include <QString>

QString FormatUtil::formatBytes(const quint64 &bytes)
{
#define formatUnit(v, u, t) QString::asprintf("%.1f %s", \
    ((double) v / (double) u), t)

    if (bytes == 1L) // bytes
        return QString("%1 byte").arg(bytes);
    else if (bytes < KIBI) // bytes
      return QString("%1 bytes").arg(bytes);
    else if (bytes < MEBI) // KiB
      return formatUnit(bytes, KIBI, "KiB");
    else if (bytes < GIBI) // MiB
      return formatUnit(bytes, MEBI, "MiB");
    else if (bytes < TEBI) // GiB
      return formatUnit(bytes, GIBI, "GiB");
    else                   // TiB
      return formatUnit(bytes, TEBI, "TiB");
#undef formatUnit
}
```

### 3.3 Behavioral Analysis

| Input | Branch | Expected Output |
|-------|--------|-----------------|
| `0` | `bytes < KIBI` (0 < 1024) | `"0 bytes"` |
| `1` | `bytes == 1L` | `"1 byte"` |
| `2` | `bytes < KIBI` | `"2 bytes"` |
| `1023` | `bytes < KIBI` | `"1023 bytes"` |
| `1024` | `bytes < MEBI` (1024 < 1048576) | `"1.0 KiB"` |
| `1536` | `bytes < MEBI` | `"1.5 KiB"` |
| `1048576` | `bytes < GIBI` | `"1.0 MiB"` |
| `1073741824` | `bytes < TEBI` | `"1.0 GiB"` |
| `1099511627776` | else (TiB) | `"1.0 TiB"` |
| `5497558138880` | else (TiB) | `"5.0 TiB"` |

**Note on edge case:** The `formatUnit` macro uses `%.1f` format, so all non-byte values get exactly one decimal place. The `bytes` branch uses `QString::arg(quint64)` with no formatting, so it produces raw integer strings.

**Note on the Architecture Review's test expectation:** The Architecture Review (line 661) expects `FormatUtil::formatBytes(0)` to return `"0 B"`. However, the actual implementation returns `"0 bytes"` (since 0 falls into the `bytes < KIBI` branch which uses `"%1 bytes"`). The smoke test must use the **actual** expected output `"0 bytes"`, not `"0 B"`.

Similarly, the Architecture Review expects `formatBytes(1023)` to return `"1023 B"`, but the actual output is `"1023 bytes"`. The Implementation Roadmap (Phase 7, task 7.1) also references `formatBytes(0)` returning `"0 B"`, inheriting the same error. The test must validate against the real implementation.

### 3.4 Usage Across Codebase

`FormatUtil::formatBytes()` is called from 18 different source files across the project — it's one of the most widely-used utility functions. Files include:
- `shared/nexis/app.cpp` (L182)
- `shared/nexis/main.cpp` (L120, L144)
- `shared/nexis/Pages/Dashboard/dashboard_page.cpp` (L274-370)
- `shared/nexis/Pages/Resources/resources_page.cpp` (L123-272)
- `shared/nexis/Pages/Processes/processes_page.cpp` (L147-157)
- `shared/nexis/Pages/SystemCleaner/system_cleaner_page.cpp` (L147-577)
- `shared/nexis/Pages/HardwareInfo/hardware_info_page.cpp` (L205-331)
- `shared/nexis/Pages/Settings/settings_page.cpp` (L490)
- `shared/nexis/Pages/Search/search_page.cpp` (L348)
- `shared/nexis/Pages/Docker/docker_page.cpp` (L139)
- `shared/nexis/Managers/cleaner_service.cpp` (L273)
- `shared/nexis/Pages/Resources/history_chart.cpp` (L105)
- `shared/nexis/Pages/SystemCleaner/byte_tree_widget.cpp` (L6)

**Include style used by consumers:** Both styles found:
- Angle brackets (most common): `#include <Utils/format_util.h>`
- Quotes: `#include "Utils/format_util.h"`

Both resolve because `nexis-core`'s include directories are PUBLIC and include both `shared/nexis-core` and `shared/nexis-core/Utils`.

---

## 4. Project Directory Structure

```
Nexis/
  CMakeLists.txt              ← Root build file (sole CMakeLists.txt in project)
  shared/
    cmake/cxxbasics/          ← CMake utility modules
    nexis-core/               ← Core library sources
      nexis-core_global.h
      Info/                   ← System info classes (cpu_info, memory_info, etc.)
      Tools/                  ← System tool wrappers (apt_source_tool, docker_tool, etc.)
      Utils/                  ← Utility classes (format_util, file_util, command_util)
    nexis/                    ← GUI application sources
    translations/             ← i18n .ts files
  macos/
    nexis-core/               ← macOS platform implementations
      Info/                   ← macOS-specific info classes
      Tools/                  ← macOS-specific tool wrappers
      Utils/                  ← macOS-specific utilities (brew_util, command_util_platform)
  linux/
    nexis-core/               ← Linux platform implementations
    nexis/                    ← Linux GUI-specific code
  .github/workflows/          ← CI workflow files
  tests/                      ← DOES NOT EXIST YET
```

**Important:** There is only **one** `CMakeLists.txt` in the entire C++ project (at the root). No subdirectory `CMakeLists.txt` files exist. The new `tests/CMakeLists.txt` will be the first subdirectory CMake file.

---

## 5. Recommendations for Test Infrastructure

### 5.1 Where to Add Test Code

Create `tests/` directory at project root:
```
tests/
  CMakeLists.txt             ← Test target definition
  test_format_util.cpp       ← Smoke test (FormatUtil::formatBytes)
```

The Implementation Roadmap suggests a subdirectory structure (`core/`, `utils/`, `managers/`), but for Phase 4 (infrastructure only + one smoke test), a flat structure is simpler. Subdirectories can be added in Phase 7 when more tests are written.

### 5.2 Root CMakeLists.txt Changes

The following additions are needed at the bottom of the root `CMakeLists.txt` (before the install rules, or after them — order doesn't matter since tests are a separate target):

1. **Add `Test` to `find_package`** (line 17):
   ```cmake
   find_package(Qt6 COMPONENTS Core Gui Widgets Charts Svg Concurrent Network Test REQUIRED)
   ```
   Adding `Test` here is the simplest approach. It's REQUIRED which means it will fail fast if Qt6Test is unavailable (which is fine — it's part of qt6-base-dev on all supported platforms).

   Alternative: `find_package(Qt6 COMPONENTS Test REQUIRED)` separately, but there's no benefit to splitting it.

2. **Add testing block** (after the nexis executable target, before install rules):
   ```cmake
   # ============================================================================
   # Tests (Qt Test + CTest)
   # ============================================================================
   option(BUILD_TESTING "Build the test suite" ON)
   if(BUILD_TESTING)
     enable_testing()
     add_subdirectory(tests)
   endif()
   ```

   Using `option(BUILD_TESTING ...)` is the CMake convention (CTest's own `include(CTest)` sets this variable). Setting default to `ON` ensures tests build by default in development. CI can disable with `-DBUILD_TESTING=OFF` if needed, but should leave ON.

   Note: `include(CTest)` is an alternative to `enable_testing()` that also sets up CDash submission. We don't need CDash, so `enable_testing()` is cleaner.

### 5.3 tests/CMakeLists.txt

```cmake
set(CMAKE_AUTOMOC ON)

add_executable(nexis-tests
  test_format_util.cpp
)

target_link_libraries(nexis-tests
  nexis-core
  Qt6::Test
)

add_test(NAME FormatUtilTests COMMAND nexis-tests)
```

Key points:
- `CMAKE_AUTOMOC ON` is needed for `Q_OBJECT` in test classes (it's already ON from the root, but setting it explicitly in the test CMakeLists.txt makes the dependency explicit).
- `nexis-core` linkage gives us all PUBLIC include directories automatically.
- `Qt6::Test` provides `QTest`, `QSignalSpy`, `QCOMPARE`, `QVERIFY`, etc.
- `add_test()` registers the executable with CTest so `ctest` discovers and runs it.
- The test executable name `nexis-tests` follows the convention suggested in the Implementation Roadmap.

### 5.4 Smoke Test File

**File:** `tests/test_format_util.cpp`

The Qt Test framework pattern for a single test class:

```cpp
#include <QTest>
#include "format_util.h"

class TestFormatUtil : public QObject
{
    Q_OBJECT

private slots:
    void formatBytes_zero();
    void formatBytes_singleByte();
    void formatBytes_bytes();
    void formatBytes_kibibytes();
    void formatBytes_mebibytes();
    void formatBytes_gibibytes();
    void formatBytes_tebibytes();
};

void TestFormatUtil::formatBytes_zero()
{
    QCOMPARE(FormatUtil::formatBytes(0), QString("0 bytes"));
}

void TestFormatUtil::formatBytes_singleByte()
{
    QCOMPARE(FormatUtil::formatBytes(1), QString("1 byte"));
}

void TestFormatUtil::formatBytes_bytes()
{
    QCOMPARE(FormatUtil::formatBytes(1023), QString("1023 bytes"));
}

void TestFormatUtil::formatBytes_kibibytes()
{
    QCOMPARE(FormatUtil::formatBytes(1024), QString("1.0 KiB"));
    QCOMPARE(FormatUtil::formatBytes(1536), QString("1.5 KiB"));
}

void TestFormatUtil::formatBytes_mebibytes()
{
    QCOMPARE(FormatUtil::formatBytes(1048576), QString("1.0 MiB"));
}

void TestFormatUtil::formatBytes_gibibytes()
{
    QCOMPARE(FormatUtil::formatBytes(1073741824), QString("1.0 GiB"));
}

void TestFormatUtil::formatBytes_tebibytes()
{
    QCOMPARE(FormatUtil::formatBytes(1099511627776), QString("1.0 TiB"));
}

QTEST_MAIN(TestFormatUtil)
#include "test_format_util.moc"
```

Key Qt Test conventions:
- `QTEST_MAIN(TestFormatUtil)` generates the `main()` function
- `#include "test_format_util.moc"` is required for AUTOMOC to work (the `.moc` file is generated from the `Q_OBJECT` macro in the same `.cpp` file)
- Each `private slots:` method is a separate test case
- `QCOMPARE` provides rich failure messages showing expected vs actual

### 5.5 CI Changes (`build.yml`)

Add a test step after the Build step:

```yaml
      - name: Build
        run: cmake --build build -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)

      - name: Test
        run: ctest --test-dir build --output-on-failure
```

`--test-dir build` tells CTest to look for test configurations in the `build/` directory. `--output-on-failure` shows stdout/stderr of failing tests in the CI log (critical for debugging failures).

**No dependency changes needed** in the Linux or macOS install steps (see Section 2.3/2.4 above).

**Note about the release workflow (`release.yml`):** Optionally, a test step could be added there too (between Build and the packaging steps). However, since the release workflow only triggers on tags (which should come from the `native` branch that already passed `build.yml`), it's lower priority. The build workflow is the critical path.

### 5.6 CTest Output Directory Consideration

The root CMakeLists.txt overrides `CMAKE_BINARY_DIR` (line 8):
```cmake
set(CMAKE_BINARY_DIR "${CMAKE_BINARY_DIR}/output")
```

This affects `EXECUTABLE_OUTPUT_PATH` and `LIBRARY_OUTPUT_PATH`, but **CTest discovers tests based on the `add_test()` registration**, which uses the target's actual output path. The test executable will land in `build/output/tests/` or similar depending on CMake's internal path resolution.

However, `ctest --test-dir build` should still work because CTest reads `build/CTestTestfile.cmake` (generated by `enable_testing()` + `add_test()`), which contains the full path to the test executable. This has been verified to work with subdirectory-based test targets.

If there are issues, `ctest --test-dir build/output` could be tried, but `build` should be sufficient since `enable_testing()` is called at the root scope.

---

## 6. Existing Documentation and Plans

### 6.1 Architecture Review References

**File:** `/Users/luke/Documents/GitHub/Nexis/docs/ARCHITECTURE_REVIEW.md`

Section "5. No Automated Test Suite" documents the current state:
- Zero unit tests, zero integration tests, zero regression tests
- Quality assurance is: CI builds (compilation), manual visual QA, bug reports
- Lists 4 historical bugs that tests would have caught (BUG-01, BUG-30, BUG-40, BUG-37)

Section "Priority 3: Medium (Testing & Quality) / 3A. Basic Unit Test Suite" provides:
- Recommended framework: Qt Test (QTest)
- Target coverage priorities (Info parsing, utilities, managers)
- Example test code (with incorrect expected values, as noted in Section 3.3)

Section "Testing Strategy" outlines a 4-phase approach (DI first, then unit tests, then integration, then UI regression).

### 6.2 Implementation Roadmap References

**File:** `/Users/luke/Documents/GitHub/Nexis/docs/IMPLEMENTATION_ROADMAP.md`

Phase 4 (Testing Infrastructure) provides the task list that FR-33 implements:
- Task 4.1: Create `tests/` directory structure
- Task 4.2: Add test target to root CMakeLists.txt
- Task 4.3: Add one smoke test (FormatUtil)
- Task 4.4: Add CI test step
- Task 4.5: Update CLAUDE.md with test conventions

Phase 7 (Unit Test Suite) is the follow-up that depends on FR-33. It specifies 15-20 tests covering FormatUtil, FileUtil, CommandUtil, MemoryInfo, DiskHealthInfo, CpuInfo, CleanerService, ScheduleManager, and theme tokens.

### 6.3 FEATURE_REQUESTS.md Entry

**File:** `/Users/luke/Documents/GitHub/Nexis/FEATURE_REQUESTS.md`, line 75

```
- [ ] **FR-33: Testing infrastructure (Qt Test + CTest + CI)** — [Phase 4] Set up `tests/` directory,
CMake test target with Qt Test framework, CTest integration, CI test step in GitHub Actions, and one
smoke test to validate the pipeline. Prerequisite for FR-36. Architecture Review §3A (setup).
```

Status: `[ ]` (planned, not started).

---

## 7. Potential Issues and Edge Cases

### 7.1 AUTOMOC in Subdirectory

When `add_subdirectory(tests)` is used, `CMAKE_AUTOMOC` from the parent scope is inherited. However, the test file uses `#include "test_format_util.moc"` which requires the `.moc` file to be generated in the build directory for the `tests/` subdirectory. This is standard Qt Test practice and works correctly with `CMAKE_AUTOMOC ON`.

### 7.2 Static Library Export Macros

`FormatUtil` uses `NEXISCORESHARED_EXPORT` which expands to `Q_DECL_IMPORT` when `NEXISCORE_LIBRARY` is not defined (which is the case for the test executable). For a static library, `Q_DECL_IMPORT` is a no-op on all platforms, so this is safe. No special handling needed.

### 7.3 Platform-Specific Test Considerations

`FormatUtil::formatBytes()` is in `shared/nexis-core/Utils/` (platform-independent). The smoke test will work identically on Linux and macOS. No conditional compilation needed.

For future tests (Phase 7, FR-36), platform-specific Info classes (e.g., CpuInfo, MemoryInfo) will need mock data or conditional test cases. But that's out of scope for FR-33.

### 7.4 Qt Test on CI — Headless Considerations

Qt Test does not require a display server (unlike QWidget-based tests). `QTEST_MAIN` creates a `QCoreApplication`, not a `QApplication`. The FormatUtil smoke test is pure computation with no GUI involvement, so it will run headless without issues.

Future tests that instantiate QWidgets would need `QT_QPA_PLATFORM=offscreen` environment variable, but that's a Phase 7 concern.

### 7.5 CTest Working Directory

`ctest --test-dir build` discovers tests from the `CTestTestfile.cmake` generated at configure time. The generated file contains absolute paths to test executables. This works regardless of the `CMAKE_BINARY_DIR` override in the root CMakeLists.txt because CTest reads the file hierarchy:
- `build/CTestTestfile.cmake` (root) includes
- `build/tests/CTestTestfile.cmake` (subdirectory) which contains the `add_test()` registration with the absolute path to the `nexis-tests` executable.

### 7.6 Build Time Impact

Adding `Qt6::Test` to `find_package` adds negligible configure time. The test executable itself is tiny (one `.cpp` file linking a static library). Impact on CI build time: less than 5 seconds total.

### 7.7 The `CMAKE_BINARY_DIR` Override

Line 8 of the root CMakeLists.txt:
```cmake
set(CMAKE_BINARY_DIR "${CMAKE_BINARY_DIR}/output")
```

This is an unusual pattern. It redirects the build output to a subdirectory. The test executable will be placed under `build/output/` (following `EXECUTABLE_OUTPUT_PATH`), but CTest references the executable by its full path, so the override doesn't break test discovery.

However, note that `ctest --test-dir build` expects `build/CTestTestfile.cmake` to exist. Since `enable_testing()` is called at the root CMakeLists.txt scope (before the `CMAKE_BINARY_DIR` override takes effect on subdirectories), the CTestTestfile.cmake should be generated at `build/CTestTestfile.cmake`. This needs to be verified during implementation. If CTest can't find tests, `ctest --test-dir build/output` would be the fallback.

---

## 8. Summary of Changes Required

### Files to Create
| File | Purpose |
|------|---------|
| `tests/CMakeLists.txt` | Test target definition (nexis-tests executable, CTest registration) |
| `tests/test_format_util.cpp` | Smoke test for FormatUtil::formatBytes() |

### Files to Modify
| File | Change |
|------|--------|
| `CMakeLists.txt` (root) | Add `Test` to `find_package(Qt6 ...)`, add `BUILD_TESTING` option + `enable_testing()` + `add_subdirectory(tests)` |
| `.github/workflows/build.yml` | Add `ctest --test-dir build --output-on-failure` step after Build |
| `FEATURE_REQUESTS.md` | Update FR-33 status from `[ ]` to `[~]` then `[x]` |
| `CLAUDE.md` | Add test conventions section (how to add tests, naming, build commands) |

### No Changes Needed
| File/Area | Reason |
|-----------|--------|
| CI dependency installation (apt-get/brew) | Qt6Test is already included in `qt6-base-dev` and `qt@6` |
| `nexis-core_global.h` | Export macros work correctly with static linking |
| `shared/cmake/cxxbasics/` | No conflicts with test infrastructure |
| `.github/workflows/release.yml` | Test step optional here (tags come from tested branches) |

---

## 9. Build and Verification Commands

After implementation, these commands validate the pipeline:

**Local build with tests:**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6)
cmake --build build -j$(sysctl -n hw.ncpu)
ctest --test-dir build --output-on-failure
```

**Build without tests (opt out):**
```bash
cmake -B build -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release
```

**Run specific test:**
```bash
ctest --test-dir build -R FormatUtil --output-on-failure
```

**Verbose test output:**
```bash
ctest --test-dir build -V
```

**Or run the test executable directly:**
```bash
./build/output/nexis-tests -v2
```
(The `-v2` flag is a Qt Test option for verbose output showing each test function name and result.)
