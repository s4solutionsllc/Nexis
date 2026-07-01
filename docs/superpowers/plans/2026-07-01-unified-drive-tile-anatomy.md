# Unified Drive Tile Anatomy — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the donut Drive tile use the shared "unified chrome," tie drive health to the disk *type* (surviving every style), center the primary value in the four shape styles, and identify drives by their friendly input name.

**Architecture:** Tiles are `MetricTileBase` subclasses (class-per-style). The base owns the header/footer "chrome." We (1) extend the base header with a muted input-name label and a color-coded health segment, (2) rewrite the donut (`DiskTile`) to use the shared chrome, (3) paint the primary value in the center of gauge/ring/hybrid/donut, and (4) rewire `DashboardPage` to feed the friendly name + health to every disk style.

**Tech Stack:** C++17, Qt6 (Widgets, Charts), CMake, Qt Test (QTest/CTest).

## Global Constraints

- Build: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)` (Linux); no prompts for cmake/make.
- Tests build by default (`BUILD_TESTING=ON`); run with `ctest --test-dir build --output-on-failure`.
- No hardcoded hex colors in C++ (BUG-47) — colors come from `values.ini` tokens via `AppManager::ins()->getStyleValues()`, or from QSS via object name + dynamic properties.
- Token names must not be substrings of other tokens (BUG-49).
- After changing a QSS dynamic property, call `unpolish()`/`polish()` on the widget (BUG-56).
- Conventional commits, reference `GH#191` (this is the GH#191 follow-up). End commit messages with the Co-Authored-By trailer used on this branch.
- Documentation sync before final commit: `CHANGELOG.md`, `docs/APPLICATION_OVERVIEW.md`, `docs/ARCHITECTURE_REVIEW.md`.
- Work stays on branch `claude/unified-drive-tile-anatomy` (already created).

## Decisions locked (from the design spec)

- Used/available → header **sub-header** line (all styles); donut fixed to match.
- Primary value → **center** of the 4 shape styles (donut/gauge/ring/hybrid), **all types**.
- Health → color-coded segment **appended to the sub-header** (`200 / 219 GiB · Good`), rendered by the **base** so it shows on every disk style (type-level).
- Friendly drive name → muted label in the **title row** beside "DISK"; SMART model → **tooltip**. Disk-only scope.
- Footer trend → **Ring & Hybrid** only. Gauge & Donut do not surface a footer value or trend.
- No new per-type secondary values (YAGNI). Memory unchanged except value-in-center.

## File Structure

- Create `shared/nexis/Pages/Dashboard/drive_tile_format.{h,cpp}` — pure string helpers (usage text, sub-header-with-health text). Testable without the GUI, mirroring `dashboard_layout_util`.
- Create `tests/managers/test_drive_tile_format.cpp` — unit tests for the helper.
- Modify `shared/nexis/Pages/Dashboard/metric_tile_base.{h,cpp}` — title-row input label, health sub-header segment, use of the helper.
- Modify `shared/nexis/Pages/Dashboard/disk_tile.{h,cpp}` — adopt unified chrome; drop bespoke layout.
- Modify `shared/nexis/Pages/Dashboard/gauge_tile.cpp`, `ring_tile.{h,cpp}`, `hybrid_tile.cpp` — center value; footer trend rules.
- Modify `shared/nexis/Pages/Dashboard/dashboard_page.cpp` — wire friendly name + health for all disk styles.
- Modify `shared/nexis/static/themes/default/style/style.qss` and `.../light/style/style.qss` — style the new input label; health colors already exist globally.
- Modify `tests/CMakeLists.txt` — register the new test.
- Modify `CHANGELOG.md`, `docs/APPLICATION_OVERVIEW.md`, `docs/ARCHITECTURE_REVIEW.md`.

---

### Task 1: Pure sub-header/health text helper (TDD)

Extracts the "used / total" composition (currently duplicated between
`MetricTileBase::setDiskInfo` and the old `DiskTile`) plus the health-append
logic into a pure, testable unit.

**Files:**
- Create: `shared/nexis/Pages/Dashboard/drive_tile_format.h`
- Create: `shared/nexis/Pages/Dashboard/drive_tile_format.cpp`
- Test: `tests/managers/test_drive_tile_format.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces:
  - `QString DriveTileFormat::usageText(const QString &used, const QString &total);` → `"used / total"`. Centralizes the user-facing usage format used by `MetricTileBase::setDiskInfo`.

- [ ] **Step 1: Write the failing test**

Create `tests/managers/test_drive_tile_format.cpp`:

```cpp
#include <QtTest>
#include "Pages/Dashboard/drive_tile_format.h"

