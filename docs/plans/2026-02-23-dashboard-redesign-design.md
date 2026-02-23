# Dashboard Redesign: Split Tiles + Customizable Layout

**Date:** 2026-02-23
**Approach:** Enhanced QGridLayout (Approach A)

## 1. Split CPU and Memory Tiles

Remove the `HeroCard` wrapper. CPU and Memory become independent `MetricTile` widgets placed directly in the bentoGrid as equal peers.

- `HeroCard` class (`hero_card.h`, `hero_card.cpp`) is removed
- QSS rules for `#heroCard` and `#heroCardDivider` are removed
- Both tiles start in `Normal` display mode (18pt) — no more Hero/Large default distinction
- Signal/slot connections to `onCpuUpdated` and `onMemoryUpdated` are unchanged
- Data flow from `DataRefreshService` is untouched

**Default grid after split:**
```
Row 0:  CPU  |  Memory  |  Disk  |  Network
Row 1:  GPU* |  Temp*   |  Battery*  |  (stretch)
```

## 2. Edit Mode Toggle

### Entering/exiting
- Pencil/grid icon button in the top-right corner, to the left of the kiosk button
- Click to enter edit mode; click "Done" button or press `Ctrl+E` to exit and save
- `bool mEditMode` flag on `DashboardPage`

### Mutual exclusion with kiosk mode
- When **edit mode is active**: kiosk button is hidden, kiosk shortcut is disabled
- When **kiosk mode is active**: edit button is hidden, edit shortcut is disabled
- Neither mode can be entered while the other is active

### Visual indicators in edit mode
- Tiles get a dashed border overlay (replaces solid `@borderColor`)
- Thin toolbar at the top: "Customize Layout" label, "Reset Layout" button, "Done" button
- Cursor changes to grab/move when hovering tiles
- Resize grip appears at bottom-right corner of each tile

### When NOT in edit mode
- Dashboard behaves exactly as today — no drag handles, no resize grips
- The edit button is the only new visible element

## 3. Drag-and-Drop Reordering

- Press and hold on a tile initiates drag after ~5px movement threshold
- Dragged tile becomes semi-transparent (opacity ~0.5) and follows cursor
- Drop indicator highlights the target grid cell(s)
- Releasing snaps the tile into target position
- **Swap model:** dragging tile A onto tile B swaps their positions (including spans)
- Drop rejected if a tile's span doesn't fit at the target position (tile returns to original)
- Hidden tiles (unavailable hardware) are excluded from the grid entirely

### Grid constraints
- Fixed 4 columns
- Dynamic row count based on tile count and spans
- Minimum tile: 1x1, maximum tile: 2x2

## 4. Snap-to-Grid Resizing

- Small triangular resize grip at bottom-right of each tile (edit mode only)
- Drag grip to expand/contract in grid-cell increments
- Ghost outline shows proposed span before release
- Valid spans: 1x1, 1x2, 2x1, 2x2

### Collision handling
- Expanding into an occupied cell is rejected (ghost outline turns red/dashed, snaps back)
- No automatic pushing/reflowing of neighbors — user must move tiles out of the way first

### Content adaptation by span
- `MetricTile` display mode mapping: 1x1 → Normal (18pt), 1x2/2x1 → Large (30pt), 2x2 → Hero (36pt)
- `DiskTile` and `NetworkTile` scale content proportionally via `resizeEvent()`
- Donut chart, sparklines, and font sizes respond to allocated size

## 5. Layout Persistence

### Storage
- `settings.ini` under `[DashboardLayout]` group
- Single JSON string key `TileLayout`:
```json
[
  {"id": "cpu", "row": 0, "col": 0, "rowSpan": 1, "colSpan": 1},
  {"id": "memory", "row": 0, "col": 1, "rowSpan": 1, "colSpan": 1},
  {"id": "disk", "row": 0, "col": 2, "rowSpan": 1, "colSpan": 1},
  {"id": "network", "row": 0, "col": 3, "rowSpan": 1, "colSpan": 1},
  {"id": "gpu", "row": 1, "col": 0, "rowSpan": 1, "colSpan": 1},
  {"id": "temp", "row": 1, "col": 1, "rowSpan": 1, "colSpan": 1},
  {"id": "battery", "row": 1, "col": 2, "rowSpan": 1, "colSpan": 1}
]
```

### Save/load behavior
- Saved on exiting edit mode (not on every drag/resize)
- Loaded during `DashboardPage::init()` before adding tiles to grid
- Tiles referenced in layout but unavailable on this system are skipped
- Tiles present on system but missing from saved layout are appended to next available position

### SettingManager additions
- `QString getDashboardLayout()` — returns JSON string (empty if not set)
- `void setDashboardLayout(const QString &json)` — writes JSON string
- `void clearDashboardLayout()` — removes key (triggers default)

## 6. Reset Layout

### In edit mode toolbar
- "Reset Layout" button clears saved layout and rebuilds default grid immediately

### In Settings page
- New "Dashboard" group with "Reset Dashboard Layout" button
- Description label: "Restore the default tile arrangement"
- Button disabled if no custom layout is saved
- Clicking calls `SettingManager::clearDashboardLayout()` and emits signal

### Signal flow
- `SignalMapper` emits `sigDashboardLayoutReset`
- `DashboardPage` connects to signal and rebuilds layout
- Settings and Dashboard remain decoupled
