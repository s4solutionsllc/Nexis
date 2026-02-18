# BUG-07 Plan: HiDPI / 4K Scaling (Lightweight Fix)

## Summary

Fix HiDPI scaling without a QML migration by: (1) adding a `Dpi::scale()` utility for C++ code, (2) extending the QSS token system with DPI-aware pixel tokens, and (3) relaxing unnecessary fixed sizes in .ui files.

## Architecture

### `Dpi` utility class (`shared/nexis/dpi.h`)

A static helper that scales pixel values by the primary screen's device pixel ratio:

```cpp
class Dpi {
public:
    static int scale(int px);         // returns px * dpr, rounded
    static QSize scale(QSize size);   // convenience for QSize
    static qreal factor();            // cached DPR
};
```

Computed once at app startup from `qApp->primaryScreen()->devicePixelRatio()` (with a fallback of 1.0 if no screen). All C++ hardcoded pixel values become `Dpi::scale(N)` calls.

### QSS DPI token replacement

Extend `AppManager::updateStylesheet()` to replace `@dpN` tokens (e.g., `@dp8`, `@dp36`) with DPI-scaled pixel values. The tokens are embedded directly in `style.qss` — no changes to `values.ini` needed. The replacement loop runs after color token replacement.

Example: `width: @dp8;` on a 2× display becomes `width: 16;`.

### .ui file cleanup

Remove min+max pairs that create unnecessary fixed sizes. Replace with either:
- `minimumSize` only (let Qt stretch as needed), or
- Remove the constraint entirely if the layout handles it.

---

## Tasks

### Phase 1: Create Dpi utility

- [x] **1.1** Create `shared/nexis/dpi.h` with `Dpi::scale(int)`, `Dpi::scale(QSize)`, and `Dpi::factor()`.
- [x] **1.2** Add `dpi.h` to `CMakeLists.txt` (header-only, no .cpp needed).
- [x] **1.3** Build to verify.

### Phase 2: Scale C++ hardcoded pixel values

- [x] **2.1** `hardware_info_page.cpp:55-58` — Scale `rowHeight = 30` and `headerHeight = 36` with `Dpi::scale()`.
- [x] **2.2** `apt_source_manager_page.cpp:52,58` — Scale header height (30) and icon size (20×20).
- [x] **2.3** `uninstaller_page.cpp:27` — Scale header height (30).
- [x] **2.4** `processes_page.cpp:49` — Scale header height (36).
- [x] **2.5** `disk_usage_launcher_widget.cpp:51,55,66,73,74` — Scale icon size (48×48), margins (12/6), and spacing (8).
- [x] **2.6** `system_cleaner_page.cpp:49-50,64-67,81,83` — Scale icon pixmap size (64×64), label fixed size (64×64), column width (600), header height (30).
- [x] **2.7** `search_page.cpp:40` — Scale header height (32).
- [x] **2.8** `host_manage.cpp:57` — Scale header height (32).
- [x] **2.9** `app.cpp:331` — Scale sidebar icon size (20×20).
- [x] **2.10** `circlebar.cpp:49` — Scale chart negative margins (-20, -20, -20, -65).
- [x] **2.11** `history_chart.cpp:65` — Scale chart negative margins (-11, -11, -11, -11).
- [x] **2.12** Build to verify.

### Phase 3: Add QSS DPI token replacement

- [x] **3.1** Extend `AppManager::updateStylesheet()` to scan for `@dpN` tokens and replace them with `Dpi::scale(N)` values after color replacement.
- [x] **3.2** Replace structural QSS pixel values with `@dp` tokens. Priority targets:
  - `#sidebar` min-width/max-width: `200` → `@dp200` (lines 414-415)
  - `#sidebar QPushButton` height: `36` → `@dp36` (line 421)
  - `QScrollBar:vertical` width: `8` → `@dp8` (line 11)
  - `QScrollBar:horizontal` height: `8` → `@dp8` (line 38)
  - `QScrollBar::handle` min-height/min-width: `30px` → `@dp30` (lines 19, 46)
  - `QCheckBox::indicator` width/height: `44px`/`24px` → `@dp44`/`@dp24` (lines 146-147)
  - `QCheckBox[circle]::indicator` width/height: `18px` → `@dp18` (lines 164-165)
  - `QSlider::handle:horizontal` width/height: `16px` → `@dp16` (lines 237-238)
  - `QSlider::handle:horizontal` margin: `-6px` → `@dpn6` (line 239) *(negative token)*
  - `QSlider::groove:horizontal` height: `4px` → `@dp4` (line 229)
  - `QSlider::handle:horizontal` border-radius: `8px` → `@dp8` (line 240)
  - `QSpinBox::up-button` / `down-button` width: `16px` → `@dp16` (lines 209, 218)
  - `QComboBox::drop-down` width: `20` → `@dp20` (line 264)
  - `QComboBox::down-arrow` width/height: `10`/`6` → `@dp10`/`@dp6` (lines 273-274)
  - `QHeaderView::up-arrow, down-arrow` width/height: `10`/`8` → `@dp10`/`@dp8` (lines 331-337)
  - `QMenu::indicator` width/height: `14px` → `@dp14` (lines 103-104)
  - `QRadioButton::indicator` width/height: `16` → `@dp16` (lines 125-126)
  - `#lineChartProgress` max-height: `6` → `@dp6` (line 497)
  - `#btnDownloadUpdate` max-height: `22px` → `@dp22` (line 1010)
  - `#txtProcessSearch` width: `180` → `@dp180` (line 805)
  - `#listWidgetSnapPackages::item` min-height: `36` → `@dp36` (line 931)
  - All context-specific indicator width/height values (8 selectors)
