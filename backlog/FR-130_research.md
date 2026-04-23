# FR-130 Research: System Cleaner Redesign

## Summary

Replace the current icon-grid category selector on the System Cleaner page with a design-system-compliant two-column card layout with square checkboxes, source-path subtitles, a page header with Schedule/Scan buttons, and an estimated-recoverable footer bar. The detailed file-tree results page is preserved as a secondary view.

---

## Current Architecture

### File Inventory

| File | Role |
|---|---|
| `shared/nexis/Pages/SystemCleaner/system_cleaner_page.h` | Class definition, enums, member vars |
| `shared/nexis/Pages/SystemCleaner/system_cleaner_page.cpp` | All logic |
| `shared/nexis/Pages/SystemCleaner/system_cleaner_page.ui` | UI layout (QStackedWidget, two pages) |
| `shared/nexis/Pages/SystemCleaner/byte_tree_widget.h/.cpp` | Tree item subclass with byte-aware sorting |
| `shared/nexis/Pages/SystemCleaner/schedule_editor_dialog.h/.cpp` | Schedule create/edit modal dialog |
| `shared/nexis/Pages/SystemCleaner/exclusion_manager_dialog.h/.cpp` | Exclusion rules gear dialog |
| `shared/nexis/static/themes/default/style/style.qss` (lines 1063–1202) | All System Cleaner QSS |

### UI Structure (current)

```
QVBoxLayout (outer, 15px L/R/B margins)
  QLabel#lblCleanerTitle  ("System Cleaner")
  QStackedWidget#stackedWidget
    [page 0] QWidget#cleanerCategories  (current index = 0)
      QGridLayout#gridLayout_3 (11 columns, rows 0-11)
        row 0: vertical spacer
        row 2: lbl*Img labels (48x48 SVG icons, cols 2-10)
        row 3: lbl* category labels (centered text, cols 2-10)
        row 4: check* QCheckBoxes (accessibleName="circle", cols 2-10)
        row 5: categoryTrendLabel QLabels (size trend, built in buildTrendRow())
        row 6: vertical spacer (fixed 10px)
        row 7: QPushButton#btnScan (colspan 8, centered orange)
        row 8: QCheckBox#checkSelectAllSystemScan ("Select All", RtL, colspan 8)
        row 9: QLabel#lblLoadingScanner (100x100, spinning GIF)
        row 11: vertical spacer
        spacers: cols 1 and 10 for horizontal padding
    [page 1] QWidget#cleanerPage  (scan results tree)
      QVBoxLayout
        QWidget#widgetCleaner
          QGridLayout
            [0,0] QPushButton#btnBackToCategories ("← Back")
            [0,1] QLabel#lblRemovedTotalSize
            [0,3] HBoxLayout: lblSortBy + QComboBox#cbSortBy
            [1-2, 0-3] QTreeWidget#treeWidgetScanResult
            [4,0] QCheckBox#checkSelectAll ("Select All")
            [4,3] QLabel#lblTotalBytes
            [5,0] spacer  [5,1] QPushButton#btnClean  [5,3] spacer
            [6,1] QLabel#lblLoadingCleaner
```

### Categories (enum `CleanCategories` + Linux addition)

| Enum | UI Col | Linux? | macOS? |
|---|---|---|---|
| PACKAGE_CACHE | 2 | ✓ | ✓ (brew) |
| CRASH_REPORTS | 3 | ✓ | ✓ |
| APPLICATION_LOGS | 4 | ✓ | ✓ |
| APPLICATION_CACHES | 5 | ✓ | ✓ |
| TRASH | 6 | ✓ | ✓ |
| DEV_TOOL_CACHES | 7 | ✓ | ✓ |
| BROKEN_SYMLINKS | 8 | ✓ | ✓ |
| BROWSER_PRIVACY | 9 | ✓ | ✓ |
| SNAP_FLATPAK_REVISIONS | 10 | ✓ (programmatic) | ✗ |

### Key Logic in system_cleaner_page.cpp

**`init()`** (lines 63–172):
- Calls `setPixmap()` lambda for all 8 icon labels
- Adds Snap/Flatpak row programmatically (Linux only, col 10)
- Calls `buildTrendRow()` and `refreshTrendCells()` (FR-114)
- Creates floating `mBtnExclusions` (QToolButton) overlay — top-right
- Configures `treeWidgetScanResult` columns and headers
- Calls `initScheduleIndicator()` — creates floating `mScheduleIndicator` QFrame overlay (bottom)

**`on_btnScan_clicked()`** (lines 518–578):
- Reads checkbox states from `ui->check*` members
- Reads label texts from `ui->lbl*` members (used as tree root titles)
- Hides `btnScan`, starts loading GIF, disables checkboxes
- Launches `systemScan()` on `QtConcurrent`
- `onScanFinished()` auto-navigates to page 1 (results tree)

