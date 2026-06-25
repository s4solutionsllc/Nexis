# Dashboard Plan A2 — Fixed-Cell Responsive Grid Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax.
>
> **Supersedes the grid model of Plan A** (`2026-06-24-dashboard-A-grid-and-migration.md`). Plan A is merged; A2 reuses its `DashboardLayout` helper, v2 envelope, `tierForArea`, and 2×2 defaults, and **rewrites** the grid rendering, occupancy, hit-testing, and adds responsive columns + reflow + vertical scroll. Builds on top of Plan A's commits.

**Goal:** Replace the proportional stretch grid (which produced uneven rows and sparse gaps) with **fixed-size cells (120×98px, 10px gap)** and a **responsive column count** that adapts to window width, with vertical scrolling and automatic reflow of tiles when the column count changes.

**Architecture:** Cells are a fixed pixel size; the column count is `clamp(floor((panelWidth+gap)/pitch), 4, 16)`, recomputed on resize. The fixed `mOccupancy[8][8]` array becomes a dynamic `rowCount × visibleCols` structure. The bento grid is re-parented into a vertical `QScrollArea`. Pure helpers (`columnsForWidth`, `reflow`) live in `DashboardLayout` and are unit-tested; the widget integration (fixed-size placement, scroll wrapping, fixed-pitch hit-testing) lives in `DashboardPage`.

**Tech Stack:** C++17, Qt6 (QtWidgets, QScrollArea, QGridLayout), Qt Test.

## Global Constraints

- GPL-3.0-only; no new third-party dependencies; all code under `shared/` (cross-platform).
- No hardcoded hex colors in C++ (BUG-47) — no color work here, don't introduce any.
- **Fixed geometry (exact values):** cell 120×98px, gap 10px, pitch 130×108px, MIN cols 4, MAX cols 16.
- **Responsive:** `visibleCols = clamp(floor((panelWidth + gap) / (kCellW + kGap)), kMinCols, kMaxCols)`.
- **Reflow:** on load and on any resize that changes `visibleCols`, clamp each tile's colSpan to ≤ visibleCols and repack in input order (row-major, first free region); persist the repacked positions. No per-width layout memory.
- **Vertical scroll only**; tiles never shrink. Horizontal scroll only when window < MIN-cols width.
- Build/test commands per Plan A. Conventional commits, ≤72 chars, `GH#191`, branch `claude/gh191-dashboard-tiles` only.
- Docs deferred to Plan C.

## File Structure

- **Modify:** `shared/nexis/Pages/Dashboard/dashboard_layout_util.h` / `.cpp` — replace `kGridRows/kGridCols` with cell geometry + `kMinCols/kMaxCols`; add `columnsForWidth()` and `reflow()`; adjust `migrate()` clamp target.
- **Modify:** `tests/managers/test_dashboard_layout_util.cpp` — tests for `columnsForWidth` and `reflow`; fix any test referencing removed constants.
- **Modify:** `shared/nexis/Pages/Dashboard/dashboard_page.h` / `.cpp` — dynamic occupancy + `mVisibleCols`/`mRowCount`; scroll-area wrapping; fixed-size `buildGrid`; fixed-pitch hit-testing; responsive recompute + reflow on resize/load.

## Reused from Plan A (do not re-implement)

`tierForArea`, the v2 envelope (`layoutEnvelope`/`persistLayout`), envelope-aware `deserializeLayout`/style-pre-extract, and the 2×2 `defaultLayout`. These stay; this plan changes how those tiles are sized/placed/scrolled.

---

