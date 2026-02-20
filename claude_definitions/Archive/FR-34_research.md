# FR-34 Research: Abstract Base Classes for Platform-Specific Code

## 1. Executive Summary

FR-34 proposes replacing the current include-path-shadowing mechanism (where a single shared header is implemented by platform-specific `.cpp` files selected at build time) with explicit abstract base classes and named subclasses (`CpuInfoLinux`, `CpuInfoMacOS`, etc.). This would make missing platform methods a compile-time error instead of a link-time error.

This research covers all 16 platform-specific classes: 10 Info classes and 6 Tool classes (including `DockerTool` which is shared-only). The analysis documents every public method signature, data member, shared implementation file, asymmetric API surfaces, and how each class is instantiated by its Manager.

---

## 2. Current Architecture: Include-Path Shadowing

### 2.1 How It Works

The CMake build system (`/Users/luke/Documents/GitHub/Nexis/CMakeLists.txt`) uses include directory ordering to resolve platform-specific implementations:

```cmake
# Platform directory: macos/ or linux/
if(APPLE)
  set(PLATFORM_DIR "${PROJECT_ROOT}/macos")
else()
  set(PLATFORM_DIR "${PROJECT_ROOT}/linux")
endif()

# Include dirs: platform first (so platform headers shadow shared when both exist).
target_include_directories(nexis-core PUBLIC
  "${CORE_PLAT_DIR}"        # macos/nexis-core/ or linux/nexis-core/
  "${CORE_PLAT_DIR}/Info"
  "${CORE_PLAT_DIR}/Tools"
  "${CORE_PLAT_DIR}/Utils"
  "${CORE_SHARED_DIR}"      # shared/nexis-core/
  "${CORE_SHARED_DIR}/Info"
  "${CORE_SHARED_DIR}/Tools"
  "${CORE_SHARED_DIR}/Utils"
)
```

Both platform `.cpp` files and shared `.cpp` files are globbed into the `nexis-core` static library:

```cmake
file(GLOB_RECURSE CORE_SHARED_SRCS "${CORE_SHARED_DIR}/**.cpp")
file(GLOB_RECURSE CORE_PLAT_SRCS   "${CORE_PLAT_DIR}/**.cpp")
add_library(nexis-core STATIC ${CORE_SHARED_SRCS} ${CORE_SHARED_HDRS} ${CORE_PLAT_SRCS} ${CORE_PLAT_HDRS})
```

### 2.2 The Pattern

For most Info classes:
- **Shared header** (`shared/nexis-core/Info/foo_info.h`) declares the class with all public methods.
- **Platform `.cpp`** (`linux/nexis-core/Info/foo_info.cpp` and `macos/nexis-core/Info/foo_info.cpp`) each implement the platform-specific methods.
- **Optional shared `.cpp`** (`shared/nexis-core/Info/foo_info_shared.cpp`) implements cross-platform getters/constructors.

When `#include "cpu_info.h"` is used, the compiler finds the shared header (since only shared has `.h` files for Info classes). Both platform `.cpp` files include it and provide their implementations. Only one platform `.cpp` is compiled (the other platform directory is never globbed).

### 2.3 Exceptions to the Pattern

Two classes deviate:
1. **PackageTool** has **separate platform headers** (`linux/nexis-core/Tools/package_tool.h` and `macos/nexis-core/Tools/package_tool.h`) with completely different method sets. The include-path shadowing selects the correct header.
2. **GnomeSettingsTool** has a shared header but platform-specific **constants headers** (`gnome_settings_constants.h`) that are shadowed per-platform.

### 2.4 The GUI Layer: ToolManager

The `ToolManager` also has platform-specific `.cpp` files:
- `linux/nexis/Managers/tool_manager.cpp`
- `macos/nexis/Managers/tool_manager.cpp`

These delegate to the appropriate `PackageTool` static methods per-platform. The header (`shared/nexis/Managers/tool_manager.h`) is shared.

---

## 3. InfoManager: How Info Objects Are Held

**File:** `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Managers/info_manager.h`

InfoManager is a **singleton** (manual, not thread-safe):
```cpp
class InfoManager {
public:
    static InfoManager *ins();
private:
    static InfoManager *instance;

    CpuInfo ci;
    DiskInfo di;
    MemoryInfo mi;
    NetworkInfo ni;
    SystemInfo si;
    ProcessInfo pi;
    ThermalInfo ti;
    GpuInfo gi;
    BatteryInfo bi;
    DiskHealthInfo dhi;
};
```

**All 10 Info objects are held BY VALUE** as private data members. This is critical for the refactor: switching to abstract base classes requires changing these to pointers (e.g., `std::unique_ptr<CpuInfo>`) since you cannot hold an abstract class by value.

**File:** `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Managers/info_manager.cpp`

The singleton allocates `InfoManager` on the heap via `new InfoManager`, and the value-held Info objects are default-constructed.

---

## 4. ToolManager: How Tool Objects Are Held

**File:** `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Managers/tool_manager.h`

ToolManager is also a **singleton** with no data members for tools. All Tool classes use **static methods only** -- there are no instances of Tool classes:

```cpp
class ToolManager {
public:
    static ToolManager *ins();
private:
    static ToolManager *instance;
};
```

