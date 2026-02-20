# FR-37: Centralized DataRefreshService — Implementation Plan

## Overview

Replace 6 per-page QTimers (across Dashboard, Resources, and Processes) with a single `DataRefreshService` singleton that owns 4 QTimers, polls InfoManager once per interval, and distributes typed data via Qt signals. Pages become reactive subscribers. Adds pause/resume when the app is minimized to tray (skipped in kiosk mode).

**Key wins:**
- 6 QTimers → 4 QTimers (in one location)
- Eliminates double `updateMemoryInfo()`, `updateGpuInfo()`, and `refreshDiskHealth()` calls
- Fixes `getCpuPercents()` static-delta bug (two callers consuming same state)
- Battery optimization: all polling pauses when minimized (unless kiosk mode)
- Pages become simpler — subscribe to data, don't manage polling

---

## Phase 1: Create DataRefreshService Singleton

### Task 1.1: Create `data_refresh_service.h`
- [x] New file: `shared/nexis/Managers/data_refresh_service.h`
- [x] Class inherits `QObject` with `Q_OBJECT` macro
- [x] Static `ins()` singleton accessor (same pattern as other managers)
- [x] DI constructor parameter: `InfoManager *infoManager = nullptr` (with `nullptr` → `InfoManager::ins()` fallback)
- [x] Four `QTimer *` members: `mFastTimer` (1s), `mMediumTimer` (5s), `mSlowTimer` (30s), `mProcessTimer` (configurable)
- [x] `bool mPaused` member
- [x] Public methods: `start()`, `pause()`, `resume()`, `isPaused()`, `setProcessRefreshInterval(int seconds)`
- [x] Signals — typed data payloads:
  - `void cpuUpdated(QList<int> percents, double clockGHz, QList<double> loadAvgs)`
  - `void memoryUpdated(quint64 used, quint64 total, quint64 swapUsed, quint64 swapTotal)`
  - `void networkUpdated(quint64 rxBytes, quint64 txBytes)`
  - `void gpuUpdated(QList<GpuDevice> devices)` (only emitted if `hasGpu()`)
  - `void tempUpdated()` (signal-only trigger; pages read sensor by index themselves)
  - `void batteryUpdated(BatteryData data)` (only emitted if `hasBattery()`)
  - `void diskIOUpdated(QList<quint64> io)`
  - `void diskUsageUpdated(QList<Disk> disks)`
  - `void diskHealthUpdated(QList<DriveHealth> drives)` (only emitted if `hasDiskHealth()`)
  - `void processesUpdated(QList<Process> processes, QString userName)`
- [x] Private slots for each timer tier:
  - `onFastTick()` — calls InfoManager for CPU/memory/network/GPU/temp/battery/diskIO, emits signals
  - `onMediumTick()` — calls `updateDiskInfo()`, emits `diskUsageUpdated`
  - `onSlowTick()` — calls `refreshDiskHealth()`, emits `diskHealthUpdated`
  - `onProcessTick()` — calls `updateProcesses()`, emits `processesUpdated`

### Task 1.2: Create `data_refresh_service.cpp`
- [x] New file: `shared/nexis/Managers/data_refresh_service.cpp`
- [x] Singleton implementation (`static DataRefreshService *instance`)
- [x] Constructor: create 4 timers, connect `timeout` signals to tick slots, **do not start** (caller uses `start()`)
- [x] `start()`: start all 4 timers at their intervals, fire one immediate tick for each tier to initialize data
- [x] `pause()`: if already paused or kiosk mode active, return. Stop all 4 timers, set `mPaused = true`
- [x] `resume()`: if not paused, return. Set `mPaused = false`, fire immediate fast+medium ticks, restart all 4 timers
- [x] `setProcessRefreshInterval(int ms)`: update `mProcessTimer->setInterval(ms)`
- [x] `onFastTick()` implementation:
  ```
  1. Call im->getCpuPercents(), im->getCpuClock(), im->getCpuLoadAvgs() → emit cpuUpdated(...)
  2. Call im->updateMemoryInfo() → emit memoryUpdated(used, total, swapUsed, swapTotal)
  3. Read im->getRXbytes(), im->getTXbytes() → emit networkUpdated(...)
  4. Read im->getDiskIO() → emit diskIOUpdated(...)
  5. If hasGpu(): call im->updateGpuInfo() → emit gpuUpdated(im->getGpuDevices())
  6. If hasThermalSensors(): emit tempUpdated()
  7. If hasBattery(): call im->updateBatteryInfo() → emit batteryUpdated(im->getBatteryData())
  ```
