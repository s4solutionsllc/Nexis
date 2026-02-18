# FR-29 Phase 3 Research — Dashboard & Resources Integration

**Date:** February 2026
**Scope:** Disk health CircleBar on Dashboard + disk health HistoryChart on Resources page
**Prerequisites:** Phase 1 (Battery Health) commit `66646a8`, Phase 2 (Disk Health / SMART) commit `fd6e170`

---

## 1. Current Dashboard Architecture

### Layout
- `dashboard_page.ui`: QGridLayout with 3 rows × 4 columns
  - **Row 0** (colspan 4): `circleBars` QHBoxLayout — CPU, MEMORY, DISK CircleBars
  - **Row 1**: Four containers — `tempContainer` (col 0), `gpuContainer` (col 1), `lineBars` (col 2), `batteryContainer` (col 3)
  - **Row 2** (colspan 4): `widgetUpdateBar` — hidden update notification

### CircleBar Widget Pattern
- Constructor: `CircleBar(title, {color1, color2}, parent)`
- Update: `setValue(percent, labelText)`
- Each bar is a QPieSeries in a QChart, displaying 0-100%
- Color pairs: CPU green `#2ec27e/#26a269`, Memory orange `#E95420/#c64516`, Disk red `#e01b24/#c01c28`, Temp blue `#1c71d8/#1a5fb4`, GPU purple `#813d9c/#613583`, Battery yellow `#f5c211/#e5a50a`

### How Battery Bar Was Added (Phase 1 pattern)
1. **UI**: Added `batteryContainer` QWidget with QVBoxLayout at Row 1, Col 3
2. **Header**: Added `CircleBar* mBatteryBar;` member
3. **Constructor**: Created `mBatteryBar` with title "BATTERY" and yellow colors
4. **init()**: Check `im->hasBattery()` → add to layout or hide container; connect timer; initial call
5. **updateBatteryBar()**: Refresh data, compute display value, set label, fire alert if threshold crossed

### Alert Pattern
- Uses `static bool isShow` flag to fire once per threshold crossing
- Battery alert has additional sophistication: snooze mechanism, last-health tracking, 5% hysteresis
- All alerts use `AppManager::ins()->getTrayIcon()->showMessage()`

### Graceful Degradation
- Features check `im->hasSomething()` → if false, `container->hide()` and `bar->hide()`
- This prevents orphan widgets rendering at (0,0) and keeps layout clean

---

## 2. Current Resources Page Architecture

### Layout
- `resources_page.ui`: QScrollArea with vertical QVBoxLayout (`chartsLayout`)
- Charts added programmatically in `init()` via `ui->chartsLayout->addWidget(chart)`
- Order: CPU → CPU Load Avg → [GPU if present] → Disk R/W → Memory → Network → Disk Launcher

### HistoryChart Widget Pattern
- Constructor: `HistoryChart(title, seriesCount, axisType, parent)`
  - `seriesCount`: number of QSplineSeries (one per data line)
  - `axisType`: `nullptr` for numeric Y axis, `new QCategoryAxis` for byte-formatted Y
- `setYMax(value)`: set Y axis range
- `getSeriesList()` / `setSeriesList()`: access underlying series for data manipulation
- Rolling 61-point window: shift all points right by 1, insert new at index 0, remove > 61

### How GPU Chart Was Added (pattern to follow)
1. **Header**: Added `HistoryChart *mChartGpu;` member
2. **Constructor**: Set `mChartGpu(nullptr)` (lazy init)
3. **init()**: Check `im->hasGpu()` → create chart with series count = GPU count, set Y max, insert into widget list at desired position
4. **Timer connect**: `connect(mTimer, &QTimer::timeout, this, &ResourcesPage::updateGpuChart)`
5. **Update slot**: Get data, shift points, insert new point, update series name/legend

---

## 3. Disk Health Data Available from InfoManager

From Phase 2 `InfoManager` interface:
- `QList<DriveHealth> getDriveHealth()` — all detected drives with SMART data
- `void refreshDiskHealth()` — re-scan drives
- `bool hasDiskHealth()` — any drives detected?
- `bool hasSmartctl()` — smartctl available?

Key fields for Dashboard CircleBar:
- `healthPercent` (0-100, -1 if unavailable) — NVMe: 100-percentageUsed; SATA SSD: wearLevelingCount; HDD: -1
- `healthVerdict` — "Good", "Caution", "Critical", "Unknown"
- `model` — drive name for label

Key fields for Resources HistoryChart:
- `healthPercent` — trackable over time per drive
- `temperatureCelsius` — trackable temperature
- These values change slowly (unlike CPU/GPU which fluctuate every second), so a longer poll interval is appropriate

---

## 4. Dashboard Integration Design

### Disk Health CircleBar
- **Position**: Row 1, Col 2 area. But there are already 4 containers in Row 1 (temp, gpu, linebars, battery). Options:
  - **Option A**: Add a new column (col 4) — stretches the grid wider
  - **Option B**: Replace or merge with existing Disk Usage bar — confusing since "DISK" already shows usage %
  - **Option C**: Add to Row 0 alongside CPU/MEM/DISK — Row 0 uses HBoxLayout, easy to append
  - **Recommendation: Option C** — Add a "DISK HEALTH" CircleBar to Row 0's `circleBarsLayout`, right after the DISK bar. This keeps health alongside the other top-level gauges. Row 0 already handles variable widgets (3 minimum) and HBoxLayout distributes space evenly.

- **Display Value**: Use the "worst" drive health as the displayed percentage
  - If multiple drives, show the lowest `healthPercent` (most concerning)
  - If all drives have `healthPercent == -1`, show 100 (healthy by SMART passed) or hide
  - Label: Show health percent + verdict

