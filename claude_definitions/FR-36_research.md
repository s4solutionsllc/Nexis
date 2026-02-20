# FR-36 Research: Unit Test Suite (15-20 Tests)

## Overview

This document provides deep research into every class, function, and parsing routine that should be covered by the FR-36 unit test suite. For each target, it documents the exact methods to test, their input/output contracts, edge cases, past bugs that would have been caught, mocking requirements, and whether the code can be tested in isolation.

FR-36 depends on FR-33 (testing infrastructure) and FR-35 (dependency injection). The tests described here assume Qt Test framework with CTest integration.

---

## 1. Utility Classes (Pure Functions)

### 1A. FormatUtil::formatBytes()

**File:** `shared/nexis-core/Utils/format_util.h` + `shared/nexis-core/Utils/format_util.cpp`

**Signature:**
```cpp
static QString formatBytes(const quint64 &bytes);
```

**Constants defined in class:**
- `KIBI = 1024`
- `MEBI = 1048576` (1024^2)
- `GIBI = 1073741824` (1024^3)
- `TEBI = 1099511627776` (1024^4)

**Implementation logic (branch-by-branch):**
1. `bytes == 1` -> returns `"1 byte"` (singular)
2. `bytes < KIBI` -> returns `"{n} bytes"` (plural)
3. `bytes < MEBI` -> returns `"{n.n} KiB"`
4. `bytes < GIBI` -> returns `"{n.n} MiB"`
5. `bytes < TEBI` -> returns `"{n.n} GiB"`
6. `bytes >= TEBI` -> returns `"{n.n} TiB"`

Format uses `QString::asprintf("%.1f %s", ...)` with 1 decimal place.

**Testability:** Fully testable in isolation. Pure static function, no dependencies, no state. Does not need QApplication.

**Test cases:**
| # | Input | Expected Output | Notes |
|---|-------|-----------------|-------|
| 1 | `0` | `"0 bytes"` | Zero edge case |
| 2 | `1` | `"1 byte"` | Singular form |
| 3 | `2` | `"2 bytes"` | Plural form, small value |
| 4 | `1023` | `"1023 bytes"` | Just below KiB boundary |
| 5 | `1024` | `"1.0 KiB"` | Exact KiB boundary |
| 6 | `1536` | `"1.5 KiB"` | Fractional KiB |
| 7 | `1048576` | `"1.0 MiB"` | Exact MiB boundary |
| 8 | `1073741824` | `"1.0 GiB"` | Exact GiB boundary |
| 9 | `1099511627776` | `"1.0 TiB"` | Exact TiB boundary |
| 10 | `2199023255552` | `"2.0 TiB"` | Multi-TiB value |
| 11 | `UINT64_MAX` | Large TiB value | Max quint64 (16384 PiB) |

**Bugs prevented:** None directly from BUGS.md, but this function is used in CleanerService::logCleanResult(), System Cleaner UI, and Dashboard line bars. Any formatting regression would cascade.

---

### 1B. FileUtil

**File:** `shared/nexis-core/Utils/file_util.h` + `shared/nexis-core/Utils/file_util.cpp`

#### 1B-i. readStringFromFile()

**Signature:**
```cpp
static QString readStringFromFile(const QString &path, const QIODevice::OpenMode &mode = QIODevice::ReadOnly);
```

**Implementation:** Opens file via `QFile`, reads all content with `readAll()`, returns as QString. Returns empty QString if file doesn't exist or can't be opened.

**Test cases:**
| # | Scenario | Expected |
|---|----------|----------|
| 1 | Valid file with content | Returns full content |
| 2 | Empty file | Returns empty QString |
| 3 | Non-existent path | Returns empty QString (no crash) |
| 4 | File with Unicode content | Returns correct UTF-8 |
| 5 | Large file (multi-line) | Returns complete content |

**Setup:** Create temp files with `QTemporaryFile` or `QTemporaryDir`.

#### 1B-ii. readListFromFile()

**Signature:**
```cpp
static QStringList readListFromFile(const QString &path, const QIODevice::OpenMode &mode = QIODevice::ReadOnly);
```

**Implementation:** Calls `readStringFromFile()`, then `.trimmed().split("\n")`. Note: when file is empty, `"".trimmed().split("\n")` returns `[""]` (a list with one empty string), not an empty list.

**Test cases:**
| # | Scenario | Expected |
|---|----------|----------|
| 1 | File with 3 lines | 3-element QStringList |
| 2 | Empty file | QStringList with one empty string (`[""]`) |
| 3 | File with trailing newline | Last empty entry trimmed by `.trimmed()` |
| 4 | Non-existent file | QStringList with one empty string |

**Bug relevance:** This is what MemoryInfo uses to read `/proc/meminfo`. BUG-27 (no bounds checking) was caused by the filter returning fewer than 8 lines. Understanding the empty-file behavior is critical.

#### 1B-iii. writeFile()

**Signature:**
```cpp
static bool writeFile(const QString &path, const QString &content, const QIODevice::OpenMode &mode = QIODevice::WriteOnly | QIODevice::Truncate);
```

**Implementation:** Opens file, writes via QTextStream, appends `Qt::endl`. Returns `true` on success, `false` on failure.

**Note:** Writes `content.toUtf8()` followed by `Qt::endl`, so the output always has a trailing newline.

**Test cases:**
| # | Scenario | Expected |
|---|----------|----------|
| 1 | Write to new file | File created, returns true, content matches |
| 2 | Write to existing file (truncate) | Old content replaced |
| 3 | Write to invalid path (e.g. `/nonexistent/dir/file`) | Returns false |
| 4 | Round-trip: write then read | Content matches (minus trailing newline semantics) |

#### 1B-iv. getFileSize()

**Signature:**
```cpp
static quint64 getFileSize(const QString &path);
```

**Implementation:** Recursive. For files, returns `QFileInfo::size()`. For directories, iterates `QDir::entryInfoList(NoDotAndDotDot | Files | Dirs)` and sums recursively. Returns 0 for non-existent paths.

**Test cases:**
| # | Scenario | Expected |
|---|----------|----------|
| 1 | Single file (known size) | Exact byte count |
| 2 | Directory with files | Sum of all file sizes recursively |
| 3 | Empty directory | 0 |
| 4 | Non-existent path | 0 |
| 5 | Nested directories | Correct recursive sum |

**Bug relevance:** Used in BUG-10 context (System Cleaner memory leak was partially caused by redundant `getFileSize()` calls).