- [x] `onMediumTick()`: call `im->updateDiskInfo()`, emit `diskUsageUpdated(im->getDisks())`
- [x] `onSlowTick()`: call `im->refreshDiskHealth()`, emit `diskHealthUpdated(im->getDriveHealth())`
- [x] `onProcessTick()`: call `im->updateProcesses()`, emit `processesUpdated(im->getProcesses(), im->getUserName())`

### Task 1.3: Register in CMakeLists.txt
- [x] Add `data_refresh_service.cpp` to `GUI_SHARED_SRCS`
- [x] Add `data_refresh_service.h` to `GUI_SHARED_HDRS`

### Task 1.4: Build verification
- [x] Incremental build succeeds with new files (no consumers yet)

**Acceptance:** New singleton compiles, links, and is ready for consumers. No behavior change yet.

---

## Phase 2: Add App Visibility Signal & Wire Pause/Resume

### Task 2.1: Add `sigAppVisibilityChanged` to SignalMapper
- [x] Add signal `void sigAppVisibilityChanged(bool visible)` to `signal_mapper.h`

### Task 2.2: Emit visibility signal from App
- [x] In `App::changeEvent()` (line 201): before `hide()`, emit `SignalMapper::ins()->sigAppVisibilityChanged(false)`
- [x] In the tray icon `activated` handler (line 223): after `show()`, emit `SignalMapper::ins()->sigAppVisibilityChanged(true)`
- [x] In `App::clickSidebarButton()` when `isShow == true` (line 256): emit `sigAppVisibilityChanged(true)`

### Task 2.3: Start DataRefreshService from App::init()
- [x] In `App::init()`, after page construction but before `updateStylesheet()`: call `DataRefreshService::ins()->start()`
- [x] Include `data_refresh_service.h` in `app.cpp`

### Task 2.4: Wire pause/resume
- [x] In `DataRefreshService` constructor: connect `SignalMapper::ins()->sigAppVisibilityChanged(bool)` to:
  - `true` → `resume()`
  - `false` → `pause()`
- [x] In `pause()`: check `SettingManager::ins()->getKioskMode()` — if kiosk, skip pause

### Task 2.5: Build verification
- [x] Incremental build succeeds
- [x] App runs — DataRefreshService is started, but no pages subscribe yet (signals fire into void)

**Acceptance:** Service starts with app, pauses on minimize, resumes on restore. No visible behavior change yet.

---

## Phase 3: Convert Dashboard Page

### Task 3.1: Remove Dashboard timers
- [x] Remove `QTimer *mTimer` member from `dashboard_page.h`
- [x] Remove `#include <QTimer>` from `dashboard_page.h`
- [x] Remove `mTimer(new QTimer(this))` from constructor initializer list
- [x] Remove all `connect(mTimer, ...)` calls from `init()` (lines 89, 121, 130, 147-149)
- [x] Remove `mTimer->start(1 * 1000)` (line 155)
- [x] Remove local `QTimer *timerDisk` creation and start (lines 151-153)
- [x] Remove local `QTimer *timerDiskHealth` creation and start (lines 138-143)