class TestDriveTileFormat : public QObject
{
    Q_OBJECT

private slots:
    void usageText_joinsWithSlash()
    {
        QCOMPARE(DriveTileFormat::usageText("200.2 GiB", "219.0 GiB"),
                 QStringLiteral("200.2 GiB / 219.0 GiB"));
    }

    void usageText_emptyTotal_stillJoins()
    {
        QCOMPARE(DriveTileFormat::usageText("6.0 TiB", "9.0 TiB"),
                 QStringLiteral("6.0 TiB / 9.0 TiB"));
    }
};

QTEST_MAIN(TestDriveTileFormat)
#include "test_drive_tile_format.moc"
```

- [ ] **Step 2: Register the test in CMake**

In `tests/CMakeLists.txt`, next to `DashboardLayoutUtilTests` (search for that name), add:

```cmake
# GH#191 follow-up: pure Drive tile sub-header/health text helper.
add_nexis_test(NAME DriveTileFormatTests
  SOURCES
    managers/test_drive_tile_format.cpp
    "${GUI_SHARED_DIR}/Pages/Dashboard/drive_tile_format.cpp"
  INCLUDES
    "${GUI_SHARED_DIR}"
    "${GUI_SHARED_DIR}/Pages/Dashboard"
)
```

- [ ] **Step 3: Run test to verify it fails**

Run: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc) --target test-DriveTileFormatTests`
Expected: FAIL — `drive_tile_format.h: No such file or directory`.

- [ ] **Step 4: Write the helper**

Create `shared/nexis/Pages/Dashboard/drive_tile_format.h`:

```cpp
#ifndef DRIVE_TILE_FORMAT_H
#define DRIVE_TILE_FORMAT_H

#include <QString>

// Pure formatting helpers for Drive tile chrome, kept GUI-free so they can be
// unit-tested without pulling in nexis-gui (mirrors dashboard_layout_util).
namespace DriveTileFormat {

// "used / total"
QString usageText(const QString &used, const QString &total);

} // namespace DriveTileFormat

#endif // DRIVE_TILE_FORMAT_H
```

Create `shared/nexis/Pages/Dashboard/drive_tile_format.cpp`:

```cpp
#include "drive_tile_format.h"

namespace DriveTileFormat {

QString usageText(const QString &used, const QString &total)
{
    return QStringLiteral("%1 / %2").arg(used, total);
}

} // namespace DriveTileFormat
```

- [ ] **Step 5: Run test to verify it passes**

Run: `cmake --build build -j$(nproc) --target test-DriveTileFormatTests && ctest --test-dir build -R DriveTileFormatTests --output-on-failure`
Expected: PASS (2 tests).

- [ ] **Step 6: Commit**

```bash
git add shared/nexis/Pages/Dashboard/drive_tile_format.h shared/nexis/Pages/Dashboard/drive_tile_format.cpp tests/managers/test_drive_tile_format.cpp tests/CMakeLists.txt
git commit -m "feat(dashboard): pure Drive tile sub-header/health text helper (GH#191)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: Base chrome — title-row input-name label

Adds the muted friendly-name label beside the type label, plus a setter. No
behavior change until a caller uses it (Task 6), so non-disk tiles are untouched.

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/metric_tile_base.h`
- Modify: `shared/nexis/Pages/Dashboard/metric_tile_base.cpp:235-247` (title row in `buildChrome`)

**Interfaces:**
- Produces: `void MetricTileBase::setInputName(const QString &friendly, const QString &model = QString());` — sets/show the muted name; sets a tooltip (on the label and the tile) to `model` when `model` is non-empty.
- Consumes: nothing new.

- [ ] **Step 1: Declare the label + setter (header)**

In `metric_tile_base.h`, in the public section (near line 37 `QToolButton *gearButton();`) add:

```cpp
    void setInputName(const QString &friendly, const QString &model = QString());
```

In the protected members, after `QLabel *mLblTitle = nullptr;` (line 82) add:

```cpp
    QLabel *mLblInput = nullptr;     // muted input/source name in the title row (e.g. "Data-02")
```

- [ ] **Step 2: Create the label in `buildChrome()`**

In `metric_tile_base.cpp`, in `buildChrome()`, the title row currently reads:

```cpp
    mLblTitle = new QLabel(mTitle, mHeaderWidget);
    mLblTitle->setObjectName("metricTileTitle");
    mTitleRow->addWidget(mLblTitle);
    mTitleRow->addStretch();
```

Replace with:

```cpp
    mLblTitle = new QLabel(mTitle, mHeaderWidget);
    mLblTitle->setObjectName("metricTileTitle");
    mTitleRow->addWidget(mLblTitle);

    mLblInput = new QLabel(mHeaderWidget);
    mLblInput->setObjectName("metricTileInput");
    mLblInput->hide();   // shown only when setInputName() is called (disk tiles)
    mTitleRow->addWidget(mLblInput);

    mTitleRow->addStretch();
```

- [ ] **Step 3: Implement `setInputName()`**

In `metric_tile_base.cpp`, after `setSource()` (ends line 306) add:

```cpp
void MetricTileBase::setInputName(const QString &friendly, const QString &model)
{
    if (!mLblInput)
        return;
    mLblInput->setText(friendly);
    mLblInput->setVisible(!friendly.isEmpty());
    if (!model.isEmpty()) {
        mLblInput->setToolTip(model);
        setToolTip(model);
    }
}
```

- [ ] **Step 4: Build**

Run: `cmake --build build -j$(nproc)`
Expected: builds clean (GUI target `nexis-gui`).

- [ ] **Step 5: Run existing tests**

Run: `ctest --test-dir build --output-on-failure`
Expected: all PASS (no behavior change yet).

- [ ] **Step 6: Commit**

```bash
git add shared/nexis/Pages/Dashboard/metric_tile_base.h shared/nexis/Pages/Dashboard/metric_tile_base.cpp
git commit -m "feat(dashboard): title-row input-name label in shared tile chrome (GH#191)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: Base chrome — health segment in the sub-header

Wraps the sub-header line so a color-coded health verdict can sit after the
`used / available` text, rendered by the base (so every style shows it). Replaces
the old no-op `setDriveHealth`/`clearDriveHealth` with `setDriveHealthSegment`.

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/metric_tile_base.h:52-55` (remove old virtuals, add new API + members)
- Modify: `shared/nexis/Pages/Dashboard/metric_tile_base.cpp` (`buildChrome` sub-header, remove old no-op defs, add new def, use helper in `setDiskInfo`)

**Interfaces:**
- Produces:
  - `void MetricTileBase::setDriveHealthSegment(const QString &verdict, bool healthy);` — empty verdict hides the segment; else shows `· verdict` colored via the `status` property (`success`/`error`).
- Consumes: `DriveTileFormat::usageText` (Task 1).
- Removes: `virtual void setDriveHealth(...)`, `virtual void clearDriveHealth()` from the base API (call sites updated in Task 4/6).

- [ ] **Step 1: Update the header API**

In `metric_tile_base.h`, remove these two lines (52-55 area):

```cpp
    virtual void setDriveHealth(const QString &driveName, const QString &status, int healthPercent, bool healthy);
    virtual void clearDriveHealth();
```

Keep `virtual void setDiskInfo(...)`. In its place add (public):

```cpp
    void setDriveHealthSegment(const QString &verdict, bool healthy);
```

In protected members, after the `mLblSource` declaration (line 83) add:

```cpp
    QLabel *mLblHealth = nullptr;     // color-coded health verdict after the sub-header
    QLabel *mLblHealthSep = nullptr;  // "·" separator shown only with a verdict
```

- [ ] **Step 2: Add `#include` for the helper**

In `metric_tile_base.cpp`, near the top includes (after `#include "Managers/app_manager.h"`), add:

```cpp
#include "drive_tile_format.h"
```

- [ ] **Step 3: Wrap the sub-header line in a row with the health labels**

In `buildChrome()`, the sub-header currently reads:

```cpp
    mLblSource = new QLabel(mHeaderWidget);
    mLblSource->setObjectName("metricTileSource");
    mLblSource->setMinimumHeight(13);
    textCol->addWidget(mLblSource);
```

Replace with:

```cpp
    auto *sourceRow = new QHBoxLayout();
    sourceRow->setContentsMargins(0, 0, 0, 0);
    sourceRow->setSpacing(4);

    mLblSource = new QLabel(mHeaderWidget);
    mLblSource->setObjectName("metricTileSource");
    mLblSource->setMinimumHeight(13);
    sourceRow->addWidget(mLblSource);

    mLblHealthSep = new QLabel(QStringLiteral("·"), mHeaderWidget);
    mLblHealthSep->setObjectName("metricTileSource");
    mLblHealthSep->hide();
    sourceRow->addWidget(mLblHealthSep);

    mLblHealth = new QLabel(mHeaderWidget);
    mLblHealth->setObjectName("diskHealthStatus");
    mLblHealth->hide();
    sourceRow->addWidget(mLblHealth);

    sourceRow->addStretch();
    textCol->addLayout(sourceRow);
```

(`QHBoxLayout` is already included at the top of the file.)