- [x] **3.3** Replace padding/margin values with `@dp` tokens for key interactive elements:
  - `QMenu::item` padding: `6 24` → `@dp6 @dp24` (line 90)
  - `#sidebar QPushButton` margin/padding (lines 424-425)
  - `QPushButton` padding: `8 16` → `@dp8 @dp16` (line 371)
  - `QPushButton` min-height: `18` → `@dp18` (line 372)
  - All `QTableView::item`, `QHeaderView::section` padding values
  - `QLineEdit` / `QSpinBox` / `QComboBox` padding values
  - All tree/list item padding values
- [x] **3.4** Leave `border-radius` and `border-width` values unscaled — these are cosmetic and look fine at 1px/small radius on HiDPI. Only scale them if testing reveals issues.
- [x] **3.5** Leave `font-size` values untouched — they already use `pt` units which scale automatically.
- [x] **3.6** Build to verify.

### Phase 4: Relax .ui file fixed sizes

- [x] **4.1** `app.ui` — Change sidebar from fixed 220px (min==max) to `minimumSize: 220`, remove `maximumSize: 220`. Add stretch factor to the horizontal layout so the sidebar grows proportionally. Update nav button iconSize from 28×28 to be set dynamically in C++ via `Dpi::scale(28)`.
- [x] **4.2** `service_item.ui` — Remove `maximumSize` height constraint (45px). Keep `minimumSize: 45`. Same for inner `serviceItemWidget`. Scale `lblServiceIcon` min+max from 25×25 to be set dynamically.
- [x] **4.3** `startup_app.ui` — Same pattern: remove max height (45px), keep min. Scale icon sizes dynamically.
- [x] **4.4** `apt_source_repository_item.ui` — Same pattern: remove max height (45px), keep min. Scale icon sizes dynamically.
- [x] **4.5** `system_cleaner_page.ui` — Remove max size constraints on the 6 icon labels (64×64). Keep min. Scale `btnScan` min from 120×40, `btnClean` from 100×100, `lblLoadingScanner` from 100×100 — set dynamically.
- [x] **4.6** `search_page.ui` — Remove max height constraints on combo boxes and spin boxes (28px). Keep min.
- [x] **4.7** `linebar.ui` — Remove max height on `lineChartProgress` (20px). Keep min.
- [x] **4.8** `startup_app_edit.ui` — Scale minimumSize (380px) dynamically.
- [x] **4.9** `uninstallerpage.ui` — Remove max width constraint on `txtPackageSearch` (170px). Keep min.
- [x] **4.10** Build to verify.

### Phase 5: Final verification

- [x] **5.1** Full clean rebuild.
- [x] **5.2** Update `BUGS.md` — mark BUG-07 as `[x]` with resolution note.

---

## Acceptance Criteria

- App compiles and runs correctly at 1× (96 DPI) with no visual regressions.
- At 2× (e.g., `QT_SCALE_FACTOR=2`), all controls, icons, headers, sidebar, and interactive elements scale proportionally.
- No text truncation in service items, startup apps, or APT source items at 2×.
- Scrollbars, checkboxes, sliders, and combo box dropdowns are usable at 2×.
- Sidebar width scales proportionally (wider on HiDPI, not fixed at 200px).

## Known Limitations

- `border-radius` values are not scaled (cosmetic only — sharp corners at HiDPI are acceptable).
- `border-width: 1px` stays at 1px (hairline borders are standard on HiDPI).
- QChart negative margin hacks may need manual tuning per DPI level.
- Some GNOME Settings combo box `minimumSize: 200` constraints are kept as-is (they're already generous).

## Files Changed

| File | Change |
|---|---|
| `shared/nexis/dpi.h` | **NEW** — DPI scaling utility |
| `CMakeLists.txt` | Add `dpi.h` to sources |
| `shared/nexis/Managers/app_manager.cpp` | Add `@dp` token replacement |
| `shared/nexis/static/themes/default/style/style.qss` | Replace ~80 pixel values with `@dp` tokens |
| `shared/nexis/Pages/HardwareInfo/hardware_info_page.cpp` | Scale row/header heights |
| `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp` | Scale header, icon size |
| `shared/nexis/Pages/Uninstaller/uninstaller_page.cpp` | Scale header height |
| `shared/nexis/Pages/Processes/processes_page.cpp` | Scale header height |
| `shared/nexis/Pages/Resources/disk_usage_launcher_widget.cpp` | Scale icon, margins, spacing |
| `shared/nexis/Pages/SystemCleaner/system_cleaner_page.cpp` | Scale icons, column, header |
| `shared/nexis/Pages/Search/search_page.cpp` | Scale header height |
| `shared/nexis/Pages/Helpers/host_manage.cpp` | Scale header height |
| `shared/nexis/app.cpp` | Scale sidebar icon size |
| `shared/nexis/Pages/Dashboard/circlebar.cpp` | Scale chart margins |
| `shared/nexis/Pages/Resources/history_chart.cpp` | Scale chart margins |
| `app.ui` | Relax sidebar fixed width |
| `service_item.ui` | Remove max height constraint |
| `startup_app.ui` | Remove max height constraint |
| `apt_source_repository_item.ui` | Remove max height constraint |
| `system_cleaner_page.ui` | Remove max size constraints on icons/buttons |
| `search_page.ui` | Remove max height constraints on controls |
| `linebar.ui` | Remove max height on progress bar |
| `startup_app_edit.ui` | Scale minimum size dynamically |
| `uninstallerpage.ui` | Remove max width on search field |
| `BUGS.md` | Mark BUG-07 resolved |