ToolManager delegates to static methods:
- `ServiceTool::getServices()`, `ServiceTool::changeServiceStatus(...)`, etc.
- `PackageTool::getDpkgPackages()`, `PackageTool::getHomebrewPackages()`, etc.
- `AptSourceTool::getSourceList()`, etc.
- `GnomeSettingsTool::isAvailable()`, etc.
- `DockerTool::isDockerInstalled()`, etc.

ToolManager itself has platform-specific `.cpp` implementations (`linux/nexis/Managers/tool_manager.cpp` and `macos/nexis/Managers/tool_manager.cpp`).

---

## 5. Info Classes: Detailed Analysis

### 5.1 CpuInfo

| Aspect | Detail |
|--------|--------|
| **Shared header** | `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Info/cpu_info.h` |
| **Linux impl** | `/Users/luke/Documents/GitHub/Nexis/linux/nexis-core/Info/cpu_info.cpp` |
| **macOS impl** | `/Users/luke/Documents/GitHub/Nexis/macos/nexis-core/Info/cpu_info.cpp` |
| **Shared impl** | None |
| **QObject?** | No |
| **Constructor** | Implicit default (no custom constructor) |
| **Data members** | None |

**Public methods:**
```cpp
int getCpuPhysicalCoreCount() const;
int getCpuCoreCount() const;
QList<int> getCpuPercents() const;
QList<double> getLoadAvgs() const;
double getAvgClock() const;
QList<double> getClocks() const;
```

**Private methods:**
```cpp
int getCpuPercent(const QList<double> &cpuTimes, const int &processor = 0) const;
```

**Asymmetric API:** No -- both platforms implement all 6 public + 1 private methods. The macOS `getCpuPercent()` is a no-op stub (`Q_UNUSED`; returns 0) since macOS `getCpuPercents()` uses Mach APIs directly instead.

**Static state:** Both implementations use `static` local variables for caching core counts and previous CPU tick values. These would need careful handling in a subclass model.

---

### 5.2 MemoryInfo

| Aspect | Detail |
|--------|--------|
| **Shared header** | `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Info/memory_info.h` |
| **Linux impl** | `/Users/luke/Documents/GitHub/Nexis/linux/nexis-core/Info/memory_info.cpp` |
| **macOS impl** | `/Users/luke/Documents/GitHub/Nexis/macos/nexis-core/Info/memory_info.cpp` |
| **Shared impl** | `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Info/memory_info_shared.cpp` |
| **QObject?** | No |
| **Constructor** | Explicit, in shared impl -- zero-initializes all members |

**Data members (private):**
```cpp
quint64 memTotal, memFree, memUsed, buffers, cached, sreclaimable, shmem;
quint64 swapTotal, swapFree, swapUsed;
```

**Public methods:**
```cpp
MemoryInfo();
void updateMemoryInfo();          // PLATFORM-SPECIFIC
quint64 getMemTotal() const;      // shared
quint64 getMemFree() const;       // shared
quint64 getMemUsed() const;       // shared
quint64 getSwapTotal() const;     // shared
quint64 getSwapFree() const;      // shared
quint64 getSwapUsed() const;      // shared
```

**Asymmetric API:** No. Both platforms implement only `updateMemoryInfo()`. The 6 getters and constructor are shared.

**Split:** Only `updateMemoryInfo()` is platform-specific; everything else is shared.

---

### 5.3 DiskInfo

| Aspect | Detail |
|--------|--------|
| **Shared header** | `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Info/disk_info.h` |
| **Linux impl** | `/Users/luke/Documents/GitHub/Nexis/linux/nexis-core/Info/disk_info_platform.cpp` |
| **macOS impl** | `/Users/luke/Documents/GitHub/Nexis/macos/nexis-core/Info/disk_info_platform.cpp` |
| **Shared impl** | `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Info/disk_info_shared.cpp` |
| **QObject?** | No |
| **Constructor** | Implicit default |

**Data members (private):**
```cpp
QList<Disk> disks;
```

**Struct `Disk` (in header):**
```cpp
struct Disk {
    QString name, device, fileSystemType;
    quint64 size = 0, free = 0, used = 0;
};
```

**Public methods:**
```cpp
QList<Disk> getDisks() const;        // shared
void updateDiskInfo();               // shared (uses QStorageInfo -- cross-platform)
QList<quint64> getDiskIO() const;    // PLATFORM-SPECIFIC
QStringList getDiskNames() const;    // PLATFORM-SPECIFIC
QList<QString> fileSystemTypes();    // shared
QList<QString> devices();            // shared
```

**Asymmetric API:** No. Both platforms implement `getDiskIO()` and `getDiskNames()`.

**NOTE:** `updateDiskInfo()`, `getDisks()`, `devices()`, `fileSystemTypes()` are all in the shared `.cpp` using `QStorageInfo`. Only `getDiskIO()` and `getDiskNames()` are platform-specific. The platform files are named `disk_info_platform.cpp` (not `disk_info.cpp`).

---

### 5.4 NetworkInfo

| Aspect | Detail |
|--------|--------|
| **Shared header** | `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Info/network_info.h` |
| **Linux impl** | `/Users/luke/Documents/GitHub/Nexis/linux/nexis-core/Info/network_info.cpp` |
| **macOS impl** | `/Users/luke/Documents/GitHub/Nexis/macos/nexis-core/Info/network_info.cpp` |
| **Shared impl** | None |
| **QObject?** | No |
| **Constructor** | Explicit, in EACH platform `.cpp` |