**`on_btnClean_clicked()`** (lines 580–630):
- Reads tree widget checkbox states (individual file selections)
- Collects `mFilesToDelete`, `mCleanTrash`, `mCleanSnapFlatpak`
- Launches `systemClean()` on `QtConcurrent`

**`on_btnBackToCategories_clicked()`** (lines 632–654):
- Navigates stacked widget back to page 0
- Re-enables all checkboxes

**FR-114 trend cells** (lines 870–942):
- `buildTrendRow()` adds `QLabel#categoryTrendLabel` at grid row 5, one per category column
- `refreshTrendCells()` reads from `mCleanerService->getCategoryTrend(cat)` and updates labels
- Called after every scan via `onScanFinished()`

**Floating overlays** (positioned in `resizeEvent` / `showEvent`):
- `mBtnExclusions` — top-right, `repositionExclusionsButton()` pins it at `(width - 32 - 15, 8)`
- `mScheduleIndicator` — bottom, `repositionScheduleIndicator()` pins at `(15, height - indicatorH - 15)`

### Sidebar Badge System (`app.cpp`)

- `mCleanerBadge` (QLabel, `#sidebarBadge`) — floating over `btnSystemCleaner`
- `mCleanerBadgeDot` (QLabel, `#sidebarBadgeDot`) — shown when sidebar collapsed
- Currently driven by `SignalMapper::sigCleanableSizeChanged` (emits scheduled cleaner byte count)
- `repositionBadges()` (line 1198) positions via `btn->mapTo(sidebar, 0,0)`
- Badge text currently shows formatted bytes; design calls for count of checked categories

### QSS (lines 1063–1202, `style.qss`)

```qss
#lblCleanerTitle { color: @color11; padding: 10 0; font-size: 11pt; }

#cleanerCategories QCheckBox[accessibleName=circle]::indicator { width: @dp20; height: @dp20; }
#cleanerCategories QLabel, #checkSelectAllSystemScan { font-size: 10pt; color: @color05; }

#SystemCleanerPage #btnScan {
    border: 0; border-radius: @dp6;
    background-color: @accentColor; color: @color07;
    font-size: 12pt; font-weight: bold; padding: @dp8 @dp24;
}
#SystemCleanerPage #btnScan:hover { background-color: @accentHover; }

#SystemCleanerPage #btnClean {
    border: 0;
    background: url(:/static/themes/@themeName/img/clean.png) no-repeat center;
}

#btnBackToCategories { border: 0; font-size: 11pt; color: @accentColor; background-color: @accentBgTint; }
#scheduleIndicator { background-color: @cardBg; border: 1px solid @borderColor; border-radius: 8; }
#lblRemovedTotalSize { font-size: 11pt; color: @successColor; }
#lblTotalBytes { font-size: 11pt; color: @color05; }
```

### Relevant Theme Tokens

| Token | Dark value | Meaning |
|---|---|---|
| `@color05` / `@color11` | `#F0F2F5` | Primary text |
| `@color06` / `@color04` | `#9A9DA6` | Secondary/muted text |
| `@cardBg` | `#2A2C32` | Card surface background |
| `@cardBgElevated` | `#32343A` | Elevated card (use for footer) |
| `@borderColor` | `#4A4D5A` | Default border |
| `@accentColor` | `#FF6B1A` | Orange accent |
| `@accentHover` | `#E95420` | Orange hover |
| `@accentBgTint` | `#3D2A22` | Orange tint background |
| `@successColor` | `#2ec27e` | Green (estimated size) |
| `@destructiveColor` | `#E05454` | Red/coral (clean button) |
| `@monoFontFamily` | resolved in AppManager | JetBrains Mono, SF Mono, Menlo, … |

`@monoFontFamily` is resolved programmatically in `AppManager::updateStylesheet()` — not a values.ini entry.

### Schedule Editor Dialog Invocation

`ScheduleEditorDialog` is a modal QDialog (`.exec()`). It emits `scheduleCreated` or `scheduleUpdated` signals. Currently only invoked from `SettingsPage`. We need a "Schedule…" button on the cleaner page that opens it for *creating* new schedules (no existing schedule to edit).

---

## Design Delta (Current → New)

### Page 0 (Categories View) Changes

