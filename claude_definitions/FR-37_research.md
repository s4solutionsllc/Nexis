# FR-37: Centralized DataRefreshService — Research

## 1. Feature Request Description

From `FEATURE_REQUESTS.md`:
> **FR-37: Centralized DataRefreshService** — [Phase 8] Replace ~25 per-page QTimers with a single `DataRefreshService` singleton that polls at defined intervals (1s/5s/30s) and emits typed data-change signals. Pages become reactive subscribers. Adds pause/resume for battery optimization when minimized. Depends on FR-36. Architecture Review section 2A.

---

## 2. Complete Timer Inventory

### 2.1 Dashboard Page

**File:** `shared/nexis/Pages/Dashboard/dashboard_page.cpp`

#### Timer 1: `mTimer` (1-second fast timer)
- **Declaration:** Line 29 — `mTimer(new QTimer(this))`
- **Interval:** 1000ms (line 150: `mTimer->start(1 * 1000)`)
- **Connected slots (lines 84, 116, 125, 142-144):**

| Slot | Line | InfoManager Methods Called | Notes |
|------|------|--------------------------|-------|
| `updateCpuBar()` | 142 | `im->getCpuPercents()`, `im->getCpuClock()` | Also reads `mSettingManager->getCpuAlertPercent()` for tray alerts |
| `updateMemoryBar()` | 143 | `im->updateMemoryInfo()`, `im->getMemUsed()`, `im->getMemTotal()` | Explicit `updateMemoryInfo()` call before reads |
| `updateNetworkBar()` | 144 | `im->getRXbytes()`, `im->getTXbytes()` | Uses static vars for delta calculation |
| `updateTempBar()` | 84 | `im->getThermalTemperature(mSelectedSensorIndex)` | Conditional — only if `hasThermalSensors()` |
| `updateGpuBar()` | 116 | `im->updateGpuInfo()`, `im->getGpuDevices()` | Conditional — only if `hasGpu()` |
| `updateBatteryBar()` | 125 | `im->updateBatteryInfo()`, `im->getBatteryData()` | Conditional — only if `hasBattery()` |

**Total: 6 slots triggered every 1s (3 unconditional + 3 conditional)**

#### Timer 2: `timerDisk` (5-second disk timer)
- **Declaration:** Line 146 — anonymous `QTimer *timerDisk = new QTimer(this)`
- **Interval:** 5000ms (line 148: `timerDisk->start(5 * 1000)`)
- **Connected slot (line 147):**

| Slot | Line | InfoManager Methods Called | Notes |
|------|------|--------------------------|-------|
| `updateDiskBar()` | 147 | `im->updateDiskInfo()`, `im->getDisks()` | Also reads `mSettingManager->getDiskName()` for disk selection |

**Total: 1 slot triggered every 5s**

#### Timer 3: `timerDiskHealth` (30-second disk health timer)
- **Declaration:** Line 133 — anonymous `QTimer *timerDiskHealth = new QTimer(this)`
- **Interval:** 30000ms (line 138: `timerDiskHealth->start(30 * 1000)`)
- **Connected lambda (line 134-137):**

| Action | Line | InfoManager Methods Called | Notes |
|--------|------|--------------------------|-------|
| Lambda | 134 | `im->refreshDiskHealth()` then `updateDiskHealthBar()` | `refreshDiskHealth()` runs `smartctl` subprocess — expensive |

`updateDiskHealthBar()` (line 476) reads `im->getDriveHealth()` — no additional update call needed since refreshDiskHealth was just called.

**Total: 1 lambda triggered every 30s. Conditional — only if `hasDiskHealth()`**

**Dashboard page total: 3 QTimer instances, 8 update slots/lambdas**

---

### 2.2 Resources Page

**File:** `shared/nexis/Pages/Resources/resources_page.cpp`

#### Timer 1: `mTimer` (1-second fast timer)
- **Declaration:** Line 22 — `mTimer(new QTimer(this))`
- **Interval:** 1000ms (line 87: `mTimer->start(1000)`)
- **Connected slots (lines 71-78):**

| Slot | Line | InfoManager Methods Called | Notes |
|------|------|--------------------------|-------|
| `updateCpuChart()` | 71 | `im->getCpuPercents()` | Per-core data (index 1+) |
| `updateCpuLoadAvg()` | 72 | `im->getCpuLoadAvgs()` | 1/5/15 min averages |
| `updateDiskReadWrite()` | 73 | `im->getDiskIO()` | Uses static vars for delta calculation |
| `updateMemoryChart()` | 74 | `im->updateMemoryInfo()`, `im->getSwapUsed()`, `im->getSwapTotal()`, `im->getMemUsed()`, `im->getMemTotal()` | Explicit updateMemoryInfo() call |
| `updateNetworkChart()` | 75 | `im->getRXbytes()`, `im->getTXbytes()` | Uses static vars for delta calculation |
| `updateGpuChart()` | 78 | `im->updateGpuInfo()`, `im->getGpuDevices()` | Conditional — only if `hasGpu()` |

