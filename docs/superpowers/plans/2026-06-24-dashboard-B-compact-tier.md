# Dashboard Plan B — Compact Display Tier Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> **Depends on Plan A** (`2026-06-24-dashboard-A-grid-and-migration.md`) being merged: this plan consumes `DashboardLayout::tierForArea()` and the `Compact` tier introduced there.

**Goal:** Add a `Compact` display tier so that small tiles (1×1 / 1×2 on the 8×8 grid) stay readable by dropping the heavy visualization (gauge/sparkline/dial/ticks) and showing just the title + a large value.

**Architecture:** Add a `Compact` value to `MetricTileBase::DisplayMode` and route the `Compact` tier to it from `applyDisplayModeForSpan`. Each tile renders Compact minimally: child-widget tiles (sparkline, hybrid) hide their `QChartView`; custom-painted tiles (gauge, ring, speedometer, vumeter, disk) take a compact branch in `paintEvent` that draws only the centered value text. `NetworkTile` (a plain `QWidget`, outside the display-mode system) gets an explicit `setCompact()` and is handled separately in `applyDisplayModeForSpan`. QSS gains `compactMode` font selectors for label-based tiles.

**Tech Stack:** C++17, Qt6 (QtWidgets, QtGui painting, QtCharts), Qt Test. Compact rendering is custom `QPainter` code and is verified by build + manual/visual inspection and the existing `ScreenshotTests`; it has no unit-testable seam.

## Global Constraints

- **License/free:** GPL-3.0-only. No new dependencies.
- **Platforms:** Cross-platform; all files under `shared/`.
- **No hardcoded hex colors in C++** (BUG-47): Compact paint code MUST reuse each tile's existing resolved color members (`mTextColor`, `mSecondaryTextColor`, `mMetricColor`, etc.), which already come from theme tokens. Do not introduce literal hex.
- **Dynamic-property re-polish** (BUG-56): after toggling a `compactMode` QSS property, call `unpolish()`/`polish()` on the affected widget (mirrors the existing `heroMode`/`largeMode` handling in `metric_tile.cpp`).
- **Build/test commands:** same as Plan A's Global Constraints.
- **Commit style:** Conventional commits, ≤72 chars, `GH#191`, feature branch only.
- **Docs:** deferred to Plan C.

## File Structure

- **Modify:** `shared/nexis/Pages/Dashboard/metric_tile_base.h` — add `Compact` to `DisplayMode`.
- **Modify:** `shared/nexis/Pages/Dashboard/dashboard_page.cpp` — `applyDisplayModeForSpan` Compact arm → `MetricTileBase::Compact`; add `NetworkTile` Compact handling.
- **Modify:** the eight `MetricTileBase` subclasses' `setDisplayMode`/`paintEvent`: `metric_tile.cpp`, `gauge_tile.cpp`, `ring_tile.cpp`, `speedometer_tile.cpp`, `vumeter_tile.cpp`, `disk_tile.cpp`, `hybrid_tile.cpp`, `health_score_tile.cpp`.
- **Modify:** `shared/nexis/Pages/Dashboard/network_tile.h` / `network_tile.cpp` — add `setCompact(bool)`.
- **Modify:** theme QSS (`shared/nexis/.../themes/.../*.qss` or the styled template — locate with grep in Task 5) — `compactMode` font selectors.

---

### Task 1: Add the `Compact` enum value and route the tier to it

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/metric_tile_base.h:20`
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp` — `applyDisplayModeForSpan` (the `Compact` arm added in Plan A Task 4)

**Interfaces:**
- Produces: `MetricTileBase::DisplayMode` gains `Compact` (placed first so existing `Normal/Hero/Large` ordinals are unaffected only if appended; see step note).

- [ ] **Step 1: Add `Compact` to the enum**

In `metric_tile_base.h`, replace line 20:

```cpp
    enum DisplayMode { Normal, Hero, Large };
```

with:

```cpp
    enum DisplayMode { Normal, Hero, Large, Compact };
```

(Append `Compact` at the end so the existing ordinals for `Normal/Hero/Large` are unchanged — no persisted data depends on these, but appending is the safe habit.)

- [ ] **Step 2: Route the Compact tier to the new mode**

In `dashboard_page.cpp` `applyDisplayModeForSpan`, change the combined arm from Plan A:

```cpp
    case DashboardLayout::Normal:
    case DashboardLayout::Compact:
        // Plan B replaces the Compact arm with MetricTileBase::Compact.
        metric->setDisplayMode(MetricTileBase::Normal);
        break;
```