### Task 1: Helper — cell geometry, `columnsForWidth`, `reflow`; adjust `migrate`

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_layout_util.h` / `.cpp`
- Test: `tests/managers/test_dashboard_layout_util.cpp`

**Interfaces:**
- Removes: `kGridRows`, `kGridCols`, `kLegacyGridRows` (no longer meaningful).
- Adds: `kCellW=120, kCellH=98, kGap=10, kMinCols=4, kMaxCols=16, kLegacyScale=2`. Keeps `kSchemaVersion=2`.
- Adds: `int columnsForWidth(int panelWidth);` → clamped responsive column count.
- Adds: `QJsonArray reflow(const QJsonArray &tiles, int cols);` → tiles repacked row-major into the first free region of a `cols`-wide grid, colSpan clamped to ≤cols, rowSpan ≥1; `row`/`col`/`rowSpan`/`colSpan` rewritten.
- Changes: `migrate()` scales by `kLegacyScale` (was `kGridCols/kLegacyGridCols`) and clamps `col` to `[0, kMaxCols-1]` (no row upper bound; reflow handles fit).

- [ ] **Step 1: Write the failing tests** — replace the constants used and add cases in `test_dashboard_layout_util.cpp`:

```cpp
    void columnsForWidth_clampsAndDivides()
    {
        QCOMPARE(columnsForWidth(1680), 13); // floor((1680+10)/130)=13
        QCOMPARE(columnsForWidth(100), kMinCols);   // tiny window -> MIN(4)
        QCOMPARE(columnsForWidth(100000), kMaxCols); // huge -> MAX(16)
    }

    void reflow_clampsColSpanToCols()
    {
        QJsonObject t; t["id"]="disk"; t["row"]=0; t["col"]=0; t["rowSpan"]=2; t["colSpan"]=4;
        QJsonArray in; in.append(t);
        QJsonArray out = reflow(in, 3);            // only 3 columns available
        QJsonObject o = out.at(0).toObject();
        QCOMPARE(o["colSpan"].toInt(), 3);          // clamped to cols
        QCOMPARE(o["col"].toInt(), 0);
        QCOMPARE(o["row"].toInt(), 0);
    }

    void reflow_repacksWithoutOverlap()
    {
        // Two 2x2 tiles that both claim (0,0) in a 4-col grid: second must move.
        auto mk=[](const QString &id,int r,int c){ QJsonObject o; o["id"]=id; o["row"]=r; o["col"]=c; o["rowSpan"]=2; o["colSpan"]=2; return o; };
        QJsonArray in; in.append(mk("a",0,0)); in.append(mk("b",0,0));
        QJsonArray out = reflow(in, 4);
        QJsonObject a=out.at(0).toObject(), b=out.at(1).toObject();
        QCOMPARE(a["row"].toInt(),0); QCOMPARE(a["col"].toInt(),0);
        QCOMPARE(b["row"].toInt(),0); QCOMPARE(b["col"].toInt(),2); // packed to the right
    }

    void reflow_wrapsToNextRowWhenWidthExhausted()
    {
        auto mk=[](const QString &id){ QJsonObject o; o["id"]=id; o["row"]=0; o["col"]=0; o["rowSpan"]=2; o["colSpan"]=2; return o; };
        QJsonArray in; for (const char *id : {"a","b","c"}) in.append(mk(id));
        QJsonArray out = reflow(in, 4); // 4 cols -> two 2x2 per row
        QCOMPARE(out.at(0).toObject()["col"].toInt(), 0);
        QCOMPARE(out.at(1).toObject()["col"].toInt(), 2);
        QCOMPARE(out.at(2).toObject()["row"].toInt(), 2); // wrapped to next row
        QCOMPARE(out.at(2).toObject()["col"].toInt(), 0);
    }
```

Keep the existing `tierForArea_*` and `migrate_*` tests. The `migrate_v1_clampsToBounds` case (row/col 3 → 6, span 2) still holds under the new clamp (`col` ≤ 15, no row bound) — verify it still passes; if it asserted an 8-bound it does not here, but the expected values (6,6,2,2) are unchanged.

- [ ] **Step 2: Run to verify failure** — `cmake --build build --target test-DashboardLayoutUtilTests` → FAIL (undeclared `columnsForWidth`/`reflow`/`kMinCols`).

- [ ] **Step 3: Update the header** — in `dashboard_layout_util.h`, replace the grid-constant block:

```cpp
// Fixed bento cell geometry (GH#191 revised model — fixed cells, responsive
// columns). A 1x1 cell is kCellW x kCellH with kGap between cells.
inline constexpr int kCellW = 120;
inline constexpr int kCellH = 98;
inline constexpr int kGap = 10;