**Total: 6 slots triggered every 1s (5 unconditional + 1 conditional)**

#### Timer 2: `diskHealthTimer` (30-second disk health chart timer)
- **Declaration:** Line 81 — anonymous `QTimer *diskHealthTimer = new QTimer(this)`
- **Interval:** 30000ms (line 83: `diskHealthTimer->start(30 * 1000)`)
- **Connected slot (line 82):**

| Slot | Line | InfoManager Methods Called | Notes |
|------|------|--------------------------|-------|
| `updateDiskHealthChart()` | 82 | `im->refreshDiskHealth()`, `im->getDriveHealth()` | Conditional — only if `hasDiskHealth()` and temp drives exist |

**Total: 1 slot triggered every 30s. Conditional.**

**Resources page total: 2 QTimer instances, 7 update slots**

---

### 2.3 Processes Page

**File:** `shared/nexis/Pages/Processes/processes_page.cpp`

#### Timer 1: `mTimer` (user-configurable timer)
- **Declaration:** Line 19 — `mTimer(new QTimer(this))`
- **Initial interval:** 1000ms (line 58: `mTimer->setInterval(1000)`)
- **User-configurable:** Lines 211-215 — slider range 1-10s, `mTimer->setInterval(i * 1000)`
- **Connected slot (line 57):**

| Slot | Line | InfoManager Methods Called | Notes |
|------|------|--------------------------|-------|
| `loadProcesses()` | 57 | `im->updateProcesses()`, `im->getProcesses()`, `im->getUserName()` | Full process list rebuild every tick |

**Processes page total: 1 QTimer instance, 1 update slot (user-configurable interval 1-10s)**

---

### 2.4 Services Page

**File:** `shared/nexis/Pages/Services/services_page.cpp`

**No QTimer instances.** Services are loaded once at init via `QtConcurrent::run` (line 25). User can re-filter with combo boxes, but no periodic refresh.

---

### 2.5 Docker Page

**File:** `shared/nexis/Pages/Docker/docker_page.cpp`

**No QTimer instances.** Docker data is loaded on-demand:
- Daemon check at init via `QThreadPool::globalInstance()->start()` (line 60)
- Images/Containers/Volumes fetched lazily when tabs are selected (line 368-372)
- Manual refresh via `btnRefresh` (line 399)

---

### 2.6 System Cleaner Page

**File:** `shared/nexis/Pages/SystemCleaner/system_cleaner_page.cpp`

**No QTimer instances for periodic refresh.** The page uses:
- `QMovie` for loading animations (not QTimer-based)
- `QtConcurrent::run` for scan/clean worker threads
- Signal/slot for scan/clean completion

---

### 2.7 Hardware Info Page

**File:** `shared/nexis/Pages/HardwareInfo/hardware_info_page.cpp`

**No QTimer instances.** All data is loaded once during `init()` — static hardware info that doesn't change at runtime.

---

### 2.8 Settings Page

**File:** `shared/nexis/Pages/Settings/settings_page.cpp`

**No QTimer instances.** Pure configuration page.

---

### 2.9 Search Page

**File:** `shared/nexis/Pages/Search/search_page.cpp`

**No QTimer instances.** User-triggered search via button click.

---

### 2.10 Helpers Page

**File:** `shared/nexis/Pages/Helpers/helpers_page.cpp`

**No QTimer instances.**

---

### 2.11 Startup Apps Page

**File:** `shared/nexis/Pages/StartupApps/startup_apps_page.cpp`

**No QTimer instances.** Uses `QFileSystemWatcher` for directory changes.

---

### 2.12 Uninstaller Page

**File:** `shared/nexis/Pages/Uninstaller/uninstaller_page.cpp`

**No QTimer instances.** Uses `QtConcurrent::run` for package list loading.

---

### 2.13 APT Source Manager Page

**File:** `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp`

**No QTimer instances.**

---

### 2.14 GNOME Settings Page

**File:** `shared/nexis/Pages/GnomeSettings/gnome_settings_page.cpp`

**No periodic QTimer instances.** Uses `QTimer::singleShot(4000, ...)` at line 76 for error message auto-hide.

**File:** `shared/nexis/Pages/GnomeSettings/gnome_mouse_tab.cpp`

Uses two single-shot debounce timers (not periodic):
- `mMouseSpeedTimer` — 200ms single-shot for slider debounce (line 15-17)
- `mTouchpadSpeedTimer` — 200ms single-shot for slider debounce (line 19-21)