to:

```cpp
    case DashboardLayout::Normal:
        metric->setDisplayMode(MetricTileBase::Normal);
        break;
    case DashboardLayout::Compact:
        metric->setDisplayMode(MetricTileBase::Compact);
        break;
```

- [ ] **Step 3: Build**

Run: `cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)`
Expected: builds clean. At this point every tile receives `Compact` but most still render as their `default`/`Normal` paint arm (safe no-op) — except `HealthScoreTile` and `SpeedometerTile`, which have `mDisplayMode != Normal` checks that Tasks 3-4 fix. Do not ship between Task 1 and Task 4.

- [ ] **Step 4: Commit**

```bash
git add shared/nexis/Pages/Dashboard/metric_tile_base.h shared/nexis/Pages/Dashboard/dashboard_page.cpp
git commit -m "feat(dashboard): add Compact display mode + route tier (GH#191)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 2: Compact for child-chart tiles (MetricTile, HybridTile)

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/metric_tile.cpp` — `setDisplayMode` (lines ~150-156)
- Modify: `shared/nexis/Pages/Dashboard/hybrid_tile.cpp` — `setDisplayMode` (lines ~129+)

**Interfaces:**
- Consumes: `MetricTileBase::Compact`.
- Produces: in Compact, the sparkline / hybrid chart is hidden and the value label is emphasized.

- [ ] **Step 1: MetricTile — hide the sparkline and flag compact**

In `metric_tile.cpp`, replace `setDisplayMode` (lines 150-156):

```cpp
void MetricTile::setDisplayMode(DisplayMode mode)
{
    mDisplayMode = mode;

    mLblValue->setProperty("heroMode", mode == Hero ? "true" : "false");
    mLblValue->setProperty("largeMode", mode == Large ? "true" : "false");

    mLblValue->style()->unpolish(mLblValue);
    mLblValue->style()->polish(mLblValue);
}
```

with:

```cpp
void MetricTile::setDisplayMode(DisplayMode mode)
{
    mDisplayMode = mode;

    const bool compact = (mode == Compact);
    mChartView->setVisible(!compact);

    mLblValue->setProperty("heroMode", mode == Hero ? "true" : "false");
    mLblValue->setProperty("largeMode", mode == Large ? "true" : "false");
    mLblValue->setProperty("compactMode", compact ? "true" : "false");

    mLblValue->style()->unpolish(mLblValue);
    mLblValue->style()->polish(mLblValue);
}
```

- [ ] **Step 2: HybridTile — hide the chart, collapse the gauge area in Compact**

In `hybrid_tile.cpp`, replace the `setDisplayMode` switch with one that adds a Compact case:

```cpp
void HybridTile::setDisplayMode(DisplayMode mode)
{
    mDisplayMode = mode;

    switch (mode) {
    case Hero:
        mChartView->show();
        mGaugeArea->setMinimumHeight(100);
        mChartView->setFixedHeight(40);
        break;
    case Large:
        mChartView->show();
        mGaugeArea->setMinimumHeight(80);
        mChartView->setFixedHeight(35);
        break;
    case Compact:
        // Drop the sparkline entirely; the painted gauge + value carry the tile.
        mChartView->hide();
        mGaugeArea->setMinimumHeight(0);
        break;
    case Normal:
    default:
        mChartView->show();
        mGaugeArea->setMinimumHeight(0);
        mChartView->setFixedHeight(30);
        break;
    }
    update();
}
```

(If `HybridTile::paintEvent` scales its painted gauge by `mDisplayMode`, its existing `default` arm already covers `Compact` acceptably; no paint change required here. Verify visually in Step 3.)

- [ ] **Step 3: Build and visually verify**

Run: `cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)`
Then launch the app, enter dashboard edit mode, and resize a CPU (sparkline) tile and a hybrid-style tile down to 1×1. Expected: sparkline/chart disappears, the percentage value remains clearly visible and larger relative to the tile.

- [ ] **Step 4: Commit**

