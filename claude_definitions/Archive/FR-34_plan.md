# FR-34 Plan: Platform Interface Formalization

> Phase 5 of the Implementation Roadmap
> Date: 2026-02-20

---

## Overview

Convert the 10 Info classes and 4 platform-split Tool classes from convention-based shadowing to compiler-enforced abstract interfaces. Missing platform implementations will become compile-time errors (pure virtual) rather than link-time errors (undefined symbol).

**Scope:** 10 Info classes, 4 Tool classes (excluding DockerTool — already shared-only), InfoManager, ToolManager, CMakeLists.txt.

**Design approach:** Two-tier hierarchy for classes with shared state, single-tier for classes without:

```
With shared state (7 classes):
  FooInfo (abstract interface + shared state + shared getters, in shared/)
    -> FooInfoLinux (platform methods, in linux/)
    -> FooInfoMacOS (platform methods, in macos/)

Without shared state (3 classes: CpuInfo, NetworkInfo, ThermalInfo):
  FooInfo (pure virtual interface, in shared/)
    -> FooInfoLinux (all methods, in linux/)
    -> FooInfoMacOS (all methods, in macos/)
```

**Why two-tier not three-tier:** A three-tier hierarchy (IFoo → FooBase → FooLinux/FooMacOS) adds an unnecessary abstraction layer. Since there is no use case for swapping implementations at runtime or having multiple interface consumers, we can combine the interface and shared implementation into one abstract class. The shared header becomes the abstract base with `= 0` on platform methods and concrete implementations for shared getters. This preserves the existing shared `.cpp` files unchanged.

**Key constraints:**
- No runtime behavior change — the app must function identically after refactoring
- ProcessInfo inherits QObject — the abstract base must also inherit QObject
- Tool classes change from static-only to instance methods (virtual methods cannot be static)
- PackageTool has divergent per-platform APIs — use a unified abstract interface

---

## Tasks

### Phase 5.1: Establish the pattern — CpuInfo (simplest Info class)

**Why first:** CpuInfo has no shared `.cpp`, no data members in the header, no constructor, no QObject. It's the simplest class to convert and establishes the pattern for all others.

**Files to modify:**
- `shared/nexis-core/Info/cpu_info.h` — convert to abstract base
- `linux/nexis-core/Info/cpu_info.cpp` — add `CpuInfoLinux` class
- `macos/nexis-core/Info/cpu_info.cpp` — add `CpuInfoMacOS` class
- New: `linux/nexis-core/Info/cpu_info_linux.h` — platform subclass header
- New: `macos/nexis-core/Info/cpu_info_macos.h` — platform subclass header
- `shared/nexis/Managers/info_manager.h` — change `CpuInfo ci;` to `std::unique_ptr<CpuInfo> ci;`
- `shared/nexis/Managers/info_manager.cpp` — instantiate `CpuInfoLinux`/`CpuInfoMacOS`
- `CMakeLists.txt` — add new header files to source lists

**Changes to `cpu_info.h`:**
```cpp
class NEXISCORESHARED_EXPORT CpuInfo
{
public:
    virtual ~CpuInfo() = default;

    virtual int getCpuPhysicalCoreCount() const = 0;
    virtual int getCpuCoreCount() const = 0;
    virtual QList<int> getCpuPercents() const = 0;
    virtual QList<double> getLoadAvgs() const = 0;
    virtual double getAvgClock() const = 0;
    virtual QList<double> getClocks() const = 0;
};
```

The private method `getCpuPercent()` moves to the platform subclasses as a private implementation detail.

**Platform subclass pattern (`cpu_info_linux.h`):**
```cpp
#include <Info/cpu_info.h>

class CpuInfoLinux : public CpuInfo
{
public:
    int getCpuPhysicalCoreCount() const override;
    int getCpuCoreCount() const override;
    QList<int> getCpuPercents() const override;
    QList<double> getLoadAvgs() const override;
    double getAvgClock() const override;
    QList<double> getClocks() const override;

private:
    int getCpuPercent(const QList<double> &cpuTimes, const int &processor = 0) const;
};
```

