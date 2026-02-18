# BUG-10 Plan: Fix Memory Leak in System Cleaner

## Overview

Fix 5 memory management issues in the System Cleaner that cause unbounded RSS growth (150MB → 2GB+) during long sessions. The fixes are ordered by impact and dependency.

## Phase 1: Concurrency Guard (CRITICAL)

Prevent overlapping scan/clean workers that cause data races on shared `QFileInfoList` members.

### Task 1.1: Add in-progress state flags

**File:** `shared/nexis/Pages/SystemCleaner/system_cleaner_page.h`

- [x] Add two member variables:
  ```cpp
  bool mScanInProgress = false;
  bool mCleanInProgress = false;
  ```

### Task 1.2: Guard scan entry point

**File:** `shared/nexis/Pages/SystemCleaner/system_cleaner_page.cpp` — `on_btnScan_clicked()`

- [x] At the top of `on_btnScan_clicked()`, return early if `mScanInProgress || mCleanInProgress`
- [x] Set `mScanInProgress = true` before launching `QtConcurrent::run`
- [x] Set `mScanInProgress = false` at the end of `onScanFinished()`

### Task 1.3: Guard clean entry point

**File:** `shared/nexis/Pages/SystemCleaner/system_cleaner_page.cpp` — `on_btnClean_clicked()`

- [x] At the top of `on_btnClean_clicked()`, return early if `mScanInProgress || mCleanInProgress`
- [x] Set `mCleanInProgress = true` before launching `QtConcurrent::run`
- [x] Set `mCleanInProgress = false` at the end of `onCleanFinished()`

### Task 1.4: Guard back-to-categories during active worker

**File:** `shared/nexis/Pages/SystemCleaner/system_cleaner_page.cpp` — `on_btnBackToCategories_clicked()`

- [x] At the top, return early if `mScanInProgress || mCleanInProgress` (prevents user navigating back and re-triggering scan while worker runs)

**Acceptance:** Rapid clicking Scan or navigating back+Scan during an active scan does nothing. Only one worker runs at a time.

## Phase 2: Worker Lifecycle Safety

Ensure the worker thread doesn't outlive the page object.

### Task 2.1: Wait for worker in destructor

**File:** `shared/nexis/Pages/SystemCleaner/system_cleaner_page.cpp` — `~SystemCleanerPage()`

- [x] Add `mWorkerFuture.waitForFinished();` before `delete ui`

### Task 2.2: Scope the SignalMapper theme connection

**File:** `shared/nexis/Pages/SystemCleaner/system_cleaner_page.cpp` — `init()`

- [x] Change the `connect()` call from:
  ```cpp
  connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme, [=] {
  ```
  to:
  ```cpp
  connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme, this, [this] {
  ```
  This makes Qt auto-disconnect when `this` is destroyed and avoids capturing all members by value.

**Acceptance:** Destroying SystemCleanerPage (or closing the app during a scan) does not crash or access freed memory.

## Phase 3: Reduce Memory Footprint

Eliminate unnecessary retained data and redundant filesystem traversals.

### Task 3.1: Clear scan result lists after tree is built

**File:** `shared/nexis/Pages/SystemCleaner/system_cleaner_page.cpp` — `onScanFinished()`

- [x] At the end of `onScanFinished()` (after tree is fully built), clear all result lists.
  These lists are only needed to build the tree — all data is now in QTreeWidgetItems.

### Task 3.2: Eliminate redundant getFileSize() in onCleanFinished()

**File:** `shared/nexis/Pages/SystemCleaner/system_cleaner_page.cpp` — `onCleanFinished()`

- [x] Instead of re-scanning the filesystem, compute the remaining size by summing the `SortRole` data of the remaining children in each top-level item. The size data is already stored per-child in `ByteTreeWidget::setValues()` via `SortRole`. No filesystem access needed.

**Acceptance:** After cleaning, category sizes update correctly without spawning recursive directory walks. Memory peak during clean is significantly reduced.

## Phase 4: Build & Verify

- [x] Incremental build: `cmake --build build -j$(sysctl -n hw.ncpu)`
- [ ] Manual test: perform 5+ consecutive scans, verify no crash or memory spike
- [ ] Manual test: click Scan, immediately click Back, click Scan again — should not crash
- [ ] Manual test: perform a clean, verify category sizes update correctly
- [x] Update BUGS.md with resolution note

## Summary of Changes

| File | Changes |
|---|---|
| `system_cleaner_page.h` | Add `mScanInProgress`, `mCleanInProgress` members |
| `system_cleaner_page.cpp` | Guards in scan/clean/back handlers; wait in destructor; scope lambda; clear lists after use; remove redundant getFileSize(); add `nexis_roles.h` include |

No new files. No UI changes. No API changes.