**These are NOT data refresh timers — they are UI input debounce timers and should NOT be centralized.**

---

### 2.15 App.cpp

**File:** `shared/nexis/app.cpp`

Uses `QTimer::singleShot(1500, ...)` at line 471 for kiosk overlay fade animation. Not a data timer.

---

## 3. Timer Summary Table

| # | Page | Timer Name | Interval | Slot Count | Conditional? |
|---|------|-----------|----------|------------|--------------|
| 1 | Dashboard | `mTimer` | 1s | 6 | 3 of 6 are conditional |
| 2 | Dashboard | `timerDisk` | 5s | 1 | No |
| 3 | Dashboard | `timerDiskHealth` | 30s | 1 | Yes (hasDiskHealth) |
| 4 | Resources | `mTimer` | 1s | 6 | 1 of 6 is conditional |
| 5 | Resources | `diskHealthTimer` | 30s | 1 | Yes (hasDiskHealth + temp drives) |
| 6 | Processes | `mTimer` | 1-10s (user) | 1 | No |

**Total periodic QTimer instances: 6**
**Total connected update slots: 16**

---

## 4. Duplicate InfoManager Calls (Dashboard vs Resources)

Both Dashboard (1s) and Resources (1s) call these InfoManager methods independently every second:

| InfoManager Method | Dashboard | Resources | Duplication Risk |
|-------------------|-----------|-----------|-----------------|
| `getCpuPercents()` | Yes (updateCpuBar) | Yes (updateCpuChart) | **HIGH** — both call simultaneously, both get deltas from same static state |
| `updateMemoryInfo()` | Yes (updateMemoryBar) | Yes (updateMemoryChart) | **HIGH** — double filesystem reads |
| `getMemUsed()/getMemTotal()` | Yes | Yes | Data dependent on updateMemoryInfo |
| `getSwapUsed()/getSwapTotal()` | No | Yes | Only Resources |
| `getRXbytes()/getTXbytes()` | Yes (updateNetworkBar) | Yes (updateNetworkChart) | **MEDIUM** — live syscall each time, but cheap |
| `updateGpuInfo()` | Yes (updateGpuBar) | Yes (updateGpuChart) | **HIGH** — subprocess or sysfs read |
| `getCpuLoadAvgs()` | No | Yes | Only Resources |
| `getDiskIO()` | No | Yes | Only Resources — runs `iostat` subprocess |
| `updateDiskInfo()` | Yes (5s timer) | No | Only Dashboard |
| `refreshDiskHealth()` | Yes (30s timer) | Yes (30s timer) | **HIGH** — both run `smartctl` subprocess independently |
| `updateBatteryInfo()` | Yes | No | Only Dashboard |
| `getThermalTemperature()` | Yes | No | Only Dashboard |

### Critical Duplication Issues

1. **`getCpuPercents()`** — Uses static local variables (`l_idles`, `l_totals`) inside `CpuInfo::getCpuPercents()` (macos/cpu_info.cpp lines 120-127) for delta calculation. When called twice per second by two different pages, the second call will see a near-zero delta because the first call already consumed the accumulated CPU time difference. This means one of the two pages will show inaccurate/lower-than-actual CPU usage.

2. **`updateMemoryInfo()`** — Called by both Dashboard and Resources every second. Each call reads from sysfs/sysctl. Not harmful but wasteful.

3. **`updateGpuInfo()`** — Called by both Dashboard and Resources every second. Reads from sysfs or runs subprocess. Wasteful.

4. **`refreshDiskHealth()`** — Called by BOTH Dashboard (30s) and Resources (30s), but on independent timers. This means `smartctl` could be called twice within seconds of each other. `smartctl` is a subprocess call and relatively expensive.

---

## 5. InfoManager Method Analysis — Statefulness and Caching

### 5.1 Stateless "Read-Through" Methods (no update() needed)

These methods read live from the OS every time they are called:

| Method | Backend | Cost | Thread-Safe? |
|--------|---------|------|-------------|
| `getCpuPercents()` | Mach `host_processor_info()` + static delta vars | Low syscall, but **static state is shared** | **NOT thread-safe** — uses `static` locals |
| `getCpuLoadAvgs()` | `getloadavg()` | Trivial | Safe |
| `getCpuClock()` | `sysctlbyname()` or lookup table | Trivial to low | Safe |
| `getRXbytes()` | `getifaddrs()` | Low syscall | Safe |
| `getTXbytes()` | `getifaddrs()` | Low syscall | Safe |
| `getThermalTemperature()` | SMC or sysfs read | Low | Safe |
| `getThermalSensors()` | Returns cached list | Trivial | Safe (immutable) |

