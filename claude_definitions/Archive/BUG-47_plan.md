# BUG-47 Plan: Eliminate All Hardcoded Colors — Full Theme Token Coverage

## Context

Themes are not being applied correctly when switching via the Appearance dropdown on the Settings page. A comprehensive audit found **45+ hardcoded color instances across 12 files**. The root causes are:

1. Widgets bake `QColor` values at construction time, never re-resolving on theme change
2. `sigChangedAppTheme` listeners are incomplete (e.g., only update chart backgrounds)
3. Some widgets have no theme listener at all
4. Inline `setStyleSheet()` calls with hardcoded hex colors override global QSS

**Goal:** Zero hardcoded colors in C++ code. Every color must come from a theme token in `values.ini`, resolved at runtime via `AppManager::ins()->getStyleValues()`.

---

## Files to Modify

| File | Issue |
|------|-------|
| `static/themes/default/style/values.ini` | Add 24 new tokens |
| `static/themes/light/style/values.ini` | Add 24 new tokens |
| `Pages/Dashboard/metric_tile.h/.cpp` | Baked `QColor`; incomplete listener; hardcoded `#ffffff` |
| `Pages/Dashboard/network_tile.h/.cpp` | Hardcoded `#E05454`; baked `QColor`; incomplete listener; hardcoded `#ffffff` |
| `Pages/Dashboard/disk_tile.h/.cpp` | Baked `QColor`; NO listener; hardcoded health colors |
| `Pages/Dashboard/dashboard_page.cpp` | Resolved colors at construction; hardcoded `#6B6E78`, `#888888` |
| `Pages/Resources/history_chart.h/.cpp` | 20 hardcoded series colors; listener doesn't update them |
| `Pages/Resources/disk_usage_launcher_widget.cpp` | 17× hardcoded `#2ec27e`/`#77767b` |
| `Pages/HardwareInfo/hardware_info_page.h/.cpp` | Hardcoded health verdict colors |
| `Pages/Settings/settings_page.cpp` | Hardcoded `#E95420` accent |
| `command_palette.h/.cpp` | Hardcoded `QColor(0,0,0,100)` shadow; no listener |
| `utilities.h` | `addDropShadow()` uses `QColor(0,0,0,alpha)` |
| `app.cpp` | Kiosk overlay `rgba(0,0,0,160)` and `color: white` |

**Dead code (no fix needed):** `circlebar.cpp` — never instantiated anywhere.

---

## Phase 1: Add All Missing Theme Tokens

**Files:** `default/style/values.ini`, `light/style/values.ini`

New tokens to add to **both** files:

```ini
# Network
@networkUploadColor=#E05454

# Overlay / Shadow
@overlayBackground=rgba(0, 0, 0, 160)
@overlayText=#ffffff
@shadowColor=rgba(0, 0, 0, 100)       # dark theme: heavier shadow
@shadowColor=rgba(0, 0, 0, 60)        # light theme: lighter shadow

# Chart Series Palette (20 colors for HistoryChart data lines)
@chartSeries01=#2ec27e
@chartSeries02=#e01b24
@chartSeries03=#1c71d8
@chartSeries04=#e5a50a
@chartSeries05=#E95420
@chartSeries06=#26a269
@chartSeries07=#813d9c
@chartSeries08=#241f31
@chartSeries09=#c64516
@chartSeries10=#c01c28
@chartSeries11=#613583
@chartSeries12=#cd9309
@chartSeries13=#a51d2d
@chartSeries14=#3d3846
@chartSeries15=#77767b
@chartSeries16=#1a5fb4
@chartSeries17=#33d17a
@chartSeries18=#f66151
@chartSeries19=#f8e45c
@chartSeries20=#5e5c64
```

Note: `@overlayBackground` and `@overlayText` use CSS color format instead of hex — verify that QSS token replacement and `QColor()` constructor both accept `rgba(...)` strings. If not, use 8-digit hex (`#000000A0` for rgba(0,0,0,160)). Same for `@shadowColor`. The light theme should get lighter variants where appropriate (lighter series colors optimised for white backgrounds, lighter shadows).

- [x] Add all tokens to default `values.ini`
- [x] Add all tokens to light `values.ini` (with light-appropriate variants where needed)
- [x] Verify no QSS token-name collisions (the `@chartSeriesNN` tokens are C++-only, not in `style.qss`)
- [x] Build and verify no new warnings

---

## Phase 2: MetricTile — Token-Based Colors + Full Refresh

**Files:** `metric_tile.h`, `metric_tile.cpp`, `dashboard_page.cpp`