**Data members (private):**
```cpp
QString defaultNetworkInterface;
QString rxPath;    // Linux-only (sysfs path); macOS: unused
QString txPath;    // Linux-only (sysfs path); macOS: unused
```

**Public methods:**
```cpp
NetworkInfo();
QString getDefaultNetworkInterface() const;
QList<QNetworkInterface> getAllInterfaces();
quint64 getRXbytes() const;
quint64 getTXbytes() const;
```

**Asymmetric API:** The `rxPath` and `txPath` data members are only used on Linux (sysfs paths). macOS uses `getifaddrs()` directly and ignores these fields. The header declares them unconditionally, which is a code smell for the refactor.

**Both platforms implement all 5 methods.** `getAllInterfaces()` and `getDefaultNetworkInterface()` have identical implementations across platforms.

---

### 5.5 SystemInfo

| Aspect | Detail |
|--------|--------|
| **Shared header** | `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Info/system_info.h` |
| **Linux impl** | `/Users/luke/Documents/GitHub/Nexis/linux/nexis-core/Info/system_info.cpp` |
| **macOS impl** | `/Users/luke/Documents/GitHub/Nexis/macos/nexis-core/Info/system_info.cpp` |
| **Shared impl** | `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Info/system_info_shared.cpp` |
| **QObject?** | No |
| **Constructor** | Explicit, in EACH platform `.cpp` |

**Data members (private):**
```cpp
QString cpuCore, cpuModel, cpuSpeed, username;
```

**Public methods:**
```cpp
SystemInfo();                          // PLATFORM-SPECIFIC
QString getHostname() const;           // shared (QSysInfo)
QString getPlatform() const;           // shared (QSysInfo)
QString getDistribution() const;       // shared (QSysInfo)
QString getKernel() const;             // shared (QSysInfo)
QString getCpuModel() const;           // shared
QString getCpuSpeed() const;           // shared
QString getCpuCore() const;            // shared
QString getUsername() const;           // shared
QFileInfoList getCrashReports() const; // PLATFORM-SPECIFIC
QFileInfoList getAppLogs() const;      // PLATFORM-SPECIFIC
QFileInfoList getAppCaches() const;    // PLATFORM-SPECIFIC
QFileInfoList getDevToolCaches() const;// PLATFORM-SPECIFIC
QStringList getUserList() const;       // PLATFORM-SPECIFIC
QStringList getGroupList() const;      // PLATFORM-SPECIFIC
```

**Asymmetric API:** No -- both platforms implement the constructor + 6 platform-specific methods. The 8 getter methods (hostname, platform, distribution, kernel, cpuModel, cpuSpeed, cpuCore, username) are shared.

---

### 5.6 ProcessInfo

| Aspect | Detail |
|--------|--------|
| **Shared header** | `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Info/process_info.h` |
| **Linux impl** | `/Users/luke/Documents/GitHub/Nexis/linux/nexis-core/Info/process_info.cpp` |
| **macOS impl** | `/Users/luke/Documents/GitHub/Nexis/macos/nexis-core/Info/process_info.cpp` |
| **Shared impl** | `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Info/process_info_shared.cpp` |
| **QObject?** | **YES** -- inherits `QObject`, uses `Q_OBJECT` macro |
| **Constructor** | Implicit default (QObject default constructor) |

**Data members (private):**
```cpp
QList<Process> processList;
```

**Public methods:**
```cpp
QList<Process> getProcessList() const;  // shared
```

**Public slots:**
```cpp
void updateProcesses();  // PLATFORM-SPECIFIC
```

**Asymmetric API:** No. Both platforms implement only `updateProcesses()`.

**IMPORTANT:** This is the only Info class that inherits `QObject`. The `Q_OBJECT` macro and `public slots` usage means the abstract base class must also be a QObject (or this needs redesign). `Q_OBJECT` classes cannot be `virtual`-ly multiply-inherited easily.

---

### 5.7 ThermalInfo

| Aspect | Detail |
|--------|--------|
| **Shared header** | `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Info/thermal_info.h` |
| **Linux impl** | `/Users/luke/Documents/GitHub/Nexis/linux/nexis-core/Info/thermal_info.cpp` |
| **macOS impl** | `/Users/luke/Documents/GitHub/Nexis/macos/nexis-core/Info/thermal_info.cpp` |
| **Shared impl** | None |
| **QObject?** | No |
| **Constructor** | Explicit, in EACH platform `.cpp` (calls `discoverSensors()`) |

**Data members (private):**
```cpp
QList<ThermalSensor> mSensors;
```

**Struct `ThermalSensor` (in header):**
```cpp
struct ThermalSensor {
    QString id, deviceName, label, inputPath;
    double maxTemp, critTemp;
};
```

**Public methods:**
```cpp
ThermalInfo();
QList<ThermalSensor> getSensors() const;
double getTemperature(int index) const;
bool hasSensors() const;
```

**Private methods:**
```cpp
void discoverSensors();
```

**Asymmetric API:** No. Both platforms implement all methods. However, `getSensors()` and `hasSensors()` have identical implementations (just return the member). Only the constructor, `discoverSensors()`, and `getTemperature()` differ.

---

