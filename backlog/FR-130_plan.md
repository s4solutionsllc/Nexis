# FR-130 Implementation Plan: System Cleaner Redesign

## Overview

Redesign the System Cleaner categories page (page 0 of the QStackedWidget) from an icon-grid layout to a design-system-compliant two-column card layout. The scan-results tree (page 1) is preserved as a secondary detail view.

**Files changed:**
- `shared/nexis/Pages/SystemCleaner/system_cleaner_page.h`
- `shared/nexis/Pages/SystemCleaner/system_cleaner_page.cpp`
- `shared/nexis/Pages/SystemCleaner/system_cleaner_page.ui`
- `shared/nexis/static/themes/default/style/style.qss`
- `shared/nexis/static/themes/light/style/style.qss`
- `shared/nexis/app.cpp` (badge wiring)
- `shared/nexis/app.h` (signal forward declaration)

---

## Task 1 — Simplify the .ui file's categories page

**Acceptance:** `cleanerCategories` widget in the .ui has a plain `QVBoxLayout` with no content widgets (all category widgets will be built programmatically in `init()`).

- [ ] Open `system_cleaner_page.ui`
- [ ] Inside `cleanerCategories`, delete:
  - All `lbl*Img` labels (icon labels rows 2)
  - All `lbl*` category name labels (row 3)
  - All `check*` QCheckBox items (row 4) — `checkPackageCache`, `checkCrashReports`, `checkAppLog`, `checkAppCache`, `checkTrash`, `checkDevToolCache`, `checkBrokenSymlinks`, `checkBrowserPrivacy`
  - `checkSelectAllSystemScan` (row 8, "Select All")
  - `lblLoadingScanner` (row 9, loading GIF label)
  - `btnScan` (row 7, centered Scan button)
  - All spacer items in the grid
- [ ] Replace the `QGridLayout#gridLayout_3` inside `cleanerCategories` with a `QVBoxLayout` (zero margins, zero spacing) — leave the widget itself (`cleanerCategories`) in place
- [ ] Keep the outer VBoxLayout and `lblCleanerTitle` in place (used by `lblCleanerTitle` reference in cpp)
- [ ] Keep the full `cleanerPage` (page 1 of the stacked widget) untouched
- [ ] Build: `cmake --build build -j$(sysctl -n hw.ncpu)` — expect linker errors for removed `ui->check*` / `ui->btn*` references; fix in Task 3

---

## Task 2 — Update header (.h): add new member variables and signal

**Acceptance:** `system_cleaner_page.h` compiles cleanly with new struct and members.

- [ ] Add `CategoryCard` struct inside the class:
  ```cpp
  struct CategoryCard {
      QFrame    *frame    = nullptr;
      QCheckBox *check    = nullptr;
      QLabel    *lblSize  = nullptr;
      quint64    lastSize = 0;
  };
  ```
- [ ] Add member variables:
  ```cpp
  QVector<CategoryCard> mCards;          // indexed by CleanCategories enum

  QPushButton *mBtnScanSystem    = nullptr;
  QPushButton *mBtnSchedule      = nullptr;
  QPushButton *mBtnCleanSelected = nullptr;
  QPushButton *mBtnViewResults   = nullptr;
  QLabel      *mLblEstimated     = nullptr;
  QFrame      *mCleanerFooter    = nullptr;

  bool mHasScanned = false;

  // Retained scan results for "Clean selected" on page 0
  QFileInfoList mRetainedPackageCaches;
  QFileInfoList mRetainedCrashReports;
  QFileInfoList mRetainedAppLogs;
  QFileInfoList mRetainedAppCaches;
  QFileInfoList mRetainedDevToolCaches;
  QFileInfoList mRetainedBrokenSymlinks;
  QFileInfoList mRetainedBrowserPrivacy;
  QFileInfoList mRetainedSnapFlatpak;
  ```
- [ ] Remove old per-category checkbox/label member pointers from the class (the ones now managed via `mCards`)
- [ ] Add signal: `void checkedCategoryCountChanged(int count);`
- [ ] Add private method declarations:
  ```cpp
  void buildCategoryHeader();
  void buildCategoryCards();
  void buildCleanerFooter();
  void updateFooterTotal();
  void updateCleanerCheckBadge();
  void quickCleanByCategory();
  ```
