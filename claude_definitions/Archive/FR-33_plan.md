# FR-33 Plan: Testing Infrastructure (Phase 4)

> Implements Phase 4 of the Implementation Roadmap.
> Prerequisite: Phases 1-3 complete. Prerequisite for: Phase 7 (FR-36).
> Reference: `claude_definitions/FR-33_research.md`

---

## Task 1: Add `Test` to Qt6 find_package

- [x] **1.1** In `CMakeLists.txt` line 17, add `Test` to the `find_package(Qt6 COMPONENTS ...)` call.

**File:** `CMakeLists.txt`
**Change:** `Core Gui Widgets Charts Svg Concurrent Network` → `Core Gui Widgets Charts Svg Concurrent Network Test`

**Acceptance:** `cmake -B build` succeeds and finds `Qt6::Test`.

---

## Task 2: Add BUILD_TESTING option and test subdirectory

- [x] **2.1** In `CMakeLists.txt`, add a testing block between the nexis executable section (ends ~line 386) and the install rules section (starts ~line 388).

**Insert:**
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

**Acceptance:** `cmake -B build` succeeds with `BUILD_TESTING=ON` (default). `cmake -B build -DBUILD_TESTING=OFF` skips test configuration.

---

## Task 3: Create `tests/` directory and CMakeLists.txt

- [x] **3.1** Create `tests/CMakeLists.txt` with the test target definition.

**File:** `tests/CMakeLists.txt`
```cmake
# Test sources are listed explicitly. When adding a test file, update this list.
add_executable(nexis-tests
  utils/test_format_util.cpp
)

target_link_libraries(nexis-tests
  nexis-core
  Qt6::Test
)

add_test(NAME FormatUtilTests COMMAND nexis-tests)
```

**Notes:**
- `CMAKE_AUTOMOC` is inherited from root — no need to re-set.
- `nexis-core` linkage provides all PUBLIC include directories (Info/, Tools/, Utils/).
- `Qt6::Test` provides QTest, QCOMPARE, QVERIFY, QTEST_MAIN.
- Test executable name `nexis-tests` matches roadmap convention.

**Acceptance:** `cmake --build build --target nexis-tests` compiles.

---

## Task 4: Create the smoke test

- [x] **4.1** Create `tests/utils/test_format_util.cpp` — validates `FormatUtil::formatBytes()` across all branches.

**File:** `tests/utils/test_format_util.cpp`

Test cases:
| Method | Input | Expected |
|--------|-------|----------|
| `formatBytes_zero` | 0 | `"0 bytes"` |
| `formatBytes_singleByte` | 1 | `"1 byte"` |
| `formatBytes_bytes` | 1023 | `"1023 bytes"` |
| `formatBytes_kibibytes` | 1024, 1536 | `"1.0 KiB"`, `"1.5 KiB"` |
| `formatBytes_mebibytes` | 1048576 | `"1.0 MiB"` |
| `formatBytes_gibibytes` | 1073741824 | `"1.0 GiB"` |
| `formatBytes_tebibytes` | 1099511627776 | `"1.0 TiB"` |

**Acceptance:** `ctest --test-dir build --output-on-failure` runs and all 7 test methods pass.

---

## Task 5: Build verification

- [x] **5.1** Clean rebuild with tests enabled.
  ```bash
  rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) && cmake --build build -j$(sysctl -n hw.ncpu)
  ```
- [x] **5.2** Run tests via CTest.
  ```bash
  ctest --test-dir build --output-on-failure
  ```
- [x] **5.3** Verify opt-out works.
  ```bash
  rm -rf build && cmake -B build -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) && cmake --build build -j$(sysctl -n hw.ncpu)
  ```
  Confirm no `nexis-tests` target and no `CTestTestfile.cmake`.

**Acceptance:** Clean build passes. CTest reports 1 test, 1 passed. Opt-out build has no test artifacts.

---

## Task 6: Add CI test step

- [x] **6.1** In `.github/workflows/build.yml`, add a "Test" step between the "Build" step and the "Upload build artifact" step.

**Insert after the Build step:**
```yaml
      - name: Test
        run: ctest --test-dir build --output-on-failure
```

**Note:** If `ctest --test-dir build` doesn't find tests (due to the `CMAKE_BINARY_DIR` override), fall back to `ctest --test-dir build/output`. Verified during Task 5.

**Acceptance:** CI builds on all 3 matrix runners now run tests; one passing test visible in CI logs.

---

## Task 7: Update CLAUDE.md with test conventions

- [x] **7.1** Add a "Testing" section to `CLAUDE.md` after the "Build" section.

**Content:**
- Test framework: Qt Test (QTest)
- Test location: `tests/` directory, organized by category (`utils/`, `core/`, `managers/`)
- File naming: `test_<classname>.cpp` (e.g., `test_format_util.cpp`)
- Each test file is self-contained with `QTEST_MAIN()` — no shared test runner
- Adding a new test: create `.cpp` file in appropriate subdirectory, add to `tests/CMakeLists.txt` source list
- Build commands:
  - Build tests: `cmake --build build -j$(sysctl -n hw.ncpu)` (built by default)
  - Run tests: `ctest --test-dir build --output-on-failure`
  - Skip tests: `cmake -B build -DBUILD_TESTING=OFF ...`

**Acceptance:** CLAUDE.md has a clear testing section that future sessions can reference.

---

## Task 8: Update tracking files and documentation

- [x] **8.1** Update `FEATURE_REQUESTS.md` — mark FR-33 as `[x]` with resolution note.
- [x] **8.2** Update `docs/IMPLEMENTATION_ROADMAP.md` — mark Phase 4 tasks 4.1-4.5 as `[x]`.
- [x] **8.3** Update `docs/APPLICATION_OVERVIEW.md` — add note about test infrastructure if relevant.
- [x] **8.4** Update `docs/ARCHITECTURE_REVIEW.md` — mark §3A (setup) as addressed, update the "No Automated Test Suite" weakness section to note that test infrastructure now exists.
- [x] **8.5** Update `CLAUDE.md` Key Directories to include `tests/`.

---

## Task 9: Commit and push

- [x] **9.1** Stage all new and modified files.
- [x] **9.2** Commit: `feat(tests): add Qt Test infrastructure with CTest and CI integration (FR-33)`
- [x] **9.3** Push to `native` branch.
- [x] **9.4** Move `claude_definitions/FR-33_research.md` and `FR-33_plan.md` to `claude_definitions/Archive/`.

---

## Summary of files

### Created
| File | Purpose |
|------|---------|
| `tests/CMakeLists.txt` | Test target definition |
| `tests/utils/test_format_util.cpp` | FormatUtil smoke test |

### Modified
| File | Change |
|------|--------|
| `CMakeLists.txt` | Add Qt6::Test, BUILD_TESTING option, enable_testing(), add_subdirectory(tests) |
| `.github/workflows/build.yml` | Add ctest step after build |
| `CLAUDE.md` | Add Testing section, update Key Directories |
| `FEATURE_REQUESTS.md` | Mark FR-33 as done |
| `docs/IMPLEMENTATION_ROADMAP.md` | Mark Phase 4 tasks as done |
| `docs/APPLICATION_OVERVIEW.md` | Note test infrastructure |
| `docs/ARCHITECTURE_REVIEW.md` | Update §3A weakness section |