// Responsive column clamp.
inline constexpr int kMinCols = 4;
inline constexpr int kMaxCols = 16;

// Legacy (v1, 4x4) layouts scale by this factor so an old 1x1 becomes a 2x2.
inline constexpr int kLegacyScale = 2;

inline constexpr int kSchemaVersion = 2;
```

(Delete `kGridRows`, `kGridCols`, `kLegacyGridRows`, `kLegacyGridCols`.) Keep `enum DisplayTier`/`tierForArea`/`migrate` declarations; add:

```cpp
// Responsive visible column count for a given panel (viewport) width.
int columnsForWidth(int panelWidth);

// Repacks tiles row-major into the first free region of a `cols`-wide grid:
// colSpan is clamped to <= cols, rowSpan >= 1, and row/col are rewritten so no
// two tiles overlap and tiles fill from the top-left. Input order is preserved
// as packing priority. Rows are unbounded.
QJsonArray reflow(const QJsonArray &tiles, int cols);
```

- [ ] **Step 4: Update the implementation** — in `dashboard_layout_util.cpp`:

```cpp
#include <QSet>

int columnsForWidth(int panelWidth)
{
    int pitch = kCellW + kGap;
    int cols = (panelWidth + kGap) / pitch;
    return std::clamp(cols, kMinCols, kMaxCols);
}

QJsonArray reflow(const QJsonArray &tiles, int cols)
{
    cols = std::max(1, cols);
    QSet<qint64> occ;
    auto key = [cols](int r, int c) { return static_cast<qint64>(r) * cols + c; };
    auto isFree = [&](int r, int c, int rs, int cs) {
        for (int rr = r; rr < r + rs; ++rr)
            for (int cc = c; cc < c + cs; ++cc)
                if (occ.contains(key(rr, cc))) return false;
        return true;
    };
    auto mark = [&](int r, int c, int rs, int cs) {
        for (int rr = r; rr < r + rs; ++rr)
            for (int cc = c; cc < c + cs; ++cc)
                occ.insert(key(rr, cc));
    };

    QJsonArray out;
    for (const QJsonValue &v : tiles) {
        QJsonObject o = v.toObject();
        int rs = std::max(1, o.value("rowSpan").toInt(1));
        int cs = std::clamp(o.value("colSpan").toInt(1), 1, cols);
        int pr = -1, pc = -1;
        for (int r = 0; pr < 0; ++r)                  // always terminates: an
            for (int c = 0; c <= cols - cs; ++c)      // empty row always fits
                if (isFree(r, c, rs, cs)) { pr = r; pc = c; break; }
        mark(pr, pc, rs, cs);
        o["row"] = pr; o["col"] = pc; o["rowSpan"] = rs; o["colSpan"] = cs;
        out.append(o);
    }
    return out;
}
```

And in `migrate()`, change the scale + clamp:

```cpp
    constexpr int scale = kLegacyScale;             // was kGridCols / kLegacyGridCols
    // ...
    int row = std::max(0, obj.value("row").toInt() * scale);
    int col = std::clamp(obj.value("col").toInt() * scale, 0, kMaxCols - 1);
    int rowSpan = std::max(1, obj.value("rowSpan").toInt(1) * scale);
    int colSpan = std::clamp(obj.value("colSpan").toInt(1) * scale, 1, kMaxCols - col);