### 5.2 Explicit Update Methods (update then read)

These require an explicit `updateXxx()` call before reading:

| Update Method | Backend | Cost | Thread-Safe? |
|--------------|---------|------|-------------|
| `updateMemoryInfo()` | Reads `/proc/meminfo` or `sysctl` | Low | **NOT thread-safe** — writes member vars |
| `updateDiskInfo()` | `QStorageInfo::mountedVolumes()` | Low-Medium | **NOT thread-safe** — writes member vars |
| `updateGpuInfo()` | sysfs or subprocess | Medium | **NOT thread-safe** — writes member vars |
| `updateBatteryInfo()` | `ioreg` or sysfs | Medium | **NOT thread-safe** — writes member vars |
| `updateProcesses()` | `ps aux` subprocess | High | **NOT thread-safe** — writes member vars |
| `refreshDiskHealth()` | `smartctl` subprocess | **Very High** | **NOT thread-safe** — writes member vars |

### 5.3 Key Finding: getCpuPercents() Static State Bug

`CpuInfo::getCpuPercents()` (macos/cpu_info.cpp line 104-174) uses **function-scope `static` variables** for delta calculation:

```cpp
static QVector<double> l_idles;   // line 120
static QVector<double> l_totals;  // line 121
```

When Dashboard calls `getCpuPercents()` and Resources calls it within the same 1s window (which they do — both timers fire at 1s), the second caller gets a near-zero delta because the first already consumed the accumulated CPU time. This is a **data accuracy bug** that DataRefreshService would fix by calling it once and distributing the result.

### 5.4 Multiple Calls Per Second Concerns

If DataRefreshService calls each method once per tick and distributes results:
- **Safe:** All methods are safe to call once per second from the main thread
- **Improved:** Eliminates the double-call problem for CPU, memory, GPU, network
- **Critical improvement:** Disk health (`smartctl`) is called once per 30s instead of potentially twice

---

## 6. App Minimize-to-Tray / Restore Mechanism

**File:** `shared/nexis/app.cpp`

### 6.1 Minimize Event

```cpp
void App::changeEvent(QEvent *event)    // line 201
{
    if (event->type() == QEvent::WindowStateChange &&
        windowState().testFlag(Qt::WindowMinimized)) {
        hide();          // hides from taskbar
        event->ignore();
        return;
    }
    QMainWindow::changeEvent(event);
}
```

When minimized, the app calls `hide()` which removes it from the taskbar. The window still exists but is not visible. **All QTimers continue running** even when hidden/minimized.

### 6.2 Restore Event

```cpp
// Tray icon click handler (line 223)
connect(mTrayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason) {
    setWindowState(windowState() & ~Qt::WindowMinimized);
    show();
    if (windowHandle())
        windowHandle()->requestActivate();
});
```

### 6.3 Close Event

```cpp
void App::closeEvent(QCloseEvent *event)    // line 189
{
    mTrayIcon->hide();
    event->accept();
    QThreadPool::globalInstance()->waitForDone();
    qApp->quit();
}
```

### 6.4 Pause/Resume Implementation Points

For DataRefreshService pause/resume:
- **Pause trigger:** `QEvent::WindowStateChange` where `windowState().testFlag(Qt::WindowMinimized)` — add signal emission before `hide()`
- **Resume trigger:** The tray icon `activated` signal handler (line 223) — add signal emission after `show()`
- **Alternative:** Override `App::showEvent()` and `App::hideEvent()` for cleaner detection
- **Best approach:** Emit a `SignalMapper` signal (`sigAppVisibilityChanged(bool visible)`) from both locations. DataRefreshService subscribes.

### 6.5 Kiosk Mode Considerations

When in kiosk mode (`mKioskMode == true`):
- The app is fullscreen on Dashboard page only
- `applyKioskMode(true)` calls `showFullScreen()` (line 429)
- The tray minimize behavior still works (Cmd+M / Super+D on macOS)
- **When kiosk + minimized:** Dashboard timers MUST keep running because kiosk mode implies "monitoring display" usage — the user will restore and expects current data
- **Implementation:** DataRefreshService should check `SettingManager::ins()->getKioskMode()` before pausing. If kiosk is active, do NOT pause even when minimized.

---

## 7. SignalMapper Analysis

**File:** `shared/nexis/signal_mapper.h`

Current signals (7):

```cpp
signals:
    void sigChangedAppTheme();                                      // 1
    void sigUninstallStarted();                                     // 2
    void sigUninstallFinished();                                    // 3
    void sigScheduledCleanStarted(QString scheduleName);            // 4
    void sigScheduledCleanFinished(quint64 bytesFreed, int count);  // 5
    void sigKioskToggleRequested();                                 // 6
    void sigKioskModeChanged(bool enabled);                         // 7
```