- [ ] **Step 4: Replace the old no-op drive-health defs with the new one**

In `metric_tile_base.cpp`, delete:

```cpp
void MetricTileBase::setDriveHealth(const QString &, const QString &, int, bool)
{
}

void MetricTileBase::clearDriveHealth()
{
}
```

Add in their place:

```cpp
void MetricTileBase::setDriveHealthSegment(const QString &verdict, bool healthy)
{
    if (!mLblHealth || !mLblHealthSep)
        return;
    const bool show = !verdict.isEmpty();
    mLblHealth->setText(verdict);
    mLblHealth->setProperty("status", healthy ? "success" : "error");
    mLblHealth->style()->unpolish(mLblHealth);   // BUG-56: re-evaluate property selector
    mLblHealth->style()->polish(mLblHealth);
    mLblHealth->setVisible(show);
    mLblHealthSep->setVisible(show);
}
```

(`#include <QStyle>` is already present.)

- [ ] **Step 5: Route `setDiskInfo` through the helper**

Replace `MetricTileBase::setDiskInfo` (lines 23-27):

```cpp
void MetricTileBase::setDiskInfo(int percent, const QString &usedText, const QString &totalText)
{
    setValue(percent, QString("%1%").arg(percent));
    setSubtitle(DriveTileFormat::usageText(usedText, totalText));
}
```

- [ ] **Step 6: Build**

Run: `cmake --build build -j$(nproc)`
Expected: **fails to link** — `DiskTile` still overrides the now-removed `setDriveHealth`/`clearDriveHealth`. That is fixed in Task 4. If it builds clean instead (some compilers tolerate the vestigial overrides), proceed; otherwise continue to Task 4 before re-running.

> Note for the executor: Tasks 3 and 4 together form one compilable unit (the base API change and the `DiskTile` override removal). Do them back-to-back; commit Task 3 only after Task 4 builds, or squash their commits. To keep commits atomic, **defer the Task 3 commit until Task 4 Step-build passes**, then commit both together with the Task 4 message. Skip Step 7 here if deferring.

- [ ] **Step 7: (If base builds standalone) run tests + commit**

Run: `ctest --test-dir build --output-on-failure` → PASS.

```bash
git add shared/nexis/Pages/Dashboard/metric_tile_base.h shared/nexis/Pages/Dashboard/metric_tile_base.cpp
git commit -m "feat(dashboard): health verdict segment in shared sub-header (GH#191)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 4: Donut (`DiskTile`) adopts the unified chrome

Removes the bespoke under-ring subtitle and health container; routes
used/available to the shared sub-header; adds the shared footer; honors the
value text.

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/disk_tile.h`
- Modify: `shared/nexis/Pages/Dashboard/disk_tile.cpp`

**Interfaces:**
- Consumes: `buildChrome()`, `appendFooter()`, `setSource()` (base); health now via base `setDriveHealthSegment` (Task 3).
- Produces: `DiskTile` no longer overrides `setDiskInfo`/`setDriveHealth`/`clearDriveHealth`.

- [ ] **Step 1: Trim the header**

In `disk_tile.h`, remove these declarations:

```cpp
    // Disk-specific overrides
    void setDiskInfo(int percent, const QString &usedText, const QString &totalText) override;
    void setDriveHealth(const QString &driveName, const QString &status, int healthPercent, bool healthy) override;
    void clearDriveHealth() override;
```

Remove these members and the struct:

```cpp
    QString mUsedText;
    QString mTotalText;

    QWidget *mHealthContainer;
    QHBoxLayout *mHealthLayout;

    struct HealthEntry {
        QLabel *statusLabel;
        bool healthy;
    };
    QList<HealthEntry> mHealthEntries;
```

Add a value-text member next to `int mPercent;`:

```cpp
    QString mValueText;
```

The `#include <QHBoxLayout>` and `#include <QLabel>` may now be unused; leave
`<QColor>` and remove `<QHBoxLayout>` if the compiler warns (optional).

- [ ] **Step 2: Rewrite `buildLayout()`**

In `disk_tile.cpp`, replace `buildLayout()` (lines 24-43):

```cpp
void DiskTile::buildLayout()
{
    auto *layout = buildChrome();
    layout->addStretch(1);    // donut is painted in the body
    appendFooter(layout);     // unified footer band (kept minimal for the donut)
}
```

- [ ] **Step 3: Remove the bespoke `setDiskInfo`, route `setSubtitle` to the header**

Delete `DiskTile::setDiskInfo` (lines 45-52). Replace `DiskTile::setSubtitle` (lines 64-67):

```cpp
void DiskTile::setSubtitle(const QString &text)
{
    setSource(text);
}
```