### 5.8 GpuInfo

| Aspect | Detail |
|--------|--------|
| **Shared header** | `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Info/gpu_info.h` |
| **Linux impl** | `/Users/luke/Documents/GitHub/Nexis/linux/nexis-core/Info/gpu_info.cpp` |
| **macOS impl** | `/Users/luke/Documents/GitHub/Nexis/macos/nexis-core/Info/gpu_info.cpp` |
| **Shared impl** | `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Info/gpu_info_shared.cpp` |
| **QObject?** | No |
| **Constructor** | Explicit, in EACH platform `.cpp` (calls `discoverGpus()`) |

**Data members (private):**
```cpp
QList<GpuDevice> mDevices;
```

**Struct `GpuDevice` (in header):**
```cpp
struct GpuDevice {
    QString name, vendor;
    int utilization;
    QString sysfsLoadPath;   // Linux-only
    QString queryCommand;    // Linux-only (NVIDIA PCI bus ID or Intel max freq path)
    int deviceIndex;
};
```

**Public methods:**
```cpp
GpuInfo();
QList<GpuDevice> getGpuDevices() const;   // shared
void updateGpuInfo();                     // PLATFORM-SPECIFIC
bool hasGpu() const;                      // shared
```

**Private methods:**
```cpp
void discoverGpus();  // PLATFORM-SPECIFIC
```

**Asymmetric API:** The `GpuDevice` struct has Linux-specific fields (`sysfsLoadPath`, `queryCommand`) that are unused on macOS. Both platforms implement constructor, `discoverGpus()`, and `updateGpuInfo()`.

---

### 5.9 BatteryInfo

| Aspect | Detail |
|--------|--------|
| **Shared header** | `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Info/battery_info.h` |
| **Linux impl** | `/Users/luke/Documents/GitHub/Nexis/linux/nexis-core/Info/battery_info.cpp` |
| **macOS impl** | `/Users/luke/Documents/GitHub/Nexis/macos/nexis-core/Info/battery_info.cpp` |
| **Shared impl** | `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Info/battery_info_shared.cpp` |
| **QObject?** | No |
| **Constructor** | Explicit, in EACH platform `.cpp` |

**Data members (private):**
```cpp
BatteryData mData;
QString mBatteryPath;  // Comment: "Linux: sysfs path; macOS: unused"
```

**Struct `BatteryData` (in header):** Large struct with 20+ fields for battery state.

**Public methods:**
```cpp
BatteryInfo();
BatteryData getBatteryData() const;   // shared
bool hasBattery() const;              // shared
void updateBatteryInfo();             // PLATFORM-SPECIFIC
```

**Private methods:**
```cpp
void discoverBattery();  // PLATFORM-SPECIFIC
```

**Asymmetric API:** The `mBatteryPath` data member is Linux-only. The `BatteryData` struct has fields like `chargeStartThreshold` / `chargeStopThreshold` that are Linux TLP-only. The struct itself is shared, but some fields are always -1 on macOS.

---

### 5.10 DiskHealthInfo

| Aspect | Detail |
|--------|--------|
| **Shared header** | `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Info/disk_health_info.h` |
| **Linux impl** | `/Users/luke/Documents/GitHub/Nexis/linux/nexis-core/Info/disk_health_info.cpp` |
| **macOS impl** | `/Users/luke/Documents/GitHub/Nexis/macos/nexis-core/Info/disk_health_info.cpp` |
| **Shared impl** | `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Info/disk_health_info_shared.cpp` |
| **QObject?** | No |
| **Constructor** | Explicit, in EACH platform `.cpp` |

**Data members (private):**
```cpp
QList<DriveHealth> mDrives;
bool mHasSmartctl = false;
```

**Public methods:**
```cpp
DiskHealthInfo();
QList<DriveHealth> getDrives() const;         // shared
bool hasDrives() const;                       // shared
bool hasSmartctl() const;                     // shared
void refreshHealth();                         // PLATFORM-SPECIFIC
void refreshHealthElevated(const QString &device);  // PLATFORM-SPECIFIC
```

**Private methods:**
```cpp
void discoverDrives();                                // PLATFORM-SPECIFIC
void deriveHealthVerdict(DriveHealth &drive);          // shared
```

**Asymmetric API:** The macOS `refreshHealthElevated()` uses `CommandUtil::sudoExec()` (osascript-based) while Linux uses `pkexec`. Both provide the method. The `parseSmartctlJson()` helper is duplicated as a file-static function in both platform `.cpp` files -- it could be extracted to shared code.

**Structs (in header):** `SmartAttribute` and `DriveHealth` (large struct with NVMe and SATA fields).

---

## 6. Tool Classes: Detailed Analysis

### 6.1 PackageTool

**THIS IS THE MOST COMPLEX CASE.** PackageTool has **separate headers per platform** with completely different APIs.

| Aspect | Detail |
|--------|--------|
| **Shared types header** | `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Tools/package_tool_shared.h` (defines `Package` struct, `PackageTools` enum) |
| **Linux header** | `/Users/luke/Documents/GitHub/Nexis/linux/nexis-core/Tools/package_tool.h` |
| **macOS header** | `/Users/luke/Documents/GitHub/Nexis/macos/nexis-core/Tools/package_tool.h` |
| **Linux impl** | `/Users/luke/Documents/GitHub/Nexis/linux/nexis-core/Tools/package_tool.cpp` |
| **macOS impl** | `/Users/luke/Documents/GitHub/Nexis/macos/nexis-core/Tools/package_tool.cpp` |
| **Shared impl** | `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Tools/package_tool_shared.cpp` (only `friendlySectionName()`) |
| **QObject?** | No |
| **Instantiation** | ALL STATIC METHODS + one static const data member |

