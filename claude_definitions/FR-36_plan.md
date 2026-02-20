# FR-36 Plan: Unit Test Suite (Phase 7)

> **Tracking:** FR-36, Architecture Review §3A (implementation)
> **Dependencies:** FR-33 (test infra) ✅, FR-35 (dependency injection) ✅
> **Research:** `claude_definitions/FR-36_research.md`
> **Target:** 20+ tests covering utilities, Info class parsing, manager logic, and theme validation

---

## Phase Overview

| Phase | Description | Test Count | Refactoring Needed |
|-------|------------|------------|-------------------|
| 7.1 | Test infrastructure expansion | 0 (setup) | CMake changes |
| 7.2 | Utility class tests | 7 | None |
| 7.3 | DiskHealthInfo tests (verdict + parsing) | 6 | Expose static functions |
| 7.4 | ScheduleManager tests | 5 | Add `now` parameter to `getNextRunTime()` |
| 7.5 | Theme token validation tests | 4 | None (reads source files directly) |
| 7.6 | CommandUtil tests | 4 | None |
| 7.7 | CleanerService static tests | 2 | None |
| 7.8 | CI verification + tracking updates | 0 (admin) | None |
| **Total** | | **28** | |

---

## 7.1 — Test Infrastructure Expansion

**Goal:** Restructure `tests/CMakeLists.txt` to support multiple independent test executables (one per test class), since `QTEST_MAIN` generates `main()` per file.

### Tasks

- [x] **7.1.1 — Restructure tests/CMakeLists.txt for per-file executables**
  - Qt Test requires one `QTEST_MAIN` per executable, so each test `.cpp` becomes its own target.
  - Add a CMake helper macro `add_nexis_test(NAME sources... [LIBS extra_libs...])` that:
    1. Creates an executable `test-{NAME}`
    2. Links `nexis-core` + `Qt6::Test` + any extra libs
    3. Registers with CTest via `add_test(NAME {NAME} COMMAND test-{NAME})`
  - Migrate existing `test_format_util.cpp` to use this macro.
  - Create directory structure: `tests/utils/`, `tests/core/`, `tests/managers/`, `tests/theme/`

- [x] **7.1.2 — Add PROJECT_SOURCE_DIR compile definition for theme tests**
  - Theme tests read `.qss` and `.ini` files from the source tree.
  - Add `target_compile_definitions(test-{NAME} PRIVATE PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")` to the macro.
  - This avoids embedding absolute paths in test code.

- [x] **7.1.3 — Build verification**
  - Incremental build passes.
  - Existing FormatUtil tests still run via `ctest --test-dir build --output-on-failure`.

**Acceptance:** `ctest` discovers and runs the existing FormatUtil test under the new structure. No regressions.

---

## 7.2 — Utility Class Tests (Pure Functions)

**Goal:** Test `FormatUtil`, `FileUtil` in isolation. These are pure/static functions with no manager dependencies.

### Tasks

- [x] **7.2.1 — Expand FormatUtil tests**
  - File: `tests/utils/test_format_util.cpp` (extend existing)
  - Add tests for:
    - `formatBytes_multiTiB`: `2199023255552` → `"2.0 TiB"`
    - `formatBytes_maxUint64`: `UINT64_MAX` → valid TiB string (no crash, no overflow)
    - `formatBytes_nearBoundary`: `1023 * 1024` → `"1023.0 KiB"` (just below MiB)
  - Total for this file: 10 test methods (7 existing + 3 new)

- [x] **7.2.2 — Create FileUtil tests**
  - File: `tests/utils/test_file_util.cpp`
  - Test methods:
    1. `readStringFromFile_validFile` — Write temp file, read back, verify content.
    2. `readStringFromFile_nonexistent` — Read missing path → empty QString.
    3. `readStringFromFile_emptyFile` — Create empty file → empty QString.
    4. `readListFromFile_multipleLines` — File with 3 lines → 3-element list.
    5. `writeFile_roundTrip` — Write then read, verify round-trip.
    6. `writeFile_invalidPath` — Write to `/nonexistent/dir/file.txt` → returns false.
    7. `getFileSize_singleFile` — Write known bytes to temp file → exact size.
    8. `getFileSize_recursiveDir` — Create nested temp dirs with files → correct sum.
    9. `getFileSize_nonexistent` — Non-existent path → 0.
    10. `directoryList_withFiles` — Temp dir with files + subdirs → only files returned.
  - Use `QTemporaryDir` for all filesystem operations (auto-cleanup).
  - Register as `add_nexis_test(NAME FileUtilTests SOURCES utils/test_file_util.cpp)`