#### 1B-v. directoryList()

**Signature:**
```cpp
static QStringList directoryList(const QString &path);
```

**Implementation:** Returns file names (not paths) in directory, filtering `NoDotAndDotDot | Files` only (no subdirectories).

**Test cases:**
| # | Scenario | Expected |
|---|----------|----------|
| 1 | Directory with files and subdirs | Only file names returned |
| 2 | Empty directory | Empty list |
| 3 | Non-existent path | Empty list |

**Testability:** All FileUtil methods are fully testable in isolation using temp files/dirs. No Qt widgets needed, only `QCoreApplication` for some internal Qt paths.

---

### 1C. CommandUtil

**File:** `shared/nexis-core/Utils/command_util.h`, `shared/nexis-core/Utils/command_util_shared.cpp`, plus platform-specific `command_util_platform.cpp`.

#### 1C-i. exec()

**Signature:**
```cpp
static QString exec(const QString &cmd, QStringList args = {}, QByteArray data = {}, int timeoutMs = 30000);
```

**Implementation:**
1. Creates `QProcess`, starts with `cmd` and `args`.
2. If `data` is non-empty, writes to stdin and closes write channel.
3. `waitForFinished(timeoutMs)`.
4. Reads stdout via `QTextStream`, calls `.trimmed()`.
5. If `process->error() != QProcess::UnknownError`, **throws** `QString` (the error string).
6. Returns trimmed stdout.

**Critical behavior:** This function throws a `QString` exception on error. All callers must catch.

**Test cases:**
| # | Scenario | Expected |
|---|----------|----------|
| 1 | `echo "hello"` | Returns `"hello"` |
| 2 | Command with args: `printf "%s %s" foo bar` | Returns `"foo bar"` |
| 3 | Non-existent command | Throws QString exception |
| 4 | Stdin data: `echo` reading from pipe | Input data processed |
| 5 | Timeout: `sleep 5` with timeoutMs=100 | Throws or returns empty (timeout behavior) |
| 6 | Trimming: command outputting trailing whitespace | Output trimmed |

#### 1C-ii. execWithStatus()

**Signature:**
```cpp
static ExecResult execWithStatus(const QString &cmd, QStringList args = {}, int timeoutMs = 30000);
```

**Returns:** `ExecResult { QString output; QString error; int exitCode; }`

**Implementation:**
- Returns structured result with exit code, stdout, stderr.
- On failure to start: `exitCode = -1`, error = process error string.
- On normal exit: `exitCode` from process.
- On crash/signal: `exitCode = -1`.

**Test cases:**
| # | Scenario | Expected |
|---|----------|----------|
| 1 | Successful command | exitCode=0, output populated |
| 2 | Command that exits with error | exitCode>0, stderr populated |
| 3 | Non-existent command | exitCode=-1, error populated |
| 4 | `false` command (always exits 1) | exitCode=1 |

**Bug relevance:** BUG-31 was fixed by adding this function to check `gsettings set` exit codes.

#### 1C-iii. isExecutable()

**Signature:**
```cpp
static bool isExecutable(const QString &cmd);
```

**Implementation:** `!QStandardPaths::findExecutable(cmd).isEmpty()`

**Test cases:**
| # | Scenario | Expected |
|---|----------|----------|
| 1 | `"ls"` | true |
| 2 | `"nonexistent_binary_xyz"` | false |
| 3 | `""` (empty string) | false |

**Bug relevance:** BUG-15 (Homebrew not found on macOS) was caused by PATH not being set for GUI apps. `isExecutable()` relies on PATH.

#### 1C-iv. sudoExec() (platform-specific)

**Linux:** Delegates to `exec("pkexec", ...)`.
**macOS:** Builds an osascript `do shell script ... with administrator privileges` command.

**Testability:** Hard to test directly (requires privilege escalation). Mock at the `exec()` level for testing callers. Not recommended for automated testing.

**Testability summary:** `exec()`, `execWithStatus()`, and `isExecutable()` are testable using real commands like `echo`, `printf`, `false`, etc. `sudoExec()` requires mocking or should be tested via integration tests only.

---

## 2. Info Class Parsing (Regression Targets)

### 2A. MemoryInfo (Linux) — /proc/meminfo Parsing

**File:** `linux/nexis-core/Info/memory_info.cpp`

**Method:** `MemoryInfo::updateMemoryInfo()`

**Implementation (line by line):**
```cpp
QStringList lines = FileUtil::readListFromFile(PROC_MEMINFO)
    .filter(QRegularExpression("^MemTotal|^MemFree|^Buffers|^Cached|^SwapTotal|^SwapFree|^Shmem|^SReclaimable"));
```
The regex filters for 8 specific lines from `/proc/meminfo`. The filtered result is expected in alphabetical order since the regex matches the order these appear in the kernel's output:
- Index 0: `Buffers:`
- Index 1: `Cached:`
- Index 2: `MemFree:`
- Index 3: `MemTotal:`
- Index 4: `SReclaimable:`
- Index 5: `Shmem:`
- Index 6: `SwapFree:`
- Index 7: `SwapTotal:`

**Wait** - re-examining the actual `/proc/meminfo` order. The kernel outputs these in this order:
```
MemTotal:        ...
MemFree:         ...
Buffers:         ...
Cached:          ...
SwapTotal:       ...
SwapFree:        ...
Shmem:           ...
SReclaimable:    ...
```

The `.filter()` preserves the input order (not alphabetical), so the indices match the kernel output order:
- Index 0: `MemTotal`
- Index 1: `MemFree`
- Index 2: `Buffers`
- Index 3: `Cached`
- Index 4: `SwapTotal`
- Index 5: `SwapFree`
- Index 6: `Shmem`
- Index 7: `SReclaimable`

Then the code assigns:
```cpp
memTotal = getValue(0);       // MemTotal
memFree = getValue(1);        // MemFree
buffers = getValue(2);        // Buffers
cached = getValue(3);         // Cached
swapTotal = getValue(4);      // SwapTotal
swapFree = getValue(5);       // SwapFree
shmem = getValue(6);          // Shmem
sreclaimable = getValue(7);   // SReclaimable
```

The `getValue` macro: `lines.at(l).split(sep).at(1).toLongLong() << 10` -- splits line by whitespace, takes second field (the number), converts to long long, shifts left by 10 (multiplies by 1024 since `/proc/meminfo` values are in kB).

