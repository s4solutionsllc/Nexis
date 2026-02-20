# Nexis — Implementation Roadmap

> Prioritized plan for incremental architectural improvements.
> Derived from [Architecture Review](./ARCHITECTURE_REVIEW.md) recommendations, open bugs, and open feature requests.
> Strategy: **Incremental Evolution** — security first, then build/theme hardening, then testing infrastructure.
> Last updated: February 2026

---

## Table of Contents

1. [Guiding Principles](#guiding-principles)
2. [Phase Overview](#phase-overview)
3. [Phase 1: Security & Data Integrity](#phase-1-security--data-integrity)
4. [Phase 2: Build System Hardening](#phase-2-build-system-hardening)
5. [Phase 3: Theme System Validation](#phase-3-theme-system-validation)
6. [Phase 4: Testing Infrastructure](#phase-4-testing-infrastructure)
7. [Phase 5: Platform Interface Formalization](#phase-5-platform-interface-formalization)
8. [Phase 6: Dependency Injection](#phase-6-dependency-injection)
9. [Phase 7: Unit Test Suite](#phase-7-unit-test-suite)
10. [Phase 8: Centralized Data Refresh](#phase-8-centralized-data-refresh)
11. [Phase 9: Future Considerations](#phase-9-future-considerations)
12. [Version Tagging Strategy](#version-tagging-strategy)

---

## Guiding Principles

1. **Security before features.** Anything that risks data loss or system integrity gets fixed first.
2. **Testing enables confident change.** Once test infrastructure exists, every subsequent phase is safer.
3. **One phase at a time.** Each phase is a self-contained unit that can be reviewed, merged, and released independently.
4. **No regressions.** Every phase includes verification steps. Build + visual QA after each change.
5. **Backward compatible.** No phase breaks existing behavior for end users.

---

## Phase Overview

| Phase | Priority | Focus Area | Tracking IDs | Est. Effort | Release |
|-------|----------|-----------|--------------|-------------|---------|
| **1** | **Critical** | Security & data integrity | BUG-43 | Medium | v1.2.2 |
| **2** | **High** | Build system hardening | Arch §1A | Small | v1.3.0 |
| **3** | **High** | Theme system validation | Arch §2C | Small | v1.3.0 |
| **4** | **High** | Testing infrastructure | Arch §3A (setup) | Small | v1.3.0 |
| **5** | **Medium** | Platform interface formalization | Arch §1B | Medium | v1.4.0 |
| **6** | **Medium** | Dependency injection | Arch §2B | Small-Med | v1.4.0 |
| **7** | **Medium** | Unit test suite | Arch §3A (tests) | Medium | v1.4.0 |
| **8** | **Low** | Centralized data refresh | Arch §2A | Large | v1.5.0 |
| **9** | **Deferred** | GUI consistency, plugin arch, event bus, CI screenshots | FR-38, FR-39, FR-40, FR-41 | — | — |

---

## Phase 1: Security & Data Integrity

**Goal:** Eliminate the most serious remaining vulnerability — the Host Manager's insecure file handling and missing validation.

**Tracking:** BUG-43

**Why first:** This is the only open bug with security implications. The Host Manager writes to `/etc/hosts` (a system-critical file) using a predictable temp file path, no input validation, no backup, no error handling, and no confirmation dialog. A symlink attack on the temp file could corrupt arbitrary system files.

### Tasks

- [x] **1.1 Replace predictable temp file with sudo tee**
  - File: `shared/nexis/Pages/Helpers/host_manage.cpp`
  - Eliminated temp file entirely; content piped via stdin to `sudo tee /etc/hosts`
  - Also fixes file ownership preservation (tee writes as root in-place)

- [x] **1.2 Add error handling on save with user feedback**
  - File: `shared/nexis/Pages/Helpers/host_manage.cpp`
  - Verifies tee success via stdout echo; auth cancellation detected via empty return
  - `QMessageBox::critical()` on failure; success message in `lblChangesMsg`

- [x] **1.3 Fix deletion logic — remove empty slot accumulation**
  - File: `shared/nexis/Pages/Helpers/host_manage.cpp`
  - Replaced `replace(lineNumber, "")` with `removeAt()` + full model rebuild via `loadTableData()`
  - No more blank lines in saved file

- [x] **1.4 Add input validation for host entries**
  - File: `shared/nexis/Pages/Helpers/host_manage.cpp`
  - IPv4/IPv6 via `QHostAddress::setAddress()`; hostname via RFC 1123 regex (with underscore tolerance)
  - Aliases validated individually; inline errors in `lblErrorMsg`
  - Fixed trailing space when aliases empty

- [x] **1.5 Create backup before write**
  - File: `shared/nexis/Pages/Helpers/host_manage.cpp`
  - `sudo cp -p /etc/hosts /etc/hosts.nexis-backup` before each save
  - Backup failure warns but does not block save

- [x] **1.6 Add confirmation dialog before save**
  - File: `shared/nexis/Pages/Helpers/host_manage.cpp`
  - `QMessageBox::question()` with change summary (N added, N modified, N deleted)
  - No-change detection shows "No changes to save" without dialog
  - Original content baseline updated after successful save

- [x] **1.7 Build verification + update tracking files**
  - Incremental build passes
  - BUG-43 marked `[x]` in `BUGS.md`
  - `docs/APPLICATION_OVERVIEW.md` Helpers section updated
  - Also removed unused `isAddHost` member from header

**Estimated effort:** 4-6 hours
**Release target:** v1.2.2 (patch — security fix)

---

## Phase 2: Build System Hardening

**Goal:** Replace `GLOB_RECURSE` with explicit source lists so file additions/removals are deterministic.

**Tracking:** Architecture Review §1A

**Why here:** This is a foundational fix that prevents a class of "file not compiling" issues. It's small effort, high payoff, and independent of other phases. Bundled into v1.3.0 since it doesn't affect user-visible behavior.

### Tasks

- [x] **2.1 Enumerate all source files currently collected by GLOB_RECURSE**
  - Enumerated into 10 categories: core shared/platform .cpp/.h, GUI shared/platform .cpp/.h, plus translations
  - Full inventory documented in `claude_definitions/PHASE2_research.md`

- [x] **2.2 Replace GLOB_RECURSE with explicit `set()` lists**
  - File: `CMakeLists.txt`
  - Replaced all 9 `file(GLOB_RECURSE ...)` calls with inline `set()` blocks
  - Platform sources use `if(APPLE)/else()` conditional blocks

- [x] **2.3 Add a comment in CMakeLists.txt documenting the convention**
  - Added: "Source files are listed explicitly (not globbed). When adding or removing a source file, update the appropriate set() block."
  - Clean rebuild passes with zero errors

- [x] **2.4 Update CLAUDE.md build instructions if needed**
  - No changes needed — build commands are unchanged

**Estimated effort:** 1-2 hours
**Release target:** v1.3.0

---

## Phase 3: Theme System Validation

**Goal:** Add build-time/runtime validation that catches QSS token mismatches, preventing an entire class of theme bugs.

**Tracking:** Architecture Review §2C

**Why here:** This is 10-15 lines of code that prevents BUG-21/33/36/38-class regressions. Tiny effort, high ongoing value. Bundled into v1.3.0.

### Tasks

- [ ] **3.1 Add token validation to `AppManager::updateStylesheet()`**
  - File: `shared/nexis/Managers/app_manager.cpp`
  - After loading QSS and values.ini, scan for `@token` patterns
  - Emit `qWarning()` for any token found in QSS but missing from values.ini
  - Skip `@dp\d+` tokens (handled separately by DPI regex)
  - Acceptance: `qWarning` output when a token is missing; existing tokens all resolve cleanly

- [ ] **3.2 Validate color format in values.ini**
  - After loading values.ini, check that color tokens start with `#` and are valid hex
  - Emit `qWarning()` for malformed values
  - Acceptance: typo like `color01 = 1e1e1e` (missing `#`) produces a warning

- [ ] **3.3 Update Architecture Review**
  - Mark §2C as addressed
  - Update the QSS Token Validation Gap weakness section

**Estimated effort:** 30 minutes
**Release target:** v1.3.0

---

## Phase 4: Testing Infrastructure

**Goal:** Set up the test framework, directory structure, and CMake configuration — without writing tests yet. This is the prerequisite for Phase 7.

**Tracking:** Architecture Review §3A (infrastructure only)

### Tasks

- [ ] **4.1 Create `tests/` directory structure**
  ```
  tests/
    CMakeLists.txt         (test target configuration)
    test_main.cpp          (Qt Test runner entry point)
    core/                  (tests for nexis-core classes)
    utils/                 (tests for utility classes)
    managers/              (tests for manager logic)
  ```

- [ ] **4.2 Add test target to root CMakeLists.txt**
  - Add `enable_testing()` and `add_subdirectory(tests)` (behind a `BUILD_TESTING` option defaulting to ON)
  - Link test executable against `nexis-core` and `Qt6::Test`
  - Register with CTest via `add_test()`
  - Acceptance: `cmake -B build -DBUILD_TESTING=ON && cmake --build build --target nexis-tests` compiles

- [ ] **4.3 Add one smoke test to validate the framework**
  - `tests/utils/test_format_util.cpp` — test `FormatUtil::formatBytes(0)` returns `"0 B"`
  - Acceptance: `ctest --test-dir build` runs and passes

- [ ] **4.4 Add CI test step (GitHub Actions)**
  - Add `ctest --test-dir build --output-on-failure` to the CI build workflow
  - Acceptance: CI builds now run tests; one passing test visible in CI logs

- [ ] **4.5 Update CLAUDE.md with test conventions**
  - Document how to add new tests, test file naming, and the test build command

**Estimated effort:** 2-3 hours
**Release target:** v1.3.0

---

## Phase 5: Platform Interface Formalization

**Goal:** Add abstract base classes for all platform-specific code so missing implementations are caught at compile time instead of link time.

**Tracking:** Architecture Review §1B

**Why now (not earlier):** This is a medium-effort refactor touching 16 classes. It's safer to do after test infrastructure exists (Phase 4) and after the build system is solid (Phase 2).

### Tasks

- [ ] **5.1 Convert Info classes to abstract interfaces (11 classes)**
  - For each class in `shared/nexis-core/Info/`:
    - Make methods `virtual ... = 0` in the shared header
    - Add `virtual ~ClassName() = default`
    - Rename platform implementations (e.g., `CpuInfoLinux`, `CpuInfoMacOS`)
    - Platform `.cpp` files include the platform-specific header and implement overrides
  - Classes: `CpuInfo`, `MemoryInfo`, `DiskInfo`, `NetworkInfo`, `SystemInfo`, `ProcessInfo`, `ThermalInfo`, `GpuInfo`, `BatteryInfo`, `DiskHealthInfo`, plus shared `disk_info_shared.cpp` logic
  - Can be done incrementally — one class at a time

- [ ] **5.2 Convert Tool classes to abstract interfaces (5 classes)**
  - Same pattern for: `PackageTool`, `ServiceTool`, `AptSourceTool`, `GnomeSettingsTool`, `DockerTool`
  - Note: `DockerTool` has a shared implementation (no platform split) — it becomes the concrete class directly

- [ ] **5.3 Update InfoManager to use platform-specific subclasses**
  - File: `shared/nexis/Managers/info_manager.h`
  - Change member types from concrete classes to `std::unique_ptr<Interface>`
  - Use `#ifdef Q_OS_MAC` to instantiate the correct subclass
  - Same for `ToolManager`

- [ ] **5.4 Update explicit source lists (Phase 2) with renamed files**

- [ ] **5.5 Build verification on macOS**
  - Full clean rebuild
  - Acceptance: all 16 classes compile; no linker errors; app behavior unchanged

- [ ] **5.6 Update Architecture Review**
  - Mark §1B as addressed
  - Update weakness §1 (No Formal Platform Interfaces) to note it's resolved

**Estimated effort:** 8-12 hours (can be split across multiple sessions)
**Release target:** v1.4.0

---

## Phase 6: Dependency Injection

**Goal:** Add constructor parameters with defaults to all page constructors, enabling test mocking without breaking production code.

**Tracking:** Architecture Review §2B

**Why after Phase 5:** Abstract base classes (Phase 5) give us interfaces to inject. Without them, DI would inject concrete singletons — less useful for testing.

### Tasks

- [ ] **6.1 Add DI constructor parameters to all 14 page classes**
  - Pattern:
    ```cpp
    explicit DashboardPage(QWidget *parent = nullptr,
                           InfoManager *infoMgr = InfoManager::ins(),
                           SettingManager *settingMgr = SettingManager::ins());
    ```
  - Default values mean zero changes to production call sites
  - Each page gets parameters for the managers it actually uses

- [ ] **6.2 Replace member initializers with constructor args**
  - Change `im(InfoManager::ins())` in member initializer lists to use the injected parameter
  - Acceptance: `grep -r "InfoManager::ins()" shared/nexis/Pages/` shows zero results outside constructors' default arguments

- [ ] **6.3 Build verification**
  - Acceptance: incremental build passes; no behavior change

- [ ] **6.4 Update Architecture Review**
  - Mark §2B as addressed
  - Update weakness §2 (Singleton Coupling) to note DI escape hatches are in place

**Estimated effort:** 3-4 hours
**Release target:** v1.4.0

---

## Phase 7: Unit Test Suite

**Goal:** Write 15-20 unit tests covering the core library and utility functions, targeting the highest-risk areas identified by past bugs.

**Tracking:** Architecture Review §3A (implementation)

**Why after Phase 6:** DI constructors (Phase 6) enable testing manager-dependent code with mocks.

### Tasks

- [ ] **7.1 Utility class tests (pure functions — easiest)**
  - `test_format_util.cpp` — expand smoke test to cover:
    - `formatBytes()`: 0, 1023, 1024, 1536, 1 GiB, 1 TiB, negative values
  - `test_file_util.cpp`:
    - `readStringFromFile()` — existing file, missing file, empty file
    - `getFileSize()` — regular file, directory, nonexistent
  - `test_command_util.cpp`:
    - `exec()` with valid command, invalid command, timeout

- [ ] **7.2 Info class parsing tests (regression prevention)**
  - `test_memory_info.cpp`:
    - Parse known `/proc/meminfo` content with correct values (prevents BUG-01 regression)
    - Handle short meminfo (fewer than 8 lines — BUG-27 guard)
  - `test_disk_health_info.cpp`:
    - Parse known `smartctl` JSON output for NVMe and SATA drives
    - Handle missing/empty smartctl output
  - `test_cpu_info.cpp`:
    - Core count parsing (ensure `int` not `quint8` — BUG-28)

- [ ] **7.3 Manager logic tests**
  - `test_cleaner_service.cpp`:
    - Scan categorization (given a mock directory structure, verify correct category assignment)
    - Min-age filtering
  - `test_schedule_manager.cpp`:
    - CRUD operations on JSON schedule data
    - Frequency calculation (next run time)

- [ ] **7.4 Theme validation tests**
  - `test_theme_tokens.cpp`:
    - Load `default/values.ini` and `light/values.ini`
    - Verify all tokens referenced in `style.qss` have values in both themes
    - Verify all color values start with `#` and are valid hex
    - This codifies Phase 3's runtime validation as a permanent test

- [ ] **7.5 CI integration**
  - All tests run in CI on both macOS and Linux
  - Acceptance: 15+ tests passing in CI; coverage of FormatUtil, MemoryInfo parsing, DiskHealthInfo parsing, CleanerService categorization, and theme token validation

**Estimated effort:** 6-8 hours
**Release target:** v1.4.0

---

## Phase 8: Centralized Data Refresh

**Goal:** Replace the ~25 per-page QTimers with a centralized `DataRefreshService` that polls at defined intervals and emits data-change signals. Add pause/resume for battery optimization.

**Tracking:** Architecture Review §2A

**Why last (among active phases):** This is the largest refactor, touching all 14 pages. It's safer to do after the test suite exists (Phase 7) so regressions are caught automatically. The current timer approach works — it's inefficient, not broken.

### Tasks

- [ ] **8.1 Create DataRefreshService**
  - File: `shared/nexis/Managers/data_refresh_service.h/.cpp`
  - Singleton with 3 QTimers:
    - Fast (1s): CPU, memory, network, GPU
    - Medium (5s): disk usage, battery
    - Slow (30s): disk health, thermal
  - Emits typed signals with data payloads (not raw pointers)
  - `pause()` and `resume()` methods for background optimization

- [ ] **8.2 Add App-level pause/resume integration**
  - File: `shared/nexis/app.cpp`
  - Call `DataRefreshService::pause()` when window is minimized to tray
  - Call `DataRefreshService::resume()` when window is restored
  - Kiosk mode always keeps refresh active

- [ ] **8.3 Migrate DashboardPage (3 timers → 0 timers)**
  - Remove `mTimer`, `timerDisk`, `timerDiskHealth`
  - Connect to `DataRefreshService` signals
  - Acceptance: Dashboard displays identical data; no QTimers in DashboardPage

- [ ] **8.4 Migrate ResourcesPage (multiple timers → 0 timers)**
  - Same pattern
  - Verify duplicate `updateMemoryInfo()` calls are eliminated

- [ ] **8.5 Migrate remaining pages incrementally**
  - Processes, Services, Docker, and other pages with timers
  - Some pages (Settings, Hardware Info, Search) may not have timers — skip if not applicable

- [ ] **8.6 Remove dead timer code**
  - Grep for any remaining `new QTimer` in page code
  - Acceptance: `grep -r "new QTimer" shared/nexis/Pages/` returns zero results

- [ ] **8.7 Add unit tests for DataRefreshService**
  - Test pause/resume state transitions
  - Test that signal emission rates match configured intervals (mock timer)

- [ ] **8.8 Update Architecture Review**
  - Mark §2A as addressed
  - Update weakness §6 (Fragmented Timer/Polling) to note centralization

**Estimated effort:** 12-16 hours (split across multiple sessions)
**Release target:** v1.5.0

---

## Phase 9: Future Considerations

These items are intentionally deferred. They should be reconsidered if/when their trigger conditions are met.

### 9A. GUI Consistency (FR-38)
- **Scope:** Replace all `QIcon::fromTheme()` with bundled SVGs (FR-25), remove hardcoded Ubuntu font from `.ui` files (FR-26), fix Settings page grid layout (BUG-44)
- **Current status:** Deferred — functional but visually inconsistent across desktop environments
- **When to revisit:** When ready to focus on visual polish and cross-DE consistency

### 9B. Plugin Architecture (FR-39)
- **Scope:** Define a `NexisPlugin` interface for third-party pages, cleaning categories, and monitoring widgets. Architecture Review §4A.
- **Trigger:** Community demand for extensibility (e.g., custom cleaner categories, hardware-specific monitoring)
- **Current status:** Not needed. 14 built-in pages cover all core use cases.
- **When to revisit:** If Nexis gains significant community adoption and extension requests appear

### 9C. Event Bus Upgrade (FR-40)
- **Scope:** Replace `SignalMapper` with a typed event bus library (e.g., eventpp). Architecture Review §4B.
- **Trigger:** SignalMapper signal count exceeds ~15
- **Current status:** 7 signals — well within the simple singleton's comfort zone
- **When to revisit:** If multiple new cross-component features push the signal count significantly higher

### 9D. CI Screenshot Regression Tests (FR-41)
- **Scope:** Per-page screenshot capture with perceptual diff comparison in CI. Architecture Review §3B.
- **Trigger:** After Phase 7 (unit tests) is stable and BUG-30-class visual regressions recur
- **Current status:** Deferred — requires maintaining reference images and setting up headless rendering in CI
- **When to revisit:** After the unit test suite has been running successfully for several releases

---

## Version Tagging Strategy

| Version | Contents | Type |
|---------|----------|------|
| **v1.2.2** | Phase 1 (BUG-43 security fix) | Patch |
| **v1.3.0** | Phases 2-4 (build, theme, test infra) | Minor |
| **v1.4.0** | Phases 5-7 (platform interfaces, DI, tests) | Minor |
| **v1.5.0** | Phase 8 (centralized data refresh) | Minor |

Each release follows the existing workflow: version bump in `CMakeLists.txt`, clean rebuild, annotated tag, GitHub release.

---

## Dependencies Between Phases

```
Phase 1 (Security)          ← Independent, do first
Phase 2 (Build Hardening)   ← Independent
Phase 3 (Theme Validation)  ← Independent
Phase 4 (Test Infra)        ← Independent

Phase 5 (Interfaces)        ← Depends on Phase 2 (explicit source lists)
Phase 6 (DI)                ← Depends on Phase 5 (interfaces to inject)
Phase 7 (Tests)             ← Depends on Phase 4 + Phase 6 (framework + DI)

Phase 8 (Data Refresh)      ← Depends on Phase 7 (tests catch regressions)
```

Phases 1-4 can be done in any order or even in parallel. Phases 5-8 must follow the order shown.

---

## Tracking

When work begins on a phase:
1. Create `claude_definitions/{ID}_research.md` and `{ID}_plan.md` per the standard workflow
2. Update the checkboxes in this document as tasks complete
3. Update `BUGS.md` and `FEATURE_REQUESTS.md` with resolution notes
4. Update `docs/APPLICATION_OVERVIEW.md` and `docs/ARCHITECTURE_REVIEW.md` per the Documentation Maintenance rules in `CLAUDE.md`