- [x] **7.2.3 — Build and run**
  - All FileUtil tests pass.
  - No new warnings.

**Acceptance:** 13 utility tests passing (10 FormatUtil + 3 net new, though the test file has 10 methods with 7 existing).

Actually let me be precise: FormatUtil will have 10 methods total. FileUtil will have 10 methods. Total for this phase: 20 test methods across 2 files, of which 13 are new.

---

## 7.3 — DiskHealthInfo Tests (Verdict + Parsing)

**Goal:** Test `deriveHealthVerdict()` (pure struct logic) and `parseSmartctlJson()` (JSON parsing). Requires minimal refactoring to expose the currently file-static parser.

### Refactoring (Prerequisite)

- [x] **7.3.1 — Expose parseSmartctlJson as a public static method**
  - In `shared/nexis-core/Info/disk_health_info.h`, add:
    ```cpp
    static void parseSmartctlJsonInto(const QByteArray &json, DriveHealth &drive);
    ```
  - In `shared/nexis-core/Info/disk_health_info_shared.cpp`, implement by moving the parsing logic from the platform files into the shared file. Both platforms' `parseSmartctlJson()` implementations are identical — this is a deduplication + exposure.
  - Remove the `static` `parseSmartctlJson()` from both `macos/` and `linux/` `.cpp` files. Update callers in `discoverDrives()` and `refreshHealthElevated()` to call `DiskHealthInfo::parseSmartctlJsonInto()`.
  - **Risk:** Low — moves identical code from 2 platform files to 1 shared file, changes visibility only.

- [x] **7.3.2 — Make deriveHealthVerdict() public**
  - Change `deriveHealthVerdict()` from `protected` to `public` in `disk_health_info.h`.
  - It's already in the shared `.cpp` and operates purely on a `DriveHealth` struct.
  - **Risk:** None — only changes access modifier.

### Tests

- [x] **7.3.3 — Create DiskHealthInfo verdict tests**
  - File: `tests/core/test_disk_health_info.cpp`
  - Test methods for `deriveHealthVerdict()`:
    1. `verdict_nvmeGood` — percentageUsed=3, availableSpare=100, mediaErrors=0 → "Good", healthPercent=97
    2. `verdict_nvmeCaution_lowSpare` — availableSpare=8, percentageUsed=50 → "Caution", healthPercent=50
    3. `verdict_nvmeCaution_highUsage` — percentageUsed=85 → "Caution", healthPercent=15
    4. `verdict_nvmeCritical_mediaErrors` — mediaErrors=1 → "Critical"
    5. `verdict_nvmeCritical_100pctUsed` — percentageUsed=100 → "Critical", healthPercent=0
    6. `verdict_nvmeCritical_smartFailed` — smartPassed=false → "Critical"
    7. `verdict_sataHddGood` — all sectors=0 → "Good", healthPercent=-1
    8. `verdict_sataHddCaution` — reallocated=5 → "Caution"
    9. `verdict_sataHddCritical` — sum>100 → "Critical"
    10. `verdict_sataSsdGood` — wearLeveling=95 → "Good", healthPercent=95
    11. `verdict_sataSsdCaution_lowWear` — wearLeveling=25 → "Caution"
    12. `verdict_sataSsdCritical` — wearLeveling=5 → "Critical", healthPercent=5
    13. `verdict_unknownPassed` — Unknown type, smartPassed=true → "Good"
    14. `verdict_unknownFailed` — Unknown type, smartPassed=false → "Critical"

