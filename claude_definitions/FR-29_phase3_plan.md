# FR-29 Phase 3 (Dashboard & Resources Integration) — Implementation Plan

**Date:** February 2026
**Scope:** Dashboard CircleBar for disk health + Resources page disk temperature history chart
**Prerequisites:** Phase 2 (Disk Health / SMART) complete — commit `fd6e170`. Research: `FR-29_phase3_research.md`

---

## Overview

Phase 3 adds disk health monitoring to two existing pages:
1. **Dashboard** — "DISK HEALTH" CircleBar in Row 0, showing worst-drive health % with color-coded verdict and tray alerts
2. **Resources** — "Disk Temperature" HistoryChart with one series per drive, 30-second refresh interval

---

## Task 1: Dashboard — Disk Health CircleBar

### 1.1 — Add member and slot declarations
- [x] Edit `dashboard_page.h`:
  - Add `CircleBar* mDiskHealthBar;` to private members
  - Add `void updateDiskHealthBar();` to private slots

### 1.2 — Construct and wire in constructor/init
- [x] Edit `dashboard_page.cpp`:
  - In constructor initializer list: `mDiskHealthBar(new CircleBar(tr("DISK HEALTH"), {"#26a69a", "#00897b"}, this))`
  - In `init()`, after adding mDiskBar to circleBarsLayout:
    - Check `im->hasDiskHealth()`:
      - If true: `ui->circleBarsLayout->addWidget(mDiskHealthBar)`
      - Connect a 30-second QTimer for disk health refresh
      - Initial call to `updateDiskHealthBar()`
    - If false: `mDiskHealthBar->hide()`
  - Add `mDiskHealthBar` to the drop shadow widgets list (if disk health available)

### 1.3 — Implement updateDiskHealthBar()
- [x] Get drives via `im->getDriveHealth()` (no refresh here — too expensive)
- [x] Find worst health: iterate drives, find the minimum `healthPercent` (skip -1 values)
- [x] If all healthPercent are -1: use 100% if all SMART passed, otherwise 0%
- [x] Build label text:
  - Single drive: `"{healthPercent}%\n{verdict}"`
  - Multiple drives: `"{healthPercent}%\n{worstDriveModel}"`
- [x] Call `mDiskHealthBar->setValue(displayPercent, label)`

### 1.4 — Implement disk health alert
- [x] Check `mSettingManager->getDiskHealthAlertEnabled()`
- [x] If any drive has verdict "Caution" or "Critical":
  - Use static bool flag (same pattern as CPU/Memory alerts) to fire once
  - Show tray notification with drive name and verdict
  - Reset flag when all drives return to "Good"

### 1.5 — Add 30-second refresh timer
- [x] Create `QTimer *timerDiskHealth = new QTimer(this)` in `init()`
- [x] Connect to a lambda or helper that calls `im->refreshDiskHealth()` then `updateDiskHealthBar()`
- [x] Start at 30000ms
- [x] This timer refreshes the underlying data (subprocess calls); the display update reads from cache

### 1.6 — Build verification
- [x] Incremental build succeeds
- [x] Dashboard shows DISK HEALTH CircleBar when drives available
- [x] CircleBar hidden when no disk health data

---

## Task 2: Resources — Disk Temperature History Chart

### 2.1 — Add member and slot declarations
- [x] Edit `resources_page.h`:
  - Add `HistoryChart *mChartDiskHealth;` member
  - Add `void updateDiskHealthChart();` slot
  - Add `QTimer *mDiskHealthTimer;` member (separate 30s timer)

### 2.2 — Construct and wire in constructor/init
- [x] In constructor initializer list: set `mChartDiskHealth(nullptr)`, `mDiskHealthTimer(nullptr)`
- [x] In `init()`:
  - Check `im->hasDiskHealth()`:
    - Count drives with temperature data
    - If count > 0: create `HistoryChart(tr("History of Disk Temperature"), driveCount, nullptr, this)`
    - Set `mChartDiskHealth->setYMax(100)` (100°C max)
    - Insert into widgets list after Network chart, before Disk Launcher
    - Create 30s timer, connect to `updateDiskHealthChart()`
    - Initial call to `updateDiskHealthChart()`

### 2.3 — Implement updateDiskHealthChart()
- [x] Call `im->refreshDiskHealth()` to get fresh temperature readings
- [x] Get series list from chart
- [x] For each drive with temperature:
  - Shift existing points right by 1
  - Insert new temperature value at index 0
  - Update series name: `"{model}: {temp} °C"`
  - Remove points beyond 61
- [x] Set series list back

### 2.4 — Build verification
- [x] Incremental build succeeds
- [x] Resources page shows Disk Temperature chart when temperature data available
- [x] Chart hidden when no temperature data

---

## Task 3: Final Verification & Cleanup

### 3.1 — Clean rebuild
- [x] Full clean rebuild on macOS
- [x] Verify zero new warnings

### 3.2 — Functional testing
- [x] Dashboard: DISK HEALTH CircleBar shows correct health %
- [x] Dashboard: Disk health alert fires for Caution/Critical verdicts
- [x] Resources: Disk Temperature chart shows live temperature data
- [x] Kiosk mode: DISK HEALTH bar visible in fullscreen

### 3.3 — Update tracking files
- [x] Mark FR-29 Phase 3 tasks complete
- [x] Update FEATURE_REQUESTS.md — mark FR-29 as `[x]` complete
- [x] Commit and push

---

## Summary of All Files

### Modified (4 files)
| # | File | Changes |
|---|------|---------|
| 1 | `shared/nexis/Pages/Dashboard/dashboard_page.h` | +2 members, +1 slot |
| 2 | `shared/nexis/Pages/Dashboard/dashboard_page.cpp` | +60 lines (construct, init, update, alert) |
| 3 | `shared/nexis/Pages/Resources/resources_page.h` | +2 members, +1 slot |
| 4 | `shared/nexis/Pages/Resources/resources_page.cpp` | +50 lines (construct, init, update) |

**No new files.** Total estimated ~110 new lines of code.

---

## Key Design Decisions

| Decision | Implementation |
|----------|---------------|
| **CircleBar position** | Row 0 `circleBarsLayout`, after DISK bar |
| **Color** | Teal `#26a69a/#00897b` (distinct from all existing bars) |
| **Dashboard refresh** | 30-second QTimer (disk health data changes slowly; subprocess calls are expensive) |
| **Resources chart** | Disk Temperature (not health %) — temperature fluctuates meaningfully; health % is near-static |
| **Resources refresh** | 30-second QTimer (balances freshness with subprocess overhead) |
| **Worst-drive display** | Dashboard bar shows the drive with lowest healthPercent |
| **Alert behavior** | Fires tray notification once when any drive reaches Caution/Critical; resets when all return to Good |
