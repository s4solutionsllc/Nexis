# Nexis — Architecture Review

> A deep and comprehensive review of the Nexis architecture: how logic and UI work together, what's working well, what should change, and where the application should go next.
> Last updated: February 2026

---

## Table of Contents

1. [Architecture at a Glance](#architecture-at-a-glance)
2. [Architecture Strengths](#architecture-strengths)
   - [Platform Abstraction via Include-Path Shadowing](#1-platform-abstraction-via-include-path-shadowing)
   - [Singleton Manager Facades](#2-singleton-manager-facades)
   - [QSS Token System](#3-qss-token-system)
   - [Graceful Degradation](#4-graceful-degradation)
   - [SignalMapper for Cross-Component Events](#5-signalmapper-for-cross-component-events)
3. [Architecture Weaknesses](#architecture-weaknesses)
   - [No Formal Platform Interfaces](#1-no-formal-platform-interfaces)
   - [Singleton Coupling Limits Testability](#2-singleton-coupling-limits-testability)
   - [Manager Layer Inconsistency](#3-manager-layer-inconsistency)
   - [CMake GLOB_RECURSE](#4-cmake-glob_recurse)
   - [No Automated Test Suite](#5-no-automated-test-suite)
   - [Fragmented Timer/Polling](#6-fragmented-timerpolling)
   - [QSS Token Validation Gap](#7-qss-token-validation-gap)
4. [Recommended Improvements](#recommended-improvements)
   - [Priority 1: Critical](#priority-1-critical-foundational)
   - [Priority 2: High](#priority-2-high-scalability)
   - [Priority 3: Medium](#priority-3-medium-testing--quality)
   - [Priority 4: Low](#priority-4-low-future-proofing)
5. [Strategic Direction](#strategic-direction)
   - [QWidgets vs QML](#qwidgets-vs-qml)
   - [Testing Strategy](#testing-strategy)
   - [Incremental Evolution vs Clean-Slate Redesign](#incremental-evolution-vs-clean-slate-redesign)
   - [Architectural Vision](#architectural-vision)

---

## Architecture at a Glance

Nexis is structured as a **three-tier desktop application**:

```
┌────────────────────────────────────────────────────────────────────┐
│  UI Layer: 14 QWidget Pages                                       │
│  Each page owns its .ui file, QTimers, and presentation logic     │
│  Files: shared/nexis/Pages/*/*.cpp                                │
├────────────────────────────────────────────────────────────────────┤
│  Manager Layer: 6 Singletons                                      │
│  InfoManager, AppManager, SettingManager, ToolManager,            │
│  CleanerService, ScheduleManager                                  │
│  Files: shared/nexis/Managers/*.cpp                               │
├────────────────────────────────────────────────────────────────────┤
│  Core Library: nexis-core (static lib)                            │
│  11 Info providers + 5 Tools + 3 Utils                            │
│  Files: shared/nexis-core/**/*.cpp + {platform}/nexis-core/**     │
└────────────────────────────────────────────────────────────────────┘
```

**Key architectural decisions:**
- **Compile-time platform abstraction** — CMake include-path precedence, not runtime polymorphism
- **Singleton managers** — Static `ins()` accessors, no dependency injection framework
- **Timer-driven polling** — QTimers in each page drive data refresh (1s/5s/30s intervals)
- **QSS theming** — Single stylesheet template with `@token` replacement at runtime
- **Qt signals** — `SignalMapper` singleton as a lightweight global event bus

**Scale:** ~5,000–6,000 lines of C++ across core library + GUI, 14 pages, 34 translations, 3 themes.

---

## Architecture Strengths

### 1. Platform Abstraction via Include-Path Shadowing

**How it works:** CMake places `${PLATFORM_DIR}` (macos/ or linux/) *before* `${SHARED_DIR}` in include paths. When a page writes `#include "cpu_info.h"`, the compiler resolves to the platform-specific header first if it exists, falling back to the shared header otherwise.

```cmake
# CMakeLists.txt:49-58 — platform directories listed first
target_include_directories(nexis-core PUBLIC
  "${CORE_PLAT_DIR}"        # macos/nexis-core/ or linux/nexis-core/
  "${CORE_PLAT_DIR}/Info"
  ...
  "${CORE_SHARED_DIR}"      # shared/nexis-core/
  "${CORE_SHARED_DIR}/Info"
  ...
)
```

**In practice:** `shared/nexis-core/Info/cpu_info.h` defines the interface. `macos/nexis-core/Info/cpu_info.cpp` implements it using `sysctl` and Mach APIs. `linux/nexis-core/Info/cpu_info.cpp` implements it using `/proc/stat` and sysfs. The same header, different implementations — selected at compile time.

**Why this is good:**
- **Zero runtime overhead** — no virtual dispatch, no vtables, no factory methods
- **Clean separation** — shared logic stays in `shared/`, platform deltas are isolated
- **Simple mental model** — one header file defines the contract; platform `.cpp` files fulfill it
- **Pragmatic** — appropriate for a desktop app where the target platform is known at build time

**Assessment:** This pattern is **elegant and appropriate** for the project's scale and constraints. It would need rethinking only if Nexis needed to support platform selection at runtime (which it doesn't).

---

### 2. Singleton Manager Facades

Six manager singletons act as **stable API surfaces** over the core library:

```cpp
// shared/nexis/Managers/info_manager.h — facade over 11 Info classes
class InfoManager {
public:
    static InfoManager *ins();

    // CPU — delegates to CpuInfo ci member
    int getCpuCoreCount() const;
    QList<int> getCpuPercents() const;

    // Memory — delegates to MemoryInfo mi member
    void updateMemoryInfo();
    quint64 getMemUsed() const;
    quint64 getMemTotal() const;
    // ... 50+ methods across 11 info providers

private:
    CpuInfo ci;     // Held by value — no heap allocation
    MemoryInfo mi;
    DiskInfo di;
    // ... 10 total
};
```

**Why this works:**
- **Stable API for pages** — `InfoManager::ins()->getCpuPercents()` won't change even if the underlying `CpuInfo` class is refactored
- **Lazy instance caching** — Info objects are constructed once and reused across all pages
- **Centralized refresh** — `updateMemoryInfo()` ensures a single refresh call feeds all consumers
- **Minimal boilerplate** — no DI frameworks, no factories, no configuration files

**Assessment:** For a ~5,000-line desktop app with no unit tests, singletons are **pragmatically correct**. The anti-pattern critique of singletons assumes multi-threaded systems or test-heavy codebases. Nexis is single-threaded GUI with a single user. The managers are effectively namespaced globals — and that's fine at this scale.

---

### 3. QSS Token System

The theme system uses a single QSS template with placeholder tokens, replaced at runtime:

```cpp
// shared/nexis/Managers/app_manager.cpp:88-121
void AppManager::updateStylesheet() {
    QString themeName = resolveThemeName();  // "default" (dark), "light", or auto-detect

    // Load color/spacing values for this theme
    mStyleValues = new QSettings(
        QString(":/static/themes/%1/style/values.ini").arg(themeName), ...);

    // Load the single QSS template (always from "default")
    mStylesheetFileContent = FileUtil::readStringFromFile(
        ":/static/themes/default/style/style.qss");

    // Replace @tokens: @color01 → "#36363a", @accentColor → "#E95420", etc.
    for (const QString &key : mStyleValues->allKeys()) {
        mStylesheetFileContent.replace(key, mStyleValues->value(key).toString());
    }

    // Replace @dpN tokens with DPI-scaled pixel values
    static const QRegularExpression dpRx("@dp(\\d+)");
    // ... regex replacement loop ...

    qApp->setStyleSheet(mStylesheetFileContent);
    emit SignalMapper::ins()->sigChangedAppTheme();
}
```

**Why this is good:**
- **Single source of truth** — one `style.qss` file, not three duplicated stylesheets
- **Theme = data, not code** — switching themes means loading a different `values.ini`
- **DPI scaling without QML** — the `@dpN` tokens solved HiDPI scaling (BUG-07) in QSS, avoiding a costly QML migration
- **Live switching** — Auto mode responds to `QStyleHints::colorSchemeChanged` (Qt 6.5+)

**Assessment:** **Elegant and maintainable.** This approach is better than the common alternative of maintaining separate QSS files per theme, which leads to divergence and missed updates.

---

### 4. Graceful Degradation

Optional features hide themselves when their hardware or software dependencies are absent:

```cpp
// shared/nexis/Pages/Dashboard/dashboard_page.cpp:49-53
if (im->hasDiskHealth()) {
    ui->circleBarsLayout->addWidget(mDiskHealthBar);
} else {
    mDiskHealthBar->hide();
}
```

```cpp
// shared/nexis/app.cpp — conditional page registration
if (ToolManager::ins()->checkDocker()) {
    dockerPage = new DockerPage();
    mListPages.insert(dockerIdx, dockerPage);
} else {
    ui->btnDocker->hide();  // Sidebar button hidden entirely
}
```

**The pattern applies at three levels:**
1. **Page level** — Docker, GNOME Settings, and APT/Homebrew pages are hidden entirely if tools aren't detected
2. **Widget level** — Battery, GPU, temperature, and disk health gauges hide if hardware is absent
3. **Feature level** — macOS filters Apple system agents from startup apps; purge option hidden on non-APT systems

**Why this matters:** Cross-platform apps often show all features with "not available on this platform" messages, which clutters the UI. Nexis's approach presents a **clean, relevant interface** tailored to each system's actual capabilities.

---

### 5. SignalMapper for Cross-Component Events

A lightweight singleton event bus handles app-wide notifications:

```cpp
// shared/nexis/Managers/signal_mapper.h
class SignalMapper : public QObject {
    Q_OBJECT
signals:
    void sigChangedAppTheme();
    void sigUninstallStarted();
    void sigUninstallFinished();
    void sigScheduledCleanStarted(QString scheduleName);
    void sigScheduledCleanFinished(quint64 bytesFreed, int fileCount);
    void sigKioskToggleRequested();
    void sigKioskModeChanged(bool enabled);
};
```

**Usage pattern:**
- Settings page changes theme → emits `sigChangedAppTheme()`
- All 14 pages listen → reload theme-dependent icons, GIF loaders, and colors
- Dashboard kiosk button → emits `sigKioskToggleRequested()` → App toggles kiosk mode → emits `sigKioskModeChanged(bool)` → Dashboard button swaps icon, tray action syncs checkmark
- No page needs a pointer to any other page — complete decoupling

**Assessment:** With 7 signals, this is still **appropriately simple**. The kiosk mode signals (FR-30) demonstrate the pattern working well for bidirectional communication between App and DashboardPage without coupling them. A full event bus library (like eventpp) would be overkill until the signal count grows significantly (15+).

---

## Architecture Weaknesses

### 1. No Formal Platform Interfaces

**The problem:** Include-path shadowing works by convention. There's no compiler-enforced contract that `linux/Info/cpu_info.cpp` and `macos/Info/cpu_info.cpp` implement the same methods.

```cpp
// shared/nexis-core/Info/cpu_info.h — the "interface" (but it's just a class, not abstract)
class CpuInfo {
public:
    int getCpuPhysicalCoreCount() const;
    int getCpuCoreCount() const;
    QList<int> getCpuPercents() const;
    QList<double> getLoadAvgs() const;
    double getAvgClock() const;
    QList<double> getClocks() const;
};
```

**Risk scenario:** A developer adds `getCoreTempCelsius()` to `linux/cpu_info.cpp` but forgets the macOS implementation. The Linux build succeeds. The macOS build fails with a **linker error** (undefined symbol) — not a compiler error. The problem only surfaces when someone builds on the other platform, or in CI.

**Impact:** Medium. CI covers both platforms, so missing implementations are caught. But errors are **late** (link-time) rather than **early** (compile-time), and the error messages are opaque linker errors rather than clear "unimplemented pure virtual" messages.

**All 11 Info classes and 5 Tool classes have this exposure** — 16 classes where platform parity is enforced only by convention.

---

### 2. Singleton Coupling Limits Testability

Every page hardcodes its manager dependencies via static `ins()` calls:

```cpp
// shared/nexis/Pages/Dashboard/dashboard_page.cpp:28
DashboardPage::DashboardPage(QWidget *parent) :
    QWidget(parent),
    ...
    im(InfoManager::ins()),          // Hardcoded singleton
    mSettingManager(SettingManager::ins()),  // Hardcoded singleton
    ...
```

**Consequence:** It's impossible to test `DashboardPage` with mock data without either:
1. Replacing the global `InfoManager::instance` pointer (fragile, not thread-safe)
2. Running on a real OS with real CPU/memory data (not a unit test — it's an integration test)

**Current state:** No tests exist, so this isn't actively blocking anything. But it means that adding tests in the future requires refactoring every page constructor first.

**Scope of coupling:** All 14 pages reference `InfoManager::ins()` and/or `SettingManager::ins()` directly. Several also reference `ToolManager::ins()`, `AppManager::ins()`, and `SignalMapper::ins()`.

---

### 3. Manager Layer Inconsistency

The manager layer has two distinct personalities that create confusion about where logic belongs:

**Thin facades** (pass-through wrappers):
```cpp
// info_manager.cpp — just delegates
int InfoManager::getCpuCoreCount() const { return ci.getCpuCoreCount(); }
quint64 InfoManager::getMemUsed() const  { return mi.getMemUsed(); }
```

**Thick services** (real business logic):
- `CleanerService` — 300+ lines of scan logic across 6 categories, file partitioning, min-age filtering, statistics collection
- `ScheduleManager` — 500+ lines of CRUD operations, JSON persistence, OS-native scheduler sync (launchd plists, systemd timers, cron entries)

**The problem:** There's no clear architectural principle for when logic belongs in:
- **Info/Tool classes** — data acquisition and system operations
- **Managers** — business logic and orchestration
- **Pages** — presentation and user interaction

This leads to ambiguity. The `CleanerService` duplicates some scanning logic that originated in `SystemCleanerPage`. New features face the question "does this go in the Manager or the Page?" without a guiding principle.

---

### 4. ~~CMake GLOB_RECURSE~~ (Resolved)

**Status:** Resolved in Phase 2. All `GLOB_RECURSE` calls replaced with explicit `set()` source lists in `CMakeLists.txt`. Source files are now organized into 10 explicit lists (shared/platform, core/GUI, .cpp/.h) plus a translations list. When adding or removing a file, developers update the corresponding `set()` block — CMake reconfiguration is deterministic.

---

### 5. ~~No Automated Test Suite~~ (Infrastructure Added)

**Status:** Test infrastructure established in Phase 4 (FR-33). Qt Test framework configured with CTest integration, CI test step on all 3 matrix runners, and one smoke test (`FormatUtil::formatBytes()`). The `tests/` directory, `BUILD_TESTING` CMake option, and CI pipeline are in place. Unit test coverage is minimal (1 test) — expanding to 15-20 tests is tracked in Phase 7 (FR-36).

**Remaining barriers to broader testing:**
- Singleton coupling (§2) blocks mock injection — addressed in Phase 6 (FR-35)
- Info classes read real OS state — no abstraction for test data — addressed in Phase 5 (FR-34)
- Pages tightly bound to `.ui` files and Qt widgets

---

### 6. Fragmented Timer/Polling

Each page manages its own QTimer refresh cycles independently:

```cpp
// dashboard_page.cpp — 3 separate timers in one page
mTimer = new QTimer(this);
connect(mTimer, &QTimer::timeout, this, &DashboardPage::updateCpuBar);
connect(mTimer, &QTimer::timeout, this, &DashboardPage::updateMemoryBar);
connect(mTimer, &QTimer::timeout, this, &DashboardPage::updateNetworkBar);
mTimer->start(1000);  // 1s

QTimer *timerDisk = new QTimer(this);
connect(timerDisk, &QTimer::timeout, this, &DashboardPage::updateDiskBar);
timerDisk->start(5000);  // 5s

QTimer *timerDiskHealth = new QTimer(this);
connect(timerDiskHealth, &QTimer::timeout, [this]() { im->refreshDiskHealth(); });
timerDiskHealth->start(30000);  // 30s
```

**The problems:**
- **~25 active QTimers** across all pages when the app is open
- **Duplicate work** — Dashboard and Resources both call `InfoManager::ins()->updateMemoryInfo()` on their own 1s timers
- **No background optimization** — all timers fire even when the app is minimized to tray, wasting CPU and battery
- **No coordination** — each page is an island, unaware of what other pages are polling

**Impact:** On a laptop, unnecessary polling while minimized drains battery. On a busy system, redundant `InfoManager` update calls do double the work. The timer count will grow linearly with every new monitoring feature.

---

### ~~7. QSS Token Validation Gap~~ (Addressed)

**Status:** Addressed in Phase 3 (FR-32). `AppManager::updateStylesheet()` now includes two runtime validation passes before token replacement:

1. **Token existence check:** Scans the raw QSS template for `@token` patterns (regex `@([a-zA-Z][a-zA-Z0-9_]*)`), skips `@dpN` DPI tokens, and emits `qWarning()` for any token not found in the active theme's `values.ini`.
2. **Color format validation:** Iterates all `values.ini` entries (excluding `@themeName`) and warns if any value is not a valid CSS hex color (`#rgb`, `#rgba`, `#rrggbb`, or `#rrggbbaa`).

Both checks emit `qWarning()` at runtime (visible in debug output) without altering application behavior. This catches typos, missing tokens, and malformed color values during development and theme switching.

**Remaining gap:** Hardcoded inline `setStyleSheet()` calls (e.g., in `disk_usage_launcher_widget.cpp`) and missing QSS rules for unstyled widgets are not caught by token validation. These represent a different failure class (BUG-21, BUG-33, BUG-36, BUG-38, BUG-40) that would require static analysis or screenshot regression tests (FR-41) to detect.

---

## Recommended Improvements

### Priority 1: Critical (Foundational)

#### ~~1A. Replace GLOB_RECURSE with Explicit Source Lists~~ (Done)

**Status:** Completed in Phase 2. All 9 `GLOB_RECURSE` calls replaced with inline `set()` blocks using Option A (inline lists). Source files organized into core shared/platform and GUI shared/platform categories with platform-conditional `if(APPLE)/else()` blocks.

---

#### 1B. Add Abstract Base Classes for Platform Code

**What:** Define pure virtual interfaces for the 11 Info classes and 5 Tool classes. Platform implementations become named subclasses.

**Why:** Catches missing platform methods at compile time (not link time). Makes the platform contract explicit. Future-proofs for additional platforms (Windows, BSD).

**How:**

```cpp
// shared/nexis-core/Info/cpu_info.h — becomes an interface
class CpuInfo {
public:
    virtual ~CpuInfo() = default;
    virtual int getCpuPhysicalCoreCount() const = 0;
    virtual int getCpuCoreCount() const = 0;
    virtual QList<int> getCpuPercents() const = 0;
    virtual QList<double> getLoadAvgs() const = 0;
    virtual double getAvgClock() const = 0;
    virtual QList<double> getClocks() const = 0;
};

// linux/nexis-core/Info/cpu_info_linux.h
class CpuInfoLinux : public CpuInfo {
public:
    int getCpuCoreCount() const override;  // reads /proc/cpuinfo
    // ... all pure virtuals implemented
};

// macos/nexis-core/Info/cpu_info_macos.h
class CpuInfoMacOS : public CpuInfo {
public:
    int getCpuCoreCount() const override;  // calls sysctl
    // ... all pure virtuals implemented
};
```

InfoManager would use `#ifdef Q_OS_MAC` to instantiate the correct subclass:

```cpp
#ifdef Q_OS_MAC
    std::unique_ptr<CpuInfo> ci = std::make_unique<CpuInfoMacOS>();
#else
    std::unique_ptr<CpuInfo> ci = std::make_unique<CpuInfoLinux>();
#endif
```

**Trade-off:** Adds virtual dispatch overhead (one vtable lookup per call). For a desktop app polling at 1s intervals, this overhead is **completely negligible**.

**Effort:** Medium (touch all 16 classes — split into header + 2 implementations each). Can be done incrementally, one class at a time.

**Files affected:** All files in `shared/nexis-core/Info/`, `shared/nexis-core/Tools/`, and their platform counterparts.

---

### Priority 2: High (Scalability)

#### 2A. Centralized DataRefreshService

**What:** Replace per-page QTimers with a single service that polls at defined intervals and emits data-change signals.

**Why:** Eliminates duplicate polling, enables background optimization, and makes pages reactive instead of active.

**Design:**

```cpp
class DataRefreshService : public QObject {
    Q_OBJECT
public:
    static DataRefreshService *ins();
    void start();
    void pause();   // called when app is minimized/backgrounded
    void resume();  // called when app is restored

signals:
    // 1-second data
    void cpuDataUpdated(QList<int> percents);
    void memoryDataUpdated(quint64 used, quint64 total, quint64 swapUsed, quint64 swapTotal);
    void networkDataUpdated(quint64 rx, quint64 tx);
    void gpuDataUpdated(QList<GpuDevice> devices);

    // 5-second data
    void diskDataUpdated(QList<Disk> disks);

    // 30-second data
    void diskHealthUpdated(QList<DriveHealth> drives);
    void thermalDataUpdated(QList<ThermalSensor> sensors);

private:
    QTimer *mFastTimer;    // 1s — CPU, memory, network, GPU
    QTimer *mMediumTimer;  // 5s — disk usage
    QTimer *mSlowTimer;    // 30s — SMART health, temperature
};
```

**Pages become reactive subscribers:**

```cpp
// Before: DashboardPage owns timers and calls InfoManager
DashboardPage::DashboardPage() {
    mTimer = new QTimer(this);
    connect(mTimer, &QTimer::timeout, this, &DashboardPage::updateCpuBar);
    mTimer->start(1000);
}

// After: DashboardPage subscribes to data updates
DashboardPage::DashboardPage() {
    connect(DataRefreshService::ins(), &DataRefreshService::cpuDataUpdated,
            this, [this](QList<int> percents) {
        int avg = std::accumulate(percents.begin(), percents.end(), 0) / percents.size();
        mCpuBar->setValue(avg);
    });
}
```

**Benefits:**
- ~25 QTimers → 3 QTimers
- Zero duplicate `InfoManager::updateMemoryInfo()` calls
- `pause()`/`resume()` for battery optimization when minimized
- Pages are simpler — just react to data, don't manage polling

**Effort:** Large (refactor all 14 pages to use signals instead of timers). Can be done incrementally — start with Dashboard and Resources, which have the most timers.

**Files affected:** All page `.cpp` files, new `DataRefreshService` manager class.

---

#### 2B. Dependency Injection for Managers

**What:** Add constructor parameters with default values so pages can receive injected managers for testing, while production code remains unchanged.

**Why:** Unblocks unit testing without breaking any existing code.

**How:**

```cpp
// Before
class DashboardPage : public QWidget {
    InfoManager *im = InfoManager::ins();  // hardcoded
};

// After — backward compatible with default argument
class DashboardPage : public QWidget {
public:
    explicit DashboardPage(QWidget *parent = nullptr,
                           InfoManager *infoMgr = InfoManager::ins());
private:
    InfoManager *im;
};

// Production code unchanged:
dashboardPage = new DashboardPage();  // uses singleton default

// Test code can inject mocks:
MockInfoManager mockIM;
DashboardPage testPage(nullptr, &mockIM);
```

**Effort:** Small-medium (touch 14 page constructors + their header files). No runtime behavior change.

**Files affected:** All page `.h` and `.cpp` files (constructor signatures only).

---

#### ~~2C. QSS Token Validation~~ (Done)

**Status:** Completed in Phase 3 (FR-32). Two validation passes added to `updateStylesheet()`: token existence check (QSS `@tokens` vs `values.ini` keys) and color format validation (hex format check on all non-`@themeName` values). Both emit `qWarning()` diagnostics.

---

### Priority 3: Medium (Testing & Quality)

#### 3A. Basic Unit Test Suite (Infrastructure Done)

**Infrastructure (Phase 4, FR-33):** Complete. Qt Test framework configured with CTest, `tests/` directory created, `BUILD_TESTING` CMake option (default ON), CI test step on all 3 matrix runners, and one smoke test validating `FormatUtil::formatBytes()` across all branches.

**Remaining (Phase 7, FR-36):** Write 15-20 unit tests covering:
1. **Info class parsing** — MemoryInfo calculations (BUG-01 was a variable swap), DiskHealthInfo SMART parsing, CpuInfo load average computation
2. **Utility classes** — FormatUtil (expand), FileUtil operations, CommandUtil timeout handling
3. **Manager logic** — CleanerService scan categorization, ScheduleManager CRUD operations

Requires §2B (DI) for testing manager-dependent code.

---

#### 3B. CI Screenshot Regression Tests

**What:** Capture screenshots of each page at startup in CI and compare against reference images to catch visual regressions.

**Why:** BUG-30 (margin regressions across 10 `.ui` files) and BUG-40 (dark mode breakage from hardcoded colors) were caught only by manual QA — sometimes after the code was already committed.

**How:**
- Use Qt's `QPixmap::grab(widget)` to capture each page programmatically
- Compare against reference PNGs using ImageMagick `compare` (perceptual diff)
- Flag differences above a threshold for manual review in CI

**Effort:** Large (initial setup + maintaining reference images). Best as a long-term investment after basic unit tests are in place.

---

### Priority 4: Low (Future-Proofing)

#### 4A. Plugin Architecture

**What:** Define a `NexisPlugin` interface that allows third-party code to add pages, cleaning categories, or monitoring widgets.

**When:** Not needed now. 14 built-in pages cover the core use cases. Consider this only if community demand appears for extensibility (e.g., custom cleaner categories for gaming platforms, Kubernetes monitoring, Raspberry Pi GPIO).

**Rough design:**

```cpp
class NexisPlugin {
public:
    virtual ~NexisPlugin() = default;
    virtual QString name() const = 0;
    virtual QIcon icon() const = 0;
    virtual QWidget *createPage() = 0;
};
// Load from ~/.nexis/plugins/*.so at startup
```

---

#### 4B. Event Bus Upgrade

**What:** Replace `SignalMapper` with a typed event bus library if the signal count grows beyond ~15.

**When:** Currently 7 signals — well within SignalMapper's comfort zone. Only consider migration if the signal count grows significantly, or if events need filtering/prioritization.

**Candidate:** [eventpp](https://github.com/wqking/eventpp) (header-only, C++11+, well-tested).

---

## Strategic Direction

### QWidgets vs QML

**Question:** Should Nexis migrate from QWidgets to QML for the UI layer?

**Context:** BUG-07 (HiDPI scaling) is commonly cited as motivation for QML migration. Nexis solved this within QWidgets using `Dpi::scale()` + `@dpN` QSS tokens.

**Analysis:**

| Factor | QWidgets (Current) | QML (Migration) |
|--------|-------------------|-----------------|
| HiDPI | Solved via Dpi::scale() + @dpN tokens | Native support |
| Animations | Basic (SlidingStackedWidget) | Rich, declarative |
| Development speed | Qt Designer + QSS (familiar tooling) | QML + JavaScript (new skills) |
| Existing investment | 14 pages, 29 .ui files, comprehensive QSS | Complete rewrite required |
| Migration effort | N/A | 3-6 months minimum |
| Community contributions | C++/QSS (common skills) | QML (niche skills) |

**Recommendation: Stay with QWidgets.**

The QWidget stack is working well. HiDPI is solved. The theme system is elegant. The `.ui` files provide visual editing. A QML migration would be a multi-month rewrite with minimal user-facing benefit — the app already looks and works well. The effort would be far better spent on new features.

QML should only be reconsidered if a future feature genuinely requires it (e.g., complex data visualizations with smooth animations that QCharts can't handle).

---

### Testing Strategy

A phased approach to introducing automated testing:

**Phase 1 (Done):** Test infrastructure established — Qt Test + CTest + CI integration (FR-33). One smoke test validates the pipeline.

**Phase 2 (Next):** Implement dependency injection (§2B) across all page constructors, then add 15-20 unit tests covering core library logic:
- FormatUtil, FileUtil, CommandUtil (pure functions, easy to test)
- MemoryInfo parsing (prevent BUG-01 class regressions)
- DiskHealthInfo SMART data parsing
- CleanerService scan categorization

**Phase 3 (Following quarter):** Add integration tests for managers:
- ScheduleManager CRUD (JSON persistence, no OS scheduler interaction)
- SettingManager defaults and overrides
- AptSourceTool format parsing (mock filesystem)

**Phase 4 (Later):** Explore UI regression testing:
- Screenshot comparison in CI
- Automated dark/light mode rendering checks

**Target:** 30-40% test coverage of core library and managers by end of the cycle. Focus on high-risk areas (file operations, system commands, data parsing) rather than UI code.

---

### Incremental Evolution vs Clean-Slate Redesign

**Two paths forward:**

**Path A: Incremental Evolution (Recommended)**
- Implement P1 and P2 improvements (explicit CMake, abstract interfaces, DI, DataRefreshService, QSS validation)
- Add tests incrementally
- Keep QWidgets, keep singleton managers (with DI escape hatches)
- Each improvement is a self-contained change that can be reviewed and merged independently
- Risk: low. Each change is small and reversible.

**Path B: Clean-Slate Redesign**
- Full DI framework (e.g., Google Fruit)
- Event-driven architecture replacing all timers and SignalMapper
- QML migration
- Comprehensive test suite from day one
- Risk: high. 3-6 month rewrite during which no features can ship. The old codebase is abandoned.

**Recommendation: Path A.** Nexis is feature-complete, stable, and actively used. The architecture's weaknesses are real but manageable. Incremental improvements deliver value continuously without risking the working product. Path B is only justified if the architecture becomes a genuine blocker for critical features — which it currently is not.

---

### Architectural Vision

**Where Nexis's architecture should be in 12 months:**

1. ~~**Explicit source lists in CMake**~~ — Done (Phase 2)
2. **Abstract base classes for all platform code** — Compile-time enforcement of platform parity
3. **Dependency injection on all page constructors** — Testable without framework overhead
4. **Centralized DataRefreshService** — 3 timers instead of 25, with pause/resume for battery optimization
5. **QSS token validation** — Build-time warnings for theme inconsistencies
6. **20-30 unit tests** covering core library parsing and utility functions
7. **Still QWidgets** — Proven, stable, with the HiDPI problem solved
8. **Still singletons** — But with DI constructors as escape hatches for testing

The architecture doesn't need a revolution. It needs **targeted reinforcements** in the areas that have historically caused bugs (theme validation, missing platform methods) and **structural preparation** for the features and quality bar the project is growing toward (testing, battery optimization, maintainability at scale).

---

## Appendix: Key Files for Architectural Work

| File | Why It Matters |
|------|---------------|
| `CMakeLists.txt` | Replace GLOB_RECURSE (§1A) |
| `shared/nexis-core/Info/cpu_info.h` | Template for abstract base class pattern (§1B) — 10 other Info classes follow this one |
| `shared/nexis/Managers/info_manager.h` | Add DI constructor args (§2B), holds all Info instances |
| `shared/nexis/Managers/app_manager.cpp` | ~~Add QSS token validation (§2C)~~ Done — token + color format validation added |
| `shared/nexis/Pages/Dashboard/dashboard_page.cpp` | Primary refactor target for DataRefreshService (§2A) — has 3 timers |
| `shared/nexis/Pages/Resources/resources_page.cpp` | Secondary refactor target — duplicates Dashboard polling |
| `shared/nexis/signal_mapper.h` | Global event bus (7 signals) — monitor signal count growth |
| `shared/nexis/Managers/cleaner_service.cpp` | Example of thick manager with real business logic |
| `shared/nexis/Managers/schedule_manager.cpp` | Example of OS-native integration complexity |