**InfoManager changes (partial — just CpuInfo for this task):**
```cpp
// info_manager.h
#include <memory>
std::unique_ptr<CpuInfo> ci;  // was: CpuInfo ci;

// info_manager.cpp
#ifdef Q_OS_MACOS
#include <Info/cpu_info_macos.h>
#else
#include <Info/cpu_info_linux.h>
#endif

InfoManager::InfoManager() {
#ifdef Q_OS_MACOS
    ci = std::make_unique<CpuInfoMacOS>();
#else
    ci = std::make_unique<CpuInfoLinux>();
#endif
}
```

All `ci.method()` calls in `info_manager.cpp` change to `ci->method()`.

**Acceptance criteria:**
- [x] `cpu_info.h` declares all public methods as `virtual ... = 0` with `virtual ~CpuInfo() = default`
- [x] `CpuInfoLinux` and `CpuInfoMacOS` classes compile and override all methods
- [x] InfoManager holds `std::unique_ptr<CpuInfo>` and uses arrow operator
- [x] Incremental build passes on current platform
- [x] App behavior unchanged (Dashboard CPU gauge, Resources CPU graph)

---

### Phase 5.2: Convert remaining stateless Info classes (NetworkInfo, ThermalInfo)

**NetworkInfo** — no shared `.cpp`, has data members and constructor.

**Files to modify:**
- `shared/nexis-core/Info/network_info.h` — abstract base (keeps `defaultNetworkInterface` member; `rxPath`/`txPath` move to Linux subclass)
- `linux/nexis-core/Info/network_info.cpp` → implements `NetworkInfoLinux`
- `macos/nexis-core/Info/network_info.cpp` → implements `NetworkInfoMacOS`
- New: `linux/nexis-core/Info/network_info_linux.h`
- New: `macos/nexis-core/Info/network_info_macos.h`

**Design choice for NetworkInfo:**
- `defaultNetworkInterface` is used identically on both platforms → stays in abstract base as `protected`
- `rxPath`, `txPath` are Linux-only → move to `NetworkInfoLinux` as private members
- Constructor becomes `protected` in base (sets `defaultNetworkInterface`), subclass constructors call base

**ThermalInfo** — no shared `.cpp`, has data members and constructor.

**Files to modify:**
- `shared/nexis-core/Info/thermal_info.h` — abstract base (keeps `mSensors` as `protected`)
- `linux/nexis-core/Info/thermal_info.cpp` → `ThermalInfoLinux`
- `macos/nexis-core/Info/thermal_info.cpp` → `ThermalInfoMacOS`
- New: `linux/nexis-core/Info/thermal_info_linux.h`
- New: `macos/nexis-core/Info/thermal_info_macos.h`

**Design choice for ThermalInfo:**
- `mSensors` is used by shared-pattern methods `getSensors()` and `hasSensors()` → keep in abstract base as `protected`
- `getSensors()` and `hasSensors()` are trivial getters identical on both platforms → implement in base (non-virtual)
- `getTemperature()` and `discoverSensors()` are platform-specific → `= 0`
- Constructor delegates to `discoverSensors()` → becomes platform subclass constructor

**InfoManager changes:** Convert `ni` and `ti` from value to `std::unique_ptr`, add factory `#ifdef`.

**Acceptance criteria:**
- [x] `NetworkInfoLinux` owns `rxPath`/`txPath`; `NetworkInfoMacOS` does not declare them
- [x] `ThermalInfo` base class provides `getSensors()` and `hasSensors()` as non-virtual; `getTemperature()` and `discoverSensors()` are pure virtual
- [x] Incremental build passes
- [x] Dashboard network/thermal readings unchanged

---

### Phase 5.3: Convert Info classes with shared state (MemoryInfo, DiskInfo, GpuInfo, BatteryInfo, DiskHealthInfo, SystemInfo)