### Task 3.2: Add DataRefreshService connections
- [x] Add `#include "Managers/data_refresh_service.h"` to `dashboard_page.cpp`
- [x] Add DI parameter: `DataRefreshService *refreshService = nullptr` to constructor (with `nullptr` → `DataRefreshService::ins()` fallback)
- [x] Add `DataRefreshService *mRefresh` member
- [x] Connect signals in `init()`:
  - `mRefresh->cpuUpdated` → new slot `onCpuUpdated(QList<int>, double, QList<double>)`
  - `mRefresh->memoryUpdated` → new slot `onMemoryUpdated(quint64, quint64, quint64, quint64)`
  - `mRefresh->networkUpdated` → new slot `onNetworkUpdated(quint64, quint64)`
  - `mRefresh->diskUsageUpdated` → new slot `onDiskUsageUpdated(QList<Disk>)`
  - Conditional: `mRefresh->tempUpdated` → `updateTempBar()` (signature unchanged — reads sensor by index)
  - Conditional: `mRefresh->gpuUpdated` → new slot `onGpuUpdated(QList<GpuDevice>)`
  - Conditional: `mRefresh->batteryUpdated` → new slot `onBatteryUpdated(BatteryData)`
  - Conditional: `mRefresh->diskHealthUpdated` → new slot `onDiskHealthUpdated(QList<DriveHealth>)`

### Task 3.3: Refactor update slot signatures
- [x] `updateCpuBar()` → `onCpuUpdated(const QList<int> &percents, double clockGHz, const QList<double> &loadAvgs)`
  - Body changes: replace `im->getCpuPercents().at(0)` with `percents.at(0)`, replace `im->getCpuClock()` with `clockGHz * 1000.0` (or pass as MHz)
  - Keep alert logic intact (reads `mSettingManager`, shows tray message)
- [x] `updateMemoryBar()` → `onMemoryUpdated(quint64 used, quint64 total, quint64 swapUsed, quint64 swapTotal)`
  - Remove `im->updateMemoryInfo()` call
  - Replace `im->getMemUsed()` with `used`, etc.
  - Keep alert logic intact
- [x] `updateNetworkBar()` → `onNetworkUpdated(quint64 rxBytes, quint64 txBytes)`
  - Replace `im->getRXbytes()` / `im->getTXbytes()` with parameters
  - Keep static delta vars and calculation logic
- [x] `updateDiskBar()` → `onDiskUsageUpdated(const QList<Disk> &disks)`
  - Remove `im->updateDiskInfo()` call
  - Replace `im->getDisks()` with `disks` parameter
  - Keep disk selection and alert logic
- [x] `updateGpuBar()` → `onGpuUpdated(const QList<GpuDevice> &gpus)`
  - Remove `im->updateGpuInfo()` call
  - Replace `im->getGpuDevices()` with `gpus` parameter
- [x] `updateBatteryBar()` → `onBatteryUpdated(const BatteryData &bat)`
  - Remove `im->updateBatteryInfo()` call
  - Replace `im->getBatteryData()` with `bat` parameter
  - Keep alert logic
- [x] `updateDiskHealthBar()` → `onDiskHealthUpdated(const QList<DriveHealth> &drives)`
  - Replace `im->getDriveHealth()` with `drives` parameter
  - Keep alert logic
- [x] `updateTempBar()` — signature unchanged (still reads sensor by index from InfoManager)

### Task 3.4: Keep initial data calls
- [x] The explicit init calls at lines 158-169 (`updateCpuBar()`, etc.) can be removed since `DataRefreshService::start()` fires an immediate tick before pages are shown. **Verify** that `start()` is called before pages are visible.

### Task 3.5: Update header file
- [x] Update slot declarations in `dashboard_page.h` with new signatures
- [x] Add `DataRefreshService *mRefresh` member
- [x] Add DI parameter to constructor

### Task 3.6: Build verification
- [x] Incremental build succeeds
- [x] Run app — Dashboard shows CPU, memory, disk, network, GPU, battery, temp, disk health updating at correct intervals

**Acceptance:** Dashboard has zero QTimers. All gauges update from DataRefreshService signals at correct intervals. Alerts still work. Initial data loads on startup.

---

## Phase 4: Convert Resources Page

### Task 4.1: Remove Resources timers
- [x] Remove `QTimer *mTimer` member from `resources_page.h`
- [x] Remove `#include <QTimer>` from `resources_page.h`
- [x] Remove `mTimer(new QTimer(this))` from constructor initializer list
- [x] Remove all `connect(mTimer, ...)` calls (lines 71-78)
- [x] Remove `mTimer->start(1000)` (line 87)
- [x] Remove local `QTimer *diskHealthTimer` creation and start (lines 81-83)