- **Color**: Use teal/cyan `#26a69a/#00897b` to distinguish from existing disk usage (red) and battery (yellow)

- **Alert**: Disk health alert — check `mSettingManager->getDiskHealthAlertEnabled()`, fire tray notification when any drive verdict is "Caution" or "Critical"

- **Refresh Rate**: Disk health data changes very slowly. Instead of every 1s, refresh every 60s (new separate QTimer) or piggyback on the 5s disk timer

### Implementation Details
1. Add `diskHealthContainer` QWidget at Row 1, Col 4 (new column) — **NO**, better to add to Row 0
2. Actually, looking more carefully at the layout: Row 0 is the `circleBars` widget with HBoxLayout. The current pattern adds CircleBars directly to this layout in `init()`. So we just add `mDiskHealthBar` to `circleBarsLayout`.
3. But Row 0 already has CPU + MEM + DISK (3 bars). Adding a 4th bar here is fine — the HBoxLayout handles it.
4. Row 1 has Temp, GPU, LineBar (Download/Upload), Battery — 4 items in 4 columns. We don't want to add a 5th column.

**Final Decision: Add to Row 0** as a 4th CircleBar. The existing DISK bar shows disk space usage, the new one shows disk health (SMART). Clear distinction via label "DISK HEALTH" and different color.

---

## 5. Resources Page Integration Design

### Disk Health History Chart
- **What to chart**: `healthPercent` per drive over time (one series per drive)
- **Problem**: Health percent barely changes over seconds/minutes — it's a slowly degrading metric. Unlike CPU (fluctuates every second), disk health might stay at 98% for months.
- **Better approach**: Chart disk temperature over time — this fluctuates meaningfully and gives the user actionable real-time data
- **Alternative**: Show both — a "Disk Health" static summary card + "Disk Temperature" history chart

### Recommendation
Add a **"Disk Temperature"** HistoryChart with one series per drive, showing temperature in °C over the 60-second rolling window. This mirrors the CPU/GPU charts and provides useful real-time monitoring.

The static health summary is already available in Hardware Info → Storage section.

### Implementation Details
- `seriesCount` = number of drives with temperature data
- `Y max` = 100 °C (reasonable max for display)
- Update every 5 seconds (matches disk timer cadence, not 1s since SMART reads have latency)
- Series name: drive model + current temp
- Position: After GPU chart, before Disk R/W

**Note**: On macOS, `diskutil info -plist` returns temperature for NVMe drives from the SMART dict. Each `refreshDiskHealth()` call re-reads diskutil, which provides fresh temperature. On Linux, smartctl re-reads the drive's SMART data. Both have I/O cost, so a 5-10s interval is appropriate.

However, there's a concern: `refreshDiskHealth()` runs `diskutil info -plist` for each drive, which is slow (subprocess per drive). We should NOT call this every 1s. Options:
1. Use a separate 10-30s timer for disk health refresh
2. Cache the last result and only refresh when the timer fires
3. Only read temperature from a lightweight source (sysfs on Linux is fast; macOS diskutil is slower)

**Decision**: Use a 30-second refresh interval for the Resources page disk temperature chart. This balances freshness with performance.

---

## 6. Files Inventory

### Dashboard Integration
| File | Changes |
|------|---------|
| `shared/nexis/Pages/Dashboard/dashboard_page.h` | +`CircleBar* mDiskHealthBar;` member, +`updateDiskHealthBar()` slot |
| `shared/nexis/Pages/Dashboard/dashboard_page.cpp` | +construct bar, +init logic, +update method, +alert logic |
| `shared/nexis/Pages/Dashboard/dashboard_page.ui` | No change needed (bar added to existing circleBarsLayout programmatically) |

### Resources Integration
| File | Changes |
|------|---------|
| `shared/nexis/Pages/Resources/resources_page.h` | +`HistoryChart *mChartDiskHealth;` member, +`updateDiskHealthChart()` slot, +`QTimer *mDiskHealthTimer;` |
| `shared/nexis/Pages/Resources/resources_page.cpp` | +construct chart, +init logic, +update method with 30s timer |
| `shared/nexis/Pages/Resources/resources_page.ui` | No change needed (chart added programmatically) |

### No New Files
All changes are modifications to existing files.

---

## 7. Edge Cases

1. **No drives detected**: `im->hasDiskHealth()` returns false → hide bar/skip chart
2. **All drives have healthPercent == -1**: Show 100% (assume healthy since SMART passed) or "N/A"
3. **No temperature data**: Skip the Resources chart entirely, or show chart with no data points
4. **Single drive vs multiple drives**: Dashboard shows worst-case; Resources shows one series per drive
5. **`needsElevation` drives**: Include in display with whatever partial data is available
6. **Refresh performance**: Don't refresh disk health too frequently; subprocess calls are expensive
7. **Kiosk mode**: New CircleBar should be visible in kiosk mode (Row 0 bars are always visible)
8. **Mac Apple Fabric drives**: May have temperature but no healthPercent — handle gracefully

---

## 8. Color Palette Reference

Existing CircleBar colors:
- CPU: `#2ec27e` / `#26a269` (green)
- Memory: `#E95420` / `#c64516` (orange)
- Disk: `#e01b24` / `#c01c28` (red)
- Temp: `#1c71d8` / `#1a5fb4` (blue)
- GPU: `#813d9c` / `#613583` (purple)
- Battery: `#f5c211` / `#e5a50a` (yellow)

Available for Disk Health: teal `#26a69a` / `#00897b` or cyan `#00bcd4` / `#0097a7`