Derived values:
```cpp
cached = (cached + sreclaimable - shmem);
memUsed = (memTotal - (memFree + buffers + cached));
swapUsed = (swapTotal - swapFree);
```

**Guard clause (BUG-27 fix):**
```cpp
if (lines.size() < 8) {
    qWarning() << "MemoryInfo: expected 8 lines from /proc/meminfo, got" << lines.size();
    return;
}
```

**Past bugs:**
- **BUG-01** (HIGH): Swapped `shmem` and `sreclaimable` assignments. Was `shmem=index 7, sreclaimable=index 6` before fix. This caused incorrect memory usage calculations.
- **BUG-27** (MEDIUM): No bounds checking on `lines.at(0)` through `lines.at(7)`. If fewer than 8 lines matched, app crashed with out-of-bounds exception.
- **BUG-29** (LOW): Used `toLong()` instead of `toLongLong()`. On 32-bit platforms, values shifted left by 10 could overflow.

**Mocking strategy:** The function reads from `/proc/meminfo` via `FileUtil::readListFromFile()`. To test:
- **Option A (recommended):** Create a temporary file with known `/proc/meminfo` content, then modify the code to accept a file path parameter (requires FR-35 dependency injection).
- **Option B:** Write a test helper that creates a temp file with meminfo format and test the parsing logic extracted into a testable function.
- **Option C:** Test at a higher level by making `MemoryInfo` accept a file path in its constructor.

**Proposed test cases:**
| # | Test Name | Input (meminfo content) | Validation | Bugs Prevented |
|---|-----------|-------------------------|------------|----------------|
| 1 | `testNormalMeminfo` | Standard 8-line meminfo with known values | All 6 getters return correct values | BUG-01 |
| 2 | `testShmemSreclaimableOrder` | Meminfo where swapping shmem/sreclaimable produces different result | Verify `cached` is calculated correctly with the correct shmem and sreclaimable | BUG-01 |
| 3 | `testInsufficientLines` | Meminfo with only 3 matching lines | All values remain 0 (initialized), no crash | BUG-27 |
| 4 | `testEmptyFile` | Empty file | All values remain 0, no crash | BUG-27 |
| 5 | `testLargeValues` | Values > 2^31 kB (> 2 TiB RAM) | Values correctly parsed as 64-bit | BUG-29 |
| 6 | `testSwapCalculation` | Known SwapTotal and SwapFree | `swapUsed = swapTotal - swapFree` | General |

**Sample test input (for case 1):**
```
MemTotal:       16384000 kB
MemFree:         4096000 kB
MemAvailable:    8000000 kB
Buffers:          512000 kB
Cached:          2048000 kB
SwapCached:        10000 kB
Active:          5000000 kB
Inactive:        3000000 kB
SwapTotal:       4096000 kB
SwapFree:        3072000 kB
Dirty:             12345 kB
Shmem:            256000 kB
SReclaimable:     128000 kB
```
After filtering (preserving order): MemTotal, MemFree, Buffers, Cached, SwapTotal, SwapFree, Shmem, SReclaimable (8 lines).

Expected: memTotal=16384000*1024, memFree=4096000*1024, etc.

---

### 2B. DiskHealthInfo — SMART Parsing and Health Verdict

**Files:**
- `shared/nexis-core/Info/disk_health_info.h` (structs + class)
- `shared/nexis-core/Info/disk_health_info_shared.cpp` (deriveHealthVerdict)
- `linux/nexis-core/Info/disk_health_info.cpp` (parseSmartctlJson + Linux discovery)
- `macos/nexis-core/Info/disk_health_info.cpp` (parsePlist + parseSmartctlJson + macOS discovery)

#### 2B-i. parseSmartctlJson() (static function, duplicated on both platforms)

**Signature (file-static):**
```cpp
static void parseSmartctlJson(const QByteArray &json, DriveHealth &drive);
```

**Input:** Raw JSON from `smartctl -j -a /dev/XXX`.

**NVMe parsing:** Reads from `root["device"]["type"]` == `"nvme"`, then from `root["nvme_smart_health_information_log"]` object. Extracts: critical_warning, temperature, available_spare, available_spare_threshold, percentage_used, data_units_read/written, power_cycles, power_on_hours, unsafe_shutdowns, media_errors.

**SATA parsing:** If not NVMe, reads `rotation_rate` to determine HDD vs SSD. Reads SMART attributes from `root["ata_smart_attributes"]["table"]` array. Each attribute has id, name, value, worst, thresh, raw.value. Specific attribute IDs mapped:
- 5 -> reallocatedSectors
- 9 -> powerOnHours
- 12 -> powerCycles
- 177 -> wearLevelingCount (uses `sa.value`, not rawValue)
- 190, 194 -> temperatureCelsius
- 196 -> reallocatedEvents
- 197 -> pendingSectors
- 198 -> uncorrectableSectors

Also reads: model_name, serial_number, firmware_version, smart_status.passed.

**Testability:** The function is file-static, so not directly callable from a test. Options:
1. Move `parseSmartctlJson()` to a public static method on DiskHealthInfo.
2. Test indirectly via `refreshHealth()` with a mock smartctl.
3. Duplicate the parsing logic into a testable function (not ideal).

**Recommendation:** Refactor to make `parseSmartctlJson()` a public static method of `DiskHealthInfo` (or a free function in the header). This is minimal-risk refactoring.

**Test cases for parseSmartctlJson():**
| # | Test Name | Input JSON | Validation |
|---|-----------|------------|------------|
| 1 | `testParseNvmeSmartctl` | NVMe smartctl JSON with all fields | All NVMe-specific fields populated correctly |
| 2 | `testParseSataHddSmartctl` | SATA HDD JSON (rotation_rate > 0) with attribute table | driveType=SATA_HDD, attributes extracted |
| 3 | `testParseSataSsdSmartctl` | SATA SSD JSON (rotation_rate=0) | driveType=SATA_SSD, wearLevelingCount from attr 177 |
| 4 | `testParseEmptyJson` | `{}` or null | No crash, fields remain default |
| 5 | `testParseInvalidJson` | `"not json"` | No crash, fields remain default |
| 6 | `testParseMissingNvmeLog` | NVMe type but no `nvme_smart_health_information_log` | driveType set to NVMe, metrics stay -1 |
| 7 | `testSmartStatusFailed` | JSON with `"smart_status": {"passed": false}` | `smartPassed` is false |