### Task 4.2: Add DataRefreshService connections
- [x] Add `#include "Managers/data_refresh_service.h"` to `resources_page.cpp`
- [x] Add DI parameter: `DataRefreshService *refreshService = nullptr` to constructor
- [x] Add `DataRefreshService *mRefresh` member
- [x] Connect signals in `init()`:
  - `mRefresh->cpuUpdated` → new slot `onCpuUpdated(QList<int>, double, QList<double>)`
  - `mRefresh->memoryUpdated` → new slot `onMemoryUpdated(quint64, quint64, quint64, quint64)`
  - `mRefresh->networkUpdated` → new slot `onNetworkUpdated(quint64, quint64)`
  - `mRefresh->diskIOUpdated` → new slot `onDiskIOUpdated(QList<quint64>)`
  - Conditional: `mRefresh->gpuUpdated` → new slot `onGpuUpdated(QList<GpuDevice>)`
  - Conditional: `mRefresh->diskHealthUpdated` → new slot `onDiskHealthUpdated(QList<DriveHealth>)`

### Task 4.3: Refactor update slot signatures
- [x] `updateCpuChart()` → `onCpuUpdated(const QList<int> &percents, double clockGHz, const QList<double> &loadAvgs)`
  - Replace `im->getCpuPercents()` with `percents`
  - Also handle loadAvgs in same slot (or split — see design note below)
- [x] **Design note:** CPU chart and CPU load avg chart are currently separate slots both triggered by 1s timer. They can either:
  - **(A)** Both connect to `cpuUpdated` signal and share the data → preferred, single signal
  - **(B)** Keep separate slot `onCpuLoadAvgUpdated` → requires separate signal or same signal
  - **Decision:** Use approach (A). Create `onCpuUpdated()` that updates both charts from the combined payload.
- [x] `updateMemoryChart()` → `onMemoryUpdated(quint64 used, quint64 total, quint64 swapUsed, quint64 swapTotal)`
  - Remove `im->updateMemoryInfo()` call
- [x] `updateNetworkChart()` → `onNetworkUpdated(quint64 rxBytes, quint64 txBytes)`
  - Replace `im->getRXbytes()` / `im->getTXbytes()` with parameters
  - Keep static delta vars
- [x] `updateDiskReadWrite()` → `onDiskIOUpdated(const QList<quint64> &io)`
  - Replace `im->getDiskIO()` with `io` parameter
  - Keep static delta vars
- [x] `updateGpuChart()` → `onGpuUpdated(const QList<GpuDevice> &gpus)`
  - Remove `im->updateGpuInfo()` call
  - Replace `im->getGpuDevices()` with `gpus` parameter
- [x] `updateDiskHealthChart()` → `onDiskHealthUpdated(const QList<DriveHealth> &drives)`
  - Remove `im->refreshDiskHealth()` call
  - Replace `im->getDriveHealth()` with `drives` parameter

### Task 4.4: Keep initial data call for disk health
- [x] Remove `updateDiskHealthChart()` explicit call at line 84 — the initial tick from `DataRefreshService::start()` handles this

### Task 4.5: Update header file
- [x] Update slot declarations in `resources_page.h`
- [x] Add `DataRefreshService *mRefresh` member
- [x] Add DI parameter to constructor
- [x] Remove `QTimer *mTimer` member

### Task 4.6: Build verification
- [x] Incremental build succeeds
- [x] Run app — Resources page charts all update at correct intervals

**Acceptance:** Resources has zero QTimers. All 7 charts update correctly. Disk health chart updates every 30s (not duplicate of Dashboard). CPU chart accuracy improved (single-caller fix).

---

## Phase 5: Convert Processes Page

### Task 5.1: Remove Processes timer
- [x] Remove `QTimer *mTimer` member from `processes_page.h`
- [x] Remove `#include <QTimer>` from `processes_page.h`
- [x] Remove `mTimer(new QTimer(this))` from constructor initializer list
- [x] Remove `connect(mTimer, ...)` call (line 57)
- [x] Remove `mTimer->setInterval(1000)` and `mTimer->start()` (lines 58-59)

