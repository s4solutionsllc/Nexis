# Unified Drive Tile Anatomy — Design Spec

**Date:** 2026-07-01
**Branch:** `claude/unified-drive-tile-anatomy`
**Related:** GH#191 (unified tile anatomy — type+input header, stat-card footer)
**Background research:** `backlog/dashboard-tile-style-consistency_research.md`

---

## 1. Problem

The Dashboard renders each tile in one of several *styles* (class-per-style,
subclasses of `MetricTileBase`). Six styles adopted the GH#191 "unified chrome"
(shared header sub-line + footer stat-card), but the **donut** (`DiskTile`) kept a
bespoke layout, and several presentation details drift between styles:

- The donut shows `used / available` **below the ring** instead of in the header
  sub-line like every other style.
- The donut and health tiles have **no shared footer band**.
- **Drive health** (SMART verdict, e.g. "Good") only renders on the donut; the
  base `setDriveHealth()` is a no-op, so switching a disk tile to any other style
  silently drops the health line.
- The donut/gauge/ring/hybrid do not present the primary value consistently
  (gauge/ring/hybrid put it in the footer; the donut centers it).
- The drive is identified by its **SMART model** ("KINGSTON SA400S3") rather than
  the **friendly input name** the user selected ("Data-02").

## 2. Goals

1. Donut adopts the unified chrome (shared header sub-line + footer). (Req 1)
2. Drive health is tied to the **type**, not the style — it renders for every disk
   style, surviving style switches. (Req 2)
3. For the 4 shape styles (donut, gauge, ring, hybrid, **all types**), the primary
   value renders in the **center of the shape**. Ring & Hybrid show their **trend
   in the footer**. (Req 3)
4. The drive is identified by its **friendly input name** ("Data-02"); the SMART
   model moves to a tooltip. (Req 4)

Non-goals: no changes to network/health tile types beyond what falls out of shared
base changes; no new per-type "secondary values"; no refactor of non-disk header
layouts. **Memory** keeps its current used/total handling (footer secondary) — it
is out of scope; only the value-in-center change (§3.2) touches it, as it does
every type.

## 3. Target anatomy

### 3.1 Header (type-level — every Drive style)

```
┌───────────────────────┐
│▍DISK  Data-02      ⚙   │   ← title row: bold type + muted friendly input name
│▍200.2 GiB / 219.0 GiB · Good │   ← sub-header: used/available · health verdict
│         ╭──────╮        │
│         │  91% │        │   ← primary value in shape center (shape styles)
│         ╰──────╯        │
└───────────────────────┘
```

- **Title row:** existing `mLblTitle` ("DISK") plus a **new muted input-name label**
  (e.g. `Data-02`) beside it. Tooltip on that label / the tile shows the SMART
  model ("KINGSTON SA400S3").
  - Scope: **disk-only.** Other types keep their current single title label and
    put their source in the sub-header (they have no header contention).
- **Sub-header** (`mLblSource`): `used / available` **·** color-coded health
  verdict. For non-disk types the sub-header is unchanged (source string, no
  health segment).

### 3.2 Body — 4 shape styles (donut / gauge / ring / hybrid), all types

- Primary value (`"91%"` / formatted reading) painted in the **center of the
  shape**. Donut already does this; gauge/ring/hybrid gain it and stop relying on
  the footer for the number.

### 3.3 Footer

| Style        | Footer contents                                              |
|--------------|-------------------------------------------------------------|
| donut        | minimal (value is centered; health is in the sub-header)     |
| gauge        | minimal (value is centered)                                  |
| ring         | **trend pill**                                               |
| hybrid       | **trend pill**                                               |
| sparkline    | value + trend (unchanged)                                    |
| speedometer  | value + trend (unchanged)                                    |
| vumeter      | value + trend (unchanged)                                    |

## 4. Changes by area

### 4.1 `metric_tile_base.{h,cpp}` — shared chrome

- **Title-row input label:** add an optional muted label in `mTitleRow`
  (`buildChrome()`, `metric_tile_base.cpp:235-247`) and a setter
  `setInputName(const QString &friendly, const QString &model = {})` that sets the
  text and a tooltip (model). Hidden/empty by default so non-disk styles are
  unaffected.
- **Health in the sub-header:** add drive-health state to the base and render it as
  a color-coded segment appended to `mLblSource`. Because a single `QLabel` can't
  two-tone easily, use **rich text** in `mLblSource` (a muted `used / available`
  span + a colored verdict span) driven by a base helper, e.g.
  `setDriveHealthSegment(const QString &verdict, bool healthy)`; keep elision
  behavior (`repositionGearButton()` / `setSource()` at `:157-164`, `:297-306`)
  working with the composed string.