### 7.1 New Signals Needed for DataRefreshService

DataRefreshService would be a **new singleton class**, not part of SignalMapper. However, it may need a few new signals on SignalMapper for app-level events:

| Signal | Purpose | Emitter | Subscriber |
|--------|---------|---------|------------|
| `sigAppVisibilityChanged(bool visible)` | Notify when app is hidden/shown (for pause/resume) | `App::changeEvent()`, tray restore handler | DataRefreshService |

DataRefreshService itself would emit its own signals (not on SignalMapper) since it is a QObject singleton:

| Signal | Interval | Data Payload | Subscribers |
|--------|----------|--------------|-------------|
| `sigFastTick()` | 1s | (none — raw trigger) | Could be used, but data signals below are better |
| `sigCpuUpdated(QList<int> percents, double clockMHz, QList<double> loadAvgs)` | 1s | All CPU data in one shot | Dashboard, Resources |
| `sigMemoryUpdated(quint64 used, quint64 total, quint64 swapUsed, quint64 swapTotal)` | 1s | Memory + swap | Dashboard, Resources |
| `sigNetworkUpdated(quint64 rxBytes, quint64 txBytes)` | 1s | Raw cumulative bytes | Dashboard, Resources |
| `sigGpuUpdated(QList<GpuDevice> gpus)` | 1s | GPU utilization data | Dashboard, Resources |
| `sigTempUpdated()` | 1s | (pages call getThermalTemperature themselves) | Dashboard |
| `sigBatteryUpdated(BatteryData data)` | 1s | Full battery snapshot | Dashboard |
| `sigDiskUpdated(QList<Disk> disks)` | 5s | Disk usage | Dashboard |
| `sigDiskIOUpdated(QList<quint64> io)` | 1s | Disk read/write bytes | Resources |
| `sigDiskHealthUpdated(QList<DriveHealth> drives)` | 30s | SMART data | Dashboard, Resources |
| `sigProcessesUpdated(QList<Process> processes)` | 1-10s (configurable) | Process list | Processes page |
| `sigRefreshPaused()` | N/A | (none) | All pages (optional — for UI dimming) |
| `sigRefreshResumed()` | N/A | (none) | All pages |

---

## 8. Proposed Timer-to-Signal Mapping

### Tier 1: Fast (1s interval)

| Current Timer | Current Location | New Signal | InfoManager Calls (centralized) |
|--------------|-----------------|------------|-------------------------------|
| Dashboard `mTimer` → `updateCpuBar` | dashboard_page.cpp:142 | `sigCpuUpdated` | `getCpuPercents()`, `getCpuClock()` |
| Dashboard `mTimer` → `updateMemoryBar` | dashboard_page.cpp:143 | `sigMemoryUpdated` | `updateMemoryInfo()`, `getMemUsed()`, `getMemTotal()`, `getSwapUsed()`, `getSwapTotal()` |
| Dashboard `mTimer` → `updateNetworkBar` | dashboard_page.cpp:144 | `sigNetworkUpdated` | `getRXbytes()`, `getTXbytes()` |
| Dashboard `mTimer` → `updateTempBar` | dashboard_page.cpp:84 | `sigTempUpdated` | `getThermalTemperature()` (all sensors) |
| Dashboard `mTimer` → `updateGpuBar` | dashboard_page.cpp:116 | `sigGpuUpdated` | `updateGpuInfo()`, `getGpuDevices()` |
| Dashboard `mTimer` → `updateBatteryBar` | dashboard_page.cpp:125 | `sigBatteryUpdated` | `updateBatteryInfo()`, `getBatteryData()` |
| Resources `mTimer` → `updateCpuChart` | resources_page.cpp:71 | `sigCpuUpdated` | (same data as Dashboard) |
| Resources `mTimer` → `updateCpuLoadAvg` | resources_page.cpp:72 | `sigCpuUpdated` | `getCpuLoadAvgs()` (bundled) |
| Resources `mTimer` → `updateDiskReadWrite` | resources_page.cpp:73 | `sigDiskIOUpdated` | `getDiskIO()` |
| Resources `mTimer` → `updateMemoryChart` | resources_page.cpp:74 | `sigMemoryUpdated` | (same data as Dashboard) |
| Resources `mTimer` → `updateNetworkChart` | resources_page.cpp:75 | `sigNetworkUpdated` | (same data as Dashboard) |
| Resources `mTimer` → `updateGpuChart` | resources_page.cpp:78 | `sigGpuUpdated` | (same data as Dashboard) |

### Tier 2: Medium (5s interval)