```bash
git add shared/nexis/Pages/Dashboard/metric_tile.cpp shared/nexis/Pages/Dashboard/hybrid_tile.cpp
git commit -m "feat(dashboard): compact rendering for sparkline + hybrid tiles (GH#191)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 3: Compact for custom-painted tiles (Gauge, Ring, Speedometer, VuMeter, Disk)

**Files:**
- Modify: `gauge_tile.cpp`, `ring_tile.cpp`, `speedometer_tile.cpp`, `vumeter_tile.cpp`, `disk_tile.cpp`

**Interfaces:**
- Consumes: `MetricTileBase::Compact`.
- Produces: each tile's `paintEvent` takes an early Compact branch that draws only the value text centered in the tile body; heavy arcs/dials/segments are skipped.

The pattern is identical for all five: a guard at the **top** of `paintEvent` (after creating the `QPainter`) that, when `mDisplayMode == Compact`, draws the tile's value string large and centered using the tile's existing resolved text color, then returns. Each tile already computes a value string and has a `mTextColor`/equivalent.

- [ ] **Step 1: GaugeTile compact branch**

In `gauge_tile.cpp` `paintEvent`, immediately after:

```cpp
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
```

insert:

```cpp
    if (mDisplayMode == Compact) {
        int top = mLblTitle->geometry().bottom() + 4;
        int bottom = mLblSubtitle->isVisible() ? mLblSubtitle->geometry().top() - 4 : height() - 6;
        QRect valueRect(8, top, width() - 16, qMax(1, bottom - top));
        QFont vf = font();
        vf.setPixelSize(qMax(16, valueRect.height() / 2));
        vf.setBold(true);
        painter.setFont(vf);
        painter.setPen(mTextColor);
        QString text = mValueText.isEmpty() ? QString("%1%").arg(mPercent) : mValueText;
        painter.drawText(valueRect, Qt::AlignCenter, text);
        return;
    }
```

- [ ] **Step 2: DiskTile compact branch + ensure repaint on mode change**

In `disk_tile.cpp`, first make `setDisplayMode` repaint (it currently only stores the mode):

```cpp
void DiskTile::setDisplayMode(DisplayMode mode)
{
    mDisplayMode = mode;
    update();
}
```

Then in `disk_tile.cpp` `paintEvent`, after the `QPainter painter(this); painter.setRenderHint(...)` lines, insert:

```cpp
    if (mDisplayMode == Compact) {
        int top = mLblTitle->geometry().bottom() + 4;
        int bottom = mLblSubtitle->isVisible() ? mLblSubtitle->geometry().top() - 4 : height() - 6;
        QRect valueRect(8, top, width() - 16, qMax(1, bottom - top));
        QFont vf = font();
        vf.setPixelSize(qMax(16, valueRect.height() / 2));
        vf.setBold(true);
        painter.setFont(vf);
        painter.setPen(mTextColor);
        painter.drawText(valueRect, Qt::AlignCenter, QString("%1%").arg(mPercent));
        return;
    }
```

- [ ] **Step 3: RingTile compact branch**

In `ring_tile.cpp` `paintEvent`, after the `QPainter`/`setRenderHint` lines, insert (Ring uses `mLblPercentage`/`mMetricColor`/`mProgressBar`):

```cpp
    if (mDisplayMode == Compact) {
        int top = mLblTitle->geometry().bottom() + 4;
        int bottom = mProgressBar->isVisible() ? mProgressBar->geometry().top() - 4 : height() - 6;
        QRect valueRect(8, top, width() - 16, qMax(1, bottom - top));
        QFont vf = font();
        vf.setPixelSize(qMax(16, valueRect.height() / 2));
        vf.setBold(true);
        painter.setFont(vf);
        painter.setPen(mMetricColor);
        painter.drawText(valueRect, Qt::AlignCenter, mLblPercentage->text());
        return;
    }
```

- [ ] **Step 4: SpeedometerTile compact branch + fix the tick-label guard**

In `speedometer_tile.cpp` `paintEvent`, after the `QPainter`/`setRenderHint` lines, insert:

```cpp
    if (mDisplayMode == Compact) {
        int top = mLblTitle->sizeHint().height() + 6;
        int bottom = mLblSubtitle->isVisible() ? mLblSubtitle->geometry().top() - 4 : height() - 6;
        QRect valueRect(8, top, width() - 16, qMax(1, bottom - top));
        QFont vf = font();
        vf.setPixelSize(qMax(16, valueRect.height() / 2));
        vf.setBold(true);
        painter.setFont(vf);
        painter.setPen(mSecondaryTextColor);
        QString text = mValueText.isEmpty() ? QString("%1%").arg(mPercent) : mValueText;
        painter.drawText(valueRect, Qt::AlignCenter, text);
        return;
    }
```

Also change the tick-label guard so Compact never shows ticks (defensive even though the early return covers it):

```cpp
    bool showTickLabels = (mDisplayMode != Normal);
