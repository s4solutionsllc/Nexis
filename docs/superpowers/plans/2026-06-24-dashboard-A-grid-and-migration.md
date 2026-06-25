# Dashboard Plan A — Denser Grid + Layout Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Double the dashboard bento grid to 8×8 for finer tile placement while keeping the out-of-box layout visually identical, and migrate existing saved 4×4 layouts forward without loss.

**Architecture:** Extract the grid dimensions, a layout-schema version, an area→display-tier mapping, and a pure v1→v2 layout-migration function into a new standalone, unit-testable helper (`dashboard_layout_util`). `DashboardPage` adopts those constants, switches to an envelope-based persisted layout (`{"version":2,"tiles":[...]}`), and migrates legacy bare-array layouts on load by scaling coordinates ×2. Default tiles move to 2×2 spans so the default dashboard looks unchanged.

**Tech Stack:** C++17, Qt6 (QtWidgets, QtCore/QJson), Qt Test (QTest) + CTest.

## Global Constraints

- **License/free:** GPL-3.0-only. No new third-party dependencies.
- **Platforms:** Cross-platform (Linux + macOS). All code in this plan lives under `shared/`; no platform-specific code.
- **No hardcoded colors in C++** (BUG-47): not applicable here (no color work in Plan A), but do not introduce any.
- **Build (macOS):** `cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) && cmake --build build -j$(sysctl -n hw.ncpu)`
- **Build (Linux):** `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)`
- **Tests:** `ctest --test-dir build --output-on-failure`
- **Commit style:** Conventional commits, ≤72 char subject, reference `GH#191`. Feature branch only (already on `claude/gh191-dashboard-tiles`); never commit to `native`/main.
- **Docs:** This plan defers all CHANGELOG/overview/architecture doc updates to Plan C (they describe the user-visible whole). Do not edit docs in Plan A.

## File Structure

- **Create:** `shared/nexis/Pages/Dashboard/dashboard_layout_util.h` — grid constants, schema version, `DisplayTier`, `tierForArea()`, `migrate()`. Pure (QtCore only).
- **Create:** `shared/nexis/Pages/Dashboard/dashboard_layout_util.cpp` — implementations of `tierForArea()` and `migrate()`.
- **Create:** `tests/managers/test_dashboard_layout_util.cpp` — QTest unit tests for the helper.
- **Modify:** `tests/CMakeLists.txt` — register the new test.
- **Modify:** `shared/nexis/Pages/Dashboard/dashboard_page.h` — replace private `GRID_ROWS/GRID_COLS` consts with the shared header's constants; declare `persistLayout()` and `layoutEnvelope()` helpers.
- **Modify:** `shared/nexis/Pages/Dashboard/dashboard_page.cpp` — adopt constants, envelope persistence, migration on load, 2×2 defaults, `tierForArea()` in `applyDisplayModeForSpan`.

Why a new file: the migration and tier logic are pure functions but currently trapped inside a 1700-line UI class that builds its whole widget tree in `init()` and cannot be instantiated in a unit test. The codebase already follows this exact pattern (`proc_info_parser`, `oomd_info_parser`, `macos_*parser` are pure units compiled directly into tests). This keeps the new logic testable cross-platform.

---

### Task 1: Pure layout helper — grid constants, tier mapping, v1→v2 migration

**Files:**
- Create: `shared/nexis/Pages/Dashboard/dashboard_layout_util.h`
- Create: `shared/nexis/Pages/Dashboard/dashboard_layout_util.cpp`
- Test: `tests/managers/test_dashboard_layout_util.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces:
  - `namespace DashboardLayout` with `inline constexpr int kGridRows = 8; kGridCols = 8; kSchemaVersion = 2; kLegacyGridRows = 4; kLegacyGridCols = 4;`
  - `enum DisplayTier { Compact = 0, Normal = 1, Large = 2, Hero = 3 };`
  - `DisplayTier tierForArea(int area);` — area ≥16→Hero, ≥8→Large, ≥4→Normal, else Compact.
  - `QJsonArray migrate(const QJsonArray &tiles, int declaredVersion);` — if `declaredVersion >= kSchemaVersion`, returns `tiles` unchanged; otherwise scales each tile's `row/col/rowSpan/colSpan` by `kGridCols/kLegacyGridCols` (=2), clamped to the 8×8 bounds, and returns the new array.

- [ ] **Step 1: Write the failing test**

Create `tests/managers/test_dashboard_layout_util.cpp`:

```cpp
#include <QtTest>
#include <QJsonArray>
#include <QJsonObject>
#include "Pages/Dashboard/dashboard_layout_util.h"