```

- [ ] **Step 5: Run tests** — `ctest --test-dir build -R DashboardLayoutUtilTests --output-on-failure` → PASS (all old + new cases).

- [ ] **Step 6: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_layout_util.h \
        shared/nexis/Pages/Dashboard/dashboard_layout_util.cpp \
        tests/managers/test_dashboard_layout_util.cpp
git commit -m "feat(dashboard): cell geometry + columnsForWidth + reflow helpers (GH#191)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

> NOTE for the implementer: after this commit `dashboard_page.cpp`/`.h` will NOT compile (they still reference `DashboardLayout::kGridRows`/`kGridCols` via `GRID_ROWS`/`GRID_COLS`). That is expected — Task 2 fixes the page. Do not try to make the app build at the end of Task 1; only the `DashboardLayoutUtilTests` target must build and pass. Verify with `--target test-DashboardLayoutUtilTests`.

---

### Task 2: Fixed-size cells + vertical scroll + dynamic occupancy

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.h` / `.cpp`

**Interfaces:**
- Consumes: `kCellW/kCellH/kGap/kMinCols/kMaxCols`, `columnsForWidth`, `reflow`.
- Produces: dynamic occupancy + `int mVisibleCols`, `int mRowCount`; a `QScrollArea` wrapping the grid; `buildGrid` placing fixed-size tiles with no stretch.

This task makes cells fixed-size and scrollable at a **fixed** column count first (use `mVisibleCols = kMaxCols` as a constant placeholder); Task 3 makes `mVisibleCols` responsive. Splitting this way keeps each task independently buildable and reviewable.

- [ ] **Step 1: Replace the static grid constants with dynamic members** — in `dashboard_page.h`, replace:

```cpp
    static const int GRID_ROWS = DashboardLayout::kGridRows;
    static const int GRID_COLS = DashboardLayout::kGridCols;
    QString mOccupancy[GRID_ROWS][GRID_COLS];
```

with:

```cpp
    int mVisibleCols = DashboardLayout::kMaxCols;   // responsive in Task 3
    int mRowCount = 0;                              // grows to fit placed tiles
    QVector<QVector<QString>> mOccupancy;           // [row][col] -> tile uid/id
    QScrollArea *mGridScroll = nullptr;
    QWidget *mGridContainer = nullptr;              // holds bentoGrid; scrolled
```

Add `#include <QScrollArea>` and `#include <QVector>`. Every remaining `GRID_ROWS`/`GRID_COLS` reference in the `.cpp` becomes `mRowCount`/`mVisibleCols` (the sweep is Steps 3-5).

- [ ] **Step 2: Wrap the grid in a vertical scroll area** — the `.ui` declares `bentoGrid` (a `QGridLayout`) directly inside `mainLayout`. In `init()`, after `ui->setupUi(this)` runs, re-parent the grid into a container inside a `QScrollArea`:

```cpp
    // GH#191: host the bento grid in a vertical scroll area so fixed-size tiles
    // overflow downward instead of compressing.
    mGridContainer = new QWidget;
    mGridContainer->setObjectName("bentoGridContainer");
    mGridContainer->setStyleSheet("#bentoGridContainer{background:transparent;}");
    mGridContainer->setLayout(ui->bentoGrid);          // steals the layout from the .ui parent

    mGridScroll = new QScrollArea(this);
    mGridScroll->setObjectName("bentoScroll");
    mGridScroll->setWidgetResizable(true);
    mGridScroll->setFrameShape(QFrame::NoFrame);
    mGridScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    mGridScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    mGridScroll->setStyleSheet("QScrollArea{background:transparent;} QScrollArea>QWidget>QWidget{background:transparent;}");
    mGridScroll->setWidget(mGridContainer);
```

Then insert `mGridScroll` into `mainLayout` at the position the bento grid used to occupy. Find that index (the grid was added before `widgetUpdateBar`/`systemSummary`/`statusFooter`). Use `ui->mainLayout->insertWidget(<index>, mGridScroll)` and keep `ui->mainLayout->setStretchFactor(mGridScroll, 1)` (replacing the old `setStretchFactor(ui->bentoGrid, 1)` at `:160`). Confirm the index by reading the `.ui` child order.