**Sample NVMe JSON for testing:**
```json
{
  "device": {"type": "nvme"},
  "model_name": "Samsung 980 PRO",
  "serial_number": "S5GXNF0R123456",
  "firmware_version": "5B2QGXA7",
  "smart_status": {"passed": true},
  "nvme_smart_health_information_log": {
    "critical_warning": 0,
    "temperature": 38,
    "available_spare": 100,
    "available_spare_threshold": 10,
    "percentage_used": 3,
    "data_units_read": 12345678,
    "data_units_written": 9876543,
    "power_cycles": 150,
    "power_on_hours": 2500,
    "unsafe_shutdowns": 5,
    "media_errors": 0
  }
}
```

#### 2B-ii. deriveHealthVerdict()

**File:** `shared/nexis-core/Info/disk_health_info_shared.cpp`

**Signature:**
```cpp
void DiskHealthInfo::deriveHealthVerdict(DriveHealth &drive);
```

**This is the most testable function in DiskHealthInfo** -- it's a member function but operates purely on the `DriveHealth` struct fields. It sets `healthVerdict` and `healthPercent`.

**Logic by drive type:**

**NVMe:**
- Critical: criticalWarning > 0 OR mediaErrors > 0 OR percentageUsed >= 100 OR !smartPassed
- Caution: availableSpare <= 10 OR percentageUsed >= 80
- Good: otherwise
- healthPercent: 100 - percentageUsed (clamped 0-100), or availableSpare if percentageUsed unavailable

**SATA_HDD:**
- critical = sum of max(0, reallocatedSectors) + max(0, pendingSectors) + max(0, uncorrectableSectors)
- Critical: critical > 100 OR !smartPassed
- Caution: critical > 0
- Good: otherwise
- healthPercent: always -1 (no universal metric)

**SATA_SSD:**
- Critical: wearLevelingCount < 10 OR !smartPassed
- Caution: wearLevelingCount < 30 OR reallocatedSectors > 0
- Good: otherwise
- healthPercent: wearLevelingCount (clamped 0-100)

**Unknown:**
- Critical if !smartPassed, else Good
- healthPercent: -1

**Testability:** Excellent. Create `DriveHealth` structs with specific field values, call `deriveHealthVerdict()`, check `healthVerdict` and `healthPercent`. Requires making it public or using a friend test class.

**Test cases for deriveHealthVerdict():**
| # | Test Name | DriveType | Key Inputs | Expected Verdict | Expected % |
|---|-----------|-----------|------------|------------------|------------|
| 1 | `testNvmeHealthy` | NVMe | percentageUsed=3, availableSpare=100, mediaErrors=0 | Good | 97 |
| 2 | `testNvmeCaution_LowSpare` | NVMe | availableSpare=8, percentageUsed=50 | Caution | 50 |
| 3 | `testNvmeCaution_HighUsage` | NVMe | percentageUsed=85 | Caution | 15 |
| 4 | `testNvmeCritical_MediaErrors` | NVMe | mediaErrors=1 | Critical | depends |
| 5 | `testNvmeCritical_100pctUsed` | NVMe | percentageUsed=100 | Critical | 0 |
| 6 | `testNvmeCritical_WarningBit` | NVMe | criticalWarning=1 | Critical | depends |
| 7 | `testNvmeCritical_SmartFailed` | NVMe | smartPassed=false | Critical | depends |
| 8 | `testSataHddHealthy` | SATA_HDD | reallocated=0, pending=0, uncorrectable=0 | Good | -1 |
| 9 | `testSataHddCaution` | SATA_HDD | reallocated=5 | Caution | -1 |
| 10 | `testSataHddCritical` | SATA_HDD | reallocated=50, pending=30, uncorrectable=25 | Critical | -1 |
| 11 | `testSataSsdHealthy` | SATA_SSD | wearLeveling=95 | Good | 95 |
| 12 | `testSataSsdCaution_LowWear` | SATA_SSD | wearLeveling=25 | Caution | 25 |
| 13 | `testSataSsdCaution_Realloc` | SATA_SSD | wearLeveling=80, reallocated=1 | Caution | 80 |
| 14 | `testSataSsdCritical` | SATA_SSD | wearLeveling=5 | Critical | 5 |
| 15 | `testUnknownType_Passed` | Unknown | smartPassed=true | Good | -1 |
| 16 | `testUnknownType_Failed` | Unknown | smartPassed=false | Critical | -1 |

#### 2B-iii. parsePlist() (macOS only, file-static)

**File:** `macos/nexis-core/Info/disk_health_info.cpp`

Parses Apple plist XML into a flat `QMap<QString, QVariant>`. Handles `<string>`, `<integer>`, `<true/>`, `<false/>`, `<array>`. Nested `SMARTDeviceSpecificKeysMayVaryNotGuaranteed` dict keys are prefixed with `"SMART."`.

**Testability:** File-static, would need to be exposed. Same refactoring recommendation as `parseSmartctlJson()`.

**Test cases:** Similar to smartctl but with plist XML format. Lower priority since macOS also calls `parseSmartctlJson()` for non-Apple-Fabric drives.

---

### 2C. CpuInfo — Core Count Parsing

**Files:**
- `shared/nexis-core/Info/cpu_info.h`
- `linux/nexis-core/Info/cpu_info.cpp`
- `macos/nexis-core/Info/cpu_info.cpp`

#### Linux getCpuCoreCount()

**Implementation:** Reads `/proc/cpuinfo`, filters lines starting with `"processor"`, counts matches.

**Linux getCpuPhysicalCoreCount():** Parses `physical id` and `core id` pairs from `/proc/cpuinfo`, builds a `QSet<QPair<int,int>>`, returns size.

**Past bugs:**
- **BUG-28** (LOW): Used `static quint8 count` which overflows at 256. Fixed to `static int count`. AMD EPYC 9004 has 256 threads.

**Mocking strategy:** Both functions use static caching (`static int count = 0`), so they can only be tested once per process. This is a significant constraint. To test properly:
- The static variable needs to be resettable, or
- Each test case runs in a separate process, or
- The static caching is refactored to allow reset.

**Test cases (Linux, if mocking is possible):**
| # | Test Name | Input (/proc/cpuinfo) | Expected |
|---|-----------|----------------------|----------|
| 1 | `testSingleCore` | 1 processor entry | getCpuCoreCount()=1 |
| 2 | `test256Threads` | 256 processor entries | getCpuCoreCount()=256 (not 0) | BUG-28 |
| 3 | `testPhysicalCores` | 4 physical, 2 cores each | getCpuPhysicalCoreCount()=8 |
| 4 | `testEmptyCpuinfo` | Empty file | getCpuCoreCount()=0 |