These 6 classes follow the same pattern: shared header declares data members + shared getters in `*_shared.cpp`, platform methods in platform `.cpp`.

**For each class:**
1. Convert shared header to abstract base class:
   - Mark platform-specific methods as `virtual ... = 0`
   - Keep shared getters as concrete (non-virtual) implementations
   - Keep data members as `protected` (accessible by subclasses)
   - Add `virtual ~ClassName() = default`
2. Create platform subclass headers (`linux/`, `macos/`)
3. Update platform `.cpp` files to define the subclass
4. Shared `*_shared.cpp` files remain unchanged (they implement the base class methods)

**Class-by-class specifics:**

**MemoryInfo:**
- Pure virtual: `updateMemoryInfo()`
- Concrete: constructor (zero-init), 6 getters
- Data: 10 `quint64` members → `protected`

**DiskInfo:**
- Pure virtual: `getDiskIO()`, `getDiskNames()`
- Concrete: `getDisks()`, `updateDiskInfo()`, `fileSystemTypes()`, `devices()`
- Data: `QList<Disk> disks` → `protected`
- Note: platform file is `disk_info_platform.cpp` (not `disk_info.cpp`)

**GpuInfo:**
- Pure virtual: `updateGpuInfo()`, `discoverGpus()` (currently private → becomes `protected` pure virtual)
- Concrete: `getGpuDevices()`, `hasGpu()`
- Data: `QList<GpuDevice> mDevices` → `protected`
- `GpuDevice` struct: `sysfsLoadPath`/`queryCommand` fields are Linux-only but harmless in shared struct (empty strings on macOS). Keep in shared struct to avoid complexity.

**BatteryInfo:**
- Pure virtual: `updateBatteryInfo()`, `discoverBattery()` (currently private → `protected`)
- Concrete: `getBatteryData()`, `hasBattery()`
- Data: `BatteryData mData` → `protected`; `QString mBatteryPath` → move to `BatteryInfoLinux` (Linux-only)

**DiskHealthInfo:**
- Pure virtual: `refreshHealth()`, `refreshHealthElevated()`, `discoverDrives()` (currently private → `protected`)
- Concrete: `getDrives()`, `hasDrives()`, `hasSmartctl()`, `deriveHealthVerdict()`
- Data: `QList<DriveHealth> mDrives`, `bool mHasSmartctl` → `protected`

**SystemInfo:**
- Pure virtual: constructor, `getCrashReports()`, `getAppLogs()`, `getAppCaches()`, `getDevToolCaches()`, `getUserList()`, `getGroupList()`
- Concrete: `getHostname()`, `getPlatform()`, `getDistribution()`, `getKernel()`, `getCpuModel()`, `getCpuSpeed()`, `getCpuCore()`, `getUsername()`
- Data: `cpuCore`, `cpuModel`, `cpuSpeed`, `username` → `protected`
- Note: constructor cannot be pure virtual. Instead, make it `protected` in base (default or parameterized), platform subclass constructors populate `cpuCore`/`cpuModel`/`cpuSpeed`/`username`.

**InfoManager changes:** Convert all remaining Info members from value to `std::unique_ptr`, add factory `#ifdef` for each.

**Files per class:**
- Modify: shared header, linux `.cpp`, macos `.cpp`
- New: `linux/nexis-core/Info/{class}_linux.h`, `macos/nexis-core/Info/{class}_macos.h`
- Total new files: 12 (6 classes × 2 platforms)

**Acceptance criteria:**
- [x] All 6 classes have platform subclasses that compile
- [x] Shared `*_shared.cpp` files compile without modification
- [x] InfoManager uses `std::unique_ptr` for all 10 Info objects
- [x] All `ci.method()` calls in `info_manager.cpp` use `ci->method()` (arrow operator)
- [x] Incremental build passes
- [x] All pages display correct data

---

### Phase 5.4: Convert ProcessInfo (QObject special case)

**Why separate:** ProcessInfo inherits `QObject` and uses `Q_OBJECT` + `public slots`. This requires special handling.