| Current Timer | Current Location | New Signal | InfoManager Calls (centralized) |
|--------------|-----------------|------------|-------------------------------|
| Dashboard `timerDisk` → `updateDiskBar` | dashboard_page.cpp:147 | `sigDiskUpdated` | `updateDiskInfo()`, `getDisks()` |

### Tier 3: Slow (30s interval)

| Current Timer | Current Location | New Signal | InfoManager Calls (centralized) |
|--------------|-----------------|------------|-------------------------------|
| Dashboard `timerDiskHealth` lambda | dashboard_page.cpp:134 | `sigDiskHealthUpdated` | `refreshDiskHealth()`, `getDriveHealth()` |
| Resources `diskHealthTimer` → `updateDiskHealthChart` | resources_page.cpp:82 | `sigDiskHealthUpdated` | (same data — no longer duplicated) |

### Tier 4: User-Configurable

| Current Timer | Current Location | New Signal | InfoManager Calls (centralized) |
|--------------|-----------------|------------|-------------------------------|
| Processes `mTimer` → `loadProcesses` | processes_page.cpp:57 | `sigProcessesUpdated` | `updateProcesses()`, `getProcesses()` |

---

## 9. Data Flow for Each Timer (Current vs Proposed)

### 9.1 CPU Data Flow

**Current:**
```
Dashboard mTimer (1s) → updateCpuBar() → im->getCpuPercents() → mCpuBar->setValue()
Resources mTimer (1s) → updateCpuChart() → im->getCpuPercents() → chart series update
                      → updateCpuLoadAvg() → im->getCpuLoadAvgs() → chart series update
```
**Problem:** `getCpuPercents()` called twice per second; static delta vars consumed by first caller.

**Proposed:**
```
DataRefreshService (1s) → im->getCpuPercents() → emit sigCpuUpdated(percents, clock, loadAvgs)
                          im->getCpuClock()        ↓
                          im->getCpuLoadAvgs()      ├─→ DashboardPage::onCpuUpdated(percents, clock, loadAvgs)
                                                    └─→ ResourcesPage::onCpuUpdated(percents, clock, loadAvgs)
```

### 9.2 Memory Data Flow

**Current:**
```
Dashboard mTimer (1s) → updateMemoryBar() → im->updateMemoryInfo() → im->getMemUsed()/getMemTotal() → mMemBar
Resources mTimer (1s) → updateMemoryChart() → im->updateMemoryInfo() → im->getMemUsed()/getSwapUsed() → chart
```
**Problem:** `updateMemoryInfo()` called twice per second.

**Proposed:**
```
DataRefreshService (1s) → im->updateMemoryInfo()
                        → emit sigMemoryUpdated(used, total, swapUsed, swapTotal)
                            ├─→ DashboardPage::onMemoryUpdated()
                            └─→ ResourcesPage::onMemoryUpdated()
```

### 9.3 Network Data Flow

**Current:**
```
Dashboard mTimer (1s) → updateNetworkBar() → im->getRXbytes()/getTXbytes() → delta calc (static vars) → linebars
Resources mTimer (1s) → updateNetworkChart() → im->getRXbytes()/getTXbytes() → delta calc (static vars) → chart
```
**Important:** Each page maintains its OWN static variables for delta calculation (last RX/TX). The raw bytes are cumulative counters. Each page independently computes download/upload speed from the raw counter. This is fine — the raw counter reads are cheap.

**Proposed:**
```
DataRefreshService (1s) → im->getRXbytes()/getTXbytes()
                        → emit sigNetworkUpdated(rxBytes, txBytes)
                            ├─→ DashboardPage::onNetworkUpdated() — computes own deltas
                            └─→ ResourcesPage::onNetworkUpdated() — computes own deltas
```

### 9.4 GPU Data Flow

**Current:**
```
Dashboard mTimer (1s) → updateGpuBar() → im->updateGpuInfo() → im->getGpuDevices() → mGpuBar
Resources mTimer (1s) → updateGpuChart() → im->updateGpuInfo() → im->getGpuDevices() → chart
```
**Problem:** `updateGpuInfo()` called twice per second.

**Proposed:**
```
DataRefreshService (1s) → im->updateGpuInfo()
                        → emit sigGpuUpdated(im->getGpuDevices())
                            ├─→ DashboardPage::onGpuUpdated()
                            └─→ ResourcesPage::onGpuUpdated()
```

### 9.5 Disk Health Data Flow

**Current:**
```
Dashboard timerDiskHealth (30s) → im->refreshDiskHealth() → updateDiskHealthBar() → im->getDriveHealth()
Resources diskHealthTimer (30s) → updateDiskHealthChart() → im->refreshDiskHealth() → im->getDriveHealth()
```
**Problem:** Two independent 30s timers both calling `refreshDiskHealth()` (smartctl subprocess). Could fire within seconds of each other.