| Element | Current | New |
|---|---|---|
| Page title | `QLabel#lblCleanerTitle` (outside stacked widget) | Keep, integrate into header row |
| Subtitle | None | Add `QLabel#lblCleanerSubtitle` |
| Actions (top-right) | None | "Schedule…" + "Scan system" buttons |
| Category display | Icon (48px SVG) + centered label + circular checkbox + trend label | Horizontal card: square checkbox + name (bold) + subtitle path + size (right) |
| Card selection feedback | None (circular checkbox only) | Card border → `@accentColor` 2px when checked |
| Scan button | `#btnScan` (centered, large orange) | Repurposed as "Scan system" in header |
| Select All toggle | `#checkSelectAllSystemScan` (below scan button) | Remove from UI; optionally keep as text link in footer |
| Footer bar | None | Estimated recoverable total + "Clean selected" button |
| Sidebar badge | Bytes from `sigCleanableSizeChanged` | Add count of checked categories |

### Page 1 (Results Tree) — Preserved As-Is

The tree results page is kept. A "View scan results →" link/button is added to the footer of page 0 after a scan completes, for users who want individual file selection.

---

## Implementation Approach

### Strategy: Programmatic rebuild of page 0 only

The `.ui` file's `cleanerCategories` widget will be simplified to an empty QVBoxLayout container. All category card construction and header/footer wiring will be done in `init()`. This avoids the brittle 11-column grid in the .ui and makes the Snap/Flatpak conditional addition cleaner.

The `.ui` `cleanerPage` (page 1) is untouched.

### New Data Structures

```cpp
struct CategoryCard {
    QFrame      *frame    = nullptr;  // the card widget
    QCheckBox   *check    = nullptr;  // square checkbox (left)
    QLabel      *lblSize  = nullptr;  // size display (right)
    quint64      lastSize = 0;        // size from last scan (0 = not scanned)
};

// Indexed by CleanCategories enum value — same ordering as the enum
QVector<CategoryCard> mCards;

// New footer / header widgets
QPushButton *mBtnScanSystem     = nullptr;  // top-right "Scan system"
QPushButton *mBtnSchedule       = nullptr;  // top-right "Schedule…"
QPushButton *mBtnCleanSelected  = nullptr;  // footer "Clean selected"
QPushButton *mBtnViewResults    = nullptr;  // footer "View scan results →"
QLabel      *mLblEstimated      = nullptr;  // footer total size
QFrame      *mCleanerFooter     = nullptr;  // full-width footer frame

bool mHasScanned = false;
```

### Source-Path Subtitles (static, per-platform)

```
PACKAGE_CACHE:
  Linux: "apt · dnf · pacman · zypper"
  macOS: "brew · ~/Library/Caches (brew)"

CRASH_REPORTS:
  Linux: "/var/crash · ~/.xsession-errors"
  macOS: "~/Library/Logs/DiagnosticReports"

APPLICATION_LOGS:
  Linux: "journald · ~/.cache/*.log · /var/log"
  macOS: "~/Library/Logs · /var/log"

APPLICATION_CACHES:
  Linux: "~/.cache/*"
  macOS: "~/Library/Caches"

TRASH:
  Linux: "~/.local/share/Trash"
  macOS: "~/.Trash"

DEV_TOOL_CACHES (both): "~/.electron · ~/.cache/vscode* · ~/.npm/_cache"

BROKEN_SYMLINKS (both): "~/ recursive symlink scan"

BROWSER_PRIVACY (both): "Chrome · Firefox · Chromium · Safari"

SNAP_FLATPAK_REVISIONS (Linux only): "snap · flatpak unused runtimes"
```

### New Scan Flow

After `onScanFinished()`:
1. Update `mCards[cat].lblSize` for each scanned category with the actual size
2. Update `mCards[cat].lastSize`
3. Recompute `mLblEstimated` total (sum of `lastSize` for checked cards)
4. Show `mCleanerFooter` with updated total and enable `mBtnCleanSelected`
5. Show `mBtnViewResults` link in footer
6. **Do NOT** auto-navigate to page 1 (results tree)

### New "Clean selected" Flow (page 0)

`mBtnCleanSelected` calls a new `quickCleanByCategory()` method:
- Reads which cards are checked + have a non-zero `lastSize`
- Builds `mFilesToDelete`, `mCleanTrash`, `mCleanSnapFlatpak` from last scan data
- Launches `systemClean()` on worker thread (reusing existing worker)
- On `onCleanFinished()`: reset card sizes to 0, hide footer, keep on page 0

This requires retaining the file lists from the last scan. Currently `onScanFinished()` clears them (`mPackageCaches.clear()` etc.) after populating the tree. We'll retain them in new member variables only when not navigating to page 1.

### Sidebar Badge Update

Add a `updateCleanerCheckBadge()` helper called whenever any card checkbox toggles:
```cpp
void SystemCleanerPage::updateCleanerCheckBadge()
{
    int checked = 0;
    for (const CategoryCard &c : mCards)
        if (c.check && c.check->isChecked()) ++checked;
    emit checkedCategoryCountChanged(checked);
}
```

