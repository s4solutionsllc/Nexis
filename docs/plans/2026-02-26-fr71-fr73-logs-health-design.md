# Design: FR-71 System Log Viewer & FR-73 System Health Score

**Date:** 2026-02-26
**Status:** Approved

## FR-71: System Log Viewer

### Summary

New sidebar page under SYSTEM section providing a filterable, searchable table of recent system logs. Built with a "start simple, extend later" philosophy — quick triage UI now, architected for future advanced filtering (date ranges, unit/subsystem filters, regex, export).

### Architecture

- **Page:** `shared/nexis/Pages/SystemLogs/system_logs_page.{h,cpp}` — fully programmatic layout (no `.ui` file)
- **Log provider abstraction:** `LogProvider` virtual base with `LogProviderLinux` and `LogProviderMacOS` subclasses, each wrapping a `QProcess` call
- **Sidebar placement:** SYSTEM section, between "Helpers" and APT/Homebrew button

### UI Layout

```
┌──────────────────────────────────────────────┐
│  [Severity ▾]  [Search... ___________] [⟳]  │  Filter toolbar
├──────────────────────────────────────────────┤
│  Timestamp      │ Sev │ Unit      │ Message  │  QTableView
│  Feb 26 14:02   │ ERR │ kernel    │ ...      │
│  Feb 26 14:01   │ WRN │ sshd      │ ...      │
│  Feb 26 14:00   │ INF │ systemd   │ ...      │
│  ...            │     │           │          │
├──────────────────────────────────────────────┤
│  Showing 500 entries │ Last 1 hour            │  Status bar
└──────────────────────────────────────────────┘
```

### Data Model

- `QStandardItemModel` with 4 columns: Timestamp, Severity, Unit/Subsystem, Message
- `QSortFilterProxyModel` for search filtering and severity filtering
- Severity levels: Emergency, Alert, Critical, Error, Warning, Notice, Info, Debug
- Color-coded severity cells via delegate (red for Error+, yellow for Warning, default for Info)

### Platform Log Sources

- **Linux:** `QProcess` → `journalctl --output=json --no-pager --lines=500 --reverse`
  - JSON fields: `__REALTIME_TIMESTAMP`, `PRIORITY`, `_SYSTEMD_UNIT` / `SYSLOG_IDENTIFIER`, `MESSAGE`
- **macOS:** `QProcess` → `log show --style json --last 1h --predicate 'eventType == logEvent'`
  - JSON fields: `timestamp`, `messageType`, `subsystem` / `process`, `eventMessage`

### Refresh Behavior

- Initial load: last 500 entries (Linux) or last 1 hour (macOS)
- Manual refresh button only (no auto-polling — logs are static history)
- Severity dropdown filters: All / Error+ / Warning+ / Info+

### Extensibility Hooks

- `LogProvider` base class with `fetchLogs(params)` returning `QList<LogEntry>` — platform subclasses handle the specifics
- Filter toolbar layout accepts additional widgets for future date range, unit filter, etc.

---

## FR-73: System Health Score Tile

### Summary

New dashboard tile showing a composite 0–100 health score aggregating existing system metrics. Displays the overall score prominently with a per-component breakdown visible in larger tile sizes.

### Architecture

- **Tile:** `shared/nexis/Pages/Dashboard/health_score_tile.{h,cpp}` — inherits `MetricTileBase`
- **Calculator:** `shared/nexis/Pages/Dashboard/health_score_calculator.{h,cpp}` — lightweight helper class (not a singleton), owned by the tile
- **Theme token:** `@healthScoreColor` in `values.ini`

### Score Calculation

Six components, each producing a 0–100 sub-score:

| Component | Data Source | Scoring Logic | Weight |
|-----------|-------------|---------------|--------|
| CPU | `cpuUpdated` (load avg 1m) | 100 at load 0, 0 at load >= numCores | 15% |
| Memory | `memoryUpdated` (% used) | 100 at 0%, 0 at 100% (linear) | 20% |
| Disk | `diskUsageUpdated` (worst mount %) | 100 at 0%, 0 at 100% | 25% |
| Temperature | `tempUpdated` | 100 below 60C, 0 at >= 100C (linear) | 15% |
| Battery | `batteryUpdated` (cycle health %) | Pass-through (already 0–100) | 10% |
| Disk Health | `diskHealthUpdated` (SMART) | 100 healthy, 50 degraded, 0 failing | 15% |

- Unavailable components (no battery, no thermal, no SMART) are excluded; weights redistributed proportionally
- Final score = weighted average, rounded to nearest integer

### Color Mapping

- 80–100: `@successColor` (green) — label "Excellent"
- 60–79: `@warningColor` (amber) — label "Good" / "Fair"
- 0–59: `@destructiveColor` (red) — label "Poor"

### Tile Display

- **Normal mode (1x1):** Score number + label only
- **Large/Hero mode (2x1+):** Score number + label + per-component mini breakdown bars
- Breakdown bars are small horizontal bars drawn in `paintEvent`, colored per-component

```
┌─────────────────────┐
│     Health Score     │
│                      │
│        87            │
│       Good           │
│                      │
│  CPU ████████░░  85  │
│  MEM ██████░░░░  62  │
│  DSK █████████░  92  │
│  TMP ████████░░  80  │
│  BAT █████████░  95  │
│  HDD ██████████ 100  │
└─────────────────────┘
```

### Dashboard Integration

- Created via `createTile("health", ...)` in `dashboard_page.cpp` after fan tile
- Participates in standard `mHiddenTiles` / `wrapTile` / `rebuildLayout` system — user can hide/show via edit mode like any other tile
- Default grid position: row 0, col 0 (top-left)
- `defaultLayout()` updated to include `"health"` entry
- `tileTitle()` returns `("Health Score", "@healthScoreColor")`

### Signal Connections

Tile subscribes to `DataRefreshService` signals in `DashboardPage::init()`:
- `cpuUpdated`, `memoryUpdated`, `diskUsageUpdated`, `tempUpdated`, `batteryUpdated`, `diskHealthUpdated`
- Each update recalculates composite score via `HealthScoreCalculator` and calls `setValue()`

---

## Design Decisions

1. **Process-based log reading** over direct file parsing — `journalctl`/`log show` produce structured JSON and handle log rotation
2. **Weighted scoring with proportional redistribution** over simple thresholds — preserves nuance while gracefully handling missing components
3. **Calculator as helper class** (not singleton service) — only one consumer exists (the tile); easy to extract later if needed
4. **No `.ui` file for log viewer** — matches DiskToolsPage pattern for programmatic layouts
5. **Health tile hideable** — standard tile behavior, no special-casing