### Task 5.2: Add DataRefreshService connection
- [x] Add `#include "Managers/data_refresh_service.h"` to `processes_page.cpp`
- [x] Add DI parameter: `DataRefreshService *refreshService = nullptr` to constructor
- [x] Add `DataRefreshService *mRefresh` member
- [x] Connect: `mRefresh->processesUpdated` → new slot `onProcessesUpdated(QList<Process>, QString)`

### Task 5.3: Refactor loadProcesses
- [x] `loadProcesses()` → `onProcessesUpdated(const QList<Process> &processes, const QString &userName)`
  - Remove `im->updateProcesses()` and `im->getProcesses()` calls
  - Replace `im->getUserName()` with `userName` parameter
  - Keep filter logic (`checkAllProcesses`), table rebuild, selection restoration

### Task 5.4: Update slider handler
- [x] `on_sliderRefresh_valueChanged()` (line 211): replace `mTimer->setInterval(i * 1000)` with `mRefresh->setProcessRefreshInterval(i * 1000)`

### Task 5.5: Keep initial explicit call
- [x] The explicit `loadProcesses()` call at line 55 can be removed — `DataRefreshService::start()` fires an immediate process tick

### Task 5.6: Update header file
- [x] Update slot declarations in `processes_page.h`
- [x] Add `DataRefreshService *mRefresh` member
- [x] Add DI parameter to constructor
- [x] Remove `QTimer *mTimer` member

### Task 5.7: Build verification
- [x] Incremental build succeeds
- [x] Run app — Process list updates at slider-configured interval

**Acceptance:** Processes page has zero QTimers. Slider adjusts process refresh interval via DataRefreshService. Table updates correctly.

---

## Phase 6: Testing & Verification

### Task 6.1: Full build
- [x] Clean rebuild: `rm -rf build && cmake -B build ... && cmake --build build`
- [x] Zero errors, zero new warnings

### Task 6.2: Run existing test suite
- [x] `ctest --test-dir build --output-on-failure` — all 63 tests pass (no regression)

### Task 6.3: Manual functional verification
- [x] Dashboard: all gauges (CPU, memory, disk, network, GPU, temp, battery, disk health) update at correct rates
- [x] Resources: all charts update correctly with proper data
- [x] Processes: table refreshes, slider changes interval, search works
- [x] Minimize to tray → check console for "paused" (or no timer activity)
- [x] Restore from tray → gauges immediately resume
- [x] Kiosk mode (F11) → minimize → timers should NOT pause (kiosk override)
- [x] Alerts still fire (CPU, memory, disk, battery, disk health thresholds)
- [x] Initial startup shows data immediately (no 1s delay on first paint)

### Task 6.4: Verify timer count reduction
- [x] Grep codebase for `new QTimer` — expect:
  - 4 instances in `data_refresh_service.cpp`
  - 2 instances in `gnome_mouse_tab.cpp` (debounce — unchanged)
  - 0 instances in Dashboard, Resources, Processes pages
  - A few `QTimer::singleShot` calls in app.cpp and gnome_settings_page.cpp (unchanged)

**Acceptance:** Clean build, all tests pass, all pages functional, timer count verified.

---

## Phase 7: Documentation Updates

### Task 7.1: Update FEATURE_REQUESTS.md
- [x] Change FR-37 status from `[~]` to `[x]`
- [x] Add `**Resolved:** <summary>` with commit hash

### Task 7.2: Update docs/ARCHITECTURE_REVIEW.md
- [x] Update §2A (Centralized DataRefreshService) — mark as done, update description
- [x] Update §6 (Fragmented Timer/Polling) — mark as resolved, describe solution
- [x] Update §5 (SignalMapper) — note signal count increased by 1 (`sigAppVisibilityChanged`)
- [x] Update Architectural Vision item 4 — mark as done