- [x] **7.3.4 — Create DiskHealthInfo JSON parsing tests**
  - Same file, test methods for `parseSmartctlJsonInto()`:
    1. `parseJson_nvmeAllFields` — Full NVMe smartctl JSON → all fields populated correctly.
    2. `parseJson_sataHdd` — SATA JSON with rotation_rate>0, attribute table → driveType=SATA_HDD, attributes extracted.
    3. `parseJson_sataSsd` — SATA JSON with rotation_rate=0 → driveType=SATA_SSD, wearLeveling from attr 177.
    4. `parseJson_emptyJson` — `"{}"` → no crash, fields remain default.
    5. `parseJson_invalidJson` — `"not json"` → no crash, fields remain default.
    6. `parseJson_smartFailed` — JSON with `"smart_status": {"passed": false}` → smartPassed=false.
  - Embed sample JSON strings as `QByteArray` literals in the test file (NVMe and SATA samples from research doc).

- [x] **7.3.5 — Build and run**
  - All DiskHealthInfo tests pass.
  - Existing tests unaffected.

**Acceptance:** 20 test methods in `test_disk_health_info.cpp`. Covers all verdict branches + parsing paths. `deriveHealthVerdict()` is fully branch-covered.

---

## 7.4 — ScheduleManager Tests

**Goal:** Test `getNextRunTime()` (date math), `frequencyDisplayText()` (string formatting), and JSON serialization.

### Refactoring (Prerequisite)

- [x] **7.4.1 — Add `now` parameter to getNextRunTime()**
  - In `schedule_manager.h`, change signature to:
    ```cpp
    QDateTime getNextRunTime(const CleaningSchedule &schedule,
                             const QDateTime &now = QDateTime()) const;
    ```
  - In `schedule_manager.cpp`, change `QDateTime now = QDateTime::currentDateTime();` to:
    ```cpp
    QDateTime effectiveNow = now.isValid() ? now : QDateTime::currentDateTime();
    ```
  - Replace all uses of `now` with `effectiveNow` in the function body.
  - **Risk:** None — default parameter preserves all existing callers. Production code unchanged.

### Tests

- [x] **7.4.2 — Create ScheduleManager tests**
  - File: `tests/managers/test_schedule_manager.cpp`
  - Test methods for `getNextRunTime()`:
    1. `nextRun_dailyFuture` — schedule.hour=15, now=10:00 → today at 15:00.
    2. `nextRun_dailyPast` — schedule.hour=8, now=10:00 → tomorrow at 08:00.
    3. `nextRun_weeklyFutureDay` — dayOfWeek=5 (Fri), now=Wed → this Fri.
    4. `nextRun_weeklyPastDay` — dayOfWeek=1 (Mon), now=Wed → next Mon.
    5. `nextRun_weeklySameDay_futureTime` — dayOfWeek=Wed, hour=15, now=Wed 10:00 → this Wed 15:00.
    6. `nextRun_weeklySameDay_pastTime` — dayOfWeek=Wed, hour=8, now=Wed 10:00 → next Wed 08:00.
    7. `nextRun_monthlyFuture` — dayOfMonth=25, now=15th → 25th this month.
    8. `nextRun_monthlyPast` — dayOfMonth=5, now=15th → 5th next month.
    9. `nextRun_monthlyDay31InFeb` — dayOfMonth=31, now=Feb 15 → Feb 28 (clamped).
    10. `nextRun_everyNDays_withLastRun` — everyNDays=3, lastRun=2 days ago → tomorrow.
    11. `nextRun_everyNDays_noLastRun` — everyNDays=5, now after scheduled hour → 5 days out.
  - Test methods for `frequencyDisplayText()`:
    1. `displayText_daily` — "Daily at 03:00"
    2. `displayText_everyNDays` — "Every 5 days at 14:30"
    3. `displayText_weekly` — "Weekly on Monday at 09:00"
    4. `displayText_monthly` — "Monthly on day 15 at 22:00"
  - All tests create `CleaningSchedule` structs directly, pass fixed `now` parameter. No singleton access needed.