**macOS:** Uses `sysctlbyname("hw.logicalcpu")` and `sysctlbyname("hw.physicalcpu")`. Not practically mockable without intercepting sysctl calls. Best tested as integration tests on real hardware.

**Recommendation:** Focus Linux tests on `/proc/cpuinfo` parsing. macOS tests should only verify the return type and range (> 0).

---

## 3. Manager Logic

### 3A. CleanerService — Scan Categorization

**File:** `shared/nexis/Managers/cleaner_service.h` + `shared/nexis/Managers/cleaner_service.cpp`

**The 6 categories (enum CleanCategory):**
1. `PACKAGE_CACHE` - Package manager caches (APT, DNF, Pacman, etc.)
2. `CRASH_REPORTS` - System crash reports
3. `APPLICATION_LOGS` - Application log files
4. `APPLICATION_CACHES` - Application cache files
5. `TRASH` - User trash directory
6. `DEV_TOOL_CACHES` - Developer tool caches (npm, gradle, Electron, etc.)

**Scan flow:**
```cpp
ScanResult scan(const QList<CleanCategory> &categories);
```
For each category, calls the appropriate method on `ToolManager` or `InfoManager`:
- PACKAGE_CACHE -> `ToolManager::ins()->getPackageCaches()`
- CRASH_REPORTS -> `InfoManager::ins()->getCrashReports()`
- APPLICATION_LOGS -> `InfoManager::ins()->getAppLogs()`
- APPLICATION_CACHES -> `InfoManager::ins()->getAppCaches()`
- DEV_TOOL_CACHES -> `InfoManager::ins()->getDevToolCaches()`
- TRASH -> Hardcoded path: `~/.local/share/Trash/` (Linux) or `~/.Trash/` (macOS)

Returns `ScanResult` with `categoryFiles` map and `totalSize`.

**Testability challenges:**
- Singleton pattern (`CleanerService::ins()`) couples to `InfoManager` and `ToolManager` singletons.
- Without dependency injection (FR-35), testing requires either:
  - Creating real file system structures in temp dirs
  - Intercepting the singleton calls

**What CAN be tested without DI:**
1. `categoryName()` - Pure static function mapping enum to translated string
2. `allCategories()` - Returns the list of all 6 categories
3. `cleanFiles()` - File deletion logic (test with temp files)
4. `cleanSchedule()` - Schedule lookup and execution (requires SettingManager mock)

**Test cases for categoryName():**
| # | Input | Expected |
|---|-------|----------|
| 1 | PACKAGE_CACHE | "Package Caches" |
| 2 | CRASH_REPORTS | "Crash Reports" |
| 3 | APPLICATION_LOGS | "Application Logs" |
| 4 | APPLICATION_CACHES | "Application Caches" |
| 5 | TRASH | "Trash" |
| 6 | DEV_TOOL_CACHES | "Dev Tool Caches" |

**Test cases for allCategories():**
| # | Validation |
|---|------------|
| 1 | Returns exactly 6 elements |
| 2 | Contains all 6 enum values |
| 3 | No duplicates |

---

### 3B. ScheduleManager — CRUD and JSON Persistence

**File:** `shared/nexis/Managers/schedule_manager.h` + `shared/nexis/Managers/schedule_manager.cpp`

#### JSON Schema

Schedules are persisted as a JSON array in QSettings via `SettingManager::setSchedules()/getSchedules()`. Each schedule object:

```json
{
  "id": "uuid-string",
  "name": "Weekly Cleanup",
  "enabled": true,
  "frequency": 2,
  "everyNDays": 3,
  "dayOfWeek": 0,
  "dayOfMonth": 1,
  "hour": 3,
  "minute": 0,
  "minFileAgeSecs": 86400,
  "lastBytesFreed": 0,
  "dryRunCompleted": false,
  "categories": [0, 2, 4],
  "lastRun": "2025-01-15T03:00:00"
}
```

Frequency enum: 0=Daily, 1=EveryNDays, 2=Weekly, 3=Monthly.

#### loadSchedules() / saveSchedules()

**loadSchedules():** Reads JSON from SettingManager, deserializes into `QList<CleaningSchedule>`.
**saveSchedules():** Serializes `mSchedules` to JSON, writes to SettingManager.

This is a round-trip serialization pair. Testing: Create a schedule, save, load, verify all fields match.

#### createSchedule()

1. If `id` is empty, generates UUID via `QUuid::createUuid()`.
2. Appends to `mSchedules`.
3. Calls `saveSchedules()` and `syncToOS()`.
4. Emits `schedulesChanged()`.
5. Returns the new ID.

#### updateSchedule()

Finds by ID, replaces, saves, syncs, emits signal.

#### deleteSchedule()

Finds by ID, removes from list, saves, syncs, emits signal.

#### getNextRunTime() -- HIGHLY TESTABLE

**Signature:**
```cpp
QDateTime getNextRunTime(const CleaningSchedule &schedule) const;
```

This is the most testable function in ScheduleManager. Pure date/time calculation with no side effects.

**Logic:**
- **Daily:** Today at schedule time, or tomorrow if past.
- **EveryNDays:** If lastRun is valid, advance from lastRun by everyNDays until future. Otherwise, today or today+everyNDays.
- **Weekly:** Calculate days until target dayOfWeek. Qt uses Mon=1..Sun=7; the code converts with `% 7` to get 0=Sun. If calculated time is past, add 7 days.
- **Monthly:** Target day of this month (clamped to daysInMonth). If past, go to next month.

**Edge cases in Weekly calculation:**
```cpp
int currentDow = now.date().dayOfWeek() % 7; // Qt Mon=1..Sun=7, %7 gives Mon=1..Sat=6, Sun=0
int targetDow = schedule.dayOfWeek;           // 0=Sunday
int daysUntil = (targetDow - currentDow + 7) % 7;
```
This maps Qt's dayOfWeek to 0=Sunday convention. Potential edge case: Sunday transitions.