**Design:**
```cpp
// shared/nexis-core/Info/process_info.h (abstract base)
class NEXISCORESHARED_EXPORT ProcessInfo : public QObject
{
    Q_OBJECT
public:
    virtual ~ProcessInfo() = default;
    virtual QList<Process> getProcessList() const = 0;

public slots:
    virtual void updateProcesses() = 0;

protected:
    QList<Process> processList;
};
```

Wait — `Q_OBJECT` in an abstract base class works in Qt. The subclass must also use `Q_OBJECT`:

```cpp
// linux/nexis-core/Info/process_info_linux.h
class ProcessInfoLinux : public ProcessInfo
{
    Q_OBJECT
public:
    QList<Process> getProcessList() const override;

public slots:
    void updateProcesses() override;
};
```

**However**, `getProcessList()` is currently shared (in `process_info_shared.cpp`). We can keep it as a concrete method in the base:

```cpp
class NEXISCORESHARED_EXPORT ProcessInfo : public QObject
{
    Q_OBJECT
public:
    virtual ~ProcessInfo() = default;
    QList<Process> getProcessList() const;  // concrete, shared

public slots:
    virtual void updateProcesses() = 0;    // pure virtual

protected:
    QList<Process> processList;
};
```

This is simpler — only `updateProcesses()` needs to be overridden.

**Files to modify:**
- `shared/nexis-core/Info/process_info.h` — abstract base (keep shared method, add pure virtual)
- `shared/nexis-core/Info/process_info_shared.cpp` — no change (already implements `getProcessList()`)
- `linux/nexis-core/Info/process_info.cpp` → `ProcessInfoLinux`
- `macos/nexis-core/Info/process_info.cpp` → `ProcessInfoMacOS`
- New: `linux/nexis-core/Info/process_info_linux.h`
- New: `macos/nexis-core/Info/process_info_macos.h`

**MOC consideration:** Qt's `moc` needs to process both the base header and subclass headers. Since subclass headers are in platform dirs and only one platform is compiled, this should work correctly with CMake's `AUTOMOC`.

**Acceptance criteria:**
- [x] `ProcessInfo` base class compiles with `Q_OBJECT` and pure virtual slot
- [x] `ProcessInfoLinux`/`ProcessInfoMacOS` compile with their own `Q_OBJECT` macros
- [x] `updateProcesses()` slot is callable via signal/slot (verify Processes page works)
- [x] Incremental build passes

---

### Phase 5.5: Convert Tool classes — ServiceTool, AptSourceTool, GnomeSettingsTool

These 3 Tool classes have identical method signatures on both platforms. The main change: convert from static methods to virtual instance methods.

**ServiceTool:**
```cpp
// shared/nexis-core/Tools/service_tool.h
class NEXISCORESHARED_EXPORT ServiceTool
{
public:
    virtual ~ServiceTool() = default;

    virtual QList<Service> getServices() = 0;
    virtual bool serviceIsActive(const QString &serviceName) = 0;
    virtual bool changeServiceStatus(const QString &sname, bool status) = 0;
    virtual bool changeServiceActive(const QString &sname, bool status) = 0;
    virtual bool serviceIsEnabled(const QString &serviceName) = 0;
    virtual QString getServiceDescription(const QString &serviceName) = 0;
};
```

