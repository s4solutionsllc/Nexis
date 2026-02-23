# BUG-62: Dashboard Grid Empty Tile Slot Support

**Date:** 2026-02-23
**Status:** Approved
**Approach:** Occupancy Grid + Placeholder Widgets

## Problem

The FR-51 dashboard layout system has no concept of empty grid cells. Every cell is occupied by a real tile widget. This blocks resize (collision check rejects all occupied expansions) and drag-to-empty (gridCellAtPos cannot resolve empty space). The default 4-tile row 0 is fully packed with zero room for expansion.

## Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Tile displacement on resize | Manual clear first | Simplest model — no auto-reflow surprises. User drags blocking tiles away before resizing. |
| Grid dimensions | Fixed 4x4 (16 cells) | 7 tiles + 9 empty cells. Plenty of room for resize and rearrangement. |
| Empty cell visuals | Dashed border in edit mode | Visible drop/resize targets during editing. Invisible in normal mode. |

## Architecture

### 1. Occupancy Grid Data Model

Add a fixed 4x4 occupancy grid to `DashboardPage`:

```cpp
// dashboard_page.h
static const int GRID_ROWS = 4;
static const int GRID_COLS = 4;
QString mOccupancy[GRID_ROWS][GRID_COLS];  // tileId or "" for empty
```

`rebuildOccupancy()` clears the grid then marks cells occupied by iterating `mTileWrappers`. Called at the start of `buildGrid()`.

`regionIsFree(row, col, rowSpan, colSpan, ignoreTileId)` checks if a rectangular region is available, ignoring the requesting tile's own cells. Replaces the O(n*span) tile iteration in `onTileResizeRequested()` with an O(span) array lookup.

### 2. Placeholder Widgets in buildGrid()

After placing real tiles, `buildGrid()` fills every empty cell with an invisible placeholder widget (`QWidget` with objectName `dashPlaceholder`). Placeholders:

- Prevent `QGridLayout` from collapsing empty rows/columns
- Are invisible in normal mode but still occupy layout space
- Show dashed borders in edit mode via QSS
- Stored in `QList<QWidget*> mPlaceholders`, cleaned up on each `buildGrid()` call

Row stretch is set for all 4 rows (not just columns) to ensure equal cell sizes.

`toggleEditMode()` and `exitEditMode()` show/hide placeholders.

QSS rule:
```css
#dashPlaceholder {
    background-color: transparent;
    border: 1px dashed @borderColor;
    border-radius: 8px;
}
```

### 3. Pixel-to-Cell Resolution

Replace `gridCellAtPos()` with arithmetic computation:

- Map global position to local grid coordinates
- Compute row/col from pixel position: `col = x / cellWidth`, `row = y / cellHeight`
- Works on any cell — occupied or empty
- Return type changes from `int` to `bool`

### 4. Drag-to-Empty Support

`onTileDragFinished()` handles two cases:

1. **Drop on empty cell:** Check `regionIsFree()` for the source tile's span at the target position. If free, move the tile there.
2. **Drop on occupied cell:** Swap positions with the target tile (existing behavior).

`onTileDragMoved()` shows the drag indicator on both occupied and empty cells, computing visual rect from grid geometry arithmetic.

### 5. Resize Updates

`onTileResizeRequested()` simplified to a single `regionIsFree()` call. No tile displacement — resize only succeeds if target cells are already empty (manual clear first policy).

### 6. Default Layout

Row 0: 4 tiles packed (cpu, memory, disk, network) — unchanged.
Row 1: optional tiles (gpu, temp, battery) — unchanged.
Rows 2-3: entirely empty — available for user arrangement.

### 7. Serialization

No format changes. JSON stores only real tile entries. Empty cells inferred by absence. Grid dimensions are constants (4x4), not serialized.

Bounds clamping added to `deserializeLayout()` to prevent corrupted settings from placing tiles outside the grid.

## Files Changed

| File | Changes |
|------|---------|
| `dashboard_page.h` | Add `GRID_ROWS`/`GRID_COLS` constants, `mOccupancy[4][4]`, `mPlaceholders`, `rebuildOccupancy()`, `regionIsFree()`. Change `gridCellAtPos()` return type to `bool`. |
| `dashboard_page.cpp` | Rewrite `buildGrid()`, `gridCellAtPos()`, `onTileDragMoved()`, `onTileDragFinished()`, `onTileResizeRequested()`. Add bounds clamping in `deserializeLayout()`. Update edit mode toggle to show/hide placeholders. |
| `style.qss` | Add `#dashPlaceholder` dashed border rule. |

## Files NOT Changed

- `DashboardTileWrapper` — no changes needed
- Serialization JSON format — no schema change
- Tile types (MetricTile, DiskTile, NetworkTile) — untouched
- Kiosk mode, reset layout — work as before

## User-Visible Behavior Changes

1. Grid is 4 rows tall instead of collapsing to 1-2 rows
2. Empty cells show dashed borders in edit mode
3. Tiles can be dragged to empty cells (move, not just swap)
4. Tiles can be resized into adjacent empty cells
5. Reset Layout restores the original compact arrangement