using namespace DashboardLayout;

class TestDashboardLayoutUtil : public QObject
{
    Q_OBJECT

private slots:
    void tierForArea_boundaries()
    {
        QCOMPARE(tierForArea(1), Compact);
        QCOMPARE(tierForArea(3), Compact);
        QCOMPARE(tierForArea(4), Normal);
        QCOMPARE(tierForArea(7), Normal);
        QCOMPARE(tierForArea(8), Large);
        QCOMPARE(tierForArea(15), Large);
        QCOMPARE(tierForArea(16), Hero);
        QCOMPARE(tierForArea(64), Hero);
    }

    void migrate_v1_scalesCoordsByTwo()
    {
        QJsonObject t;
        t["id"] = "cpu";
        t["row"] = 1; t["col"] = 2; t["rowSpan"] = 1; t["colSpan"] = 1;
        QJsonArray in; in.append(t);

        QJsonArray out = migrate(in, 1);
        QCOMPARE(out.size(), 1);
        QJsonObject o = out.at(0).toObject();
        QCOMPARE(o["row"].toInt(), 2);
        QCOMPARE(o["col"].toInt(), 4);
        QCOMPARE(o["rowSpan"].toInt(), 2);
        QCOMPARE(o["colSpan"].toInt(), 2);
        QCOMPARE(o["id"].toString(), QString("cpu"));
    }

    void migrate_v1_clampsToBounds()
    {
        QJsonObject t;
        t["id"] = "disk";
        t["row"] = 3; t["col"] = 3; t["rowSpan"] = 1; t["colSpan"] = 1;
        QJsonArray in; in.append(t);

        QJsonArray out = migrate(in, 1);
        QJsonObject o = out.at(0).toObject();
        // 3*2 = 6, span 2 -> ends at row 8 == kGridRows, still in bounds.
        QCOMPARE(o["row"].toInt(), 6);
        QCOMPARE(o["col"].toInt(), 6);
        QCOMPARE(o["rowSpan"].toInt(), 2);
        QCOMPARE(o["colSpan"].toInt(), 2);
    }

    void migrate_v2_unchanged()
    {
        QJsonObject t;
        t["id"] = "fan"; t["input"] = "k10temp/fan1";
        t["row"] = 5; t["col"] = 1; t["rowSpan"] = 1; t["colSpan"] = 1;
        QJsonArray in; in.append(t);

        QJsonArray out = migrate(in, 2);
        QJsonObject o = out.at(0).toObject();
        QCOMPARE(o["row"].toInt(), 5);
        QCOMPARE(o["col"].toInt(), 1);
        QCOMPARE(o["input"].toString(), QString("k10temp/fan1"));
    }
};

QTEST_APPLESS_MAIN(TestDashboardLayoutUtil)
#include "test_dashboard_layout_util.moc"
```

Register it in `tests/CMakeLists.txt` under the Manager-tests section (after `SettingManagerTests`, before the theme section):

```cmake
# GH#191: pure dashboard layout helper — grid constants, tier mapping, and
# v1->v2 layout migration. Compiled directly so it runs cross-platform without
# pulling in nexis-gui (DashboardPage builds its whole widget tree in init()).
add_nexis_test(NAME DashboardLayoutUtilTests
  SOURCES
    managers/test_dashboard_layout_util.cpp
    "${GUI_SHARED_DIR}/Pages/Dashboard/dashboard_layout_util.cpp"
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Pages/Dashboard"
)
```

- [ ] **Step 2: Run the test to verify it fails (won't compile — header missing)**

Run: `cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu) --target test-DashboardLayoutUtilTests`
Expected: FAIL — `dashboard_layout_util.h: No such file or directory`.

- [ ] **Step 3: Create the header**

Create `shared/nexis/Pages/Dashboard/dashboard_layout_util.h`:

```cpp
#ifndef DASHBOARD_LAYOUT_UTIL_H
#define DASHBOARD_LAYOUT_UTIL_H