Note: removed `static` keyword. Methods were `const`-incorrect in the original (mutating operations like `changeServiceStatus` were not const, read operations like `getServices` should be const but weren't). Preserve current signatures exactly to minimize diff.

**Files per class (3 classes × same pattern):**
- Modify: shared header (remove `static`, add `virtual ... = 0`)
- Modify: platform `.cpp` files (implement subclass)
- New: platform subclass headers (6 files: 3 classes × 2 platforms)

**AptSourceTool** — same pattern. `APTSource` class and `APTSourcePtr` typedef stay in the shared header unchanged.

**GnomeSettingsTool** — same pattern. The shadowed `gnome_settings_constants.h` stays as-is (included by the platform `.cpp`).

**Acceptance criteria:**
- [x] All 3 Tool classes have platform subclasses
- [x] `static` keyword removed from all virtual methods
- [x] Incremental build passes
- [x] Services page, Sources page, and Settings page function correctly

---

### Phase 5.6: Convert PackageTool (most complex case)

**The problem:** Linux and macOS have completely different method sets. Linux has 14+ package-manager-specific methods; macOS has 6 Homebrew/native methods.

**Design decision: Unified abstract interface.**

Define `PackageTool` with a common set of methods that both platforms implement:

```cpp
// shared/nexis-core/Tools/package_tool.h (NEW — replaces platform headers)
class NEXISCORESHARED_EXPORT PackageTool
{
public:
    virtual ~PackageTool() = default;

    virtual QFileInfoList getPackageCaches() = 0;
    virtual QList<Package> getPackages() = 0;
    virtual bool removePackages(const QStringList &packages, bool purge = false) = 0;
    virtual QStringList dryRunRemove(const QStringList &packages) = 0;

    // Platform-specific extras (optional, default no-op)
    virtual QStringList getSnapPackages() { return {}; }
    virtual bool removeSnapPackages(const QStringList &) { return false; }
    virtual QList<Package> getInstalledApps() { return {}; }
    virtual bool trashApps(const QStringList &) { return false; }

    virtual PackageTools currentPackageTool() const = 0;

    // Shared utility (non-virtual)
    static QString friendlySectionName(const QString &section);
};
```

**Linux subclass (`PackageToolLinux`):**
- Implements the 4 core methods by dispatching to APT/DNF/YUM/Pacman based on detected package manager
- Overrides `getSnapPackages()`/`removeSnapPackages()` with real implementations
- Keeps internal dpkg/rpm/pacman methods as private implementation details

**macOS subclass (`PackageToolMacOS`):**
- Implements 4 core methods using Homebrew
- Overrides `getInstalledApps()`/`trashApps()` with real implementations

**Impact on ToolManager:**
- ToolManager currently dispatches per-platform via its own platform `.cpp` files
- After refactoring: ToolManager holds a `std::unique_ptr<PackageTool>` and delegates directly
- ToolManager's platform `.cpp` files can be **unified into a shared `.cpp`** since the dispatch logic moves into PackageTool subclasses

**Files to modify:**
- `shared/nexis-core/Tools/package_tool_shared.h` — keep `Package` struct and `PackageTools` enum only
- Replace `linux/nexis-core/Tools/package_tool.h` → `linux/nexis-core/Tools/package_tool_linux.h`
- Replace `macos/nexis-core/Tools/package_tool.h` → `macos/nexis-core/Tools/package_tool_macos.h`
- New: `shared/nexis-core/Tools/package_tool.h` — abstract base class (currently doesn't exist in shared)
- Modify: `linux/nexis-core/Tools/package_tool.cpp` — implement `PackageToolLinux`
- Modify: `macos/nexis-core/Tools/package_tool.cpp` — implement `PackageToolMacOS`
- `static const PackageTools currentPackageTool;` → becomes `virtual PackageTools currentPackageTool() const;`

**Note:** The `currentPackageTool` static member is referenced in multiple places. Changing it to a virtual method means updating all call sites from `PackageTool::currentPackageTool` to `toolManager->currentPackageTool()` or similar.

**Acceptance criteria:**
- [x] Shared `PackageTool` abstract base class in `shared/nexis-core/Tools/`
- [x] `PackageToolLinux` dispatches to correct package manager (APT/DNF/YUM/Pacman)
- [x] `PackageToolMacOS` uses Homebrew + native apps
- [x] `ToolManager` holds `std::unique_ptr<PackageTool>` and delegates
- [x] Uninstaller page, Cleaner page package caches work correctly

---

### Phase 5.7: Update ToolManager

**Current state:** ToolManager has platform-specific `.cpp` files (`linux/nexis/Managers/tool_manager.cpp` and `macos/nexis/Managers/tool_manager.cpp`) that dispatch to static Tool methods.

**Target state:** ToolManager holds `std::unique_ptr` to each Tool interface and delegates to instance methods. Platform-specific ToolManager `.cpp` files can be unified.

**Files to modify:**
- `shared/nexis/Managers/tool_manager.h` — add `std::unique_ptr` members for each Tool
- Replace `linux/nexis/Managers/tool_manager.cpp` and `macos/nexis/Managers/tool_manager.cpp` with a single `shared/nexis/Managers/tool_manager.cpp` that uses `#ifdef` for instantiation

**New ToolManager pattern:**
```cpp
class ToolManager
{
public:
    static ToolManager *ins();

    // Delegating methods (same interface as before)
    QList<Service> getServices() const;
    QList<Package> getPackages() const;
    // ...

private:
    static ToolManager *instance;

    std::unique_ptr<ServiceTool> serviceTool;
    std::unique_ptr<PackageTool> packageTool;
    std::unique_ptr<AptSourceTool> aptSourceTool;
    std::unique_ptr<GnomeSettingsTool> gnomeSettingsTool;
    // DockerTool remains static-only (no instance needed)
};
```

**ToolManager constructor:**
```cpp
ToolManager::ToolManager() {
#ifdef Q_OS_MACOS
    serviceTool = std::make_unique<ServiceToolMacOS>();
    packageTool = std::make_unique<PackageToolMacOS>();
    aptSourceTool = std::make_unique<AptSourceToolMacOS>();
    gnomeSettingsTool = std::make_unique<GnomeSettingsToolMacOS>();
#else
    serviceTool = std::make_unique<ServiceToolLinux>();
    packageTool = std::make_unique<PackageToolLinux>();
    aptSourceTool = std::make_unique<AptSourceToolLinux>();
    gnomeSettingsTool = std::make_unique<GnomeSettingsToolLinux>();
#endif
}
```

**Acceptance criteria:**
- [x] ToolManager unified into single shared `.cpp` file
- [x] Platform `.cpp` files in `linux/nexis/Managers/` and `macos/nexis/Managers/` removed
- [x] All ToolManager methods delegate to instances via arrow operator
- [x] DockerTool remains unchanged (static methods, no instance)
- [x] Incremental build passes

---

### Phase 5.8: Update CMakeLists.txt (explicit source lists)

**New files to add:**
```
# Info class platform subclass headers (CORE_PLAT_HDRS)
# Linux:
linux/nexis-core/Info/cpu_info_linux.h
linux/nexis-core/Info/memory_info_linux.h
linux/nexis-core/Info/disk_info_linux.h
linux/nexis-core/Info/network_info_linux.h
linux/nexis-core/Info/system_info_linux.h
linux/nexis-core/Info/process_info_linux.h
linux/nexis-core/Info/thermal_info_linux.h
linux/nexis-core/Info/gpu_info_linux.h
linux/nexis-core/Info/battery_info_linux.h
linux/nexis-core/Info/disk_health_info_linux.h

# macOS (same pattern with _macos.h):
macos/nexis-core/Info/cpu_info_macos.h
... (10 files)

# Tool class platform subclass headers (CORE_PLAT_HDRS)
linux/nexis-core/Tools/service_tool_linux.h
linux/nexis-core/Tools/apt_source_tool_linux.h
linux/nexis-core/Tools/gnome_settings_tool_linux.h
linux/nexis-core/Tools/package_tool_linux.h

macos/nexis-core/Tools/service_tool_macos.h
macos/nexis-core/Tools/apt_source_tool_macos.h
macos/nexis-core/Tools/gnome_settings_tool_macos.h
macos/nexis-core/Tools/package_tool_macos.h

# New shared header:
shared/nexis-core/Tools/package_tool.h  (abstract base — replaces platform headers)
```

**Files to remove from lists:**
```
# Old platform-specific PackageTool headers (replaced by subclass headers):
linux/nexis-core/Tools/package_tool.h  → linux/nexis-core/Tools/package_tool_linux.h
macos/nexis-core/Tools/package_tool.h  → macos/nexis-core/Tools/package_tool_macos.h

# ToolManager platform .cpp files (unified into shared):
linux/nexis/Managers/tool_manager.cpp  → shared/nexis/Managers/tool_manager.cpp
macos/nexis/Managers/tool_manager.cpp  → (removed, merged into shared)
```

**GUI source list changes:**
- `tool_manager.cpp` moves from `GUI_PLAT_SRCS` to `GUI_SHARED_SRCS`

**Acceptance criteria:**
- [x] All new header files listed in `CMakeLists.txt`
- [x] Removed files no longer listed
- [x] Clean build passes with zero warnings about missing files

---

### Phase 5.9: Build verification

**What to do:**
1. Full clean rebuild on macOS: `rm -rf build && cmake -B build ... && cmake --build build`
2. Run existing tests: `ctest --test-dir build --output-on-failure`
3. Launch the app and verify every page:
   - Dashboard: CPU, memory, disk, network, battery, thermal, GPU gauges
   - Resources: CPU/memory/network graphs
   - Processes: process list loads
   - Services: service list loads
   - Startup Apps: loads correctly
   - Cleaner: cache scanning works
   - Uninstaller: package list loads
   - Sources: source list loads
   - Settings: GNOME/macOS settings page loads
   - Docker: container/image/volume lists load
   - Hardware Info: all hardware data populates
   - Search: search works across pages
4. Verify no `qWarning()` regressions in console output

**Acceptance criteria:**
- [x] Clean rebuild: zero errors, zero new warnings
- [x] All existing tests pass
- [x] App launches and all 14 pages display correct data
- [x] No runtime crashes or assertion failures

---

### Phase 5.10: Update tracking and documentation

**Files to update:**

1. `FEATURE_REQUESTS.md` — Mark FR-34 as `[x]` with resolution note
2. `docs/IMPLEMENTATION_ROADMAP.md` — Mark all Phase 5 tasks as `[x]`
3. `docs/ARCHITECTURE_REVIEW.md`:
   - Update weakness §1 (No Formal Platform Interfaces) → mark as addressed
   - Note the two-tier hierarchy pattern
   - Update class counts if they've changed
4. `docs/APPLICATION_OVERVIEW.md`:
   - Update architecture section to mention abstract base classes
   - Note the platform subclass naming convention
5. `CLAUDE.md` — if any build or testing conventions change, update

**Acceptance criteria:**
- [x] FR-34 marked `[x]` in `FEATURE_REQUESTS.md`
- [x] Phase 5 tasks marked `[x]` in roadmap
- [x] Architecture Review §1B updated
- [x] Commit and push

---

## New File Inventory

| File | Type | Description |
|------|------|-------------|
| `shared/nexis-core/Tools/package_tool.h` | New shared header | Abstract base class for PackageTool |
| `linux/nexis-core/Info/cpu_info_linux.h` | New platform header | CpuInfoLinux subclass |
| `linux/nexis-core/Info/memory_info_linux.h` | New platform header | MemoryInfoLinux subclass |
| `linux/nexis-core/Info/disk_info_linux.h` | New platform header | DiskInfoLinux subclass |
| `linux/nexis-core/Info/network_info_linux.h` | New platform header | NetworkInfoLinux subclass |
| `linux/nexis-core/Info/system_info_linux.h` | New platform header | SystemInfoLinux subclass |
| `linux/nexis-core/Info/process_info_linux.h` | New platform header | ProcessInfoLinux subclass |
| `linux/nexis-core/Info/thermal_info_linux.h` | New platform header | ThermalInfoLinux subclass |
| `linux/nexis-core/Info/gpu_info_linux.h` | New platform header | GpuInfoLinux subclass |
| `linux/nexis-core/Info/battery_info_linux.h` | New platform header | BatteryInfoLinux subclass |
| `linux/nexis-core/Info/disk_health_info_linux.h` | New platform header | DiskHealthInfoLinux subclass |
| `linux/nexis-core/Tools/service_tool_linux.h` | New platform header | ServiceToolLinux subclass |
| `linux/nexis-core/Tools/apt_source_tool_linux.h` | New platform header | AptSourceToolLinux subclass |
| `linux/nexis-core/Tools/gnome_settings_tool_linux.h` | New platform header | GnomeSettingsToolLinux subclass |
| `linux/nexis-core/Tools/package_tool_linux.h` | New platform header | PackageToolLinux subclass |
| `macos/nexis-core/Info/cpu_info_macos.h` | New platform header | (same 10 Info + 4 Tool for macOS) |
| ... (14 more macOS headers) | | |
| **Total new files:** | **29** | 28 platform headers + 1 shared abstract header |

## Files Removed

| File | Reason |
|------|--------|
| `linux/nexis-core/Tools/package_tool.h` | Replaced by `package_tool_linux.h` |
| `macos/nexis-core/Tools/package_tool.h` | Replaced by `package_tool_macos.h` |
| `linux/nexis/Managers/tool_manager.cpp` | Unified into `shared/nexis/Managers/tool_manager.cpp` |
| `macos/nexis/Managers/tool_manager.cpp` | Unified into `shared/nexis/Managers/tool_manager.cpp` |

## Files Modified (Not New)

| File | Changes |
|------|---------|
| 10 shared Info headers | Add `virtual` / `= 0`, `virtual ~Foo() = default`, change `private:` to `protected:` for data |
| 20 platform Info `.cpp` files | Define subclass, add `override` keyword |
| 3 shared Tool headers | Remove `static`, add `virtual ... = 0` |
| 6 platform Tool `.cpp` files | Define subclass |
| `shared/nexis/Managers/info_manager.h` | `std::unique_ptr` for all 10 Info members |
| `shared/nexis/Managers/info_manager.cpp` | Factory `#ifdef` block, `.` → `->` for all member access |
| `shared/nexis/Managers/tool_manager.h` | Add `std::unique_ptr` for 4 Tool members |
| `shared/nexis/Managers/tool_manager.cpp` | Unified from platform files, factory `#ifdef` block |
| `CMakeLists.txt` | Add/remove/move files in source lists |
| 7 shared `*_shared.cpp` files | **No changes** (implement base class methods, same signatures) |

---

## Risk Assessment

**Risk: Moderate.** This touches 50+ files and changes how all platform code is instantiated.

**Mitigations:**
1. Incremental approach — one class at a time, build after each
2. Shared `.cpp` files unchanged — shared getters continue to work
3. No behavior change — same code executes, just through a vtable dispatch
4. Virtual dispatch overhead is negligible (methods called at 1-30 Hz polling rates)

**Biggest risk:** PackageTool refactoring (Phase 5.6) changes the API surface. All call sites in ToolManager and any direct PackageTool callers must be updated.

**ProcessInfo risk:** `Q_OBJECT` macro in abstract class + subclass is a well-supported Qt pattern, but `AUTOMOC` needs to find both headers. Since only one platform's headers are included in the build, this should work.

---

## Order of Implementation

The tasks are ordered for incremental safety:

1. **5.1** — CpuInfo (establish pattern, simplest case)
2. **5.2** — NetworkInfo, ThermalInfo (stateless, validate pattern)
3. **5.3** — 6 Info classes with shared state (bulk conversion)
4. **5.4** — ProcessInfo (QObject edge case)
5. **5.5** — ServiceTool, AptSourceTool, GnomeSettingsTool (static→instance)
6. **5.6** — PackageTool (API unification, most complex)
7. **5.7** — ToolManager (unify platform files)
8. **5.8** — CMakeLists.txt (update source lists)
9. **5.9** — Full build verification
10. **5.10** — Documentation and tracking updates

**Estimated total effort:** 8-12 hours across multiple sessions.

**Release target:** v1.4.0