**Static data member (both platforms):**
```cpp
static const PackageTools currentPackageTool;
```

**Linux-only methods:**
```cpp
static QFileInfoList getDpkgPackageCaches();
static QList<Package> getDpkgPackages();
static bool dpkgRemovePackages(QStringList packages, bool purge = false);
static QStringList dpkgDryRunRemove(const QStringList &packages);
static QFileInfoList getYumDnfPackageCaches();
static QList<Package> getRpmPackages();
static bool dnfRemovePackages(QStringList packages);
static bool yumRemovePackages(QStringList packages);
static QStringList rpmDryRunRemove(const QStringList &packages);
static QFileInfoList getPacmanPackageCaches();
static QList<Package> getPacmanPackages();
static bool pacmanRemovePackages(QStringList packages);
static QStringList pacmanDryRunRemove(const QStringList &packages);
static QStringList getSnapPackages();
static bool snapRemovePackages(QStringList packages);
```

**macOS-only methods:**
```cpp
static QFileInfoList getHomebrewCaches();
static QList<Package> getHomebrewPackages();
static bool homebrewRemovePackages(QStringList packages);
static QStringList homebrewDryRunRemove(const QStringList &packages);
static QList<Package> getInstalledApps();
static bool trashApps(const QStringList &appPaths);
```

**Shared method (both platforms):**
```cpp
static QString friendlySectionName(const QString &section);  // in shared .cpp
```

**Asymmetric API:** EXTREMELY asymmetric. The two platforms share essentially no methods except `friendlySectionName()` and `currentPackageTool`. This is the hardest class to unify behind an abstract interface.

**Key insight:** `ToolManager` already provides a unified facade over PackageTool. The ToolManager `.cpp` is itself platform-specific and dispatches to the correct PackageTool static methods. A refactor could either:
1. Make PackageTool an abstract interface with a unified set of methods (e.g., `getPackages()`, `removePackages()`, `getPackageCaches()`, `dryRunRemove()`)
2. Leave PackageTool as-is and focus the abstraction at the ToolManager level.

---

### 6.2 ServiceTool

| Aspect | Detail |
|--------|--------|
| **Shared header** | `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Tools/service_tool.h` |
| **Linux impl** | `/Users/luke/Documents/GitHub/Nexis/linux/nexis-core/Tools/service_tool.cpp` |
| **macOS impl** | `/Users/luke/Documents/GitHub/Nexis/macos/nexis-core/Tools/service_tool.cpp` |
| **Shared impl** | `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Tools/service_tool_shared.cpp` (only `Service` constructor) |
| **QObject?** | No |
| **Instantiation** | ALL STATIC METHODS (no data members, no constructor) |

**`Service` class (data struct, in header):**
```cpp
class Service {
public:
    Service(const QString &name, const QString description, const bool status, const bool active);
    QString name, description;
    bool status, active;
};
```

**Static methods:**
```cpp
static QList<Service> getServices();
static bool serviceIsActive(const QString &serviceName);
static bool changeServiceStatus(const QString &sname, bool status);
static bool changeServiceActive(const QString &sname, bool status);
static bool serviceIsEnabled(const QString &serviceName);
static QString getServiceDescription(const QString &serviceName);
```

**Asymmetric API:** No -- both platforms implement all 6 static methods. Linux uses `systemctl`, macOS uses `launchctl`.

---

### 6.3 AptSourceTool

| Aspect | Detail |
|--------|--------|
| **Shared header** | `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Tools/apt_source_tool.h` |
| **Linux impl** | `/Users/luke/Documents/GitHub/Nexis/linux/nexis-core/Tools/apt_source_tool.cpp` |
| **macOS impl** | `/Users/luke/Documents/GitHub/Nexis/macos/nexis-core/Tools/apt_source_tool.cpp` |
| **Shared impl** | None |
| **QObject?** | No |
| **Instantiation** | ALL STATIC METHODS |

**`APTSource` class and typedef (in header):**
```cpp
class APTSource {
public:
    QString filePath, options, uri, suites, components, source;
    bool isSource, isActive;
};
typedef QSharedPointer<APTSource> APTSourcePtr;
```

**Static methods:**
```cpp
static bool checkSourceRepository();
static QList<APTSourcePtr> getSourceList();
static void removeAPTSource(const APTSourcePtr aptSource);
static void changeStatus(const APTSourcePtr aptSource, const bool status);
static void changeSource(const APTSourcePtr aptSource, const APTSourcePtr newSource);
static void addRepository(const QString &repository, const bool isSource);
```

**Asymmetric API:** Technically no -- both platforms implement all 6 methods. However, the macOS implementation is a semantic adapter: it maps APT concepts to Homebrew concepts. `changeStatus()` and `changeSource()` are no-ops on macOS. This is already a form of platform abstraction, just poorly named.

---

### 6.4 GnomeSettingsTool