**Test cases for getNextRunTime():**
| # | Test Name | Frequency | Key Params | "Now" | Expected |
|---|-----------|-----------|------------|-------|----------|
| 1 | `testDailyFuture` | Daily | hour=15 | 10:00 | Today 15:00 |
| 2 | `testDailyPast` | Daily | hour=8 | 10:00 | Tomorrow 08:00 |
| 3 | `testWeeklySameDay` | Weekly | dayOfWeek=3 (Wed), hour=15 | Wed 10:00 | This Wed 15:00 |
| 4 | `testWeeklyFutureDow` | Weekly | dayOfWeek=5 (Fri) | Wed 10:00 | Fri at schedule time |
| 5 | `testWeeklyPastDow` | Weekly | dayOfWeek=1 (Mon) | Wed 10:00 | Next Mon |
| 6 | `testWeeklySundayWrap` | Weekly | dayOfWeek=0 (Sun) | Sat 10:00 | Tomorrow (Sun) |
| 7 | `testMonthlyFuture` | Monthly | dayOfMonth=25 | 15th | 25th this month |
| 8 | `testMonthlyPast` | Monthly | dayOfMonth=5 | 15th | 5th next month |
| 9 | `testMonthlyDay31InFeb` | Monthly | dayOfMonth=31 | Feb 15 | Feb 28/29 (clamped) |
| 10 | `testEveryNDaysWithLastRun` | EveryNDays | everyNDays=3, lastRun=2 days ago | today+1 | lastRun + 3 days |
| 11 | `testEveryNDaysNoLastRun` | EveryNDays | everyNDays=5 | 10:00, hour=8 | today + 5 at 08:00 |

**Mocking note:** `getNextRunTime()` uses `QDateTime::currentDateTime()` internally, which makes tests time-dependent. For deterministic testing, consider:
- Setting the clock via test framework (difficult with Qt)
- Refactoring to accept a `QDateTime now` parameter (preferred)
- Using time-independent assertions (e.g., "result is in the future")

#### frequencyDisplayText() -- TESTABLE

**Signature:**
```cpp
static QString frequencyDisplayText(const CleaningSchedule &schedule);
```

Pure static function. Returns human-readable strings like "Daily at 03:00", "Weekly on Monday at 09:00", etc.

**Test cases:**
| # | Frequency | Key Params | Expected |
|---|-----------|------------|----------|
| 1 | Daily | hour=3, minute=0 | "Daily at 03:00" |
| 2 | EveryNDays | everyNDays=5, hour=14, minute=30 | "Every 5 days at 14:30" |
| 3 | Weekly | dayOfWeek=1 (Mon), hour=9 | "Weekly on Monday at 09:00" |
| 4 | Weekly | dayOfWeek=0 (Sun) | "Weekly on Sunday at ..." |
| 5 | Monthly | dayOfMonth=15, hour=22 | "Monthly on day 15 at 22:00" |
| 6 | Weekly | dayOfWeek=7 (invalid) | "Weekly on ? at ..." |

#### JSON Round-Trip Testing

**Test:** Create a `CleaningSchedule` with all fields populated, serialize via `saveSchedules()` logic, deserialize via `loadSchedules()` logic, verify all fields match.

**Testability challenge:** Both functions operate on the private `mSchedules` member and call `SettingManager::ins()`. To test the serialization logic:
1. Extract the JSON serialization/deserialization into static helper functions.
2. Or test through the CRUD API (create, then getAllSchedules, verify fields).

**Dependency:** The ScheduleManager constructor calls `loadSchedules()` which calls `SettingManager::ins()`. For unit testing, SettingManager needs to be injectable or mockable (FR-35 dependency).

---

## 4. Theme Token Validation

### 4A. Token Inventory

**QSS file:** `shared/nexis/static/themes/default/style/style.qss`
**Values files:**
- `shared/nexis/static/themes/default/style/values.ini` (dark theme)
- `shared/nexis/static/themes/light/style/values.ini` (light theme)

#### Color @tokens used in style.qss:

Extracted from the QSS file, these non-`@dp*` tokens are used:

| Token | Dark Value | Light Value | Usage Count (approx.) |
|-------|-----------|-------------|----------------------|
| `@themeName` | `default` | `light` | 12 (image URLs) |
| `@pageContent` | `#222226` | `#fafafb` | 4 |
| `@sidebar` | `#2e2e32` | `#ebebed` | 1 |
| `@color01` | `#36363a` | `#ffffff` | ~20 |
| `@color02` | `#3d3846` | `#f6f5f4` | ~12 |
| `@color03` | `#E95420` | `#E95420` | 0 (in QSS) |
| `@color04` | `#9a9996` | `#5e5c64` | 3 |
| `@color05` | `#ffffff` | `#241f31` | ~25 |
| `@color06` | `#9a9996` | `#77767b` | ~12 |
| `@color07` | `#ffffff` | `#ffffff` | ~8 |
| `@color08` | `#222226` | `#fafafb` | 0 (in QSS) |
| `@color09` | `#e01b24` | `#e01b24` | 1 |
| `@color10` | `#c64516` | `#c64516` | 0 (in QSS) |
| `@color11` | `#deddda` | `#3d3846` | ~6 |
| `@color12` | `#c0bfbc` | `#5e5c64` | ~6 |
| `@color13` | `#3d3846` | `#deddda` | 0 (in QSS) |
| `@color14` | `#5e5c64` | `#c0bfbc` | 0 (in QSS) |
| `@color15` | `#2ec27e` | `#26a269` | 0 (in QSS) |
| `@color16` | `#E95420` | `#E95420` | 0 (in QSS) |
| `@accentColor` | `#E95420` | `#E95420` | ~12 |
| `@accentHover` | `#c64516` | `#c64516` | ~4 |
| `@cardBg` | `#36363a` | `#ffffff` | ~8 |
| `@borderColor` | `#5e5c64` | `#deddda` | ~25 |
| `@successColor` | `#2ec27e` | `#26a269` | 2 |
| `@warningColor` | `#e5a50a` | `#cd9309` | 1 |
| `@destructiveColor` | `#e01b24` | `#c01c28` | 2 |
| `@circleChartBackgroundColor` | `#2e2e32` | `#ffffff` | 0 (in QSS, used by C++) |
| `@historyChartBackgroundColor` | `#2e2e32` | `#ffffff` | 0 (in QSS, used by C++) |
| `@chartLabelColor` | `#9a9996` | `#5e5c64` | 0 (in QSS, used by C++) |
| `@chartGridColor` | `#5e5c64` | `#deddda` | 0 (in QSS, used by C++) |

#### @dp tokens used in style.qss:

