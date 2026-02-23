# BUG-62: Dashboard Empty Tile Slot Support — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add empty cell support to the dashboard grid so tiles can be resized into adjacent empty space and dragged to empty cells.

**Architecture:** Fixed 4x4 occupancy grid (`QString mOccupancy[4][4]`) tracks which cells are occupied. Placeholder `QWidget`s fill empty cells in `QGridLayout` to prevent column/row collapse. Pixel-to-cell resolution uses arithmetic instead of tile hit-testing. Manual-clear-first resize policy (no auto-displacement).

**Tech Stack:** C++17, Qt6 (QGridLayout, QWidget), QSS theming

---

### Task 1: Add occupancy grid and helper methods to DashboardPage header

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.h:77-133`

**Step 1: Add grid constants, occupancy array, placeholder list, and new method declarations**

Replace the private section (lines 77–133) with the same content plus these additions:

After line 110 (`bool mKioskMode;`), add:
```cpp
    static const int GRID_ROWS = 4;
    static const int GRID_COLS = 4;
    QString mOccupancy[GRID_ROWS][GRID_COLS];
    QList<QWidget*> mPlaceholders;
```

After the existing `int gridCellAtPos(...)` declaration (line 132), add:
```cpp
    void rebuildOccupancy();
    bool regionIsFree(int row, int col, int rowSpan, int colSpan,
                      const QString &ignoreTileId = QString()) const;
```

Change the `gridCellAtPos` return type from `int` to `bool`:
```cpp
    bool gridCellAtPos(const QPoint &globalPos, int &outRow, int &outCol) const;
```

**Step 2: Build to verify header compiles**

Run: `cmake --build /Users/luke/Documents/GitHub/Nexis/build -j$(sysctl -n hw.ncpu) 2>&1 | tail -10`
Expected: Build succeeds (new methods declared but not yet defined — linker errors are expected at this stage, but we'll define them in the next task).

**Step 3: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_page.h
git commit -m "refactor(dashboard): add occupancy grid declarations to DashboardPage header (BUG-62)"
```

---

### Task 2: Implement rebuildOccupancy() and regionIsFree()

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp`

**Step 1: Add rebuildOccupancy() and regionIsFree() implementations**

Add these two methods before the existing `buildGrid()` method (before line 912):

```cpp
void DashboardPage::rebuildOccupancy()
{
    for (int r = 0; r < GRID_ROWS; ++r)
        for (int c = 0; c < GRID_COLS; ++c)
            mOccupancy[r][c].clear();

    for (const DashboardTileWrapper *w : mTileWrappers) {
        for (int r = w->gridRow(); r < w->gridRow() + w->gridRowSpan(); ++r)
            for (int c = w->gridCol(); c < w->gridCol() + w->gridColSpan(); ++c)
                if (r < GRID_ROWS && c < GRID_COLS)
                    mOccupancy[r][c] = w->tileId();
    }
}

bool DashboardPage::regionIsFree(int row, int col, int rowSpan, int colSpan,
                                  const QString &ignoreTileId) const
{
    if (row + rowSpan > GRID_ROWS || col + colSpan > GRID_COLS)
        return false;
    for (int r = row; r < row + rowSpan; ++r)
        for (int c = col; c < col + colSpan; ++c)
            if (!mOccupancy[r][c].isEmpty() && mOccupancy[r][c] != ignoreTileId)
                return false;
    return true;
}
```

**Step 2: Build to verify compilation**

Run: `cmake --build /Users/luke/Documents/GitHub/Nexis/build -j$(sysctl -n hw.ncpu) 2>&1 | tail -10`
Expected: Build succeeds (methods defined but not yet called).

**Step 3: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_page.cpp
git commit -m "feat(dashboard): implement rebuildOccupancy() and regionIsFree() (BUG-62)"
```

---