**Proposed:**
```
DataRefreshService (30s) → im->refreshDiskHealth()
                         → emit sigDiskHealthUpdated(im->getDriveHealth())
                             ├─→ DashboardPage::onDiskHealthUpdated()
                             └─→ ResourcesPage::onDiskHealthUpdated()
```

### 9.6 Process Data Flow

**Current:**
```
Processes mTimer (1-10s) → loadProcesses() → im->updateProcesses() → im->getProcesses() → table rebuild
```

**Proposed:**
```
DataRefreshService (configurable) → im->updateProcesses()
                                  → emit sigProcessesUpdated(im->getProcesses(), im->getUserName())
                                      └─→ ProcessesPage::onProcessesUpdated()
```

The configurable interval (1-10s via slider) needs special handling: `DataRefreshService` needs a method like `setProcessRefreshInterval(int seconds)` that the slider calls.

---

## 10. Alert System Implications

Several update methods also trigger **tray icon alert messages**. These alerts check thresholds from SettingManager:

| Alert | Current Location | Trigger |
|-------|-----------------|---------|
| CPU alert | dashboard_page.cpp:243-254 | `cpuUsedPercent > cpuAlertPercent` |
| Memory alert | dashboard_page.cpp:278-289 | `memUsedPercent > memoryAlertPercent` |
| Disk alert | dashboard_page.cpp:322-334 | `diskPercent > diskAlertPercent` |
| Battery health alert | dashboard_page.cpp:447-473 | `healthPercent < alertPercent` |
| Disk health alert | dashboard_page.cpp:526-549 | `healthVerdict == "Caution" or "Critical"` |

**Design decision:** Alerts should remain in Dashboard page (they use tray icon, settings, and have their own state tracking via `static bool isShow`). DataRefreshService should NOT handle alerts — it only provides data. Dashboard keeps its existing alert logic in the updated slot handlers.

---

## 11. Architecture Considerations

### 11.1 DataRefreshService Singleton Structure

```
DataRefreshService (QObject, singleton)
├── QTimer mFastTimer      (1s)   → polls CPU, memory, network, GPU, temp, battery, diskIO
├── QTimer mMediumTimer    (5s)   → polls disk usage
├── QTimer mSlowTimer      (30s)  → polls disk health (smartctl)
├── QTimer mProcessTimer   (1-10s, user-configurable)  → polls processes
├── bool mPaused           → when true, all timers are stopped
├── bool mKioskOverride    → when true, don't pause even if minimized
└── Methods:
    ├── pause() / resume()
    ├── setProcessRefreshInterval(int seconds)
    └── isPaused() const
```

### 11.2 Timer Consolidation

Timers reduced from **6 across 3 pages** to **4 in one service**:
- Dashboard: 3 timers (1s + 5s + 30s) → 0 timers
- Resources: 2 timers (1s + 30s) → 0 timers
- Processes: 1 timer (1-10s) → 0 timers
- DataRefreshService: 4 timers (1s + 5s + 30s + configurable)

Net reduction: 6 → 4 QTimer instances, but more importantly, **all InfoManager calls are now single-point** eliminating duplication.

### 11.3 Thread Safety

All InfoManager methods currently run on the main thread (called from QTimer slots). DataRefreshService should maintain this — its timers fire on the main thread, signals are emitted on the main thread, page slots receive on the main thread. No threading changes needed.

### 11.4 Page Conversion Pattern

Each page that currently owns a QTimer would be converted to:

```cpp
// Before:
connect(mTimer, &QTimer::timeout, this, &DashboardPage::updateCpuBar);

// After:
connect(DataRefreshService::ins(), &DataRefreshService::sigCpuUpdated,
        this, &DashboardPage::onCpuUpdated);
```

The page's update method signature changes from no-args to accepting the data:

```cpp
// Before:
void DashboardPage::updateCpuBar() {
    int cpuUsedPercent = im->getCpuPercents().at(0);
    ...
}

// After:
void DashboardPage::onCpuUpdated(const QList<int> &percents, double clockMHz, const QList<double> &loadAvgs) {
    int cpuUsedPercent = percents.at(0);
    ...
}
```

### 11.5 Timers NOT to Centralize

These timers should remain local to their pages/widgets:
- `GnomeMouseTab::mMouseSpeedTimer` / `mTouchpadSpeedTimer` — UI input debounce (200ms single-shot)
- `App::showKioskOverlay()` `QTimer::singleShot(1500, ...)` — animation timing
- `GnomeSettingsPage::showError()` `QTimer::singleShot(4000, ...)` — error auto-dismiss
- `HistoryChart` includes QTimer in header but doesn't use periodic timers

---

## 12. Pause/Resume Design