DPI-scaled pixel values: `@dp2`, `@dp4`, `@dp6`, `@dp8`, `@dp10`, `@dp12`, `@dp14`, `@dp15px`, `@dp16`, `@dp18`, `@dp20`, `@dp22px`, `@dp24`, `@dp26`, `@dp30px`, `@dp36`, `@dp44px`, `@dp100`, `@dp180`, plus variants with `px` suffix.

These are replaced by the regex `@dp(\d+)` which captures the number and scales via `Dpi::scale()`.

### 4B. Token Replacement Mechanism

**File:** `shared/nexis/Managers/app_manager.cpp` lines 88-121

**`AppManager::updateStylesheet()`:**
1. Loads `values.ini` as `QSettings` (keyed by theme name).
2. Reads `style.qss` (always from `default/style/`).
3. Iterates all keys from values.ini, replaces `@key` with `value` in the QSS string.
4. Replaces `@dpN` patterns with `Dpi::scale(N)` results.
5. Applies to `qApp->setStyleSheet()`.

**Critical observation:** The `QSettings` from an `.ini` file includes a `General/` group prefix. When `values.ini` contains `@color01=#36363a`, `QSettings::allKeys()` returns `@color01` (without `General/` prefix in the IniFormat). The replacement loop does `mStylesheetFileContent.replace(key, value)`.

### 4C. Theme Validation Test Strategy

**What to validate:**
1. Every `@token` in `style.qss` has a corresponding key in BOTH `values.ini` files.
2. Every color value in `values.ini` is valid hex format (`#RRGGBB` or `#RRGGBBAA`).
3. After token replacement, no `@` tokens remain in the processed stylesheet (except `@dp` tokens which are handled separately, and CSS `@` rules if any).
4. Both themes produce a valid stylesheet (no unresolved tokens).