#include <QJsonArray>

// Pure, UI-free helpers for the dashboard bento layout. Kept separate from
// DashboardPage so the grid math and persistence migration are unit-testable
// without instantiating the full page (which builds its widget tree in init()).
namespace DashboardLayout {

// Current bento grid resolution. GH#191: doubled from the legacy 4x4 so users
// can place more, smaller tiles; default tiles span 2x2 to look unchanged.
inline constexpr int kGridRows = 8;
inline constexpr int kGridCols = 8;

// Legacy grid resolution used by persisted v1 (unversioned) layouts.
inline constexpr int kLegacyGridRows = 4;
inline constexpr int kLegacyGridCols = 4;

// Persisted layout schema version. v1 == bare JSON array in 4x4 coords (no
// envelope). v2 == {"version":2,"tiles":[...]} in 8x8 coords.
inline constexpr int kSchemaVersion = 2;

// Display tier for a tile, derived from its grid span area (rowSpan*colSpan).
// Ordered smallest..largest. Compact is the GH#191 small-tile tier.
enum DisplayTier { Compact = 0, Normal = 1, Large = 2, Hero = 3 };

// Maps a span area to a display tier. Thresholds chosen so a 2x2 default tile
// (area 4) renders as Normal (the legacy 1x1 look), 4x4 as Hero, and a 1x1 /
// 1x2 small tile as Compact.
DisplayTier tierForArea(int area);

// Returns a layout-tile array in current (8x8) coordinates. If declaredVersion
// is already current (>= kSchemaVersion) the array is returned unchanged.
// Otherwise it is treated as a legacy 4x4 layout: row/col/rowSpan/colSpan are
// scaled by kGridCols/kLegacyGridCols and clamped to the 8x8 bounds.
QJsonArray migrate(const QJsonArray &tiles, int declaredVersion);

} // namespace DashboardLayout

#endif // DASHBOARD_LAYOUT_UTIL_H
```

- [ ] **Step 4: Create the implementation**

Create `shared/nexis/Pages/Dashboard/dashboard_layout_util.cpp`:

```cpp
#include "dashboard_layout_util.h"

#include <QJsonObject>
#include <algorithm>

namespace DashboardLayout {

DisplayTier tierForArea(int area)
{
    if (area >= 16) return Hero;
    if (area >= 8)  return Large;
    if (area >= 4)  return Normal;
    return Compact;
}

QJsonArray migrate(const QJsonArray &tiles, int declaredVersion)
{
    if (declaredVersion >= kSchemaVersion)
        return tiles;

    constexpr int scale = kGridCols / kLegacyGridCols; // 2

    QJsonArray out;
    for (const QJsonValue &val : tiles) {
        QJsonObject obj = val.toObject();

        int row = std::clamp(obj.value("row").toInt() * scale, 0, kGridRows - 1);
        int col = std::clamp(obj.value("col").toInt() * scale, 0, kGridCols - 1);
        int rowSpan = std::clamp(obj.value("rowSpan").toInt(1) * scale, 1, kGridRows - row);
        int colSpan = std::clamp(obj.value("colSpan").toInt(1) * scale, 1, kGridCols - col);

        obj["row"] = row;
        obj["col"] = col;
        obj["rowSpan"] = rowSpan;
        obj["colSpan"] = colSpan;
        out.append(obj);
    }
    return out;
}

} // namespace DashboardLayout
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu) --target test-DashboardLayoutUtilTests && ctest --test-dir build -R DashboardLayoutUtilTests --output-on-failure`
Expected: PASS (4 test functions, all pass).

- [ ] **Step 6: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_layout_util.h \
        shared/nexis/Pages/Dashboard/dashboard_layout_util.cpp \
        tests/managers/test_dashboard_layout_util.cpp \
        tests/CMakeLists.txt
git commit -m "feat(dashboard): pure layout helper for 8x8 grid + v1 migration (GH#191)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 2: Adopt shared grid constants + envelope persistence in DashboardPage

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.h` (around lines 142-144, 175-177)
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp` (`#include`, `init()` style pre-extract ~70-82, `deserializeLayout` 1201-1253, `serializeLayout` 1255-1275, save sites)