- Make **`setDriveHealth()` non-virtual-by-default meaningful**: the base stores the
  verdict and composes the sub-header, so *all* styles show it (Req 2). Remove the
  no-op base implementation semantics (`:29-31`).
- Provide a shared way for shape styles to know the primary value string so they can
  paint it centered (they already receive it via `setValue`; store the current
  formatted value string in the base or each subclass).

### 4.2 The 4 shape styles paint the value in the center

- **`gauge_tile.cpp`**, **`ring_tile.cpp`**, **`hybrid_tile.cpp`**: in each
  `paintEvent`, draw the current value string centered inside the arc/ring (mirror
  the donut's center-text drawing at `disk_tile.cpp:213-224`). Stop sending the
  value to the footer hero label for these three (or hide the footer value), so it
  isn't shown twice.
- **`disk_tile.cpp` (donut):** already centers the value — keep, but route through
  the shared path.

### 4.3 Ring & Hybrid — trend in footer

- Ring and Hybrid keep `appendFooter()` and their trend pill (`setTrendLabel`);
  ensure the footer no longer carries the (now-centered) value. Gauge & Donut do
  not surface a footer trend.

### 4.4 Donut (`DiskTile`) — adopt unified chrome (Req 1)

- Replace the bespoke `buildLayout()` (`disk_tile.cpp:24-43`): drop the
  under-ring `mLblSubtitle` and the `mHealthContainer` block; use `buildChrome()`
  header + `appendFooter()` like the arcs.
- `setDiskInfo()` (`:45-52`) routes `used / available` to the **sub-header**
  (`setSubtitle` → `setSource`), matching the other styles, instead of the
  under-ring label.
- Drive health renders via the shared sub-header segment (§4.1), not
  `mHealthContainer`.
- `paintEvent` keeps drawing `%` in the center.
- Fix `setValue()` (`:54-58`) to honor the passed `valueText` for consistency with
  the other styles (still `%`-based for disk, but no longer discards the argument).

### 4.5 Drive health tied to type (Req 2)

- With health rendered by the base sub-header, health data flows to the tile
  regardless of style. `DashboardPage::updateDiskHealthBadge()` (call site
  `dashboard_page.cpp:894-899`) calls the tile's health setter for **every** disk
  style, not just the donut.

### 4.6 Friendly name + model tooltip (Req 4)

- At the health call site (`dashboard_page.cpp:895-898`), pass the **friendly input
  name** (`w->inputKey()`, e.g. `Data-02`) as the drive identity into
  `setInputName(...)`, and pass the SMART `matched->model` as the tooltip model.
  Stop using `matched->model` as the visible identity (currently line 896).
- The health *verdict* ("Good"/"Caution"/"Critical") still comes from
  `matched->healthVerdict`.

## 5. Data flow (disk tile, per tick)

```
updateDisk()          → tile->setInputName(w->inputKey(), model?)   // Data-02, tooltip model
                      → tile->setDiskInfo(pct, used, total)         // sub-header used/available
                      → tile->setValue(pct, "NN%")                  // painted in center (shape styles)
updateDiskHealthBadge → tile->setDriveHealthSegment(verdict, good)  // sub-header "· Good", all styles
                      → tile->setInputName(friendly, model)         // tooltip model
```

## 6. Testing

- Existing: `tests/managers/test_dashboard_layout_util.cpp` (layout util) — keep green.
- Add/extend a tile test (category `managers/` or a new `dashboard/` test file per
  CLAUDE.md testing conventions) asserting:
  - Switching a disk tile across all 7 styles preserves the health verdict text
    (Req 2 regression guard).
  - `setInputName` sets the visible friendly name and a model tooltip (Req 4).
  - `setDiskInfo` populates the sub-header (not an under-ring label) for the donut.
- Manual QSS/visual pass via `/qt-ui-change` checklist after implementation.

## 7. Documentation to update (per CLAUDE.md pre-commit checklist)

- `CHANGELOG.md` — new entry under the next version (Added/Changed/Fixed).
- `docs/APPLICATION_OVERVIEW.md` — Dashboard tile styles description.
- `docs/ARCHITECTURE_REVIEW.md` — shared-chrome/health-in-header note if signal or
  chrome structure changes.

## 8. Decisions locked (from brainstorming)

- Used/available → **sub-header** (all styles).
- Primary value → **center** of the 4 shape styles (all types).
- Health → **appended to the sub-header**, color-coded (`used · Good`).
- Friendly drive name → **title row beside "DISK"**; SMART model → **tooltip**.
- Trend in footer → **Ring & Hybrid** only.
- Title-row input name → **disk-only** scope for now.
- No new per-type secondary values (YAGNI).