**QRC resource loading challenge:** The `.ini` and `.qss` files are bundled in `static.qrc`. To load them in tests:
- Tests must link against the QRC resource file (include `static.qrc` in the test target's CMakeLists.txt).
- Or read the files directly from the source tree using absolute paths (simpler for testing, doesn't require QRC compilation).

**Recommended approach:** Read files directly from the source tree paths. This avoids QRC compilation complexity and tests the source files themselves.

**Test cases:**
| # | Test Name | Description | Bugs Prevented |
|---|-----------|-------------|----------------|
| 1 | `testDarkThemeTokensCovered` | Extract all @tokens from style.qss, verify each exists in default/values.ini | BUG-21, BUG-33, BUG-36, BUG-38 |
| 2 | `testLightThemeTokensCovered` | Same for light/values.ini | BUG-21, BUG-33, BUG-36, BUG-38 |
| 3 | `testDarkThemeColorsValid` | All values in default/values.ini are valid hex colors (or known non-color tokens like @themeName) | Theme regressions |
| 4 | `testLightThemeColorsValid` | Same for light/values.ini | Theme regressions |
| 5 | `testTokensMatchBetweenThemes` | Both values.ini files define the same set of tokens | Missing token in one theme |
| 6 | `testNoUnresolvedTokensAfterReplacement` | Perform the replacement and verify no `@color*`, `@accent*`, `@card*`, etc. remain | Runtime QSS failures |

**Implementation sketch:**
```cpp
void TestThemeTokens::testDarkThemeTokensCovered()
{
    // Read style.qss from source tree
    QString qss = FileUtil::readStringFromFile(PROJECT_ROOT "/shared/nexis/static/themes/default/style/style.qss");

    // Extract all @tokens (excluding @dp\d+ which are DPI tokens)
    QRegularExpression tokenRx("@(?!dp\\d)([a-zA-Z]\\w+)");
    QSet<QString> usedTokens;
    QRegularExpressionMatchIterator it = tokenRx.globalMatch(qss);
    while (it.hasNext()) {
        usedTokens.insert(it.next().captured(0));
    }

    // Read values.ini
    QSettings values(PROJECT_ROOT "/shared/nexis/static/themes/default/style/values.ini", QSettings::IniFormat);
    QStringList definedTokens = values.allKeys();

    for (const QString &token : usedTokens) {
        QVERIFY2(definedTokens.contains(token),
                  qPrintable(QString("Token %1 used in QSS but not defined in values.ini").arg(token)));
    }
}
```

---

## 5. Test Inventory Summary

### Tier 1: Highest Priority (Pure Functions, No Dependencies)

| # | Test Class | Test Method | Target | Isolation | Past Bugs |
|---|-----------|-------------|--------|-----------|-----------|
| 1 | TestFormatUtil | testFormatBytes_boundaries | FormatUtil::formatBytes() | Full | - |
| 2 | TestFormatUtil | testFormatBytes_singular | FormatUtil::formatBytes(1) | Full | - |
| 3 | TestFormatUtil | testFormatBytes_largeValues | FormatUtil::formatBytes(UINT64_MAX) | Full | - |
| 4 | TestFileUtil | testReadWriteRoundTrip | FileUtil::readStringFromFile + writeFile | Full (temp files) | - |
| 5 | TestFileUtil | testReadNonexistent | FileUtil::readStringFromFile (missing file) | Full | - |
| 6 | TestFileUtil | testGetFileSize_recursive | FileUtil::getFileSize (dir tree) | Full (temp dirs) | BUG-10 |
| 7 | TestFileUtil | testReadListFromFile | FileUtil::readListFromFile | Full (temp files) | BUG-27 |

### Tier 2: High Priority (Regression Targets)

| # | Test Class | Test Method | Target | Isolation | Past Bugs |
|---|-----------|-------------|--------|-----------|-----------|
| 8 | TestDiskHealth | testDeriveVerdict_nvme | DiskHealthInfo::deriveHealthVerdict() NVMe cases | Full (struct-only) | FR-29 |
| 9 | TestDiskHealth | testDeriveVerdict_sataHdd | deriveHealthVerdict() SATA HDD cases | Full | FR-29 |
| 10 | TestDiskHealth | testDeriveVerdict_sataSsd | deriveHealthVerdict() SATA SSD cases | Full | FR-29 |
| 11 | TestDiskHealth | testParseSmartctlJson_nvme | parseSmartctlJson() with NVMe JSON | Needs refactor | FR-29 |
| 12 | TestDiskHealth | testParseSmartctlJson_sata | parseSmartctlJson() with SATA JSON | Needs refactor | FR-29 |
| 13 | TestDiskHealth | testParseSmartctlJson_invalid | parseSmartctlJson() with bad input | Needs refactor | - |

### Tier 3: Schedule Manager (Business Logic)

| # | Test Class | Test Method | Target | Isolation | Past Bugs |
|---|-----------|-------------|--------|-----------|-----------|
| 14 | TestScheduleManager | testGetNextRunTime_daily | getNextRunTime() daily | Full | FR-16 |
| 15 | TestScheduleManager | testGetNextRunTime_weekly | getNextRunTime() weekly | Full | FR-16 |
| 16 | TestScheduleManager | testGetNextRunTime_monthly | getNextRunTime() monthly edge (day 31) | Full | FR-16 |
| 17 | TestScheduleManager | testGetNextRunTime_everyNDays | getNextRunTime() with/without lastRun | Full | FR-16 |
| 18 | TestScheduleManager | testFrequencyDisplayText | frequencyDisplayText() all cases | Full | - |

### Tier 4: Theme Validation

| # | Test Class | Test Method | Target | Isolation | Past Bugs |
|---|-----------|-------------|--------|-----------|-----------|
| 19 | TestThemeTokens | testAllTokensResolved_dark | All @tokens in QSS exist in dark values.ini | Full (file I/O) | BUG-21,33,36,38 |
| 20 | TestThemeTokens | testAllTokensResolved_light | All @tokens in QSS exist in light values.ini | Full (file I/O) | BUG-21,33,36,38 |
| 21 | TestThemeTokens | testColorValuesValid | All color values are valid hex | Full | Theme regressions |
| 22 | TestThemeTokens | testThemesHaveSameTokens | Both INIs define identical token sets | Full | Theme regressions |

### Tier 5: Command Utility

| # | Test Class | Test Method | Target | Isolation | Past Bugs |
|---|-----------|-------------|--------|-----------|-----------|
| 23 | TestCommandUtil | testExec_success | CommandUtil::exec("echo", ...) | Full | - |
| 24 | TestCommandUtil | testExec_failure | CommandUtil::exec(invalid cmd) throws | Full | - |
| 25 | TestCommandUtil | testExecWithStatus_exitCode | execWithStatus("false") returns exitCode=1 | Full | BUG-31 |
| 26 | TestCommandUtil | testIsExecutable | isExecutable("ls") vs nonexistent | Full | BUG-15 |

### Tier 6: CleanerService (Static Helpers)

| # | Test Class | Test Method | Target | Isolation | Past Bugs |
|---|-----------|-------------|--------|-----------|-----------|
| 27 | TestCleanerService | testCategoryNames | categoryName() all 6 values | Full | FR-16 |
| 28 | TestCleanerService | testAllCategories | allCategories() returns 6 | Full | FR-03 |

---

## 6. Refactoring Requirements for Testability

The following minimal refactoring is needed to enable the above tests:

### 6A. parseSmartctlJson() exposure
**Current:** File-static function in both `linux/` and `macos/` `disk_health_info.cpp`.
**Change:** Move to a public static method `DiskHealthInfo::parseSmartctlJsonInto(const QByteArray &json, DriveHealth &drive)`.
**Risk:** Minimal. Only changes visibility, not behavior.

### 6B. deriveHealthVerdict() accessibility
**Current:** Private method of DiskHealthInfo.
**Change:** Either make public, or declare the test class as `friend`.
**Risk:** None.

### 6C. ScheduleManager::getNextRunTime() time source
**Current:** Uses `QDateTime::currentDateTime()` internally.
**Change:** Add optional `QDateTime now` parameter with default value.
**Risk:** None (default parameter preserves API).

### 6D. MemoryInfo file path injection
**Current:** Hardcoded `"/proc/meminfo"` via `static constexpr`.
**Change:** Add constructor parameter or a `setMemInfoPath()` for testing.
**Risk:** Low. Only adds a test path; production path unchanged.

### 6E. Static variable reset in CpuInfo
**Current:** `static int count = 0` in getCpuCoreCount() and getCpuPhysicalCoreCount().
**Change:** Either remove static caching (it's an optimization that prevents re-reading files), or add a `resetCaches()` static method for testing.
**Risk:** Low. Static caching is a micro-optimization; the file reads are fast.

---

## 7. Framework Requirements

### Minimum Qt Test Setup (FR-33 prerequisite)

```cmake
# tests/CMakeLists.txt
find_package(Qt6 COMPONENTS Test REQUIRED)
enable_testing()

# Test executable for core library tests
add_executable(test_nexis_core
    test_format_util.cpp
    test_file_util.cpp
    test_command_util.cpp
    test_disk_health.cpp
    test_memory_info.cpp  # Linux only
    test_schedule_manager.cpp
    test_cleaner_service.cpp
    test_theme_tokens.cpp
)

target_link_libraries(test_nexis_core
    nexis-core
    Qt6::Core
    Qt6::Test
)

# Register with CTest
add_test(NAME NexisCoreTests COMMAND test_nexis_core)
```

### Required includes per test
- `QTest` for `QVERIFY`, `QCOMPARE`, `QVERIFY2`
- `QTemporaryDir` / `QTemporaryFile` for file system tests
- `QJsonDocument` / `QJsonObject` for SMART JSON tests
- `QDateTime` for schedule tests
- `QRegularExpression` for theme token tests
- `QSettings` for reading values.ini

### No QApplication needed for
- FormatUtil, FileUtil, CommandUtil tests
- DiskHealthInfo struct manipulation (deriveHealthVerdict)
- ScheduleManager date calculations
- Theme token file validation

### QCoreApplication needed for
- QSettings (values.ini reading)
- QStandardPaths (isExecutable)
- QProcess (CommandUtil)

---

## 8. Test Count Summary

**Total proposed tests: 28**

Broken down by category:
- FormatUtil: 3 tests
- FileUtil: 4 tests
- CommandUtil: 4 tests
- DiskHealthInfo (verdict): 3 tests
- DiskHealthInfo (parsing): 3 tests
- MemoryInfo (Linux): implicit in file parsing tests (can be combined)
- ScheduleManager: 5 tests
- CleanerService: 2 tests
- Theme Tokens: 4 tests

This exceeds the 15-20 target range. Recommended prioritization: implement Tier 1 + Tier 2 + Tier 3 (18 tests) as the core suite, with Tier 4-6 (10 tests) as stretch goals.

The 18-test core suite covers:
- All utility function boundaries
- All past HIGH/MEDIUM bug regression paths
- The most complex business logic (schedule calculations)
- Theme integrity validation