### Task 3: Rewrite buildGrid() to add placeholder widgets

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp:912-931`

**Step 1: Replace the existing buildGrid() method**

Replace lines 912–931 with:

```cpp
void DashboardPage::buildGrid()
{
    while (ui->bentoGrid->count() > 0) {
        QLayoutItem *item = ui->bentoGrid->takeAt(0);
        if (item->widget())
            item->widget()->setParent(nullptr);
        delete item;
    }
    qDeleteAll(mPlaceholders);
    mPlaceholders.clear();

    rebuildOccupancy();

    for (DashboardTileWrapper *w : mTileWrappers) {
        w->setParent(this);
        ui->bentoGrid->addWidget(w, w->gridRow(), w->gridCol(),
                                  w->gridRowSpan(), w->gridColSpan());
        applyDisplayModeForSpan(w);
        w->show();
    }

    for (int r = 0; r < GRID_ROWS; ++r) {
        for (int c = 0; c < GRID_COLS; ++c) {
            if (mOccupancy[r][c].isEmpty()) {
                auto *ph = new QWidget(this);
                ph->setObjectName("dashPlaceholder");
                ph->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
                ph->setVisible(mEditMode);
                ui->bentoGrid->addWidget(ph, r, c);
                mPlaceholders.append(ph);
            }
        }
    }

    for (int c = 0; c < GRID_COLS; ++c)
        ui->bentoGrid->setColumnStretch(c, 1);
    for (int r = 0; r < GRID_ROWS; ++r)
        ui->bentoGrid->setRowStretch(r, 1);
}
```

**Step 2: Build and run the app to verify the grid renders**

Run: `cmake --build /Users/luke/Documents/GitHub/Nexis/build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: Build succeeds. Grid should now be 4 rows tall with empty rows visible as blank space.

**Step 3: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_page.cpp
git commit -m "feat(dashboard): add placeholder widgets to buildGrid() for empty cells (BUG-62)"
```

---

### Task 4: Add placeholder QSS rule and show/hide in edit mode

**Files:**
- Modify: `shared/nexis/static/themes/default/style/style.qss` (after `#dragIndicator` rule, around line 867)
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp` (toggleEditMode and exitEditMode)

**Step 1: Add QSS rule for #dashPlaceholder**

Insert after the `#dragIndicator` rule block (after line ~867):

```css
#dashPlaceholder {
    background-color: transparent;
    border: @dp1 dashed @borderColor;
    border-radius: @dp8;
}
```

**Step 2: Add placeholder visibility toggle in toggleEditMode()**

In the `else` branch of `toggleEditMode()` (line 783–791), after the `w->setEditMode(true)` loop, add:

```cpp
        for (QWidget *ph : mPlaceholders)
            ph->setVisible(true);
```

**Step 3: Add placeholder hide in exitEditMode()**

In `exitEditMode()` (line 794–805), after the `w->setEditMode(false)` loop, add:

```cpp
    for (QWidget *ph : mPlaceholders)
        ph->setVisible(false);
```

**Step 4: Build and verify**

Run: `cmake --build /Users/luke/Documents/GitHub/Nexis/build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: Build succeeds. In edit mode, empty cells should show dashed borders. Exiting edit mode hides them.

**Step 5: Commit**

```bash
git add shared/nexis/static/themes/default/style/style.qss shared/nexis/Pages/Dashboard/dashboard_page.cpp
git commit -m "feat(dashboard): add dashed placeholder styling and edit-mode visibility toggle (BUG-62)"
```

---

### Task 5: Rewrite gridCellAtPos() to use arithmetic

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp:958-969`

**Step 1: Replace the existing gridCellAtPos() method**

Replace lines 958–969 with:

```cpp
bool DashboardPage::gridCellAtPos(const QPoint &globalPos, int &outRow, int &outCol) const
{
    QWidget *gridParent = ui->bentoGrid->parentWidget();
    if (!gridParent)
        return false;

    QPoint local = gridParent->mapFromGlobal(globalPos);
    QRect gridRect = ui->bentoGrid->geometry();

    if (!gridRect.contains(local))
        return false;

    int x = local.x() - gridRect.x();
    int y = local.y() - gridRect.y();

    int cellW = gridRect.width() / GRID_COLS;
    int cellH = gridRect.height() / GRID_ROWS;

    if (cellW <= 0 || cellH <= 0)
        return false;

    outCol = qBound(0, x / cellW, GRID_COLS - 1);
    outRow = qBound(0, y / cellH, GRID_ROWS - 1);
    return true;
}
```

**Step 2: Update all call sites that check the old int return value**

The old method returned `int` (1 for found, 0 for not found). The new method returns `bool`. The call sites already use truthiness checks (`if (gridCellAtPos(...))` and `if (!gridCellAtPos(...))`), so `bool` is compatible. Verify that no call site compares against literal `1` or `0`.

Call sites to verify (no changes needed, just verify):
- Line ~984: `if (gridCellAtPos(globalPos, targetRow, targetCol))` — OK
- Line ~1007: `if (!gridCellAtPos(globalPos, targetRow, targetCol))` — OK

**Step 3: Build and verify**