Add signal `checkedCategoryCountChanged(int)` to `SystemCleanerPage`. Wire in `app.cpp`:
```cpp
connect(systemCleanerPage, &SystemCleanerPage::checkedCategoryCountChanged,
        this, [this](int count) {
    if (count > 0) {
        mCleanerBadge->setText(QString::number(count));
        repositionBadges();
    } else {
        mCleanerBadge->clear();
        mCleanerBadge->hide();
        mCleanerBadgeDot->hide();
    }
});
```

The existing `sigCleanableSizeChanged` badge (bytes from scheduled cleaner) is retained as a fallback when no manual check count is active.

### QSS Changes

New rules to add:
```qss
/* Category card */
#cleanerCategoryCard {
    background-color: @cardBg;
    border: 1px solid @borderColor;
    border-radius: 8;
    padding: 10 12;
}
#cleanerCategoryCard[checked="true"] {
    border: 2px solid @accentColor;
}

/* Category name label */
#cleanerCategoryCard #lblCatName {
    font-size: 10pt;
    font-weight: 600;
    color: @color05;
}

/* Source-path subtitle */
#cleanerCategoryCard #lblCatSubtitle {
    font-size: 8pt;
    color: @color06;
    font-family: @monoFontFamily;
}

/* Size label */
#cleanerCategoryCard #lblCatSize {
    font-size: 9pt;
    color: @color06;
}

/* Cleaner page header subtitle */
#lblCleanerSubtitle {
    font-size: 9pt;
    color: @color06;
    padding-bottom: 10;
}

/* Schedule button — outlined */
#btnScheduleCleaner {
    border: 1px solid @borderColor;
    border-radius: @dp6;
    background-color: transparent;
    color: @color05;
    padding: @dp6 @dp14;
}
#btnScheduleCleaner:hover { background-color: @cardBg; }

/* Scan system button — primary orange */
#btnScanSystem {
    border: 0;
    border-radius: @dp6;
    background-color: @accentColor;
    color: @color07;
    font-weight: 700;
    padding: @dp6 @dp16;
}
#btnScanSystem:hover { background-color: @accentHover; }

/* Footer bar */
#cleanerFooter {
    background-color: @cardBgElevated;
    border: 1px solid @borderColor;
    border-radius: 8;
    padding: 10 14;
}
#lblEstimatedLabel {
    font-size: 7pt;
    color: @color06;
    letter-spacing: 1;
}
#lblEstimatedSize {
    font-size: 14pt;
    font-weight: 700;
    color: @successColor;
}
#btnCleanSelected {
    border: 0;
    border-radius: @dp6;
    background-color: @destructiveColor;
    color: @color07;
    font-weight: 700;
    padding: @dp6 @dp16;
}
#btnCleanSelected:hover { opacity: 0.85; }
#btnViewResults {
    border: 0;
    background: transparent;
    color: @accentColor;
    font-size: 9pt;
}
```

Remove / replace:
- `#cleanerCategories QCheckBox[accessibleName=circle]::indicator` — replaced by standard QCheckBox styling
- `#cleanerCategories QLabel, #checkSelectAllSystemScan` — widgets removed
- `#SystemCleanerPage #btnScan` — repurposed; keep rule for quickScan() compat or rename

---

## Edge Cases & Risks

1. **FR-114 trend cells** — currently placed at grid row 5 in the icon-grid. New design: put size trend as tooltip on the card's `lblCatSize` label (same data, less visual clutter).
2. **`quickScan()` public method** — called from `app.cpp:765` for the tray Quick Actions submenu (FR-125). It directly manipulates `ui->check*`, `ui->btnScan`, etc. Must be updated to work with the new card widgets.
3. **`mBtnExclusions` floating button** — currently repositioned in `resizeEvent`. Will need updated coordinates since the top-right corner now has Schedule/Scan buttons in the header. Move exclusions gear into the header row instead of floating.
4. **`mScheduleIndicator` floating overlay** — currently shown at the bottom of page 0. The new footer bar occupies that zone. The schedule indicator should be moved to appear above the footer, or integrated as a schedule status row within the footer.
5. **Scan result file lists cleared in `onScanFinished()`** — retain them (don't clear) when on page 0 so "Clean selected" can use them. Clear on `on_btnBackToCategories_clicked()` or after clean completes.
6. **`quickScan()` disables `ui->checkSelectAllSystemScan`** — must be updated to disable the new card checkboxes.
7. **`.ui` widget name references** — many widget names (`ui->checkPackageCache`, `ui->lblPackageCache`, etc.) are referenced throughout `system_cleaner_page.cpp`. After redesign they become members of `CategoryCard` accessed via `mCards`. All references must be updated.
8. **Platform differences** — Snap/Flatpak card only added on Linux. macOS: Package Caches subtitle says brew, not apt.