- [ ] Remove old `buildTrendRow()` and `refreshTrendCells()` declarations (trend data moves to card tooltips)

---

## Task 3 — Rebuild `init()` and card construction in .cpp

**Acceptance:** Page 0 of the stacked widget shows the new card layout. Checkboxes are functional. No icon labels are present. Scan/Schedule buttons are in the header.

### 3a — Remove old icon setup from `init()`
- [ ] Delete the `setPixmap()` lambda and all 8 `setPixmap(ui->lbl..., ...)` calls
- [ ] Delete the Snap/Flatpak programmatic icon/label additions in `init()`
- [ ] Delete `buildTrendRow()` and `refreshTrendCells()` method bodies

### 3b — Build page header

Add `buildCategoryHeader()` called from `init()`:

- [ ] Create a `QHBoxLayout *headerRow`
- [ ] On the left: a `QVBoxLayout` with `ui->lblCleanerTitle` (existing, move into layout) and a new `QLabel#lblCleanerSubtitle` ("Reclaim disk space by removing caches, logs, and crash reports.")
- [ ] On the right: `mBtnSchedule` (`#btnScheduleCleaner`, text "Schedule…") and `mBtnScanSystem` (`#btnScanSystem`, text "Scan system")
- [ ] Add `headerRow` to the `cleanerCategories` QVBoxLayout (top)
- [ ] Connect `mBtnScanSystem` → `&SystemCleanerPage::on_btnScan_clicked` (reuse existing slot)
- [ ] Connect `mBtnSchedule` → lambda that opens `ScheduleEditorDialog` and connects `scheduleCreated` → `mScheduleManager->createSchedule(s)` then calls `updateScheduleIndicator()`

### 3c — Build category cards

Add `buildCategoryCards()` called from `init()`:

- [ ] Define a `struct CatDef { CleanCategories cat; QString name; QString subtitle; }` array with entries for all 9 categories (SNAP_FLATPAK_REVISIONS conditional on `#ifndef Q_OS_MACOS`)
- [ ] Source-path subtitles (use `#ifdef Q_OS_MACOS` where platform differs — see research):
  - PACKAGE_CACHE Linux: `"apt · dnf · pacman · zypper"` / macOS: `"brew · ~/Library/Caches (brew)"`
  - CRASH_REPORTS Linux: `"/var/crash · ~/.xsession-errors"` / macOS: `"~/Library/Logs/DiagnosticReports"`
  - APPLICATION_LOGS Linux: `"journald · ~/.cache/*.log"` / macOS: `"~/Library/Logs · /var/log"`
  - APPLICATION_CACHES Linux: `"~/.cache/*"` / macOS: `"~/Library/Caches"`
  - TRASH Linux: `"~/.local/share/Trash"` / macOS: `"~/.Trash"`
  - DEV_TOOL_CACHES (both): `"~/.electron · ~/.cache/vscode*"`
  - BROKEN_SYMLINKS (both): `"~/ recursive symlink scan"`
  - BROWSER_PRIVACY (both): `"Chrome · Firefox · Chromium · Safari"`
  - SNAP_FLATPAK_REVISIONS (Linux): `"snap revisions · flatpak unused runtimes"`
- [ ] Create a `QScrollArea` wrapping a `QWidget *cardsContainer` with a `QGridLayout *cardsGrid` (2 columns, spacing 10)
- [ ] For each category, call a `buildCard(CatDef)` helper that:
  - Creates `QFrame *card` with objectName `"cleanerCategoryCard"` and dynamic property `checked` = `false`
  - Creates `QCheckBox *check` (no text, `Qt::NoFocus`, `PointingHandCursor`)
  - Creates `QLabel *lblName` (objectName `"lblCatName"`) with category name
  - Creates `QLabel *lblSubtitle` (objectName `"lblCatSubtitle"`) with platform-specific subtitle
  - Creates `QLabel *lblSize` (objectName `"lblCatSize"`, text `"—"`, right-aligned)
  - Lays out: `[check | VBox(lblName, lblSubtitle) stretch | lblSize]` in a QHBoxLayout inside the card
  - Connects `check->toggled` → lambda that:
    - Sets card `checked` property and calls `style()->unpolish/polish(card)` for border update
    - Calls `updateCleanerCheckBadge()`
    - Calls `updateFooterTotal()`
  - Appends `CategoryCard{card, check, lblSize}` to `mCards[cat]`
  - Adds card to `cardsGrid` at position `(row, col)` where `col = index % 2`, `row = index / 2`