Run: `cmake --build /Users/luke/Documents/GitHub/Nexis/build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: Build succeeds.

**Step 4: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_page.cpp shared/nexis/Pages/Dashboard/dashboard_page.h
git commit -m "refactor(dashboard): rewrite gridCellAtPos() as arithmetic cell resolution (BUG-62)"
```

---

### Task 6: Rewrite onTileDragMoved() to show indicator on empty cells

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp` (onTileDragMoved, ~line 979-996)

**Step 1: Replace onTileDragMoved()**

Replace the existing method with:

```cpp
void DashboardPage::onTileDragMoved(DashboardTileWrapper *wrapper, const QPoint &globalPos)
{
    Q_UNUSED(wrapper)

    int targetRow, targetCol;
    if (!gridCellAtPos(globalPos, targetRow, targetCol)) {
        mDragIndicator->hide();
        return;
    }

    // Don't show indicator on the source tile's own cell
    if (mDragSource && mDragSource->gridRow() == targetRow && mDragSource->gridCol() == targetCol) {
        mDragIndicator->hide();
        return;
    }

    QRect gridRect = ui->bentoGrid->geometry();
    int cellW = gridRect.width() / GRID_COLS;
    int cellH = gridRect.height() / GRID_ROWS;
    int x = gridRect.x() + targetCol * cellW;
    int y = gridRect.y() + targetRow * cellH;
    mDragIndicator->setGeometry(x, y, cellW, cellH);
    mDragIndicator->show();
    mDragIndicator->raise();
}
```

**Step 2: Build and verify**

Run: `cmake --build /Users/luke/Documents/GitHub/Nexis/build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: Build succeeds. In edit mode, dragging a tile should show the orange indicator on both occupied tiles and empty cells.

**Step 3: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_page.cpp
git commit -m "feat(dashboard): show drag indicator on empty cells (BUG-62)"
```

---

### Task 7: Rewrite onTileDragFinished() to support drag-to-empty

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp` (onTileDragFinished, ~line 998-1040)

**Step 1: Replace onTileDragFinished()**

Replace the existing method with:

```cpp
void DashboardPage::onTileDragFinished(DashboardTileWrapper *wrapper, const QPoint &globalPos)
{
    wrapper->setWindowOpacity(1.0);
    mDragIndicator->hide();

    if (!mDragSource)
        return;

    int targetRow, targetCol;
    if (!gridCellAtPos(globalPos, targetRow, targetCol)) {
        mDragSource = nullptr;
        return;
    }

    // Check if the target cell is empty
    if (mOccupancy[targetRow][targetCol].isEmpty()) {
        int srcRS = mDragSource->gridRowSpan();
        int srcCS = mDragSource->gridColSpan();
        if (regionIsFree(targetRow, targetCol, srcRS, srcCS, mDragSource->tileId())) {
            mDragSource->setGridPosition(targetRow, targetCol, srcRS, srcCS);
            buildGrid();
        }
    } else {
        // Occupied cell — swap with the target tile (existing behavior)
        DashboardTileWrapper *target = nullptr;
        for (DashboardTileWrapper *w : mTileWrappers) {
            if (w->gridRow() == targetRow && w->gridCol() == targetCol && w != mDragSource) {
                target = w;
                break;
            }
        }
        if (target) {
            int srcRow = mDragSource->gridRow(), srcCol = mDragSource->gridCol();
            int srcRS = mDragSource->gridRowSpan(), srcCS = mDragSource->gridColSpan();
            int tgtRow = target->gridRow(), tgtCol = target->gridCol();
            int tgtRS = target->gridRowSpan(), tgtCS = target->gridColSpan();

            bool srcFitsAtTarget = (tgtCol + srcCS <= GRID_COLS);
            bool tgtFitsAtSource = (srcCol + tgtCS <= GRID_COLS);

            if (srcFitsAtTarget && tgtFitsAtSource) {
                mDragSource->setGridPosition(tgtRow, tgtCol, srcRS, srcCS);
                target->setGridPosition(srcRow, srcCol, tgtRS, tgtCS);
                buildGrid();
            }
        }
    }

    mDragSource = nullptr;
}
```

**Step 2: Build and verify**

Run: `cmake --build /Users/luke/Documents/GitHub/Nexis/build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: Build succeeds. In edit mode, dragging a tile to an empty cell should move it there.

**Step 3: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_page.cpp
git commit -m "feat(dashboard): support drag-to-empty cell movement (BUG-62)"
```

---

