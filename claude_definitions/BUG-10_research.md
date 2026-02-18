# BUG-10 Research: Memory Leak in System Cleaner

## Bug Summary

Long-running sessions see memory grow from ~150MB to 2GB+ due to improper memory management in the System Cleaner component. Upstream: [oguzhaninan/Stacer#229](https://github.com/oguzhaninan/Stacer/issues/229).

## Files Investigated

| File | Role |
|---|---|
| `shared/nexis/Pages/SystemCleaner/system_cleaner_page.h` | Header — member variables, QFuture storage |
| `shared/nexis/Pages/SystemCleaner/system_cleaner_page.cpp` | Main implementation — scan/clean logic |
| `shared/nexis/Pages/SystemCleaner/byte_tree_widget.h/.cpp` | QTreeWidgetItem subclass for size-sorted items |
| `shared/nexis-core/Utils/file_util.cpp` | `getFileSize()` — recursive directory size calculator |
| `shared/nexis/Managers/info_manager.h/.cpp` | Delegates to SystemInfo for scan data |
| `macos/nexis-core/Info/system_info.cpp` | macOS scan implementations |
| `linux/nexis-core/Info/system_info.cpp` | Linux scan implementations |

## Architecture Overview

### Scan Flow

```
User clicks "Scan"
  → on_btnScan_clicked()         [main thread]
    → reads checkbox states into member bools
    → clears mPackageCaches/mCrashReports/mAppLogs/mAppCaches/mDevToolCaches
    → mWorkerFuture = QtConcurrent::run([this]() { systemScan(); })

systemScan()                      [worker thread]
  → assigns to mPackageCaches, mCrashReports, etc. from InfoManager
  → emit scanFinishedS()

onScanFinished()                  [main thread, via queued connection]
  → ui->treeWidgetScanResult->clear()
  → for each category: addTreeRoot() → FileUtil::getFileSize() per item
  → builds QTreeWidgetItem tree
```

### Clean Flow

```
User clicks "Clean"
  → on_btnClean_clicked()         [main thread]
    → reads tree widget checked items into mFilesToDelete, mChildrenToRemove
    → mWorkerFuture = QtConcurrent::run([this]() { systemClean(); })

systemClean()                     [worker thread]
  → FileUtil::getFileSize() per file (to compute total cleaned)
  → deletes files/empties directories
  → emit cleanFinishedS()

onCleanFinished()                 [main thread]
  → removes checked children from tree
  → FileUtil::getFileSize() AGAIN per top-level item (recalculate category sizes)
```

## Root Causes Identified

### 1. No Guard Against Concurrent Scans (PRIMARY)

**Location:** `system_cleaner_page.cpp:373-416` (`on_btnScan_clicked`)

There is no `mScanInProgress` flag. The Scan button is hidden during a scan (`ui->btnScan->hide()` at line 396), but the user can still trigger a scan via:
- Navigating back to categories (`on_btnBackToCategories_clicked` at line 464 re-shows the button) while a worker is still running
- Rapid double-clicking before the button hides

If two scans overlap:
1. `on_btnScan_clicked()` clears `mPackageCaches` etc. on the main thread
2. Meanwhile, the previous worker thread is still **writing** to those same lists in `systemScan()`
3. **Data race**: simultaneous read/write to QFileInfoList (not thread-safe)
4. Result: corrupted lists, unbounded growth, undefined behavior

The same issue applies to `on_btnClean_clicked()` — a clean can overlap with a scan since they share `mWorkerFuture` (overwriting the previous future, orphaning the old thread).

### 2. QFileInfoList Results Never Cleared After Use

**Location:** `system_cleaner_page.h:90-94`

The five scan result lists (`mPackageCaches`, `mCrashReports`, `mAppLogs`, `mAppCaches`, `mDevToolCaches`) are populated during `systemScan()` and read during `onScanFinished()`, but they are **only cleared at the start of the next scan** (lines 408-412). Between scans, they hold potentially thousands of QFileInfo objects in memory unnecessarily.

On a typical macOS system:
- `getAppCaches()`: returns entries from `~/Library/Caches` — hundreds of dirs+files
- `getDevToolCaches()`: scans `~/Library/Application Support/*/Cache` — dozens of Electron apps
- `getAppLogs()`: `~/Library/Logs` + `/var/log` — hundreds of files

Each `QFileInfo` carries a `QFileInfoPrivate` with cached stat data, absolute path strings, etc. (~200-500 bytes each). With ~1000 entries total across all lists, that's ~500KB held persistently — modest, but it compounds with issue #1.

### 3. FileUtil::getFileSize() Called Redundantly (AMPLIFIER)

**Location:** `file_util.cpp:60-82`, called from `system_cleaner_page.cpp:121,134,310,361`

`getFileSize()` is a recursive directory walker. It's called:

1. **Per item in addTreeRoot()** (line 121) — once per scanned file/directory during tree construction
2. **Per file in systemClean()** (line 310) — to compute total cleaned size before deletion
3. **Per top-level item in onCleanFinished()** (line 361) — to recalculate category sizes after cleaning

For large directories (e.g., `~/.npm` with 10,000+ files across deep subdirectories), each call creates:
- A new `QFileInfo` per entry at each recursion level
- A new `QDir` per directory
- Temporary `QFileInfoList` from `entryInfoList()` at each level

This isn't a leak per se (temporaries are freed), but it creates **massive transient allocations** that fragment the heap. The C++ runtime's `malloc` may not return freed memory to the OS, so the process RSS grows monotonically even though the allocations are logically freed.

### 4. Linux QIcon::fromTheme() Per Tree Child (LINUX ONLY)

**Location:** `system_cleaner_page.cpp:153`

```cpp
item->setIcon(0, QIcon::fromTheme(text, mDefaultIcon));
```

On Linux, each child item in the tree gets a `QIcon::fromTheme(filename, ...)` call. This searches the icon theme for every single scanned file by name. For ~1000 files, that's ~1000 theme lookups. `QIcon` caches internally via `QIconLoader`, but the cache grows without bound across sessions. macOS correctly skips this (line 149-151).

### 5. Destructor Doesn't Wait for Worker Thread

**Location:** `system_cleaner_page.cpp:6-9`

```cpp
SystemCleanerPage::~SystemCleanerPage()
{
    delete ui;
}
```

The destructor deletes `ui` but does **not** call `mWorkerFuture.waitForFinished()`. If the page is destroyed while a scan/clean is in progress, the worker thread's `[this]` lambda captures a dangling pointer. BUG-05 added `QThreadPool::globalInstance()->waitForDone()` in `App::closeEvent()`, which mitigates app shutdown but doesn't protect against mid-session page destruction.

### 6. SignalMapper Theme Connection Not Scoped

**Location:** `system_cleaner_page.cpp:85`

```cpp
connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme, [=] {
```

This connection to the singleton `SignalMapper` is never disconnected. The `[=]` lambda captures `this` implicitly. If the page is destroyed, the connection remains live — next theme change invokes the lambda with a dangling `this`. In practice, the page likely lives for the app lifetime, but it's poor ownership hygiene.

## What's NOT Causing the Leak

- **QMovie objects**: Correctly parented to `this`, reused on theme change (BUG-37 fixed the old leak here).
- **QTreeWidgetItem cleanup**: `ui->treeWidgetScanResult->clear()` correctly deletes all items including children. Tree items are properly owned by the tree widget.
- **ByteTreeWidget**: Simple QTreeWidgetItem subclass, no dynamic allocations.
- **InfoManager/SystemInfo**: Returns QFileInfoList by value — no ownership issues on the manager side.

## Memory Growth Scenario

A user who performs 10 scans in a session on a system with ~1000 scannable items:

| Source | Per Scan | 10 Scans | Notes |
|---|---|---|---|
| QFileInfoList result storage | ~500 KB | ~500 KB | Cleared at start of next scan |
| QTreeWidgetItem tree | ~200 KB | ~200 KB | Cleared at start of next scan |
| getFileSize() transient allocs | ~50-200 MB peak | ~50-200 MB RSS (heap fragmentation) | Freed logically but RSS doesn't shrink |
| QIcon::fromTheme cache (Linux) | ~1 MB | ~1 MB | Grows if different files scanned |
| Concurrent scan data race | 0 (normal) / unbounded (race) | 0 / unbounded | Only if scans overlap |

The 2GB figure from the upstream report is plausible if:
1. The system has very large cache directories (common on developer machines)
2. Multiple scans are triggered before previous ones complete (data race)
3. Heap fragmentation prevents RSS from shrinking

## Recommended Fixes

### Fix A: Add scan/clean in-progress guard (CRITICAL)
- Add `bool mScanInProgress = false` and `bool mCleanInProgress = false` members
- Set to `true` at start of `on_btnScan_clicked()` / `on_btnClean_clicked()`
- Set to `false` at end of `onScanFinished()` / `onCleanFinished()`
- Return early (or disable button) if guard is true
- Prevents data races and duplicate workers entirely

### Fix B: Wait for worker in destructor (IMPORTANT)
- Add `mWorkerFuture.waitForFinished()` before `delete ui` in destructor
- Prevents use-after-free if page is destroyed mid-scan

### Fix C: Clear result lists after onScanFinished() consumes them (MODERATE)
- After building the tree in `onScanFinished()`, clear `mPackageCaches`, `mCrashReports`, etc.
- Releases ~500KB of persistent QFileInfo storage between scans

### Fix D: Avoid redundant getFileSize() calls (MODERATE)
- Compute sizes during `addTreeRoot()` (already done) and store totals
- In `onCleanFinished()`, subtract cleaned sizes from stored totals instead of re-scanning the filesystem
- Removes one full recursive directory traversal per category after each clean

### Fix E: Scope the SignalMapper connection (LOW)
- Use `connect(SignalMapper::ins(), ..., this, [this] { ... })` so Qt auto-disconnects when `this` is destroyed
- Or store the connection and disconnect in destructor
