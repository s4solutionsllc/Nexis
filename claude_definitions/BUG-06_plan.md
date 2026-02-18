# BUG-06 Plan: Fix Slow Startup with Large /etc/hosts File

## Overview

The Hosts Manager reads and parses the entire `/etc/hosts` file synchronously on the main thread during app startup, even though most users never visit the page. Systems with 10,000+ entries (Pi-hole, ad-blockers) experience UI freezing. Fix by deferring the load until the page is shown and optimizing the model population.

## Phase 1: Defer Loading Until Page Is Shown

Move the expensive work out of the constructor so it only runs when the user navigates to the Helpers page.

### Task 1.1: Add lazy-load flag and extract loading from init()

**File:** `shared/nexis/Pages/Helpers/host_manage.h`

- [x] Add `bool mLoaded = false;` member variable
- [x] Add `public` method `void loadIfNeeded();`

**File:** `shared/nexis/Pages/Helpers/host_manage.cpp`

- [x] Remove the two lines from `init()` that do the eager load
- [x] Implement `loadIfNeeded()`

### Task 1.2: Trigger load from HelpersPage when user navigates

**File:** `shared/nexis/Pages/Helpers/helpers_page.cpp`

- [x] In `on_btnHostManage_clicked()`, call `widgetHostManage->loadIfNeeded()` before switching the stacked widget index

## Phase 2: Batch Model Population

Eliminate per-row signal overhead when populating the model with many entries.

### Task 2.1: Disable signals/sorting during bulk insertion

**File:** `shared/nexis/Pages/Helpers/host_manage.cpp` — `loadTableData()`

- [x] `blockSignals(true)` and `setDynamicSortFilter(false)` before loop
- [x] `blockSignals(false)`, `setDynamicSortFilter(true)`, `invalidate()`, `reset()` after loop

## Phase 3: Pre-compile Regex

- [x] `static const QRegularExpression whitespace("\\s+")` in `loadHostItems()`

## Phase 4: Avoid Full Reload on Single-Row Operations

### Task 4.1: Optimize on_btnSave_clicked() (add/edit)

- [x] Add: append to mHostFileContent + mHostItemList, single appendRow()
- [x] Edit: update in-place via LineNumberRole lookup, no model rebuild

### Task 4.2: Optimize delete action

- [x] Map proxy indices to source, remove rows directly from model in reverse order
- [x] Remove loadTableData() call from delete branch

### Task 4.3: Optimize on_btnSaveChanges_clicked()

- [x] Removed redundant loadTableData() after file write — model already reflects current state

## Phase 5: Build & Verify

- [x] Incremental build
- [ ] Manual test: app starts, Helpers page deferred
- [ ] Manual test: add, edit, delete entries
- [x] Update BUGS.md with resolution note

## Summary of Changes

| File | Changes |
|---|---|
| `host_manage.h` | Add `mLoaded` flag, `loadIfNeeded()` method |
| `host_manage.cpp` | Defer load; batch insertion; static regex; incremental add/edit/delete; remove redundant reload after save |
| `helpers_page.cpp` | Call `loadIfNeeded()` on navigate |

No new files. No UI changes. No new dependencies.