(Per the project Qt gotcha for QScrollArea in programmatic containers, the transparent background stylesheets above are required so the viewport uses the QSS theme, not the system palette.)

- [ ] **Step 3: Rewrite `buildGrid()` for fixed-size cells** — replace the stretch-based sizing with fixed cell dimensions and a trailing spacer so tiles pack top-left:

```cpp
void DashboardPage::buildGrid()
{
    while (ui->bentoGrid->count() > 0) {
        QLayoutItem *item = ui->bentoGrid->takeAt(0);
        if (item->widget()) item->widget()->setParent(nullptr);
        delete item;
    }
    qDeleteAll(mPlaceholders);
    mPlaceholders.clear();

    rebuildOccupancy();   // sizes mOccupancy to mRowCount x mVisibleCols

    for (DashboardTileWrapper *w : mTileWrappers) {
        if (mHiddenTiles.contains(w->tileId())) { w->hide(); continue; }
        w->setParent(mGridContainer);
        w->setFixedSize(w->gridColSpan() * DashboardLayout::kCellW + (w->gridColSpan() - 1) * DashboardLayout::kGap,
                        w->gridRowSpan() * DashboardLayout::kCellH + (w->gridRowSpan() - 1) * DashboardLayout::kGap);
        ui->bentoGrid->addWidget(w, w->gridRow(), w->gridCol(), w->gridRowSpan(), w->gridColSpan());
        applyDisplayModeForSpan(w);
        w->show();
    }

    // Edit-mode placeholders for every empty cell (fixed-size too).
    for (int r = 0; r < mRowCount; ++r)
        for (int c = 0; c < mVisibleCols; ++c)
            if (mOccupancy[r][c].isEmpty()) {
                auto *ph = new QWidget(mGridContainer);
                ph->setObjectName("dashPlaceholder");
                ph->setFixedSize(DashboardLayout::kCellW, DashboardLayout::kCellH);
                ph->setVisible(mEditMode);
                ui->bentoGrid->addWidget(ph, r, c);
                mPlaceholders.append(ph);
            }

    // Fixed cell pitch: each used column/row gets the exact cell size; a
    // trailing stretch column/row absorbs extra space so tiles pack top-left.
    ui->bentoGrid->setHorizontalSpacing(DashboardLayout::kGap);
    ui->bentoGrid->setVerticalSpacing(DashboardLayout::kGap);
    for (int c = 0; c < mVisibleCols; ++c) {
        ui->bentoGrid->setColumnMinimumWidth(c, DashboardLayout::kCellW);
        ui->bentoGrid->setColumnStretch(c, 0);
    }
    for (int r = 0; r < mRowCount; ++r) {
        ui->bentoGrid->setRowMinimumHeight(r, DashboardLayout::kCellH);
        ui->bentoGrid->setRowStretch(r, 0);
    }
    ui->bentoGrid->setColumnStretch(mVisibleCols, 1);   // trailing spacer col
    ui->bentoGrid->setRowStretch(mRowCount, 1);          // trailing spacer row

    mEditButton->raise();
    mKioskButton->raise();
}
```

Note: a tile's wrapper is given an explicit `setFixedSize` so it doesn't stretch even if a future layout tweak adds stretch. `setColumnMinimumWidth`+`stretch 0` makes occupied columns exactly one cell wide.

- [ ] **Step 4: Make `rebuildOccupancy()` size the dynamic grid** — replace its fixed loops:

```cpp
void DashboardPage::rebuildOccupancy()
{
    // Row count grows to fit the lowest-reaching visible tile (min 1 row).
    mRowCount = 0;
    for (const DashboardTileWrapper *w : mTileWrappers) {
        if (mHiddenTiles.contains(w->tileId())) continue;
        mRowCount = std::max(mRowCount, w->gridRow() + w->gridRowSpan());
    }
    mRowCount = std::max(mRowCount, 1);

    mOccupancy.assign(mRowCount, QVector<QString>(mVisibleCols));
    for (const DashboardTileWrapper *w : mTileWrappers) {
        if (mHiddenTiles.contains(w->tileId())) continue;
        for (int r = w->gridRow(); r < w->gridRow() + w->gridRowSpan(); ++r)
            for (int c = w->gridCol(); c < w->gridCol() + w->gridColSpan(); ++c)
                if (r < mRowCount && c < mVisibleCols)
                    mOccupancy[r][c] = w->tileId();
    }
}
```

(`QVector::assign(n, value)` is available in Qt6. If preferred, use `mOccupancy = QVector<QVector<QString>>(mRowCount, QVector<QString>(mVisibleCols));`.)

- [ ] **Step 5: Update `regionIsFree()` to the dynamic bounds**:

```cpp
bool DashboardPage::regionIsFree(int row, int col, int rowSpan, int colSpan,
                                 const QString &ignoreTileId) const
{
    if (col + colSpan > mVisibleCols || row < 0 || col < 0)
        return false;
    for (int r = row; r < row + rowSpan; ++r)
        for (int c = col; c < col + colSpan; ++c) {
            if (r >= mOccupancy.size() || c >= mVisibleCols) continue; // beyond current rows is free
            if (!mOccupancy[r][c].isEmpty() && mOccupancy[r][c] != ignoreTileId)
                return false;
        }
    return true;
}
```

(Rows below the current `mRowCount` are implicitly free — a tile can extend the grid downward.)

- [ ] **Step 6: Build the app + run the full suite**