### 12.1 Triggers

| Event | Action | Condition |
|-------|--------|-----------|
| `App::changeEvent(WindowMinimized)` | `DataRefreshService::pause()` | NOT in kiosk mode |
| Tray icon activated | `DataRefreshService::resume()` | Always |
| `App::show()` called from tray menu | `DataRefreshService::resume()` | Always |
| Kiosk mode enabled while minimized | `DataRefreshService::resume()` | Always |
| Kiosk mode disabled while minimized | `DataRefreshService::pause()` | Only if currently minimized |

### 12.2 Pause Implementation

```cpp
void DataRefreshService::pause() {
    if (mPaused) return;
    if (SettingManager::ins()->getKioskMode()) return;  // kiosk override
    mPaused = true;
    mFastTimer->stop();
    mMediumTimer->stop();
    mSlowTimer->stop();
    mProcessTimer->stop();
    emit sigRefreshPaused();
}

void DataRefreshService::resume() {
    if (!mPaused) return;
    mPaused = false;
    // Immediately fire one tick to bring UI up to date
    onFastTick();
    onMediumTick();
    // Don't immediately fire slow tick — smartctl is expensive
    mFastTimer->start();
    mMediumTimer->start();
    mSlowTimer->start();
    mProcessTimer->start();
    emit sigRefreshResumed();
}
```

### 12.3 Signal Flow for Pause/Resume

```
App::changeEvent(minimized) → SignalMapper::sigAppVisibilityChanged(false)
                                 → DataRefreshService::pause()
                                    → stops all 4 timers
                                    → emits sigRefreshPaused()

Tray icon clicked → App::show() → SignalMapper::sigAppVisibilityChanged(true)
                                    → DataRefreshService::resume()
                                       → immediate one-shot refresh
                                       → restarts all 4 timers
                                       → emits sigRefreshResumed()
```

---

## 13. Dependency on FR-36

FR-37 states "Depends on FR-36". Looking at FEATURE_REQUESTS.md, FR-36 is likely the InfoManager refactoring or thread-safety work. The key dependency is:
- FR-36 may address the `getCpuPercents()` static-variable issue
- FR-36 may make InfoManager methods return data structs instead of requiring update+get pairs
- If FR-36 is completed first, DataRefreshService can work with cleaner InfoManager APIs

However, DataRefreshService can be implemented independently if needed — it wraps the existing InfoManager API as-is. The static variable bug in `getCpuPercents()` is actually *fixed* by centralization (single caller per tick).

---

## 14. Files That Will Be Modified

### New files:
- `shared/nexis/Managers/data_refresh_service.h`
- `shared/nexis/Managers/data_refresh_service.cpp`

### Modified files:
- `shared/nexis/signal_mapper.h` — add `sigAppVisibilityChanged(bool)` signal
- `shared/nexis/app.cpp` — emit visibility signal on minimize/restore
- `shared/nexis/app.h` — (minor — no new members needed)
- `shared/nexis/Pages/Dashboard/dashboard_page.h` — remove `QTimer *mTimer`, change slot signatures
- `shared/nexis/Pages/Dashboard/dashboard_page.cpp` — remove 3 timers, connect to service signals
- `shared/nexis/Pages/Resources/resources_page.h` — remove `QTimer *mTimer`, change slot signatures
- `shared/nexis/Pages/Resources/resources_page.cpp` — remove 2 timers, connect to service signals
- `shared/nexis/Pages/Processes/processes_page.h` — remove `QTimer *mTimer`, change slot signatures
- `shared/nexis/Pages/Processes/processes_page.cpp` — remove 1 timer, connect to service signal
- `shared/nexis/Pages/Settings/settings_page.cpp` — process refresh slider now calls `DataRefreshService::setProcessRefreshInterval()`
- `CMakeLists.txt` — add new source files

---

## 15. Risk Assessment

| Risk | Severity | Mitigation |
|------|----------|-----------|
| Static variable deltas in network/disk update methods break when call frequency changes | Medium | Pages maintain their own delta calculation from raw counters (already the case) |
| Process refresh slider no longer directly controls a local timer | Low | `DataRefreshService::setProcessRefreshInterval()` provides equivalent control |
| Kiosk mode + minimize edge case | Low | Check `getKioskMode()` before pausing |
| Pages expect initial data on construction (before first timer tick) | Medium | DataRefreshService fires initial data immediately on construction, OR pages call initialization explicitly as they do now |
| Chart history series depend on once-per-second updates for scrolling | Medium | DataRefreshService 1s timer must be reliable; pages must handle missed ticks gracefully |
| Alert static `isShow` booleans in Dashboard are coupled to update methods | Low | Alerts remain in Dashboard — data signals carry enough info for threshold checks |
