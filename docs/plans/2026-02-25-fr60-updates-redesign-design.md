# FR-60: System Update Status — Redesigned Display

**Date:** 2026-02-25
**Status:** Approved

## Problem

The initial FR-60 implementation placed update counts on a Dashboard gauge tile. All existing tile styles (sparkline, gauge, ring, speedometer, VU meter, donut) are percentage-oriented — `setValue(int percent, ...)` — and don't suit discrete update counts. The user wants to see *which* packages are outdated, not a percentage gauge.

## Approved Approach

**Modified Approach B:** Detailed update list on the Homebrew/APT page + sidebar badge indicator (no dashboard tile).

Core infrastructure stays intact: `UpdateInfo` class hierarchy, `DataRefreshService` timer/signal, `SettingManager` keys, `@updatesColor` theme token.

## Section 1: Remove Dashboard Tile

Strip all gauge/sparkline tile code for updates while keeping core infrastructure.

**Remove from `dashboard_page.h/.cpp`:**
- `mUpdatesTile` member variable
- `onSystemUpdatesChecked()` slot and its signal connection
- All "updates" entries from `tileTitle()`, `defaultLayout()`, `availableStyles()`, `defaultStyle()`, `onResetLayout()`
- The `createTile("updates", ...)` call and tile wrapping in `init()`

**Keep intact:**
- `UpdateInfo`, `UpdateInfoMacos`, `UpdateInfoLinux` (core library)
- `DataRefreshService::systemUpdatesChecked` signal, `mUpdateTimer`, `onUpdateTick()`
- `SettingManager` keys: `UpdateAlertEnabled`, `UpdateCheckIntervalMinutes`, `UpdateLastCount`
- `@updatesColor` in both theme `values.ini` files
- Settings page checkbox for enabling/disabling update checks

## Section 2: Sidebar Badge on Homebrew/APT Button

Add a notification badge on `btnAptSourceManager` using the same pattern as `mCleanerBadge`/`mCleanerBadgeDot`.

**New members in `app.h`:**
- `QLabel *mUpdatesBadge` — shows count text (e.g. "3") when sidebar expanded
- `QLabel *mUpdatesBadgeDot` — 8px colored dot when sidebar collapsed

**Implementation in `app.cpp`:**
- Create both labels in `buildSidebar()`, parented to `ui->sidebar`, using object names `updatesBadge` / `updatesBadgeDot`
- Connect to `DataRefreshService::systemUpdatesChecked` in `init()`
- Handler: if `result.totalCount > 0`, show badge text / dot; if 0, hide both
- Position relative to `btnAptSourceManager` (same math as cleaner badge)
- Toggle visibility in `applySidebarCollapse()`

**QSS styling:**
- `#updatesBadge` — same layout as `#sidebarBadge` but uses `@updatesColor` for background
- `#updatesBadgeDot` — same as `#sidebarBadgeDot` but uses `@updatesColor`

**Tray icon alert:** Move from DashboardPage to App — show system notification when count changes from 0 → N (if `UpdateAlertEnabled` is true).

## Section 3: "Available Updates" on Homebrew/APT Page

Add a collapsible section above the existing package list showing outdated packages.

**Layout:**
```
┌─ Available Updates (3)                    [Check Now] ─┐
│  Source        │ Package        │ Version              │
│  brew          │ node           │ 20.1.0 → 22.3.0     │
│  brew          │ python@3.12    │ 3.12.1 → 3.12.4     │
│  softwareupd   │ macOS Sequoia  │ Supplemental Update  │
└────────────────────────────────────────────────────────┘
```

**New members in `APTSourceManagerPage`:**
- `QWidget *mUpdatesSection` — container widget
- `QLabel *mLblUpdatesTitle` — "Available Updates (N)" header, colored with `@updatesColor`
- `QPushButton *mBtnCheckNow` — triggers `DataRefreshService::triggerUpdateCheck()`
- `QTreeWidget *mUpdatesTree` — 3-column: Source, Package, Version

**Constructor change:** Add `DataRefreshService *refreshService` parameter.

**Data flow:**
1. `DataRefreshService` emits `systemUpdatesChecked(UpdateCheckResult)` on 1h timer
2. Page connects in `init()`, populates `mUpdatesTree` from `result.entries`
3. Section hides when `totalCount == 0`, shows when > 0

**Behavior:**
- Initially hidden until first check completes
- "Check Now" button calls `mRefresh->triggerUpdateCheck()`, disables during check
- No per-row actions — user updates externally via terminal
- Uses standard QSS tree widget styling, header uses `@updatesColor`