Run: `cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu) && ctest --test-dir build --output-on-failure`
Expected: clean build; `DashboardLayoutUtilTests` pass. Launch the app: tiles now render at a uniform fixed size (no stretching); with `mVisibleCols=kMaxCols=16` the grid is wide and scrolls vertically. (Responsive column count + reflow come in Task 3, so right now it always uses 16 columns — that's expected mid-task.) The known `ScreenshotTests` dashboard-baseline failure persists (regenerated in Plan B Task 5).

- [ ] **Step 7: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_page.h shared/nexis/Pages/Dashboard/dashboard_page.cpp
git commit -m "feat(dashboard): fixed-size cells + scroll + dynamic occupancy (GH#191)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 3: Responsive columns + reflow + fixed-pitch hit-testing

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.h` / `.cpp`

**Interfaces:**
- Consumes: `columnsForWidth`, `reflow`, fixed pitch constants.
- Produces: `void DashboardPage::recomputeColumns();` — derives `mVisibleCols` from the scroll viewport width and, if it changed, reflows tiles + rebuilds + persists.

- [ ] **Step 1: Add `recomputeColumns()`** — derive the visible column count from the scroll viewport's width and reflow when it changes:

```cpp
void DashboardPage::recomputeColumns()
{
    int viewportW = mGridScroll && mGridScroll->viewport()
                      ? mGridScroll->viewport()->width()
                      : width();
    int cols = DashboardLayout::columnsForWidth(viewportW);
    if (cols == mVisibleCols && !mOccupancy.isEmpty())
        return;
    mVisibleCols = cols;

    // Reflow current tiles into the new column count (pure logic), then apply
    // the repacked positions back to the wrappers.
    QJsonArray tiles = serializeLayout();
    QJsonArray packed = DashboardLayout::reflow(tiles, mVisibleCols);
    for (const QJsonValue &v : packed) {
        QJsonObject o = v.toObject();
        QString uid = o.contains("uid") ? o["uid"].toString() : o["id"].toString();
        for (DashboardTileWrapper *w : mTileWrappers)
            if (w->tileId() == uid) {
                w->setGridPosition(o["row"].toInt(), o["col"].toInt(),
                                   o["rowSpan"].toInt(1), o["colSpan"].toInt(1));
                break;
            }
    }
    buildGrid();
    persistLayout();
}
```

(`serializeLayout`/`uid` come from Plan A / Plan C. In the Plan-A-only world `serializeLayout` emits `id` as the key and there is no `uid`; the `o.contains("uid")` guard handles both, so A2 works whether or not Plan C has landed.)

- [ ] **Step 2: Call `recomputeColumns()` on resize and at the end of init** — extend `resizeEvent`:

```cpp
void DashboardPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    mKioskButton->move(width() - mKioskButton->width() - 10, 10);
    mEditButton->move(width() - mKioskButton->width() - mEditButton->width() - 18, 10);
    recomputeColumns();
}
```

And after the initial `buildGrid()` in `init()`, call `recomputeColumns()` once so the first paint uses the real width (it will reflow the freshly-loaded layout to fit). Declare `recomputeColumns()` in the header.

- [ ] **Step 3: Fixed-pitch hit-testing** — `gridCellAtPos` currently divides the grid rect by `GRID_COLS`. Rewrite it to use the fixed pitch and the scroll container's coordinate space (which already accounts for scroll offset because we map through `mGridContainer`):

```cpp
bool DashboardPage::gridCellAtPos(const QPoint &globalPos, int &outRow, int &outCol) const
{
    if (!mGridContainer) return false;
    QPoint local = mGridContainer->mapFromGlobal(globalPos);
    if (local.x() < 0 || local.y() < 0) return false;

    int pitchX = DashboardLayout::kCellW + DashboardLayout::kGap;
    int pitchY = DashboardLayout::kCellH + DashboardLayout::kGap;
    int col = local.x() / pitchX;
    int row = local.y() / pitchY;
    if (col < 0 || col >= mVisibleCols) return false;
    outCol = col;
    outRow = std::max(0, row);
    return true;
}
```

- [ ] **Step 4: Fix the drag-indicator geometry** — in `onTileDragMoved`, the indicator rect was computed from `bentoGrid->geometry()` and `width/GRID_COLS`. Recompute using the fixed pitch in `mGridContainer` coordinates, and parent/raise the indicator over the grid container so it tracks scroll. Replace the cell-rect math:

```cpp
    int pitchX = DashboardLayout::kCellW + DashboardLayout::kGap;
    int pitchY = DashboardLayout::kCellH + DashboardLayout::kGap;
    QPoint topLeftInContainer(targetCol * pitchX, targetRow * pitchY);
    QPoint global = mGridContainer->mapToGlobal(topLeftInContainer);
    QPoint inPage = mapFromGlobal(global);
    mDragIndicator->setGeometry(inPage.x(), inPage.y(),
                                DashboardLayout::kCellW, DashboardLayout::kCellH);
    mDragIndicator->show();
    mDragIndicator->raise();
```

(The drag indicator stays a child of the page, positioned in page coordinates via the container→global→page mapping, so it lands correctly even when the grid is scrolled.)

- [ ] **Step 5: Build + run + verify behavior**

Run: `cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu) && ctest --test-dir build --output-on-failure`
Expected: clean build; tests pass. Launch and verify:
- Tiles are uniform fixed size; the column count matches the window width (resize the window wider → more columns; narrower → fewer, and tiles repack to fit).
- Vertical scrollbar appears when rows exceed the height; no proportional stretching.
- In edit mode, dragging a tile drops it on the correct cell (hit-testing aligned to the fixed pitch), including after scrolling.

- [ ] **Step 6: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_page.h shared/nexis/Pages/Dashboard/dashboard_page.cpp
git commit -m "feat(dashboard): responsive columns + reflow + fixed-pitch hit-test (GH#191)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 4: Reconcile load path + add-tile free-cell scan + full verification

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp`

**Interfaces:**
- Consumes: everything above.