- [ ] Add the `QScrollArea` to the `cleanerCategories` QVBoxLayout (middle, expanding)

### 3d — Build footer bar

Add `buildCleanerFooter()` called from `init()`:

- [ ] Create `mCleanerFooter = new QFrame` with objectName `"cleanerFooter"` — hidden initially
- [ ] Left side: `QVBoxLayout` with `QLabel#lblEstimatedLabel` ("ESTIMATED RECOVERABLE") and `mLblEstimated = new QLabel#lblEstimatedSize` ("0 bytes")
- [ ] Right side: `mBtnViewResults` (`#btnViewResults`, "View scan results →") + `mBtnCleanSelected` (`#btnCleanSelected`, "Clean selected")
- [ ] Connect `mBtnViewResults` → lambda: `ui->stackedWidget->setCurrentIndex(1)` (navigate to tree results page)
- [ ] Connect `mBtnCleanSelected` → `&SystemCleanerPage::quickCleanByCategory`
- [ ] Add `mCleanerFooter` to the `cleanerCategories` QVBoxLayout (bottom, fixed height)

### 3e — Move exclusions button into header

- [ ] Remove the floating `mBtnExclusions` creation and `repositionExclusionsButton()` logic
- [ ] Add `mBtnExclusions` as a fixed-size `QToolButton` inside the header row (between Schedule and Scan buttons, or at far right)
- [ ] Delete `repositionExclusionsButton()` method body and call sites in `resizeEvent` / `showEvent`

### 3f — Update `on_btnScan_clicked()`

- [ ] Replace `ui->check*->isChecked()` reads with `mCards[cat].check->isChecked()` for each category
- [ ] Replace `ui->lbl*->text()` label text reads with the static `CatDef::name` strings (stored as member or re-derived)
- [ ] Replace `ui->btnScan->hide()` / `show()` with `mBtnScanSystem->setEnabled(false)` / `true`
- [ ] Replace `ui->lblLoadingScanner->show()` / `hide()` with the existing `mLoadingMovie` logic, but show the GIF inside the cards container area (or as an overlay on the button)
- [ ] Replace `ui->check*->setEnabled(false)` loop with a loop over `mCards` disabling all `card.check`
- [ ] Replace `ui->checkSelectAllSystemScan->setEnabled(false)` — remove (widget gone)
- [ ] Keep `mPackageCaches.clear()` etc. in pre-scan path; also clear `mRetained*` lists

### 3g — Update `onScanFinished()`

- [ ] After existing tree population: update each `mCards[cat].lblSize` with `FormatUtil::formatBytes(size)` and `mCards[cat].lastSize = size`
- [ ] Copy scan results into `mRetained*` member lists (for page-0 clean)
- [ ] Call `updateFooterTotal()`
- [ ] Show `mCleanerFooter`
- [ ] Set `mHasScanned = true`
- [ ] **Remove** `ui->stackedWidget->setCurrentIndex(1)` — do NOT auto-navigate to page 1
- [ ] Re-enable card checkboxes: loop over `mCards`, set `card.check->setEnabled(true)`
- [ ] Re-enable `mBtnScanSystem`
- [ ] Call `refreshTrendCells()` — repurpose: update card `lblSize` tooltip with trend delta (instead of a separate row label)

### 3h — Implement `updateFooterTotal()`

```cpp
void SystemCleanerPage::updateFooterTotal()
{
    quint64 total = 0;
    for (const CategoryCard &c : mCards)
        if (c.check && c.check->isChecked())
            total += c.lastSize;
    mLblEstimated->setText(FormatUtil::formatBytes(total));
    mBtnCleanSelected->setEnabled(total > 0 && mHasScanned);
}
```

### 3i — Implement `quickCleanByCategory()`