- [ ] **Step 4: Honor `valueText` in `setValue`**

Replace `DiskTile::setValue` (lines 54-58):

```cpp
void DiskTile::setValue(int percent, const QString &valueText)
{
    mPercent = qBound(0, percent, 100);
    mValueText = valueText.isEmpty() ? QString("%1%").arg(mPercent) : valueText;
    update();
}
```

- [ ] **Step 5: Remove `setDriveHealth`/`clearDriveHealth` and health refresh loop**

Delete `DiskTile::setDriveHealth` (lines 88-113) and `DiskTile::clearDriveHealth` (lines 115-131). In `refreshThemeColors()`, delete the health-entries loop:

```cpp
    for (const HealthEntry &entry : mHealthEntries) {
        entry.statusLabel->setProperty("status", entry.healthy ? "success" : "error");
        entry.statusLabel->style()->unpolish(entry.statusLabel);
        entry.statusLabel->style()->polish(entry.statusLabel);
    }
```

- [ ] **Step 6: Paint the stored value text in the center**

In `paintEvent`, the Compact branch draws `QString("%1%").arg(mPercent)` (line 176) — change to `mValueText`. The main branch draws `percentText` built at line 222 — replace that line:

```cpp
    QString percentText = mValueText.isEmpty() ? QString("%1%").arg(mPercent) : mValueText;
```

The under-ring subtitle geometry in `paintEvent` referenced `mLblSubtitle`
(lines 169, 181). `mLblSubtitle` is a base member and is no longer created for
the donut (it was created in the old `buildLayout`), so it is now `nullptr`.
Replace those two references with the shared footer boundary `bodyBottom()`:

- Line 169: `int bottom = mLblSubtitle->isVisible() ? mLblSubtitle->geometry().top() - 4 : height() - 6;` → `int bottom = bodyBottom();`
- Line 181: `int subtitleTop = mLblSubtitle->geometry().top() - 8;` → `int subtitleTop = bodyBottom();`

- [ ] **Step 7: Build**

Run: `cmake --build build -j$(nproc)`
Expected: builds clean.

- [ ] **Step 8: Run tests**

Run: `ctest --test-dir build --output-on-failure`
Expected: all PASS.

- [ ] **Step 9: Commit (include Task 3 if deferred)**

```bash
git add shared/nexis/Pages/Dashboard/disk_tile.h shared/nexis/Pages/Dashboard/disk_tile.cpp shared/nexis/Pages/Dashboard/metric_tile_base.h shared/nexis/Pages/Dashboard/metric_tile_base.cpp
git commit -m "feat(dashboard): donut adopts unified chrome; health in shared sub-header (GH#191)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 5: Center the primary value in gauge / ring / hybrid; footer trend rules

Gauge/ring/hybrid stop putting the value in the footer and paint it in the center
of the shape. Ring & Hybrid keep the footer trend; Gauge drops it (design: only
ring/hybrid surface trend).

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/gauge_tile.cpp`
- Modify: `shared/nexis/Pages/Dashboard/ring_tile.h`, `ring_tile.cpp`
- Modify: `shared/nexis/Pages/Dashboard/hybrid_tile.cpp`

**Interfaces:**
- Consumes: `bodyTop()`, `bodyBottom()`, `resolvedColor()` (base). `mTextColor` (gauge/hybrid already have it; ring gains it).

- [ ] **Step 1: Gauge — stop footer value, drop footer trend**

In `gauge_tile.cpp`, replace `setValue` (lines 35-41):

```cpp
void GaugeTile::setValue(int percent, const QString &valueText)
{
    mPercent = qBound(0, percent, 100);
    mValueText = valueText.isEmpty() ? QString("%1%").arg(mPercent) : valueText;
    update();
}
```

Replace `setTrendDirection` (lines 57-60) so the gauge footer stays clean:

```cpp
void GaugeTile::setTrendDirection(TrendDirection)
{
    // Design: gauge surfaces the value in its center, not a footer trend.
}
```

- [ ] **Step 2: Gauge — paint the value in the arc center**

In `gauge_tile.cpp` `paintEvent`, at the end of the method (after the value-arc
`if (mPercent > 0) { ... }` block, before the closing brace), add:

```cpp
    // Primary value centered in the arc (unified anatomy).
    const QString valueText = mValueText.isEmpty() ? QString("%1%").arg(mPercent) : mValueText;
    QFont valueFont = font();
    valueFont.setPixelSize(qMax(14, diameter / 5));
    valueFont.setBold(true);
    painter.setFont(valueFont);
    painter.setPen(mTextColor);
    const QRectF valueRect(centerX - diameter / 2.0, centerY - diameter / 2.0, diameter, diameter);
    painter.drawText(valueRect, Qt::AlignCenter, valueText);
```