### Task 8: Simplify onTileResizeRequested() to use regionIsFree()

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp` (onTileResizeRequested, ~line 1042-1066)

**Step 1: Replace onTileResizeRequested()**

Replace the existing method with:

```cpp
void DashboardPage::onTileResizeRequested(DashboardTileWrapper *wrapper, int newColSpan, int newRowSpan)
{
    int row = wrapper->gridRow();
    int col = wrapper->gridCol();

    if (!regionIsFree(row, col, newRowSpan, newColSpan, wrapper->tileId()))
        return;

    wrapper->setGridPosition(row, col, newRowSpan, newColSpan);
    buildGrid();
}
```

**Step 2: Build and verify**

Run: `cmake --build /Users/luke/Documents/GitHub/Nexis/build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: Build succeeds. Tiles can now be resized into adjacent empty cells.

**Step 3: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_page.cpp
git commit -m "refactor(dashboard): simplify resize check to use regionIsFree() (BUG-62)"
```

---

### Task 9: Add bounds clamping to deserializeLayout()

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp` (deserializeLayout, ~line 875-895)

**Step 1: Add bounds clamping**

Replace lines 883–886 inside the for loop with:

```cpp
        int row = qBound(0, obj["row"].toInt(), GRID_ROWS - 1);
        int col = qBound(0, obj["col"].toInt(), GRID_COLS - 1);
        int rowSpan = qBound(1, obj["rowSpan"].toInt(1), GRID_ROWS - row);
        int colSpan = qBound(1, obj["colSpan"].toInt(1), GRID_COLS - col);
```

**Step 2: Build and verify**

Run: `cmake --build /Users/luke/Documents/GitHub/Nexis/build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: Build succeeds.

**Step 3: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_page.cpp
git commit -m "fix(dashboard): add bounds clamping to layout deserialization (BUG-62)"
```

---

### Task 10: Update swap bounds checks to use GRID_COLS constant

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp`

**Step 1: Verify all hardcoded `4` references are replaced with GRID_COLS**

Search `dashboard_page.cpp` for any remaining hardcoded `4` used as a grid column bound. The existing code had:
- `col + newColSpan > 4` in `onTileResizeRequested()` — already removed in Task 8
- `tgtCol + srcCS <= 4` and `srcCol + tgtCS <= 4` in `onTileDragFinished()` — already uses `GRID_COLS` from Task 7

No changes should be needed if Tasks 7 and 8 were applied correctly. Verify with a grep.

Run: `grep -n 'col.*<= 4\|col.*> 4\|< 4\b' shared/nexis/Pages/Dashboard/dashboard_page.cpp`
Expected: No matches (all replaced with `GRID_COLS`).

**Step 2: Commit (skip if no changes needed)**

---

### Task 11: Update BUGS.md and clean up

**Files:**
- Modify: `BUGS.md`

**Step 1: Mark BUG-62 as resolved**

Change the BUG-62 entry from `[ ]` to `[x]` and add a `**Resolved:**` line summarizing: Added 4x4 occupancy grid, placeholder widgets for empty cells, arithmetic cell resolution, drag-to-empty support, and regionIsFree()-based resize validation. Include the final commit hash.

**Step 2: Build and run full test suite**

Run: `cmake --build /Users/luke/Documents/GitHub/Nexis/build -j$(sysctl -n hw.ncpu) && ctest --test-dir /Users/luke/Documents/GitHub/Nexis/build --output-on-failure 2>&1 | tail -15`
Expected: Build succeeds. ThemeTokenTests may still fail (pre-existing, unrelated to BUG-62). ScreenshotTests will need new reference images (pre-existing issue from FR-50/51 dashboard redesign, unrelated).

**Step 3: Final commit and push**

```bash
git add BUGS.md
git commit -m "fix(dashboard): resolve BUG-62 — empty tile slot support for resize and drag"
git push
```

---

## Manual QA Checklist

After implementation, verify these behaviors manually:

1. **Normal mode:** Grid is 4 rows tall. Empty rows are visible but placeholders are hidden.
2. **Edit mode (Ctrl+E):** Empty cells show dashed border outlines.
3. **Drag to empty:** Drag a tile from row 0 to an empty cell in row 2. It should move.
4. **Drag swap:** Drag a tile onto another tile. They should swap positions.
5. **Resize into empty:** Drag the resize handle of a tile adjacent to empty space. It should expand.
6. **Resize blocked:** Try to resize a tile into an occupied cell. It should be rejected (no change).
7. **Reset Layout:** Click Reset Layout. All tiles return to the default compact arrangement.
8. **Persistence:** Rearrange tiles, exit edit mode, restart the app. Layout is preserved.
9. **Kiosk mode:** Enter kiosk mode (F11). Edit mode should be disabled. Grid should still work.