**Interfaces:**
- Consumes: `DashboardLayout::kGridRows`, `kGridCols`, `kSchemaVersion`, `migrate()` from Task 1.
- Produces:
  - `void DashboardPage::persistLayout();` — serializes the current layout as a v2 envelope and writes it to `SettingManager`.
  - `QJsonObject DashboardPage::layoutEnvelope() const;` — returns `{"version":kSchemaVersion, "tiles": serializeLayout()}`.

This task keeps the grid at its existing visual size by **not** changing `defaultLayout()` yet (Task 3 does that). After Task 2 the grid is 8×8 internally but existing saved 4×4 layouts are migrated ×2, so the running app looks the same. This is the natural review checkpoint: "grid is 8×8, nothing visually changed."

- [ ] **Step 1: Replace the grid constants in the header**

In `dashboard_page.h`, add the include near the other Dashboard includes (after line 28 `#include "dashboard_tile_wrapper.h"`):

```cpp
#include "dashboard_layout_util.h"
```

Replace lines 142-144:

```cpp
    static const int GRID_ROWS = 4;
    static const int GRID_COLS = 4;
    QString mOccupancy[GRID_ROWS][GRID_COLS];
```

with:

```cpp
    static const int GRID_ROWS = DashboardLayout::kGridRows;
    static const int GRID_COLS = DashboardLayout::kGridCols;
    QString mOccupancy[GRID_ROWS][GRID_COLS];
```

(Every `GRID_ROWS`/`GRID_COLS` use site in the `.cpp` now resolves to 8 automatically — no other math changes needed; the loops, `regionIsFree`, `gridCellAtPos`, and occupancy array all scale.)

- [ ] **Step 2: Declare the persistence helpers in the header**

In `dashboard_page.h`, in the private methods block (after line 176 `void deserializeLayout(const QString &json);`), add:

```cpp
    void persistLayout();
    QJsonObject layoutEnvelope() const;
```

- [ ] **Step 3: Update `deserializeLayout` to parse the envelope and migrate**

In `dashboard_page.cpp`, replace the top of `deserializeLayout` (lines 1201-1205):

```cpp
void DashboardPage::deserializeLayout(const QString &json)
{
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    QJsonArray arr = doc.array();

    for (const QJsonValue &val : arr) {
```

with:

```cpp
void DashboardPage::deserializeLayout(const QString &json)
{
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());

    // Layouts persist as a v2 envelope {"version":N,"tiles":[...]}. A bare
    // array is a legacy v1 (4x4) layout with no version field. (GH#191)
    QJsonArray rawTiles;
    int version = 1;
    if (doc.isObject()) {
        QJsonObject env = doc.object();
        version = env.value("version").toInt(1);
        rawTiles = env.value("tiles").toArray();
    } else {
        rawTiles = doc.array();
    }

    QJsonArray arr = DashboardLayout::migrate(rawTiles, version);

    for (const QJsonValue &val : arr) {
```

The rest of `deserializeLayout` (the per-tile loop) is unchanged — it already clamps `row/col/rowSpan/colSpan` against `GRID_ROWS/COLS`, which are now 8.

- [ ] **Step 4: Update the style pre-extraction in `init()` to handle the envelope**

In `dashboard_page.cpp` `init()`, replace lines 72-82:

```cpp
    if (!savedLayout.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(savedLayout.toUtf8());
        QJsonArray arr = doc.array();
        for (const QJsonValue &val : arr) {
            QJsonObject obj = val.toObject();
            QString id = obj["id"].toString();
            QString style = obj["style"].toString();
            if (!style.isEmpty())
                mTileStyles[id] = style;
        }
    }
```