- [x] **7.4.3 — Build and run**
  - All ScheduleManager tests pass.

**Acceptance:** 15 test methods. All 4 frequency types covered for `getNextRunTime()` with edge cases. Display text verified for all types.

---

## 7.5 — Theme Token Validation Tests

**Goal:** Codify Phase 3's runtime validation as permanent regression tests. Reads theme files directly from the source tree.

### Tasks

- [x] **7.5.1 — Create theme token tests**
  - File: `tests/theme/test_theme_tokens.cpp`
  - Uses `PROJECT_SOURCE_DIR` compile definition (from 7.1.2) to locate source files.
  - Test methods:
    1. `darkTheme_allTokensResolved` — Extract all `@token` from `style.qss` (excluding `@dp\d+`), verify each exists in `default/values.ini`.
    2. `lightTheme_allTokensResolved` — Same check for `light/values.ini`.
    3. `darkTheme_colorsValid` — All values in `default/values.ini` (except `@themeName`) are valid hex: start with `#`, length is 4/5/7/9, hex characters only.
    4. `lightTheme_colorsValid` — Same for `light/values.ini`.
    5. `themes_sameTokenSets` — Both `values.ini` files define the exact same set of tokens.
    6. `noUnresolvedTokens_dark` — Perform full token replacement on QSS with dark theme values, then verify no `@colorXX`, `@accent*`, `@card*`, `@border*`, `@success*`, `@warning*`, `@destructive*`, `@pageContent`, `@sidebar` tokens remain.
    7. `noUnresolvedTokens_light` — Same for light theme.
  - Uses `QSettings(path, QSettings::IniFormat)` for values.ini, `QRegularExpression` for token extraction.
  - Register as `add_nexis_test(NAME ThemeTokenTests SOURCES theme/test_theme_tokens.cpp)`

- [x] **7.5.2 — Build and run**
  - All theme tests pass with both theme files.

**Acceptance:** 7 test methods. Prevents BUG-21/33/36/38-class regressions permanently.

---

## 7.6 — CommandUtil Tests

**Goal:** Test command execution, error handling, and PATH detection using real system commands.

### Tasks

- [x] **7.6.1 — Create CommandUtil tests**
  - File: `tests/utils/test_command_util.cpp`
  - Test methods:
    1. `exec_echoReturnsOutput` — `exec("echo", {"hello"})` → `"hello"`.
    2. `exec_invalidCommandThrows` — `exec("nonexistent_binary_xyz_12345")` → throws `QString`.
    3. `exec_trimmedOutput` — Command with trailing whitespace → output is trimmed.
    4. `execWithStatus_successExitCode` — `execWithStatus("echo", {"test"})` → exitCode=0.
    5. `execWithStatus_failureExitCode` — `execWithStatus("false")` → exitCode=1 (BUG-31 regression).
    6. `execWithStatus_invalidCommand` — `execWithStatus("nonexistent_binary_xyz")` → exitCode=-1.
    7. `isExecutable_knownCommand` — `isExecutable("ls")` → true.
    8. `isExecutable_unknownCommand` — `isExecutable("nonexistent_binary_xyz_12345")` → false.
    9. `isExecutable_emptyString` — `isExecutable("")` → false.
  - Note: `exec()` throwing is tested via `QVERIFY_THROWS_EXCEPTION(QString, CommandUtil::exec(...))` (Qt 6.3+ macro).
  - Register as `add_nexis_test(NAME CommandUtilTests SOURCES utils/test_command_util.cpp)`

- [x] **7.6.2 — Build and run**
  - All CommandUtil tests pass on macOS.
  - `false` command works on both macOS and Linux.

**Acceptance:** 9 test methods. BUG-31 (exit code) and BUG-15 (isExecutable) regression paths covered.

---

## 7.7 — CleanerService Static Tests

**Goal:** Test the static helper methods that don't require manager singletons.

### Tasks