```

to:

```cpp
    bool showTickLabels = (mDisplayMode == Hero || mDisplayMode == Large);
```

- [ ] **Step 5: VuMeterTile compact branch**

In `vumeter_tile.cpp` `paintEvent`, after the `QPainter`/`setRenderHint` lines, insert (VuMeter has `mLblValue`):

```cpp
    if (mDisplayMode == Compact) {
        int top = mLblTitle->geometry().bottom() + 4;
        QRect valueRect(8, top, width() - 16, qMax(1, height() - top - 6));
        QFont vf = font();
        vf.setPixelSize(qMax(16, valueRect.height() / 2));
        vf.setBold(true);
        painter.setFont(vf);
        painter.setPen(mSecondaryTextColor);
        painter.drawText(valueRect, Qt::AlignCenter, mLblValue->text());
        return;
    }
```

(VuMeter's `setDisplayMode` already calls `update()`, so no change there. If `mSecondaryTextColor`/`mTextColor` member names differ in a given tile, use whichever resolved-color member that tile already paints its value text with — confirm by reading the non-compact branch directly above.)

- [ ] **Step 6: Build and visually verify all five**

Run: `cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)`
Launch, switch tiles to gauge/ring/speedometer/vumeter/donut styles (via edit-mode style menu), and shrink each to 1×1. Expected: each shows only its title + a large centered value; no clipped arcs/dials.

- [ ] **Step 7: Commit**

```bash
git add shared/nexis/Pages/Dashboard/gauge_tile.cpp shared/nexis/Pages/Dashboard/ring_tile.cpp \
        shared/nexis/Pages/Dashboard/speedometer_tile.cpp shared/nexis/Pages/Dashboard/vumeter_tile.cpp \
        shared/nexis/Pages/Dashboard/disk_tile.cpp
git commit -m "feat(dashboard): compact paint branch for gauge/ring/speedo/vu/disk (GH#191)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 4: HealthScoreTile guard + NetworkTile compact

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/health_score_tile.cpp` — `paintEvent`
- Modify: `shared/nexis/Pages/Dashboard/network_tile.h` / `network_tile.cpp`
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp` — `applyDisplayModeForSpan`

**Interfaces:**
- Produces: `void NetworkTile::setCompact(bool compact);` — hides/shows both `mRxChartView` and `mTxChartView`.

- [ ] **Step 1: HealthScoreTile — treat Compact like Normal (no breakdown bars)**

In `health_score_tile.cpp` `paintEvent`, replace:

```cpp
    if (mDisplayMode != Normal) {
        QPainter painter(this);
        paintBreakdownBars(painter);
    }
```

with:

```cpp
    // Breakdown bars are only for the larger tiers. Normal AND Compact show
    // just the score (Compact is a small tile — no room for bars). (GH#191)
    if (mDisplayMode == Large || mDisplayMode == Hero) {
        QPainter painter(this);
        paintBreakdownBars(painter);
    }
```

- [ ] **Step 2: NetworkTile — add `setCompact`**

In `network_tile.h`, in the public section, add:

```cpp
    void setCompact(bool compact);
```

In `network_tile.cpp`, add the implementation (near the other public methods):

```cpp
void NetworkTile::setCompact(bool compact)
{
    // GH#191: on a tiny tile, drop the dual rx/tx sparklines and keep the
    // rate readouts (the QLabels remain visible).
    if (mRxChartView) mRxChartView->setVisible(!compact);
    if (mTxChartView) mTxChartView->setVisible(!compact);
}
```

- [ ] **Step 3: Handle NetworkTile in `applyDisplayModeForSpan`**

In `dashboard_page.cpp` `applyDisplayModeForSpan`, replace the early bail:

```cpp
    auto *metric = qobject_cast<MetricTileBase*>(wrapper->innerWidget());
    if (!metric)
        return;
```

with:

```cpp
    int area = wrapper->gridRowSpan() * wrapper->gridColSpan();
    const bool compact = (DashboardLayout::tierForArea(area) == DashboardLayout::Compact);

    // NetworkTile is a plain QWidget (not a MetricTileBase), so it sits outside
    // the DisplayMode system; toggle its sparklines directly. (GH#191)
    if (auto *net = qobject_cast<NetworkTile*>(wrapper->innerWidget())) {
        net->setCompact(compact);
        return;
    }

    auto *metric = qobject_cast<MetricTileBase*>(wrapper->innerWidget());
    if (!metric)
        return;
```