- [ ] Guard: `if (mScanInProgress || mCleanInProgress || !mHasScanned) return;`
- [ ] Collect files to delete from `mRetained*` lists for all checked categories
- [ ] Set `mCleanTrash = mCards[TRASH].check->isChecked()`
- [ ] Set `mCleanSnapFlatpak = mCheckSnapFlatpak && mCards[SNAP_FLATPAK_REVISIONS]...` (if applicable)
- [ ] Disable `mBtnCleanSelected`, disable card checkboxes, start loading GIF
- [ ] Set `mCleanInProgress = true`
- [ ] Launch `mWorkerFuture = QtConcurrent::run([this]() { systemClean(); })`
- [ ] `onCleanFinished()` on page 0: reset `mCards[cat].lblSize` to "—" and `lastSize = 0` for cleaned categories, hide footer, clear `mRetained*`, set `mHasScanned = false`, re-enable checkboxes

### 3j — Update `quickScan()` (called from tray Quick Actions)

- [ ] Replace all `ui->check*->isChecked()` / `setEnabled()` / `setChecked()` references with `mCards[cat]` equivalents
- [ ] Replace `ui->btnScan->hide()` / `ui->checkSelectAllSystemScan->setEnabled(false)` with card equivalents

### 3k — Update `on_btnBackToCategories_clicked()`

- [ ] Add re-enable of all card checkboxes (`mCards[cat].check->setEnabled(true)`)
- [ ] Remove `ui->checkSelectAllSystemScan->setEnabled(true)` / `setChecked(false)`

### 3l — Implement `updateCleanerCheckBadge()`

```cpp
void SystemCleanerPage::updateCleanerCheckBadge()
{
    int count = 0;
    for (const CategoryCard &c : mCards)
        if (c.check && c.check->isChecked()) ++count;
    emit checkedCategoryCountChanged(count);
}
```

---

## Task 4 — Wire the new badge signal in `app.cpp`

**Acceptance:** Sidebar badge shows the count of checked categories as an integer when any card is checked, and hides when all unchecked.

- [ ] Include or forward-declare `SystemCleanerPage` signal in `app.h` if needed
- [ ] In `App::init()` (after `systemCleanerPage` is constructed), add:
  ```cpp
  connect(systemCleanerPage, &SystemCleanerPage::checkedCategoryCountChanged,
          this, [this](int count) {
      if (count > 0) {
          mCleanerBadge->setText(QString::number(count));
          repositionBadges();
          mCleanerBadge->show();
          mCleanerBadgeDot->show();
      } else {
          mCleanerBadge->clear();
          mCleanerBadge->hide();
          mCleanerBadgeDot->hide();
      }
  });
  ```
- [ ] Keep existing `sigCleanableSizeChanged` connection (scheduled cleaner size badge) — when the check-count badge is active (count > 0), it takes priority; when count = 0, the size badge resumes

---

## Task 5 — QSS: add new rules, remove obsolete ones

**Files:** `style.qss` (dark theme) and `shared/nexis/static/themes/light/style/style.qss` (light theme — same rule set, different token values).

### 5a — Remove / replace obsolete rules
- [ ] Remove: `#cleanerCategories QCheckBox[accessibleName=circle]::indicator { ... }`
- [ ] Remove: `#cleanerCategories QLabel, #checkSelectAllSystemScan { ... }`
- [ ] Remove: `#SystemCleanerPage #btnScan { ... }` and `#btnScan:hover` — the widget is gone; the logical replacement is `#btnScanSystem`
- [ ] Keep: `#SystemCleanerPage #btnClean` (still exists on page 1), `#btnBackToCategories`, `#treeWidgetScanResult` block, `#scheduleIndicator`, `#lblNextSchedule`, `#lblLastSchedule`

