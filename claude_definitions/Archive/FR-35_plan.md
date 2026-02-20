# FR-35 Plan: Dependency Injection for Page Constructors

## Overview

Add optional manager pointer parameters to all page constructors with `nullptr` defaults. When `nullptr`, the page falls back to the existing `::ins()` singleton. This enables test injection while making zero changes to production call sites in `app.cpp`.

**Depends on:** FR-34 (Phase 5 — abstract base classes) ✅ Complete

---

## Strategy

**Pattern:** `nullptr`-default constructor parameters with ternary fallback in member initializers.

```cpp
// Header
explicit DashboardPage(QWidget *parent = nullptr,
                       InfoManager *infoManager = nullptr,
                       SettingManager *settingManager = nullptr,
                       AppManager *appManager = nullptr,
                       SignalMapper *signalMapper = nullptr);

// Constructor initializer list
im(infoManager ? infoManager : InfoManager::ins()),
mSettingManager(settingManager ? settingManager : SettingManager::ins()),
mAppManager(appManager ? appManager : AppManager::ins()),
mSignalMapper(signalMapper ? signalMapper : SignalMapper::ins())
```

**Key design decisions:**

1. **Only inject what each page actually uses.** 4 pages have zero manager dependencies and need no constructor changes (StartupAppsPage, HelpersPage, GnomeSettingsPage, DockerPage).

2. **Store all managers as members.** Pages that currently use `::ins()` inline in methods (e.g., SystemCleanerPage calling `CleanerService::ins()->scan(...)`) will gain new member variables. This consolidates all singleton access into the constructor.

3. **Forward-declare managers in headers, include in .cpp.** Minimizes header coupling. Pages that already `#include` the manager header keep the include.

4. **Worker thread lambdas** already capture `this` — they will automatically use the member pointer instead of `::ins()`. No special handling needed.

5. **SignalMapper** is currently never stored as a member anywhere. Pages that use it will gain a `mSignalMapper` member.

6. **Child widget dependencies** (CircleBar, HistoryChart, etc.) are **out of scope**. They will continue using `::ins()` directly. This is documented as a known limitation.

---

## Tasks

### Phase A: Simple pages (1 manager dependency) — 4 pages

- [x] **A.1 HardwareInfoPage** — Add `InfoManager *` parameter
  - File: `shared/nexis/Pages/HardwareInfo/hardware_info_page.h`
  - File: `shared/nexis/Pages/HardwareInfo/hardware_info_page.cpp`
  - Current: `im(InfoManager::ins())` in initializer list
  - Change: Add `InfoManager *infoManager = nullptr` parameter; initializer becomes `im(infoManager ? infoManager : InfoManager::ins())`
  - Already stores `im` as member — minimal change

- [x] **A.2 ProcessesPage** — Add `InfoManager *` parameter
  - File: `shared/nexis/Pages/Processes/processes_page.h`
  - File: `shared/nexis/Pages/Processes/processes_page.cpp`
  - Same pattern as A.1

- [x] **A.3 ResourcesPage** — Add `InfoManager *` parameter
  - File: `shared/nexis/Pages/Resources/resources_page.h`
  - File: `shared/nexis/Pages/Resources/resources_page.cpp`
  - Same pattern as A.1
  - Note: `im` is used in the initializer list for `mChartCpu(new HistoryChart(..., im->getCpuCoreCount(), ...))` — the ternary must execute before this. Since member initializers run in declaration order, declare `im` before the chart members in the header.

- [x] **A.4 ServicesPage** — Add `ToolManager *` parameter
  - File: `shared/nexis/Pages/Services/services_page.h`
  - File: `shared/nexis/Pages/Services/services_page.cpp`
  - Currently no member — add `ToolManager *mToolManager;`
  - Replace `ToolManager::ins()->getServices()` call in method body with `mToolManager->getServices()`
  - Add `#include "Managers/tool_manager.h"` to header (or forward declare + include in .cpp)

### Phase B: Medium pages (2 manager dependencies) — 2 pages

- [x] **B.1 SearchPage** — Add `InfoManager *`, `SettingManager *` parameters
  - File: `shared/nexis/Pages/Search/search_page.h`
  - File: `shared/nexis/Pages/Search/search_page.cpp`
  - Currently no manager members — add `InfoManager *mInfoManager;` and `SettingManager *mSettingManager;`
  - Replace all `InfoManager::ins()->` and `SettingManager::ins()->` calls in methods with member access
  - Forward declare managers in header