Then remove the now-duplicate `int area = ...;` line further down in the function (the `switch (DashboardLayout::tierForArea(area))` keeps using the `area` computed above).

- [ ] **Step 4: Build and verify the full suite + visuals**

Run: `cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu) && ctest --test-dir build --output-on-failure`
Expected: builds clean; all tests pass.
Visual: shrink the Health tile to 1×1 → shows the score number only (no bars); shrink the Network tile to 1×1 → shows rx/tx readouts without the sparklines.

- [ ] **Step 5: Commit**

```bash
git add shared/nexis/Pages/Dashboard/health_score_tile.cpp \
        shared/nexis/Pages/Dashboard/network_tile.h shared/nexis/Pages/Dashboard/network_tile.cpp \
        shared/nexis/Pages/Dashboard/dashboard_page.cpp
git commit -m "feat(dashboard): compact handling for health + network tiles (GH#191)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 5: QSS `compactMode` font selectors + screenshot baseline

**Files:**
- Modify: theme QSS files (locate with `grep -rl "heroMode\|largeMode" shared/nexis` — apply `compactMode` selectors in the same files, for the same widget object names, e.g. `#metricTileValue`).
- Verify: `tests/screenshots/test_screenshots.cpp` (existing `ScreenshotTests`).

**Interfaces:**
- Consumes: the `compactMode` dynamic property set on value labels in Task 2 Step 1.
- Produces: themed font sizing for compact value labels in label-based tiles.

- [ ] **Step 1: Find the existing heroMode/largeMode QSS selectors**

Run: `grep -rn "heroMode\|largeMode" shared/nexis | grep -i qss`
Expected: locations where `#metricTileValue[heroMode="true"]` (or similar) set font sizes. Note the file(s) and the value-label object names.

- [ ] **Step 2: Add a `compactMode` selector beside each heroMode/largeMode rule**

For each value-label rule found, add a sibling rule that enlarges the value font for compact tiles relative to Normal (compact value should read clearly in a 1×1). Example to add next to the existing ones (adjust object name + size to match the theme's scale):

```css
#metricTileValue[compactMode="true"] {
    font-size: 20px;
    font-weight: 700;
}
```

Use the theme's existing token-driven sizing conventions; do not hardcode colors. If the theme uses `@`-token substitution for sizes, follow that pattern instead of a literal `px` where applicable.

- [ ] **Step 3: Build and refresh the screenshot baseline**

Run: `cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu) && ctest --test-dir build -R ScreenshotTests --output-on-failure`
If `ScreenshotTests` compares against committed baselines and the dashboard baseline now legitimately differs (denser grid / compact tiles), regenerate the baseline per the mechanism in `tests/screenshots/test_screenshots.cpp` (look for an env var or update flag in that file), review the new image, and commit it. If the test only asserts "renders without crashing," it will simply pass.

- [ ] **Step 4: Commit**

```bash
git add shared/nexis  # QSS + any regenerated screenshot baseline
git commit -m "style(dashboard): compactMode font tokens + screenshot baseline (GH#191)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Self-Review (completed during planning)

- **Spec coverage:** Plan B implements spec §2 (Compact display tier) for all nine tile widgets. Child-chart tiles (Tasks 2) hide their charts; painted tiles (Task 3) take a compact paint branch; the two special cases — HealthScoreTile's `!= Normal` bar trigger and NetworkTile's QWidget base — are handled in Task 4. QSS sizing in Task 5.
- **Placeholder scan:** none. The only deliberately open detail is "match the theme's value-label object name / sizing convention" in Task 5, which is a discovery step (Step 1 finds the exact names) rather than a placeholder — the code to add is shown.
- **Type consistency:** `MetricTileBase::Compact` is referenced consistently across all tiles and `applyDisplayModeForSpan`. `NetworkTile::setCompact(bool)` is declared (Task 4 Step 2) and called (Task 4 Step 3) with matching signature.
- **Risk note:** Compact paint code reuses per-tile color members whose exact names (`mTextColor` vs `mSecondaryTextColor`) vary; each step says to confirm against the tile's existing non-compact value-drawing line. This is the one place the implementer must read 1-2 lines of surrounding context — flagged in Task 3 Step 5.
- **Cross-tile assumption:** all compact branches assume `mLblTitle` and (where used) `mLblSubtitle`/`mProgressBar`/`mLblValue`/`mLblPercentage` exist on the respective tile — verified present in each header during planning.