| Aspect | Detail |
|--------|--------|
| **Shared header** | `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Tools/gnome_settings_tool.h` |
| **Linux impl** | `/Users/luke/Documents/GitHub/Nexis/linux/nexis-core/Tools/gnome_settings_tool.cpp` |
| **macOS impl** | `/Users/luke/Documents/GitHub/Nexis/macos/nexis-core/Tools/gnome_settings_tool.cpp` |
| **Shared impl** | None |
| **Platform constants** | `linux/nexis-core/Tools/gnome_settings_constants.h` and `macos/nexis-core/Tools/gnome_settings_constants.h` (shadowed by include path) |
| **QObject?** | No |
| **Instantiation** | ALL STATIC METHODS |

**Static methods:**
```cpp
static bool isAvailable();
static bool schemaExists(const QString &schema);
static QString getS(const QString &schema, const QString &key);
static bool    getB(const QString &schema, const QString &key);
static int     getI(const QString &schema, const QString &key);
static double  getD(const QString &schema, const QString &key);
static bool setS(const QString &schema, const QString &key, const QString &value);
static bool setB(const QString &schema, const QString &key, bool value);
static bool setI(const QString &schema, const QString &key, int value);
static bool setD(const QString &schema, const QString &key, double value);
```

**Private static methods:**
```cpp
static QSet<QString> cachedSchemas();
```

**Asymmetric API:** No -- identical method signatures. Linux uses `gsettings`, macOS uses `defaults`. The constants header provides the mapping from GNOME schema/key names to macOS domain/key names.

---

### 6.5 DockerTool

| Aspect | Detail |
|--------|--------|
| **Shared header** | `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Tools/docker_tool.h` |
| **Shared impl** | `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Tools/docker_tool.cpp` |
| **Platform impls** | **NONE** -- DockerTool is shared-only |
| **QObject?** | No |
| **Instantiation** | ALL STATIC METHODS |

**Static methods:**
```cpp
static bool isDockerInstalled();
static bool isDaemonRunning();
static QString dockerVersion();
static QList<DockerImage> getImages();
static QList<DockerContainer> getContainers();
static QList<DockerVolume> getVolumes();
static bool removeImages(const QStringList &ids);
static bool removeContainers(const QStringList &ids);
static bool removeVolumes(const QStringList &names);
static int pruneImages();
static int pruneContainers();
static int pruneVolumes();
static bool startContainer(const QString &id);
static bool stopContainer(const QString &id);
```

**DockerTool is completely platform-independent.** It uses the `docker` CLI which works identically on both platforms. **No abstract base class is needed for DockerTool.**

---

## 7. Summary Tables

### 7.1 Info Classes Overview

| Class | QObject? | Constructor | Shared Impl | Platform Methods | Data Members | Held in InfoManager |
|-------|----------|-------------|-------------|-----------------|--------------|-------------------|
| CpuInfo | No | Implicit | None | All 6 public + 1 private | None | By value |
| MemoryInfo | No | Shared | `memory_info_shared.cpp` | `updateMemoryInfo()` only | 10 quint64 | By value |
| DiskInfo | No | Implicit | `disk_info_shared.cpp` | `getDiskIO()`, `getDiskNames()` | `QList<Disk>` | By value |
| NetworkInfo | No | Platform | None | All 5 public | 3 QString | By value |
| SystemInfo | No | Platform | `system_info_shared.cpp` | ctor + 6 methods | 4 QString | By value |
| ProcessInfo | **YES** | Implicit | `process_info_shared.cpp` | `updateProcesses()` only | `QList<Process>` | By value |
| ThermalInfo | No | Platform | None | All 4 public + 1 private | `QList<ThermalSensor>` | By value |
| GpuInfo | No | Platform | `gpu_info_shared.cpp` | ctor + `discoverGpus()` + `updateGpuInfo()` | `QList<GpuDevice>` | By value |
| BatteryInfo | No | Platform | `battery_info_shared.cpp` | ctor + `discoverBattery()` + `updateBatteryInfo()` | `BatteryData` + `QString` | By value |
| DiskHealthInfo | No | Platform | `disk_health_info_shared.cpp` | ctor + `discoverDrives()` + `refreshHealth()` + `refreshHealthElevated()` | `QList<DriveHealth>` + `bool` | By value |

### 7.2 Tool Classes Overview

| Class | Platform Split? | Static-only? | Shared Impl | Asymmetric API? |
|-------|----------------|-------------|-------------|-----------------|
| PackageTool | **Separate headers** | Yes | `friendlySectionName()` only | **Extremely** -- different method sets |
| ServiceTool | Shared header | Yes | `Service` ctor only | No |
| AptSourceTool | Shared header | Yes | None | No (but macOS stubs some methods) |
| GnomeSettingsTool | Shared header + shadowed constants | Yes | None | No |
| DockerTool | **Shared-only (no platform split)** | Yes | All | N/A |

---

## 8. Key Architectural Challenges for the Refactor

### 8.1 Value-Held Objects in InfoManager

All 10 Info objects are held **by value** in `InfoManager`. Switching to abstract base classes requires:
- Changing to `std::unique_ptr<ICpuInfo> ci;` (or similar)
- Adding a factory function or `#ifdef` block in `InfoManager` to instantiate the correct platform subclass
- Updating all `InfoManager` methods that access members like `ci.getCpuPercents()` to use `ci->getCpuPercents()` (arrow operator)