### 5b — Add new rules (dark theme `style.qss`)
- [ ] Add `#cleanerCategoryCard` rules (background, border, border-radius, padding)
- [ ] Add `#cleanerCategoryCard[checked="true"]` rule (orange border 2px)
- [ ] Add `#cleanerCategoryCard #lblCatName` rules (font-weight 600, color)
- [ ] Add `#cleanerCategoryCard #lblCatSubtitle` rules (mono font, secondary color, smaller size)
- [ ] Add `#cleanerCategoryCard #lblCatSize` rules (secondary color)
- [ ] Add `#lblCleanerSubtitle` rule (secondary color, padding)
- [ ] Add `#btnScheduleCleaner` and `:hover` rules (outlined style)
- [ ] Add `#btnScanSystem` and `:hover` rules (orange filled, same as old `#btnScan`)
- [ ] Add `#cleanerFooter` rules (elevated card background, border, border-radius)
- [ ] Add `#lblEstimatedLabel` rules (small caps, secondary color)
- [ ] Add `#lblEstimatedSize` rules (large, bold, `@successColor`)
- [ ] Add `#btnCleanSelected` and `:hover` rules (`@destructiveColor`, white text)
- [ ] Add `#btnViewResults` rules (transparent, `@accentColor` text)

### 5c — Light theme
- [ ] Mirror all new rules in `shared/nexis/static/themes/light/style/style.qss` (tokens resolve to light-theme colors automatically — just copy the rule structures)

---

## Task 6 — Build verification

- [ ] Incremental build: `cmake --build build -j$(sysctl -n hw.ncpu)`
- [ ] Resolve any remaining `ui->check*`, `ui->lbl*` compile errors
- [ ] Verify no regressions on page 1 (results tree) by checking `ui->treeWidgetScanResult`, `ui->btnClean`, `ui->btnBackToCategories`, `ui->checkSelectAll` still exist in .ui and compile

---

## Task 7 — Test suite

- [ ] Run: `ctest --test-dir build --output-on-failure`
- [ ] All existing tests must pass

---

## Task 8 — Documentation and tracking

- [ ] Update `CHANGELOG.md` under `[Unreleased]`:
  ```
  ### Changed
  - **System Cleaner redesign (FR-130):** Replaced icon-grid category selector with a
    two-column card layout matching the Nexis design system. Cards show source-path subtitles,
    update with scan results in-place, and display a "Clean selected" footer that eliminates
    the need to navigate to the detailed scan results tree for routine cleaning.
  ```
- [ ] Update `docs/APPLICATION_OVERVIEW.md` — System Cleaner section: note new card layout, in-place scan results, footer bar
- [ ] Mark FR-130 as `[x]` in `FEATURE_REQUESTS.md`, add resolution note + commit hash
- [ ] Commit: `feat(cleaner): redesign category page with card layout (FR-130)`
- [ ] Move `FR-130_research.md`, `FR-130_plan.md`, `FR-130_uat.md` to `backlog/Archive/`

---

## Acceptance Criteria

1. **No decorative icons** — no `lbl*Img` labels on the categories view
2. **Two-column card grid** — all 8 (Linux: 9) categories displayed as horizontal list cards
3. **Square checkboxes** — standard `QCheckBox` with no `accessibleName="circle"`; orange fill when checked via QSS `[checked="true"]` property
4. **Card border highlight** — checked cards have `border: 2px solid @accentColor`
5. **Source-path subtitles** — each card shows the mono-font path hints below the category name
6. **Page header** — "System Cleaner" title + subtitle + "Schedule…" + "Scan system" buttons visible without scrolling
7. **Sizes update on cards** — after scan, each card's size label updates in-place; page does NOT auto-navigate to results tree
8. **Footer appears after scan** — estimated recoverable total (green) + "Clean selected" (red) + "View scan results →" link
9. **"Clean selected" works** — cleans all files in checked categories without visiting the tree; cards reset after clean
10. **Sidebar badge** — shows count (integer) of checked categories; disappears when all unchecked
11. **Results tree preserved** — "View scan results →" navigates to existing tree; Back button returns to cards page
12. **`quickScan()` works** — tray Quick Actions still triggers a full scan correctly
13. **Schedule dialog opens** — "Schedule…" opens `ScheduleEditorDialog` modal correctly
14. **Both themes** — dark and light themes both apply new card styles correctly
15. **All existing tests pass**

---

## Rollback

All changes are isolated to `system_cleaner_page.*`, `style.qss` (light + dark), and `app.cpp`. The `.ui` changes are the riskiest — if needed, revert `system_cleaner_page.ui` to HEAD and re-add the programmatic construction. The results-tree page (page 1) is never touched.
