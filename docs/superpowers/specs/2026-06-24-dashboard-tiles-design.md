# Dashboard Tiles Enhancement — Design

- **Date:** 2026-06-24
- **Issue:** GH#191 (Paperclip: NEX, "Add more tiles to dashboard")
- **Status:** Approved design, pending implementation plan
- **Platform scope:** Linux + macOS (cross-platform; tile data sources already exist on both)

## Background

GH#191 asks to "add more tiles to the dashboard," specifically the ability to
show multiple inputs of a type — e.g. CPU temp *and* a second thermal sensor,
or CPU fan *and* pump fan — for kiosk/second-monitor system monitoring.

Investigation of the current dashboard established:

- The dashboard is a **hardcoded 4×4 bento grid** (16 cells) with an edit
  mode supporting drag, resize, per-tile style switching, and a per-tile gear
  menu. Layout is persisted as JSON in `SettingManager` under
  `SettingKeys::DashboardLayout`.
- Tiles are **proportional, not fixed-pixel**: every row/column gets equal
  stretch (`dashboard_page.cpp:1343-1346`), cells use `Expanding` size policy,
  and cell size is `gridRect.width()/GRID_COLS` × `gridRect.height()/GRID_ROWS`
  (`:1393-1394`). There is **no minimum/maximum size constraint** on tiles.
- Tiles are **singletons per type** today: one `temp` tile with a gear menu to
  select *one* thermal sensor, one `fan` tile selecting *one* fan, etc. The
  selection is stored as a **global** setting (`TempSensorId`, `FanSensorId`,
  `GpuDeviceId`, `DiskName`).
- A **Fan Speed tile type already exists** and works (`mFanTile`,
  `updateFanTile`, `InfoManager::getFanSpeed`); fan detection is implemented on
  Linux (hwmon, ThinkPad/Dell `/proc`, NVIDIA SMI) and macOS (SMC). So the
  request's "new fan tile type" is really *multiple* fan tiles.
- Display sizing is driven purely by **span area** via
  `applyDisplayModeForSpan` (`:1359-1365`): area ≥4 → `Hero`, ≥2 → `Large`,
  else `Normal`. Modes only toggle QSS properties (`heroMode`/`largeMode`);
  there is no pixel floor and no compact rendering.
- **Network interface enumeration already exists**:
  `NetworkInfo::getAllInterfaces()` and a per-interface `getInterfaceStats()`
  map (`NetInterfaceStatsMap`, keyed by NIC name, restricted to up+running
  non-loopback interfaces).

## Goals