### Task 7.3: Update docs/APPLICATION_OVERVIEW.md
- [x] Update "Data Flow" section — replace QTimer-based flow with DataRefreshService flow
- [x] Update "Refresh Timing" table — note centralized service
- [x] Update Manager Layer section — add DataRefreshService (7th manager)
- [x] Update "By the numbers" — 7 manager singletons

### Task 7.4: Archive research and plan files
- [x] Move `claude_definitions/FR-37_research.md` → `claude_definitions/Archive/`
- [x] Move `claude_definitions/FR-37_plan.md` → `claude_definitions/Archive/`

### Task 7.5: Commit and push
- [x] Stage all changed files
- [x] Commit: `feat(core): add centralized DataRefreshService (FR-37, Phase 8)`
- [x] Push to remote

**Acceptance:** All documentation reflects the new architecture. Tracking files updated.

---

## Files Changed Summary

### New files (2):
- `shared/nexis/Managers/data_refresh_service.h`
- `shared/nexis/Managers/data_refresh_service.cpp`

### Modified files (11):
- `CMakeLists.txt` — add new source files to `GUI_SHARED_SRCS` and `GUI_SHARED_HDRS`
- `shared/nexis/signal_mapper.h` — add `sigAppVisibilityChanged(bool)` signal
- `shared/nexis/app.h` — no changes needed (signals emitted via SignalMapper)
- `shared/nexis/app.cpp` — emit visibility signal, start DataRefreshService
- `shared/nexis/Pages/Dashboard/dashboard_page.h` — remove timer, add service member, new slot signatures
- `shared/nexis/Pages/Dashboard/dashboard_page.cpp` — remove 3 timers, subscribe to service signals, refactor slots
- `shared/nexis/Pages/Resources/resources_page.h` — remove timer, add service member, new slot signatures
- `shared/nexis/Pages/Resources/resources_page.cpp` — remove 2 timers, subscribe to service signals, refactor slots
- `shared/nexis/Pages/Processes/processes_page.h` — remove timer, add service member, new slot signatures
- `shared/nexis/Pages/Processes/processes_page.cpp` — remove 1 timer, subscribe to service signal, refactor slots

### Documentation updates (4):
- `FEATURE_REQUESTS.md`
- `docs/ARCHITECTURE_REVIEW.md`
- `docs/APPLICATION_OVERVIEW.md`
- Archive: `claude_definitions/FR-37_research.md` → `Archive/`
- Archive: `claude_definitions/FR-37_plan.md` → `Archive/`

---

## Design Decisions & Rationale

1. **DataRefreshService is a separate singleton, not part of SignalMapper** — SignalMapper is a thin signal relay. DataRefreshService owns timers and calls InfoManager. Different responsibilities.

2. **Typed data signals, not raw tick signals** — Pages receive `cpuUpdated(QList<int>, double, QList<double>)` rather than a raw `tick()` that they'd handle by calling InfoManager themselves. This eliminates the race condition where two pages call `getCpuPercents()` and the second gets stale deltas.

3. **Process timer is separate from the 3 fixed tiers** — The process refresh interval is user-configurable (1-10s). It doesn't fit into the 1s/5s/30s tiers. A dedicated `mProcessTimer` with `setProcessRefreshInterval()` preserves this flexibility.

4. **Alerts stay in Dashboard** — Alert logic (threshold checks, `static bool isShow`, tray messages) is presentation logic, not data logic. It stays in Dashboard's signal handlers.

5. **Pause/resume via SignalMapper signal** — `sigAppVisibilityChanged(bool)` is emitted from App and consumed by DataRefreshService. This keeps App and DataRefreshService decoupled.

6. **Kiosk mode overrides pause** — If the user is using kiosk mode (fullscreen monitoring), pausing on minimize would defeat the purpose. `pause()` checks `SettingManager::ins()->getKioskMode()`.

7. **DI parameter on DataRefreshService** — Follows the FR-35 pattern. Constructor accepts optional `InfoManager *` for test injection. Pages accept optional `DataRefreshService *` for the same reason.

8. **`tempUpdated()` is a no-data signal** — Dashboard reads a specific sensor by `mSelectedSensorIndex`. Sending all sensor readings in the signal would be wasteful. The signal just triggers a re-read of the selected sensor.