### 8.2 ProcessInfo Inherits QObject

`ProcessInfo` is the only Info class that inherits `QObject`. The abstract base class for ProcessInfo must also inherit `QObject` (for signals/slots). Qt's `Q_OBJECT` macro imposes constraints:
- No multiple inheritance from QObject
- Must use `Q_OBJECT` in the concrete subclass too
- The abstract base `IProcessInfo` should inherit QObject, and declare `updateProcesses()` as a pure virtual slot

### 8.3 Static-Only Tool Classes

All Tool classes use only static methods and have no instances. Converting to an abstract interface requires:
- Making methods non-static (virtual methods cannot be static in C++)
- Creating instances of the Tool classes (held by `ToolManager`)
- Or, alternatively, using a different pattern like a strategy/policy template

### 8.4 PackageTool's Completely Different API Per Platform

PackageTool is the outlier. Linux has 14+ methods (dpkg, rpm, pacman, snap), macOS has 6 methods (homebrew, native apps). Options:
1. **Unified interface:** Define `IPackageTool` with abstract methods like `getPackages()`, `removePackages()`, `getPackageCaches()`, `dryRunRemove()`. Each platform implements these using its native tools.
2. **Keep current ToolManager pattern:** ToolManager already provides this unified facade. The PackageTool abstraction may not be worth the complexity.
3. **Hybrid:** Create the unified interface but leave platform-specific extras accessible via `dynamic_cast` or additional optional methods.

### 8.5 Shared Implementation Code

Many classes have shared `.cpp` files that implement cross-platform getters. The refactor must preserve this:
- Option A: Shared getters go into the abstract base class as non-virtual implementations
- Option B: Shared getters go into an intermediate "common" class between the interface and platform subclass
- Option C: Keep shared `.cpp` files compiled for both platforms (current approach) but have them implement base class methods

### 8.6 Data Members in Shared Header

Currently, private data members (like `MemoryInfo::memTotal`, `BatteryInfo::mData`) are declared in the shared header. With abstract base classes:
- Option A: Move data members to the concrete subclasses (each platform duplicates them)
- Option B: Keep data members in the abstract base class (it becomes an abstract class with state, not a pure interface)
- Option C: Use an intermediate class: `IMemoryInfo` (pure interface) -> `MemoryInfoBase` (shared state + shared getters) -> `MemoryInfoLinux` / `MemoryInfoMacOS` (platform methods)

Option C is the most maintainable and avoids code duplication.

### 8.7 Platform-Specific Data Members

Some data members are used on only one platform:
- `NetworkInfo::rxPath`, `txPath` -- Linux only
- `BatteryInfo::mBatteryPath` -- Linux only
- `GpuDevice::sysfsLoadPath`, `queryCommand` -- Linux only

These should move to the platform subclass, not the base class.

### 8.8 Static Local Variables for Caching

`CpuInfo` uses `static` local variables inside methods for caching (e.g., `static int count = 0;` in `getCpuCoreCount()`). These work correctly with the current pattern but would be shared across all instances if multiple instances are created. Since InfoManager is a singleton, this is currently safe, but it's a fragility to note.

### 8.9 The `nexis-core_global.h` Export Macro

All classes use `NEXISCORESHARED_EXPORT`. Abstract base classes and subclasses all need this macro for the shared library export to work correctly.

### 8.10 GnomeSettings Constants Shadowing

`gnome_settings_constants.h` is the only header that exists in BOTH `linux/` and `macos/` and gets shadowed. This pattern is independent of the abstract base class refactor. It maps GNOME concepts to macOS concepts using the same key names. This could be moved into the platform subclass or kept as-is.

---

## 9. Recommendations for the Implementation Plan

### 9.1 Proposed Class Hierarchy (Info Classes)

For each Info class, use a three-tier hierarchy where there is shared state:

```
IFooInfo (pure virtual interface, in shared/)
  -> FooInfoBase (shared state + shared getters, in shared/)
    -> FooInfoLinux (platform methods, in linux/)
    -> FooInfoMacOS (platform methods, in macos/)
```

For classes with no shared state (e.g., CpuInfo):

```
ICpuInfo (pure virtual interface, in shared/)
  -> CpuInfoLinux (all methods, in linux/)
  -> CpuInfoMacOS (all methods, in macos/)
```

### 9.2 Proposed Class Hierarchy (Tool Classes)

For Tool classes, convert from static methods to instance methods:

```
IServiceTool (pure virtual interface, in shared/)
  -> ServiceToolLinux (systemctl, in linux/)
  -> ServiceToolMacOS (launchctl, in macos/)
```

For PackageTool, define a unified interface:

```
IPackageTool (pure virtual: getPackages(), removePackages(), getPackageCaches(), dryRunRemove())
  -> PackageToolLinux (delegates to dpkg/rpm/pacman based on detection)
  -> PackageToolMacOS (Homebrew + native apps)
```

DockerTool: **No change needed** -- it's already shared-only.

### 9.3 InfoManager Changes

```cpp
class InfoManager {
    std::unique_ptr<ICpuInfo> ci;
    std::unique_ptr<IMemoryInfo> mi;
    // ... etc.
};
```