1. **Denser grid (req #1):** more, smaller placement cells so users can fit
   more data on the dashboard.
2. **Multiple tiles per type (req #2 + #3):** N tiles of an input-bound type,
   each pinned to a specific detected input (thermal sensor, fan, disk, GPU,
   network interface).
3. **Readability:** small tiles must remain clear.

## Non-goals

- No user-configurable cell size (fixed at 120×98px; not exposed in settings).
- No per-width layout memory — when the visible column count changes, tiles are
  repacked in place; a previous wider arrangement is not restored on widening
  (YAGNI for v1).
- No horizontal scroll in the normal case — columns are derived from width, so
  they always fit (horizontal scroll appears only when the window is narrower
  than the MIN-column floor).
- No per-tile data-refresh intervals.
- No input binding for single-input types (CPU, memory, battery, health).

## Design

### 1. Grid model — fixed cells, responsive columns, reflow

> **Revised 2026-06-24** after first-pass review: the original proportional
> 8×8 stretch grid produced uneven row heights (chart tiles forced their rows
> taller) and large sparse gaps, making tiles hard to align and compare. The
> grid now uses **fixed-size cells with a responsive column count**.

- **Fixed cell size.** A 1×1 cell is **120×98px** with a **10px gap** (pitch
  130×108). Cells no longer stretch; the grid packs top-left with a trailing
  spacer. Every 1×1 is identical and every 2×2 (=250×206px) is identical —
  eliminating the uneven-row problem.
- **Responsive column count.** `visibleCols = clamp(floor((panelWidth + gap) /
  pitchX), kMinCols, kMaxCols)` with **kMinCols = 4, kMaxCols = 16**,
  recomputed on every window resize. A ~1680px panel → ~13 columns; a laptop →
  ~8.
- **Vertical scroll.** The bento grid lives in a `QScrollArea` (vertical only);
  tiles never shrink, so overflow scrolls down rather than compressing.
- **Dynamic occupancy.** The fixed `mOccupancy[GRID_ROWS][GRID_COLS]` array
  becomes a dynamic structure sized `rowCount × visibleCols`. Hit-testing,
  drag, and resize use the **fixed cell pitch** (130×108) instead of
  `width/GRID_COLS`, and account for the scroll-area viewport offset.
- **Persistence + reflow.** Tiles persist absolute `(row, col, rowSpan,
  colSpan)` in a logical grid up to `kMaxCols` wide. On load — and whenever a
  resize changes `visibleCols` — `reflow(tiles, visibleCols)` clamps each
  tile's `colSpan`/position to fit the width and repacks tiles in order (row,
  then col) into the first free region, removing overlaps and gaps. The
  repacked positions are persisted.
- **Default layout.** `defaultLayout()` keeps the **2×2** default tiles in
  logical coordinates (cpu 0,0; memory 0,2; …); the load-time reflow fits them
  to the current column count.
- **Layout migration.** Persisted layouts use the v2 envelope
  `{"version":2,"tiles":[...]}`. A bare array is legacy v1 (4×4): each tile's
  `row`/`col`/`rowSpan`/`colSpan` is scaled ×2 (so an old 1×1 becomes a 2×2),
  clamped to `kMaxCols`; the load-time reflow then fits it to the current
  column count. `tierForArea` (for the compact tier) is unchanged.

### 2. Compact display tier

- Add `Compact` to the `DisplayMode` enum (`metric_tile_base.h:20`) as the
  smallest tier (below `Normal`).
- Rescale `applyDisplayModeForSpan` thresholds for the 8×8 grid: area ≥16 →
  `Hero`, ≥8 → `Large`, ≥4 → `Normal`, **else `Compact`**. Mapping rationale:
  a 2×2 default tile (area 4) renders as `Normal` (today's look), a 4×4 tile
  (area 16) as `Hero`, and a 1×1 / 1×2 small tile as `Compact`.
- Each tile subclass's `setDisplayMode` gains a `Compact` branch that **hides
  the gauge/sparkline sub-widget and shows only label + value + unit**, set via
  a `compactMode` QSS dynamic property — the same property mechanism already
  used for `heroMode`/`largeMode` (`metric_tile.cpp:154-155`). Subclasses to
  update: `MetricTile`, `GaugeTile`, `RingTile`, `HybridTile`,
  `SpeedometerTile`, `VuMeterTile`, `DiskTile`, `NetworkTile`,
  `HealthScoreTile`.
- Theme: add `compactMode` font/size selectors in the QSS/`values.ini` theme
  tokens. No hardcoded hex colors in C++ (BUG-47); colors resolve from theme
  tokens with `refreshThemeColors()` on `SignalMapper::sigChangedAppTheme`.
- After toggling the `compactMode` dynamic property, child widgets get explicit
  `unpolish()`/`polish()` so the property selector re-evaluates (BUG-56).

### 3. Multi-instance, input-bound tiles

This is the core refactor.

- **Per-tile binding in JSON:** each tile entry gains an `"input"` field — the
  bound input's stable key (thermal sensor `id`, fan `id`, disk name, GPU `id`,
  or network interface name). The existing `id` field stays the *type*
  (`"temp"`, `"fan"`, …). Duplicate type entries are now permitted; tile
  uniqueness is by grid position, not by type. Single-input types omit
  `"input"`.
- **DashboardPage state refactor:** the single per-type pointers (`mTempTile`,
  `mFanTile`, `mDiskTile`, `mGpuTile`, network) become **collections of
  wrappers**, each carrying its `{type, input}` metadata. The per-type update
  routines (`updateTempTile`, `updateFanTile`, `updateDiskTile`, GPU, network)
  change from "read the single `mSelected*Index`" to "**iterate every wrapper
  of this type and update each from its own bound input**." The
  `mSelectedSensorIndex` / `mSelectedFanIndex` / `mSelectedGpuIndex` fields are
  removed in favor of per-wrapper binding.
- **"Add tile" palette:** an edit-mode "＋ Add tile" control opens a small
  dialog listing tile types. Input-bound types expand to show the detected
  inputs (inputs already placed on the dashboard are flagged/disabled).
  Selecting an entry creates a new wrapper at the first free cell (reuse the
  existing free-cell scan, `:1688-1689`), bound to that input. The palette
  dialog is a new focused class. Per the project Qt gotcha, if it embeds a
  `QScrollArea` the viewport must be made transparent; icon-only transparent
  buttons use `QToolButton` with `setAutoRaise(true)` (BUG-52).
- **Per-tile gear menu:** the existing per-tile gear dropdown
  (`metric_tile_base`) re-binds *that tile's* input and writes the choice into
  that tile's layout entry — it no longer mutates a global setting.
- **Singletons unchanged:** CPU, memory, battery, health have a single input
  and get no binding UI.

### 4. Scope of input-bound types

`temp`, `fan`, `disk`, `gpu`, and `network` (per-interface).

- temp / fan / disk / gpu already enumerate their inputs via `InfoManager`
  (`getThermalSensors`, `getFanSensors`, `getDisks`, `getGpuDevices`).
- network: the per-interface data already exists
  (`NetworkInfo::getAllInterfaces()` / `getInterfaceStats()`). `InfoManager`
  gains a thin passthrough to enumerate bindable interfaces and to read a named
  interface's rate; the `NetworkTile` gains an `input` binding and renders the
  bound interface instead of always the default.

### 5. Settings migration

On first v2 load, existing global selections (`TempSensorId`, `FanSensorId`,
`GpuDeviceId`, `DiskName`) are applied to the corresponding default tile's
`input` binding. Those global getters then become legacy: still read for
one-time migration, no longer written. New per-tile bindings live only in the
dashboard layout JSON.

## Data flow (after change)

1. `DataRefreshService` emits a per-domain `*Updated` signal (unchanged
   cadence).
2. The matching handler iterates **all wrappers of that type**, resolves each
   wrapper's bound `input` key to a current reading via `InfoManager`, and calls
   `setValue` on that wrapper's tile.
3. Edit-mode changes (add/remove/move/resize/re-bind) update the in-memory
   wrapper collection and are serialized to the v2 layout JSON on edit-mode
   exit (`exitEditMode`, `:1060`).

## Files touched

- `shared/nexis/Pages/Dashboard/dashboard_page.h` / `.cpp` — grid constants,
  `defaultLayout`, `serializeLayout`/`deserializeLayout` + v1→v2 migration,
  per-type update routing over wrapper collections, add-tile palette wiring,
  display-mode thresholds, removal of `mSelected*Index` fields.
- `shared/nexis/Pages/Dashboard/metric_tile_base.h` — `Compact` enum value.
- Tile subclass `.cpp` files — `Compact` rendering branch:
  `metric_tile`, `gauge_tile`, `ring_tile`, `hybrid_tile`, `speedometer_tile`,
  `vu_meter_tile`, `disk_tile`, `network_tile`, `health_score_tile`.
- New add-tile palette dialog class under
  `shared/nexis/Pages/Dashboard/`.
- `shared/nexis/Managers/info_manager.h` / `.cpp` — network interface
  enumeration + named-interface rate passthrough.
- Theme QSS / `values.ini` — `compactMode` selectors.
- `shared/nexis/Managers/setting_manager.h` — single-select getters marked
  legacy (kept for migration).
- Tests under `tests/` (see below).
- Docs: `CHANGELOG.md`, `docs/APPLICATION_OVERVIEW.md`,
  `docs/ARCHITECTURE_REVIEW.md`.

## Testing

Qt Test (QTest) + CTest. New/updated cases:

- **Layout migration:** a v1 layout (no version field) is scaled ×2 on load and
  clamped to 8×8 bounds; a v2 layout round-trips its `input` field.
- **Display-mode thresholds:** span area → `Compact`/`Normal`/`Large`/`Hero`
  mapping is correct at the new boundaries (1, 4, 8, 16).
- **Multi-instance update routing:** given two wrappers of the same type bound
  to different inputs, each receives the value for its own bound input.
- **Add-tile palette enumeration:** lists detected inputs for a type and
  excludes inputs already placed on the dashboard.

## Documentation updates

- `CHANGELOG.md` — `### Added` (multi-instance input-bound tiles; per-interface
  network tiles; compact tile rendering) and `### Changed` (8×8 grid; per-tile
  sensor binding replaces global selection).
- `docs/APPLICATION_OVERVIEW.md` — denser dashboard, multiple temp/fan/disk/
  GPU/network tiles, add-tile palette, compact tiles; update any tile/feature
  counts.
- `docs/ARCHITECTURE_REVIEW.md` — per-tile input binding (in layout JSON)
  replaces global sensor-selection settings; update routines iterate wrapper
  collections; new `Compact` display tier and rescaled span thresholds.