with:

```cpp
    if (!savedLayout.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(savedLayout.toUtf8());
        QJsonArray arr = doc.isObject()
            ? doc.object().value("tiles").toArray()
            : doc.array();
        for (const QJsonValue &val : arr) {
            QJsonObject obj = val.toObject();
            QString id = obj["id"].toString();
            QString style = obj["style"].toString();
            if (!style.isEmpty())
                mTileStyles[id] = style;
        }
    }
```

- [ ] **Step 5: Add the `layoutEnvelope()` and `persistLayout()` helpers**

In `dashboard_page.cpp`, immediately after `serializeLayout()` (after line 1275 `}`), add:

```cpp
QJsonObject DashboardPage::layoutEnvelope() const
{
    QJsonObject env;
    env["version"] = DashboardLayout::kSchemaVersion;
    env["tiles"] = serializeLayout();
    return env;
}

void DashboardPage::persistLayout()
{
    mSettingManager->setDashboardLayout(
        QJsonDocument(layoutEnvelope()).toJson(QJsonDocument::Compact));
}
```

- [ ] **Step 6: Route every layout save through `persistLayout()`**

Replace each existing inline save of the form:

```cpp
    mSettingManager->setDashboardLayout(
        QJsonDocument(serializeLayout()).toJson(QJsonDocument::Compact));
```

with:

```cpp
    persistLayout();
```

Find every occurrence: `grep -n "setDashboardLayout(\s*$\|serializeLayout()).toJson" shared/nexis/Pages/Dashboard/dashboard_page.cpp` — known sites are in `onAddTileClicked` (lines ~1706-1707), `onTileColorChangeRequested` (~1739-1740), and the edit-mode exit / range-change / reset paths. Also check `exitEditMode` and `onResetLayout`. Replace each. (If any site needs the raw string for another reason, leave it — but all known sites just persist.)

- [ ] **Step 7: Build and verify the app loads a legacy layout unchanged**

Run: `cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)`
Expected: builds clean.

Manual check (the layout logic has no unit entry point — DashboardPage needs the full app): launch the app with an existing 4×4 saved layout in settings and confirm the dashboard renders the same tiles in the same relative positions (now backed by 8×8 coordinates). If no saved layout exists, the default still renders (Task 3 makes the default explicitly 2×2). Run the app per the project `/run` conventions.

- [ ] **Step 8: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_page.h shared/nexis/Pages/Dashboard/dashboard_page.cpp
git commit -m "feat(dashboard): 8x8 grid + versioned layout envelope w/ migration (GH#191)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 3: Default layout tiles span 2×2 in the 8×8 grid

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp` — `defaultLayout()` (lines 1167-1199)

**Interfaces:**
- Consumes: nothing new.
- Produces: a default layout whose tiles each span 2×2, mirroring the legacy 4×4 single-cell arrangement so the out-of-box dashboard is visually unchanged.

- [ ] **Step 1: Rewrite `defaultLayout()`**

Replace lines 1167-1199 with:

```cpp
QJsonArray DashboardPage::defaultLayout() const
{
    QJsonArray arr;
    // GH#191: the grid is now 8x8; default tiles span 2x2 so the out-of-box
    // dashboard matches the legacy 4x4 single-cell layout. Positions are the
    // legacy row/col multiplied by 2.
    auto addEntry = [&](const QString &id, int row, int col) {
        QJsonObject obj;
        obj["id"] = id;
        obj["row"] = row;
        obj["col"] = col;
        obj["rowSpan"] = 2;
        obj["colSpan"] = 2;
        obj["style"] = defaultStyle(id);
        arr.append(obj);
    };

    addEntry("cpu",     0, 0);
    addEntry("memory",  0, 2);
    addEntry("disk",    0, 4);
    addEntry("network", 0, 6);

    addEntry("health",  2, 6);

    // Sensor/optional tiles fill the second visual row left-to-right, wrapping
    // to a third row, mirroring the legacy addSensor() behaviour (now ×2).
    int sRow = 2, sCol = 0;
    auto addSensor = [&](const QString &id) {
        if (sCol >= 6) { sRow = 4; sCol = 0; }
        addEntry(id, sRow, sCol);
        sCol += 2;
    };
    if (im->hasGpu())            addSensor("gpu");
    if (im->hasThermalSensors()) addSensor("temp");
    if (im->hasBattery())        addSensor("battery");
    if (im->hasFanSensors())     addSensor("fan");

    return arr;
}
```

- [ ] **Step 2: Build**

Run: `cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)`
Expected: builds clean.

- [ ] **Step 3: Verify the default dashboard visually matches the legacy layout**

Delete any saved layout (or run with a fresh settings profile) and launch the app. Expected: CPU / Memory / Disk / Network across the top row, the optional tiles and Health below — same as before, each tile the same physical size as the legacy 1×1 (now a 2×2 in the denser grid). Empty grid cells remain available for adding tiles (used in Plan C).

- [ ] **Step 4: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_page.cpp
git commit -m "feat(dashboard): default tiles span 2x2 on the 8x8 grid (GH#191)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 4: Drive display mode from `tierForArea()`

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp` — `applyDisplayModeForSpan()` (lines 1353-1366)