- [x] **7.7.1 — Create CleanerService tests**
  - File: `tests/managers/test_cleaner_service.cpp`
  - Test methods:
    1. `categoryName_allCategories` — Verify each `CleanCategory` enum returns the expected translated string.
    2. `allCategories_returnsAll` — `allCategories()` returns exactly 6 elements with no duplicates.
  - Requires linking the GUI library (CleanerService is in `nexis` target, not `nexis-core`).
  - **Alternative:** If linking the full GUI library is too heavy, these 2 tests can be deferred or the functions can be tested via integration tests. We should evaluate at implementation time.
  - Register as `add_nexis_test(NAME CleanerServiceTests SOURCES managers/test_cleaner_service.cpp LIBS nexis)` or defer.

- [x] **7.7.2 — Build and run**
  - Tests pass or phase is deferred with rationale.

**Acceptance:** 2 test methods, or documented deferral.

**Result:** Deferred. CleanerService is in the `nexis` GUI executable target (not a library), so test-linking requires compiling the entire GUI stack. The 2 planned tests are trivial and better suited for integration testing.

---

## 7.8 — CI Verification & Tracking Updates

**Goal:** Ensure all tests pass in CI, update project documentation.

### Tasks

- [x] **7.8.1 — Full clean build and test run**
  - `rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) && cmake --build build -j$(sysctl -n hw.ncpu)`
  - `ctest --test-dir build --output-on-failure`
  - All tests pass. Zero new warnings.

- [x] **7.8.2 — Verify CI compatibility**
  - Review `.github/workflows/build.yml` — CTest step already runs all registered tests.
  - Verify all new test executables are discovered by CTest.
  - No CI changes expected unless test linking requires additional libraries.

- [x] **7.8.3 — Update FEATURE_REQUESTS.md**
  - Mark FR-36 as `[x]` with resolution notes and commit hash.

- [x] **7.8.4 — Update IMPLEMENTATION_ROADMAP.md**
  - Mark all Phase 7 tasks `[x]`.
  - Update the test count in 7.5 acceptance criteria.

- [x] **7.8.5 — Update docs/ARCHITECTURE_REVIEW.md**
  - Mark §3A (implementation) as addressed.
  - Update test coverage statistics.
  - Note the refactoring done (exposed static functions, `now` parameter).

- [x] **7.8.6 — Update docs/APPLICATION_OVERVIEW.md**
  - Add test suite section or update testing mention with actual counts.

- [x] **7.8.7 — Commit and push**
  - Single commit with all test files and refactoring.
  - Message: `feat(tests): add unit test suite — 20+ tests covering core library, parsers, and theme validation (FR-36, Phase 7)`

**Acceptance:** CI green. All tracking documents updated. FR-36 marked done.

---

## Summary of Refactoring Required

All refactoring is minimal and backward-compatible:

| Change | File | Risk | Reason |
|--------|------|------|--------|
| Move `parseSmartctlJson()` to shared public static | `disk_health_info.h` + `_shared.cpp` | Low | Dedup identical code in 2 platform files, enable testing |
| `deriveHealthVerdict()` → public | `disk_health_info.h` | None | Access modifier only |
| Add `now` default param to `getNextRunTime()` | `schedule_manager.h` + `.cpp` | None | Default param preserves all callers |

No changes to production behavior. No new dependencies. No API breaks.

---

## Test File Inventory

```
tests/
├── CMakeLists.txt                          (expanded with macro)
├── utils/
│   ├── test_format_util.cpp                (existing, expanded)
│   ├── test_file_util.cpp                  (NEW — 10 methods)
│   └── test_command_util.cpp               (NEW — 9 methods)
├── core/
│   └── test_disk_health_info.cpp           (NEW — 20 methods)
├── managers/
│   ├── test_schedule_manager.cpp           (NEW — 15 methods)
│   └── test_cleaner_service.cpp            (NEW — 2 methods, may defer)
└── theme/
    └── test_theme_tokens.cpp               (NEW — 7 methods)
```

**Total: 7 test files, 66 test methods, 6 CTest-registered executables.**

Of these, the core "15-20 tests" target from the roadmap counts test *classes/files* (not methods). We have 7 files with comprehensive method coverage far exceeding the target.