(`centerX`, `centerY`, `diameter` are already computed earlier in `paintEvent`;
`mTextColor` is set in `refreshThemeColors`.)

- [ ] **Step 3: Ring — add value/text-color members**

In `ring_tile.h`, add to the private members (after `int mPercent;`):

```cpp
    QString mValueText;
    QColor  mTextColor;
```

(`#include <QColor>` — `QProgressBar`/`QLabel` already pull in Qt; add
`#include <QColor>` at the top if not transitively available.)

- [ ] **Step 4: Ring — store value, stop footer value, set text color**

In `ring_tile.cpp`, replace `setValue` (lines 40-46):

```cpp
void RingTile::setValue(int percent, const QString &valueText)
{
    mPercent = qBound(0, percent, 100);
    mProgressBar->setValue(mPercent);
    mValueText = valueText.isEmpty() ? QString("%1%").arg(mPercent) : valueText;
    update();
}
```

In `refreshThemeColors()`, after `mSecondaryTextColor = QColor(...);` (line 96) add:

```cpp
    mTextColor = QColor(sv->value("@color05").toString());
```

(`RingTile` keeps `setTrendDirection` → `setTrendLabel`: it is a trend style.)

- [ ] **Step 5: Ring — paint the value in the center**

In `ring_tile.cpp` `paintEvent`, after the value-arc `if (mPercent > 0) { ... }`
block (before the closing brace), add:

```cpp
    // Primary value centered in the ring (unified anatomy).
    const QString valueText = mValueText.isEmpty() ? QString("%1%").arg(mPercent) : mValueText;
    QFont valueFont = font();
    valueFont.setPixelSize(qMax(14, diameter / 5));
    valueFont.setBold(true);
    painter.setFont(valueFont);
    painter.setPen(mTextColor);
    painter.drawText(ringRect, Qt::AlignCenter, valueText);
```

(`diameter` and `ringRect` are already computed earlier in `paintEvent`.)

- [ ] **Step 6: Hybrid — stop footer value**

In `hybrid_tile.cpp`, replace `setValue` (lines 80-86):

```cpp
void HybridTile::setValue(int percent, const QString &valueText)
{
    mPercent = qBound(0, percent, 100);
    mValueText = valueText.isEmpty() ? QString("%1%").arg(mPercent) : valueText;
    update();
}
```

(`HybridTile` keeps `setTrendDirection` → `setTrendLabel`: it is a trend style.
`setSecondaryValue` still calls `setHeroSecondary` — leave it; memory/other
secondaries continue to show in the footer for hybrid.)

- [ ] **Step 7: Hybrid — paint the value in the gauge-arc center**

In `hybrid_tile.cpp` `drawGaugeArc`, after the value-arc `painter.drawArc(...)`
call (end of the method), add:

```cpp
    // Primary value centered in the gauge arc (unified anatomy).
    const QString valueText = mValueText.isEmpty() ? QString("%1%").arg(mPercent) : mValueText;
    QFont valueFont = painter.font();
    valueFont.setPixelSize(qMax(12, side / 4));
    valueFont.setBold(true);
    painter.setFont(valueFont);
    painter.setPen(mTextColor);
    painter.drawText(arcRect, Qt::AlignCenter, valueText);
```

(`side` and `arcRect` are computed earlier in `drawGaugeArc`; `mTextColor` is set
in `refreshThemeColors`.)

- [ ] **Step 8: Build**

Run: `cmake --build build -j$(nproc)`
Expected: builds clean.

- [ ] **Step 9: Run tests**

Run: `ctest --test-dir build --output-on-failure`
Expected: all PASS.

- [ ] **Step 10: Commit**

```bash
git add shared/nexis/Pages/Dashboard/gauge_tile.cpp shared/nexis/Pages/Dashboard/ring_tile.h shared/nexis/Pages/Dashboard/ring_tile.cpp shared/nexis/Pages/Dashboard/hybrid_tile.cpp
git commit -m "feat(dashboard): center primary value in gauge/ring/hybrid shapes (GH#191)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 6: Wire friendly name + health for every disk style

Feeds the friendly input name to the title-row label and the health verdict to
the shared segment for all disk tiles — so health survives every style (Req 2)
and the drive shows its friendly name with the SMART model in a tooltip (Req 4).

**Files:**
- Modify: `shared/nexis/Pages/Dashboard/dashboard_page.cpp` (disk update ~636-652; `updateDiskHealthBadge` ~849-900)

**Interfaces:**
- Consumes: `MetricTileBase::setInputName` (Task 2), `MetricTileBase::setDriveHealthSegment` (Task 3), `MetricTileBase::setDiskInfo` (base).

- [ ] **Step 1: Set the friendly name where disks update**

In `dashboard_page.cpp`, the disk loop currently ends (around lines 647-651):

```cpp
        auto *tile = qobject_cast<MetricTileBase*>(w->innerWidget());
        if (!tile) continue;
        tile->setDiskInfo(diskPercent,
                          FormatUtil::formatBytes(disk->used),
                          FormatUtil::formatBytes(disk->size));