- [x] **B.2 APTSourceManagerPage** — Add `ToolManager *`, `SignalMapper *` parameters
  - File: `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.h`
  - File: `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp`
  - Currently no manager members — add `ToolManager *mToolManager;` and `SignalMapper *mSignalMapper;`
  - Replace all `ToolManager::ins()->` and `SignalMapper::ins()->` calls
  - SignalMapper already included in .cpp for connect/emit

### Phase C: Complex pages (3-4 manager dependencies) — 4 pages

- [x] **C.1 DashboardPage** — Add `InfoManager *`, `SettingManager *`, `AppManager *`, `SignalMapper *`
  - File: `shared/nexis/Pages/Dashboard/dashboard_page.h`
  - File: `shared/nexis/Pages/Dashboard/dashboard_page.cpp`
  - Already has `im` and `mSettingManager` members — add ternary defaults
  - Add new members `mAppManager` and `mSignalMapper`
  - Replace `AppManager::ins()->` calls (tray icon alerts) with `mAppManager->`
  - Replace `SignalMapper::ins()->` calls (kiosk signals) with `mSignalMapper->`
  - Forward declare `AppManager` and `SignalMapper` in header (AppManager already included; SignalMapper only in .cpp)

- [x] **C.2 UninstallerPage** — Add `ToolManager *`, `AppManager *`, `SignalMapper *`
  - File: `shared/nexis/Pages/Uninstaller/uninstaller_page.h`
  - File: `shared/nexis/Pages/Uninstaller/uninstaller_page.cpp`
  - Already has `tm` member — add ternary default
  - Add new members `mAppManager` and `mSignalMapper`
  - Replace `AppManager::ins()->` and `SignalMapper::ins()->` calls in init and worker lambdas
  - Worker lambdas capture `this` — member access works automatically

- [x] **C.3 SystemCleanerPage** — Add `AppManager *`, `SignalMapper *`, `CleanerService *`, `ScheduleManager *`
  - File: `shared/nexis/Pages/SystemCleaner/system_cleaner_page.h`
  - File: `shared/nexis/Pages/SystemCleaner/system_cleaner_page.cpp`
  - Currently no manager members — add all 4
  - Replace all `AppManager::ins()->`, `SignalMapper::ins()->`, `CleanerService::ins()->`, `ScheduleManager::ins()->` calls
  - Forward declare all 4 managers in header

- [x] **C.4 SettingsPage** — Add `AppManager *`, `SettingManager *`, `InfoManager *`, `ScheduleManager *`
  - File: `shared/nexis/Pages/Settings/settings_page.h`
  - File: `shared/nexis/Pages/Settings/settings_page.cpp`
  - Already has `apm` and `mSettingManager` members — add ternary defaults
  - Add new members `mInfoManager` and `mScheduleManager`
  - Replace `InfoManager::ins()->` and `ScheduleManager::ins()->` calls (extensive — ~15 call sites for ScheduleManager)
  - Forward declare `ScheduleManager` and `InfoManager` in header (or include — InfoManager already imported transitively)

### Phase D: Verification and Documentation

- [x] **D.1 Build verification**
  - Clean rebuild: `rm -rf build && cmake -B build ... && cmake --build build -j$(sysctl -n hw.ncpu)`
  - All existing tests pass: `ctest --test-dir build --output-on-failure`
  - App launches and all pages display correctly

- [x] **D.2 Verify zero `::ins()` calls remain in page code (outside default args)**
  - Run: `grep -rn "InfoManager::ins()\|SettingManager::ins()\|ToolManager::ins()\|AppManager::ins()\|SignalMapper::ins()\|CleanerService::ins()\|ScheduleManager::ins()" shared/nexis/Pages/`
  - Only matches should be in constructor default argument declarations
  - Exception: 4 pages with zero dependencies (StartupAppsPage, HelpersPage, GnomeSettingsPage, DockerPage) may have none at all