- [ ] **Step 1: Reflow on layout load** — in `init()` (and `rebuildLayout()`), after `deserializeLayout(...)` applies the migrated tile positions to the wrappers and before/at the first `buildGrid()`, ensure the loaded layout is reflowed to the current column count. The `recomputeColumns()` call added in Task 3 Step 2 already does this on first show; confirm the load order is: deserialize → set `mVisibleCols` from width (`recomputeColumns`) → reflow → buildGrid. If `init()` calls `buildGrid()` before width is known (window not yet sized), that's fine — the first `resizeEvent` triggers `recomputeColumns()` which reflows. Verify no double-persist writes a bad intermediate (persist only inside `recomputeColumns` when cols actually change).

- [ ] **Step 2: Update the add-tile free-cell scan** — `onAddTileClicked` scans for a free cell using `GRID_ROWS`/`GRID_COLS`. Replace the scan bounds with `mVisibleCols` for columns and an unbounded row search (rows grow):

```cpp
        int freeRow = -1, freeCol = -1;
        for (int r = 0; freeRow == -1; ++r)
            for (int c = 0; c < mVisibleCols; ++c)
                if (regionIsFree(r, c, 1, 1)) { freeRow = r; freeCol = c; break; }
```

(There is always a free cell because rows are unbounded; the loop terminates on the first empty row. If Plan C has landed and replaced `onAddTileClicked`, apply the same `mVisibleCols`/unbounded-row change to its 2×2 free-region scan instead.)

- [ ] **Step 3: Grep for any remaining `GRID_ROWS`/`GRID_COLS`** — `grep -n "GRID_ROWS\|GRID_COLS" shared/nexis/Pages/Dashboard/dashboard_page.cpp` must return nothing. Fix any stragglers to use `mRowCount`/`mVisibleCols`.

- [ ] **Step 4: Full build + suite + manual sweep**

Run: `cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu) && ctest --test-dir build --output-on-failure`
Manual: default layout loads and reflows cleanly; resize across the MIN/MAX range; drag/resize/add a tile; restart and confirm the layout persists (repacked positions reload stably — a v2 reload sees `version==2`, `migrate` returns unchanged, reflow re-fits to current width).

- [ ] **Step 5: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_page.cpp
git commit -m "feat(dashboard): reflow on load + dynamic add-tile scan (GH#191)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Self-Review (completed during planning)

- **Spec coverage:** A2 implements the revised spec §1 (fixed cells, responsive columns, vertical scroll, dynamic occupancy, reflow on load/resize, migration). Pure logic (`columnsForWidth`, `reflow`, `migrate`) is TDD'd (Task 1); widget integration is build- + manually-verified (no unit seam — DashboardPage builds its whole UI in init()).
- **Placeholder scan:** none — every step has concrete code or an exact command. The two spots that say "confirm the index / order" (scroll insert position) and "if Plan C has landed" are conditional integration checks, not unspecified logic.
- **Type consistency:** `mVisibleCols`/`mRowCount`/`mOccupancy`/`mGridScroll`/`mGridContainer` declared (Task 2 Step 1) and used consistently (Tasks 2-4). `columnsForWidth`/`reflow` signatures match between helper (Task 1) and call sites (Task 3). `recomputeColumns` declared + defined (Task 3).
- **Reflow termination:** the packing loop has no upper row bound but always terminates because an empty row always fits any `colSpan ≤ cols` (proven in `reflow_wrapsToNextRowWhenWidthExhausted`).
- **Cross-plan order:** A2 should land before Plan B (compact rendering is exercised by the now-genuinely-small 1×1 cells) and before/independent of Plan C (which only needs its free-cell scan switched to `mVisibleCols`, noted in Task 4 Step 2). Plan A's already-merged grid commits are superseded by A2's rewrite of the same functions.
- **Known carry-over:** `ScreenshotTests` dashboard baseline remains stale until Plan B Task 5; expected, not a regression.