**Change:** Constructor takes `QString colorToken` instead of `QColor color`. Store as `mColorToken`. Add `refreshThemeColors()` that re-resolves the token and updates ALL visual elements:
- Sparkline series pen (`mSeries->setPen()`)
- Area fill brush (`mAreaSeries->setBrush()`)
- Chart background (`mChart->setBackgroundBrush()` via `@cardBg`)
- Progress bar chunk (rebuild inline stylesheet from token)
- Action button border/text/hover (rebuild inline stylesheet; replace `#ffffff` with `@color07` token)

Replace existing incomplete listener with `connect(sigChangedAppTheme → refreshThemeColors)`. Call `refreshThemeColors()` once after `buildLayout()`. Remove all baked color code from `buildLayout()`.

**dashboard_page.cpp:** Change tile construction from `colorFromStyle("@cpuColor")` → `"@cpuColor"` (pass token name directly).

- [x] Update MetricTile constructor: `QString colorToken` instead of `QColor color`
- [x] Add `refreshThemeColors()` — re-resolves ALL colors from tokens
- [x] Replace `#ffffff` hover text with `@color07` token lookup
- [x] Remove baked colors from `buildLayout()`
- [x] Update DashboardPage tile construction to pass token names
- [x] Build and verify

---

## Phase 3: NetworkTile — Token-Based Colors + Full Refresh

**Files:** `network_tile.h`, `network_tile.cpp`

**Change:** Same pattern. Constructor takes `QString colorToken`. Upload color resolved from `@networkUploadColor` token. `refreshThemeColors()` updates:
- Download series pen + area fill
- Upload series pen + area fill
- Both chart backgrounds (`@cardBg`)
- Download/Upload label inline stylesheets
- Action button inline stylesheet (replace `#ffffff` with `@color07`)

Remove `mUploadColor("#E05454")` — resolve from token instead.

- [x] Update constructor to accept `QString colorToken`
- [x] Add `refreshThemeColors()` with full refresh
- [x] Replace `#ffffff` hover text with `@color07` token lookup
- [x] Remove baked colors from `buildLayout()`
- [x] Build and verify

---

## Phase 4: DiskTile — Add Theme Listener

**Files:** `disk_tile.h`, `disk_tile.cpp`

**Change:** Constructor takes `QString arcColorToken, QString trackColorToken`. Store token names. Keep `QColor mArcColor/mTrackColor` (needed by `paintEvent()`). Add `refreshThemeColors()` that re-resolves tokens, updates members, and calls `update()` to trigger repaint.

Fix `setDriveHealth()`: replace `"#2EC27E"`/`"#c01c28"` with `@successColor`/`@destructiveColor` token lookups.

- [x] Update constructor to accept token names
- [x] Add `refreshThemeColors()` + `sigChangedAppTheme` connection
- [x] Fix health status colors to use semantic tokens
- [x] Update DashboardPage DiskTile construction
- [x] Build and verify

---

## Phase 5: DashboardPage — Fix Summary + Cleanup

**File:** `dashboard_page.cpp`

- Replace hardcoded `#6B6E78` in system summary HTML (line 317) with `@tertiaryText` token lookup
- Remove `#888888` fallback from `colorFromStyle()` — the function itself gets removed
- Store summary data as members so the label can be rebuilt on theme change
- Remove the now-unused `colorFromStyle()` static helper

- [x] Fix system summary HTML to use `@tertiaryText` token
- [x] Add theme listener to rebuild summary label
- [x] Remove `colorFromStyle()` helper entirely
- [x] Build and verify

---

## Phase 6: HistoryChart — Token-Based Series Colors

**Files:** `history_chart.h`, `history_chart.cpp`

**Change:** Replace the hardcoded 20-color palette (lines 54-59) with token lookups from `@chartSeries01` through `@chartSeries20`. Add series color refresh to the existing `sigChangedAppTheme` listener (lines 76-89) so series colors update on theme switch.

```cpp
// In existing theme listener, add:
QSettings *sv = mAppManager->getStyleValues();
for (int i = 0; i < mSeriesList.count(); ++i) {
    QString token = QString("@chartSeries%1").arg(i + 1, 2, 10, QChar('0'));
    QColor c(sv->value(token).toString());
    dynamic_cast<QSplineSeries*>(mChart->series().at(i))->setColor(c);
}
```

- [x] Replace hardcoded palette with token lookups during init
- [x] Expand existing theme listener to update series colors
- [x] Build and verify

---

## Phase 7: DiskUsageLauncherWidget — Centralize Status Colors

**File:** `disk_usage_launcher_widget.cpp`

**Change:** Remove all 17 `mStatusLabel->setStyleSheet(...)` calls from `updateUi()`. Expand existing `applyThemeColors()` (already connected to `sigChangedAppTheme`) to also style the status label using `@successColor`/`@tertiaryText` based on installed state. Call `applyThemeColors()` at end of `updateUi()`.