- [x] **D.3 Update Architecture Review**
  - File: `docs/ARCHITECTURE_REVIEW.md`
  - Mark weakness §2 (Singleton Coupling) as partially resolved — DI escape hatches in place for all pages with manager dependencies
  - Update the code example to show the new DI constructor pattern
  - Note that child widgets still use `::ins()` directly (known limitation, future scope)

- [x] **D.4 Update IMPLEMENTATION_ROADMAP.md**
  - Mark all Phase 6 tasks `[x]`

- [x] **D.5 Update FEATURE_REQUESTS.md**
  - Mark FR-35 as `[x]` with resolution note

- [x] **D.6 Commit and push**
  - Commit message: `refactor(pages): add dependency injection to page constructors (FR-35)`

---

## Acceptance Criteria

1. All 10 pages with manager dependencies accept optional manager pointers via constructor
2. Default values are `nullptr`, falling back to `::ins()` singletons
3. No `::ins()` calls remain in page `.cpp` method bodies (only in constructor default args)
4. Production call sites in `app.cpp` are unchanged (all use default args)
5. Clean build passes with zero new warnings
6. All existing tests pass
7. App launches and all pages display correctly (visual QA)

---

## Files Changed (Summary)

| File | Change |
|------|--------|
| `shared/nexis/Pages/HardwareInfo/hardware_info_page.h` | Add `InfoManager *` constructor param |
| `shared/nexis/Pages/HardwareInfo/hardware_info_page.cpp` | Ternary in initializer |
| `shared/nexis/Pages/Processes/processes_page.h` | Add `InfoManager *` constructor param |
| `shared/nexis/Pages/Processes/processes_page.cpp` | Ternary in initializer |
| `shared/nexis/Pages/Resources/resources_page.h` | Add `InfoManager *` constructor param; reorder member declarations |
| `shared/nexis/Pages/Resources/resources_page.cpp` | Ternary in initializer |
| `shared/nexis/Pages/Services/services_page.h` | Add `ToolManager *` constructor param + member |
| `shared/nexis/Pages/Services/services_page.cpp` | Ternary in initializer; replace `::ins()` in method |
| `shared/nexis/Pages/Search/search_page.h` | Add `InfoManager *`, `SettingManager *` params + members |
| `shared/nexis/Pages/Search/search_page.cpp` | Ternary in initializer; replace `::ins()` in methods |
| `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.h` | Add `ToolManager *`, `SignalMapper *` params + members |
| `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp` | Ternary in initializer; replace `::ins()` in methods |
| `shared/nexis/Pages/Dashboard/dashboard_page.h` | Add 4 params; add `mAppManager`, `mSignalMapper` members |
| `shared/nexis/Pages/Dashboard/dashboard_page.cpp` | Ternaries; replace `::ins()` in methods |
| `shared/nexis/Pages/Uninstaller/uninstaller_page.h` | Add 3 params; add `mAppManager`, `mSignalMapper` members |
| `shared/nexis/Pages/Uninstaller/uninstaller_page.cpp` | Ternaries; replace `::ins()` in methods + worker lambdas |
| `shared/nexis/Pages/SystemCleaner/system_cleaner_page.h` | Add 4 params + 4 members |
| `shared/nexis/Pages/SystemCleaner/system_cleaner_page.cpp` | Ternaries; replace all `::ins()` calls |
| `shared/nexis/Pages/Settings/settings_page.h` | Add 4 params; add `mInfoManager`, `mScheduleManager` members |
| `shared/nexis/Pages/Settings/settings_page.cpp` | Ternaries; replace `::ins()` calls (~15 sites) |
| `docs/ARCHITECTURE_REVIEW.md` | Update §2B (Singleton Coupling) |
| `docs/IMPLEMENTATION_ROADMAP.md` | Mark Phase 6 tasks complete |
| `FEATURE_REQUESTS.md` | Mark FR-35 `[x]` |

**Files NOT changed:** `shared/nexis/app.cpp` (all default args — zero changes), StartupAppsPage, HelpersPage, GnomeSettingsPage, DockerPage (zero manager dependencies).

---

## Risk Mitigation

- **Build after each phase** (A, B, C, D) to catch issues incrementally
- **Member declaration order** matters in C++ — `im` must be declared before any members that use `im` in their initializers (ResourcesPage)
- **Worker thread safety** — no new risk; member pointers are initialized in the constructor and never reassigned
- **No ABI concern** — this is an application, not a library