```

Insert the name before `setDiskInfo`:

```cpp
        auto *tile = qobject_cast<MetricTileBase*>(w->innerWidget());
        if (!tile) continue;
        tile->setInputName(w->inputKey().isEmpty() ? disk->name : w->inputKey());
        tile->setDiskInfo(diskPercent,
                          FormatUtil::formatBytes(disk->used),
                          FormatUtil::formatBytes(disk->size));
```

- [ ] **Step 2: Replace the health-badge wiring**

In `updateDiskHealthBadge()`, the tail currently reads (lines 894-899):

```cpp
        tile->clearDriveHealth();
        if (matched) {
            QString name = matched->model.isEmpty() ? matched->deviceName : matched->model;
            bool good = (matched->healthVerdict == "Good" || matched->smartPassed);
            tile->setDriveHealth(name, matched->healthVerdict, matched->healthPercent, good);
        }
```

Replace with:

```cpp
        if (matched) {
            bool good = (matched->healthVerdict == "Good" || matched->smartPassed);
            tile->setDriveHealthSegment(matched->healthVerdict, good);
            // Req 4: identify the drive by the friendly input name; SMART model → tooltip.
            const QString friendly = w->inputKey().isEmpty() ? selectedDisk->name : w->inputKey();
            tile->setInputName(friendly, matched->model);
        } else {
            tile->setDriveHealthSegment(QString(), true);
        }
```

- [ ] **Step 3: Build**

Run: `cmake --build build -j$(nproc)`
Expected: builds clean.

- [ ] **Step 4: Run tests**

Run: `ctest --test-dir build --output-on-failure`
Expected: all PASS.

- [ ] **Step 5: Commit**

```bash
git add shared/nexis/Pages/Dashboard/dashboard_page.cpp
git commit -m "feat(dashboard): feed friendly drive name + health to every disk style (GH#191)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 7: QSS — style the input-name label (both themes)

The health verdict is already colored by the global `[status="success"|"error"]`
rules and `#diskHealthStatus` sizing. Only the new `#metricTileInput` label needs
a style. Add it to both themes.

**Files:**
- Modify: `shared/nexis/static/themes/default/style/style.qss`
- Modify: `shared/nexis/static/themes/light/style/style.qss`

- [ ] **Step 1: Add the input-label rule (default theme)**

In `shared/nexis/static/themes/default/style/style.qss`, immediately after the
`#metricTileTitle { ... }` block (around line 772-776), add:

```css
#metricTileInput {
    color: @tertiaryText;
    font-size: 8pt;
    font-weight: 500;
}
```

- [ ] **Step 2: Add the same rule to the light theme**

In `shared/nexis/static/themes/light/style/style.qss`, find the `#metricTileTitle`
rule and add the identical `#metricTileInput { ... }` block after it. (If the light
theme has no `#metricTileTitle` rule, add the `#metricTileInput` block near the
other `#metricTile*` rules.)

- [ ] **Step 3: Build + launch to verify visually**

Run: `cmake --build build -j$(nproc)`
Then run the app (`./build/nexis` on Linux) and, on the Dashboard, add/observe a
Disk tile in each style. Verify per `/qt-ui-change`:
- Title row: `DISK  Data-02` (name muted, tooltip shows the SMART model on hover).
- Sub-header: `200.2 GiB / 219.0 GiB · Good` (verdict green; red for Caution/Critical).
- Donut/gauge/ring/hybrid: `%` centered in the shape.
- Ring/hybrid: trend pill in the footer; gauge/donut: no footer trend/value.
- Switch the disk tile across all 7 styles: the health verdict stays visible every time (Req 2).

- [ ] **Step 4: Commit**

```bash
git add shared/nexis/static/themes/default/style/style.qss shared/nexis/static/themes/light/style/style.qss
git commit -m "style(dashboard): style title-row input-name label in both themes (GH#191)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 8: Documentation sync

**Files:**
- Modify: `CHANGELOG.md`
- Modify: `docs/APPLICATION_OVERVIEW.md`
- Modify: `docs/ARCHITECTURE_REVIEW.md`

- [ ] **Step 1: CHANGELOG**

Under the current unreleased/next version section (create a `## [Unreleased]` or
the next `## [x.y.z] - 2026-07-01` block per repo convention), add:

```markdown
### Changed
- Dashboard: the disk **donut** now uses the same tile anatomy as every other
  style — used/available in the sub-header and a shared footer (GH#191).
- Dashboard: the primary value is now centered inside the donut, gauge, ring, and
  hybrid shapes for all metric types (GH#191).

### Fixed
- Dashboard: drive **health** (SMART verdict) is now tied to the disk type and
  shows in every style, not just the donut — it survives a style switch (GH#191).
- Dashboard: disk tiles now identify the drive by the **friendly input name**
  (e.g. "Data-02") with the SMART model shown as a tooltip, instead of showing the
  model in place of the selected input (GH#191).
```

- [ ] **Step 2: APPLICATION_OVERVIEW**

In `docs/APPLICATION_OVERVIEW.md`, in the Dashboard tile-styles description,
update the donut/disk wording to note it shares the unified header/footer, value
is centered in the shape styles, and health renders in the sub-header for every
disk style. Bump the "Last updated" date/version if present.

- [ ] **Step 3: ARCHITECTURE_REVIEW**

In `docs/ARCHITECTURE_REVIEW.md`, update the tile-chrome section: `MetricTileBase`
now owns the title-row input-name label (`setInputName`) and the health sub-header
segment (`setDriveHealthSegment`), so drive health is type-level and style-agnostic;
`DiskTile` no longer has a bespoke layout. Note the pure `drive_tile_format` helper.
Bump the "Last updated" date/version if present.

- [ ] **Step 4: Commit**

```bash
git add CHANGELOG.md docs/APPLICATION_OVERVIEW.md docs/ARCHITECTURE_REVIEW.md
git commit -m "docs: sync tile-anatomy changes (GH#191)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 9: Full verification + PR

**Files:** none (verification only).

- [ ] **Step 1: Clean rebuild**

Run: `rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)`
Expected: builds clean.

- [ ] **Step 2: Full test suite**

Run: `ctest --test-dir build --output-on-failure`
Expected: all PASS, including `DriveTileFormatTests` and `DashboardLayoutUtilTests`.

- [ ] **Step 3: Manual UAT (per the spec §6)**

Launch the app; on the Dashboard confirm, for a Disk tile:
1. Cycle all 7 styles; the health verdict is visible in every one (Req 2).
2. Donut/gauge/ring/hybrid show `%` centered; used/available + `· Good` in the sub-header (Req 1, Req 3).
3. Ring/hybrid show a footer trend; gauge/donut do not.
4. Title row shows the friendly name; hovering shows the SMART model tooltip (Req 4).
5. A CPU/GPU/temp gauge/ring/hybrid also centers its value (Req 3, all types) and shows no health segment.

- [ ] **Step 4: Push + open PR**

```bash
git push -u origin claude/unified-drive-tile-anatomy
gh pr create --fill
```

Report the PR URL. Do not merge.

---

## Self-Review

**Spec coverage:**
- Req 1 (donut → unified chrome) → Task 4 (+ base Tasks 2/3).
- Req 2 (health tied to type, all styles) → Task 3 (base render) + Task 6 (wire for all styles). Verified Task 7 Step 3 / Task 9 Step 3.
- Req 3 (value in center of 4 shapes, all types; trend in footer for ring/hybrid) → Task 5 (+ donut already centers, Task 4).
- Req 4 (friendly name; model tooltip) → Task 2 (`setInputName`) + Task 6 (wiring).
- Sub-header used/available (all styles) → base `setDiskInfo` (Task 3) + donut `setSubtitle`→`setSource` (Task 4).
- Health appended to sub-header, color-coded → Task 3 + Task 7.
- DRY (duplicate "%1 / %2") → Task 1 helper used by base.
- `DiskTile::setValue` ignoring valueText → fixed Task 4 Step 4.
- Docs → Task 8.

**Placeholder scan:** none — every code step shows complete code; QSS/doc steps give exact text.

**Type consistency:** `setInputName(const QString&, const QString& = {})`, `setDriveHealthSegment(const QString&, bool)`, `DriveTileFormat::usageText`, members `mLblInput`/`mLblHealth`/`mLblHealthSep`/`mValueText`/`mTextColor` used consistently across Tasks 1-6. Removed base virtuals `setDriveHealth`/`clearDriveHealth` and their `DiskTile` overrides are cleared in the same compilable unit (Tasks 3+4) and their only call sites (dashboard_page) are updated in Task 6.