**Interfaces:**
- Consumes: `DashboardLayout::tierForArea()` from Task 1.
- Produces: display mode chosen by the shared tier thresholds. NOTE: `MetricTileBase::DisplayMode` does **not** yet have a `Compact` value (added in Plan B). In Plan A, the `Compact` tier maps to `MetricTileBase::Normal` so behaviour is unchanged for small tiles; Plan B introduces the distinct rendering.

- [ ] **Step 1: Rewrite `applyDisplayModeForSpan`**

Replace lines 1353-1366 with:

```cpp
void DashboardPage::applyDisplayModeForSpan(DashboardTileWrapper *wrapper)
{
    auto *metric = qobject_cast<MetricTileBase*>(wrapper->innerWidget());
    if (!metric)
        return;

    int area = wrapper->gridRowSpan() * wrapper->gridColSpan();
    switch (DashboardLayout::tierForArea(area)) {
    case DashboardLayout::Hero:
        metric->setDisplayMode(MetricTileBase::Hero);
        break;
    case DashboardLayout::Large:
        metric->setDisplayMode(MetricTileBase::Large);
        break;
    case DashboardLayout::Normal:
    case DashboardLayout::Compact:
        // Plan B replaces the Compact arm with MetricTileBase::Compact.
        metric->setDisplayMode(MetricTileBase::Normal);
        break;
    }
}
```

- [ ] **Step 2: Build and run the full test suite**

Run: `cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu) && ctest --test-dir build --output-on-failure`
Expected: builds clean; all tests pass (including `DashboardLayoutUtilTests`).

- [ ] **Step 3: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_page.cpp
git commit -m "refactor(dashboard): derive display mode from shared tier mapping (GH#191)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Self-Review (completed during planning)

- **Spec coverage:** Plan A covers spec §1 (grid resolution & migration) in full — 8×8 constants (Task 2), 2×2 defaults (Task 3), v1→v2 envelope migration (Tasks 1-2). It lays the groundwork for §2 (the `tierForArea` Compact tier, consumed by Plan B, Task 4). Spec §3/§4/§5 (multi-instance, network, settings migration) are out of scope for Plan A and covered by Plan C. Docs (spec "Documentation updates") are deferred to Plan C by design.
- **Placeholder scan:** none — every step has concrete code or an exact command.
- **Type consistency:** `tierForArea`/`migrate`/`kGridRows`/`kGridCols`/`kSchemaVersion` names match between header (Task 1), test (Task 1), and call sites (Tasks 2, 4). `persistLayout()`/`layoutEnvelope()` declared (Task 2 Step 2) and defined (Task 2 Step 5) consistently.
- **Known follow-up:** Task 4 deliberately maps `Compact`→`Normal` until Plan B adds the enum value; this is called out inline so it isn't mistaken for a gap.