- [x] Expand `applyThemeColors()` to handle status label
- [x] Remove hardcoded color strings from all `updateUi()` switch cases
- [x] Add `applyThemeColors()` call at end of `updateUi()`
- [x] Build and verify

---

## Phase 8: HardwareInfoPage — Token-Based Health Colors

**Files:** `hardware_info_page.h`, `hardware_info_page.cpp`

**Change:** Replace `QColor("#2ec27e")`, `QColor("#e5a50a")`, `QColor("#e01b24")` with `@successColor`, `@warningColor`, `@destructiveColor`. Store health item pointers and add `refreshThemeColors()` connected to `sigChangedAppTheme`.

- [x] Replace hardcoded health colors with token lookups
- [x] Add theme listener to re-color health items on switch
- [x] Build and verify

---

## Phase 9: CommandPalette — Theme-Aware Shadow

**Files:** `command_palette.h`, `command_palette.cpp`

**Change:** Store shadow effect pointer. Add `refreshThemeColors()` using `@shadowColor` token. Connect to `sigChangedAppTheme`.

- [x] Store shadow effect pointer as member
- [x] Add theme listener to update shadow color from `@shadowColor`
- [x] Build and verify

---

## Phase 10: Utilities.h — Theme-Aware Drop Shadow Helper

**File:** `utilities.h`

**Change:** `addDropShadow()` currently uses `QColor(0, 0, 0, alpha)`. Change to read shadow base color from `@shadowColor` token (via `AppManager::ins()->getStyleValues()`), applying the caller's alpha. This affects all 22 call sites app-wide. Consider returning the effect pointer so callers can update it, or connect to `sigChangedAppTheme` inside the utility.

Most pragmatic approach: have the helper read `@shadowColor` at call time (correct for initial theme), and also return the `QGraphicsDropShadowEffect*` so callers that need live updates can store and refresh it. Alternatively, create a `ThemeAwareShadow` helper class that auto-updates.

- [x] Update `addDropShadow()` to read shadow color from theme token
- [x] Ensure shadow color updates on theme change (either via returned pointer or internal listener)
- [x] Build and verify

---

## Phase 11: Kiosk Overlay — Theme-Aware Colors

**File:** `app.cpp` (lines 757-763)

**Change:** Replace hardcoded `rgba(0, 0, 0, 160)` and `color: white` with `@overlayBackground` and `@overlayText` token lookups.

```cpp
QSettings *sv = AppManager::ins()->getStyleValues();
QString overlayBg = sv->value("@overlayBackground").toString();
QString overlayText = sv->value("@overlayText").toString();
overlay->setStyleSheet(
    "background-color: " + overlayBg + ";"
    "color: " + overlayText + ";"
    "font-size: 14pt; padding: 16px 32px; border-radius: 8px;"
);
```

- [x] Replace hardcoded overlay colors with token lookups
- [x] Build and verify

---

## Phase 12: SettingsPage — Theme-Aware Credit Link

**File:** `settings_page.cpp`

**Change:** Replace hardcoded `#E95420` with `@accentColor` token. Add theme listener to rebuild credit link HTML.

- [x] Replace hardcoded accent color with token lookup
- [x] Add theme listener for credit link
- [x] Build and verify

---

## Phase 13: Final Audit + Build + Test + Documentation

- [x] Run `grep -rn '#[0-9a-fA-F]\{3,8\}' shared/nexis/ --include='*.cpp' --include='*.h'` to confirm zero remaining hardcoded hex colors in C++ (excluding comments and `style.qss`)
- [x] Clean rebuild: `rm -rf build && cmake -B build ... && cmake --build build`
- [x] Run tests: `ctest --test-dir build --output-on-failure`
- [x] Update reference screenshots
- [x] Update BUGS.md BUG-47 with resolution
- [x] Update docs (ARCHITECTURE_REVIEW.md, APPLICATION_OVERVIEW.md) if relevant
- [x] Commit and push

---

## Verification

1. **Build**: Clean compile with zero warnings
2. **Zero hardcoded colors**: grep audit passes — no hex color literals in C++ code
3. **Tests**: All CTest tests pass
4. **Manual test** — for each theme switch (Dark → Light → Dark → Auto):
   - Dashboard: MetricTile sparklines, progress bars, action buttons
   - Dashboard: NetworkTile download/upload labels and sparklines
   - Dashboard: DiskTile donut arc/track colors
   - Dashboard: System summary tertiary text color
   - Resources: HistoryChart series line colors
   - Resources: DiskUsageLauncher status label colors
   - Hardware Info: Health verdict text colors
   - Settings: Credit link accent color
   - Command Palette (Ctrl+K): shadow
   - Kiosk mode (F11 or toggle): overlay background + text
   - All drop shadows across the app