Factory pattern in constructor or `ins()`:
```cpp
#ifdef Q_OS_MACOS
    ci = std::make_unique<CpuInfoMacOS>();
#else
    ci = std::make_unique<CpuInfoLinux>();
#endif
```

### 9.4 Order of Implementation

1. Start with the simplest classes (CpuInfo, MemoryInfo) to establish the pattern
2. Move to complex classes (SystemInfo, DiskHealthInfo)
3. Handle ProcessInfo carefully (QObject inheritance)
4. Handle PackageTool last (most complex, asymmetric API)
5. Update InfoManager and ToolManager as each class is converted
6. Update CMakeLists.txt as needed

### 9.5 CMake Changes

The include-path shadowing mechanism can remain mostly intact since platform `.cpp` files will still live in `linux/` and `macos/` directories. The main change is:
- New interface headers in `shared/nexis-core/Info/` (e.g., `i_cpu_info.h`)
- New base class files in `shared/nexis-core/Info/` (e.g., `cpu_info_base.h/.cpp`)
- Platform files renamed/restructured (e.g., `cpu_info.cpp` -> `cpu_info_linux.cpp` with `CpuInfoLinux` class)
- Headers in platform dirs for the subclass declarations (e.g., `linux/nexis-core/Info/cpu_info_linux.h`)

---

## 10. File Inventory (Complete Reference)

### 10.1 Info Class Files

```
shared/nexis-core/Info/cpu_info.h
linux/nexis-core/Info/cpu_info.cpp
macos/nexis-core/Info/cpu_info.cpp

shared/nexis-core/Info/memory_info.h
shared/nexis-core/Info/memory_info_shared.cpp
linux/nexis-core/Info/memory_info.cpp
macos/nexis-core/Info/memory_info.cpp

shared/nexis-core/Info/disk_info.h
shared/nexis-core/Info/disk_info_shared.cpp
linux/nexis-core/Info/disk_info_platform.cpp
macos/nexis-core/Info/disk_info_platform.cpp

shared/nexis-core/Info/network_info.h
linux/nexis-core/Info/network_info.cpp
macos/nexis-core/Info/network_info.cpp

shared/nexis-core/Info/system_info.h
shared/nexis-core/Info/system_info_shared.cpp
linux/nexis-core/Info/system_info.cpp
macos/nexis-core/Info/system_info.cpp

shared/nexis-core/Info/process_info.h
shared/nexis-core/Info/process_info_shared.cpp
linux/nexis-core/Info/process_info.cpp
macos/nexis-core/Info/process_info.cpp

shared/nexis-core/Info/thermal_info.h
linux/nexis-core/Info/thermal_info.cpp
macos/nexis-core/Info/thermal_info.cpp

shared/nexis-core/Info/gpu_info.h
shared/nexis-core/Info/gpu_info_shared.cpp
linux/nexis-core/Info/gpu_info.cpp
macos/nexis-core/Info/gpu_info.cpp

shared/nexis-core/Info/battery_info.h
shared/nexis-core/Info/battery_info_shared.cpp
linux/nexis-core/Info/battery_info.cpp
macos/nexis-core/Info/battery_info.cpp

shared/nexis-core/Info/disk_health_info.h
shared/nexis-core/Info/disk_health_info_shared.cpp
linux/nexis-core/Info/disk_health_info.cpp
macos/nexis-core/Info/disk_health_info.cpp
```

### 10.2 Tool Class Files

```
shared/nexis-core/Tools/package_tool_shared.h    (Package struct, PackageTools enum)
shared/nexis-core/Tools/package_tool_shared.cpp   (friendlySectionName)
linux/nexis-core/Tools/package_tool.h             (Linux-specific header)
linux/nexis-core/Tools/package_tool.cpp
macos/nexis-core/Tools/package_tool.h             (macOS-specific header)
macos/nexis-core/Tools/package_tool.cpp

shared/nexis-core/Tools/service_tool.h
shared/nexis-core/Tools/service_tool_shared.cpp    (Service constructor)
linux/nexis-core/Tools/service_tool.cpp
macos/nexis-core/Tools/service_tool.cpp

shared/nexis-core/Tools/apt_source_tool.h
linux/nexis-core/Tools/apt_source_tool.cpp
macos/nexis-core/Tools/apt_source_tool.cpp

shared/nexis-core/Tools/gnome_settings_tool.h
linux/nexis-core/Tools/gnome_settings_tool.cpp
macos/nexis-core/Tools/gnome_settings_tool.cpp
linux/nexis-core/Tools/gnome_settings_constants.h   (shadowed)
macos/nexis-core/Tools/gnome_settings_constants.h   (shadowed)

shared/nexis-core/Tools/docker_tool.h              (shared-only, no platform split)
shared/nexis-core/Tools/docker_tool.cpp
```

### 10.3 Manager Files

```
shared/nexis/Managers/info_manager.h
shared/nexis/Managers/info_manager.cpp

shared/nexis/Managers/tool_manager.h
linux/nexis/Managers/tool_manager.cpp
macos/nexis/Managers/tool_manager.cpp
```

### 10.4 Support Files

```
shared/nexis-core/nexis-core_global.h    (NEXISCORESHARED_EXPORT macro)
shared/nexis-core/Info/process.h         (Process data class)
macos/nexis-core/Utils/brew_util.h       (macOS-only Homebrew helpers)
```
