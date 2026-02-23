# BUG-44 Research: Settings Page Layout Issues

## 1. Current State Analysis

### 1.1 Layout Structure

The Settings page uses a **single flat `QGridLayout`** (6 columns × ~16 rows) defined partly in `settings_page.ui` and partly built programmatically in `settings_page.cpp::initScheduledCleaning()`.

**UI file grid (settings_page.ui):**

| Row | Col 0 | Col 1 | Col 2 | Col 3 | Col 4 | Col 5 |
|-----|-------|-------|-------|-------|-------|-------|
| 0 | `lblLanguage` | `lblDisks` | `lblHomepage` | `lblAppearance` | `lblTrayIconStyle` | *(h-spacer)* |
| 1 | `cmbLanguages` | `cmbDisks` | `cmbStartPage` | `cmbColorScheme` | `cmbTrayIconStyle` | *(h-spacer)* |
| 2 | *(v-spacer, colspan 6)* | | | | | |
| 3 | `lblAlertMessages` (colspan 6) | | | — but also `cmbFont` here! | | |
| 4 | `lblCpuPercent` | `lblMemoryPercent` | `lblDiskPercent` | `lblBatteryHealthPercent` | | |
| 5 | `spinCpuPercent` | `spinMemoryPercent` | `spinDiskPercent` | `spinBatteryHealthPercent` | | |
| 6 | *(v-spacer, colspan 6)* | | | | | |
| 7 | `lblStartOnBoot` | `label` (App Quit Don't Ask) | `lblDiskAnalyzer` | `lblDiskHealthAlert` | `lblDiskAnalyzerCustomPath` | |
| 8 | `checkAutostart` | `checkAppQuitDontAsk` | `cmbDiskAnalyzer` | `checkDiskHealthAlert` | `txtDiskAnalyzerCustomPath` | |
| 9 | *(v-spacer)* | | | | | |
| 10 | `lblCreatedBy` (col 3-4, right-aligned) | | | | | |

**CRITICAL CONFLICT — Row 3 double-assignment:**
The `.ui` file places `lblAlertMessages` at `row="3" column="0" colspan="6"` (a full-width section title). But it ALSO places `lblFont` at `row="2" column="3"` and `cmbFont` at `row="3" column="3"`. This means row 3, column 3 has BOTH the "Alert messages" title (spanning all 6 cols) AND the Font combobox. Qt resolves this by growing the row to fit both, creating a layout collision.

Actually, re-reading more carefully: `lblFont` is at row 2, col 3 and `cmbFont` is at row 3, col 3. But row 2 is a full-width vertical spacer (colspan 6). So row 2 has BOTH a spacer AND `lblFont`. Similarly, row 3 has BOTH `lblAlertMessages` (colspan 6) AND `cmbFont`. This is the root cause of the layout confusion — the "Appearance" and "Font" settings are sharing rows with the vertical spacers and alert title.

### 1.2 Column Inconsistency (The Bug)

- **Rows 0-1:** 5 real items across cols 0-4, plus a horizontal spacer in col 5 = **6 columns**
- **Row 3 (Alert title):** colspan 6 = **6 columns**
- **Rows 4-5 (Alert spinboxes):** 4 items in cols 0-3 = **4 columns**
- **Rows 7-8 (Toggles row):** 5 items across cols 0-4 = **5 columns**

The row with "Disk Health Alert" at column 3 and "Custom Executable Path" at column 4 uses **5 populated columns**, while the alert spinbox rows only use 4. This means the DiskAnalyzerCustomPath (col 4) pushes out further than anything else, creating an unbalanced layout.

### 1.3 Programmatic Additions (Scheduled Cleaning)

`initScheduledCleaning()` adds rows 9-15 programmatically:

| Row | Content |
|-----|---------|
| 9 | `lblTitle` "Scheduled Cleaning" (colspan 6) |
| 10 | `mChkQuickSetup` (cols 0-1) + `mLblQuickSetupSummary` (cols 2-3) |
| 11 | `mBtnManageSchedules` (col 0) + `mBtnViewHistory` (col 1) |
| 12 | `mChkThresholdAlert` (cols 0-1) + `mSpnThresholdGB` (col 2) |
| 13 | `mChkCleaningNotifications` (cols 0-2) |
| 14 | *(re-added spacer)* |
| 15 | `lblCreatedBy` (cols 3-4, right-aligned) |

### 1.4 Minimum Size Issue

The `.ui` file sets the page geometry to 811×479 but doesn't set explicit `minimumSize`. However, the 6-column grid with `minimumWidth: 150-200` on each combobox creates an implicit minimum width of ~1000px (6 × ~160px + margins + spacing). The `txtDiskAnalyzerCustomPath` has `minimumWidth: 200`, contributing to this. This prevents the window from being resized smaller, which is especially problematic on small screens or when the sidebar is expanded.

### 1.5 Every Setting, Categorized by Purpose

**General / Application:**
1. Language (cmbLanguages) — UI language selection
2. Disks (cmbDisks) — Default disk for monitoring
3. Start Page (cmbStartPage) — Page shown at launch
4. Autostart Nexis (checkAutostart) — Launch on boot
5. App Quit Don't Ask (checkAppQuitDontAsk) — Skip quit confirmation dialog

**Appearance:**
6. Appearance / Color Scheme (cmbColorScheme) — Auto/Light/Dark theme
7. Font (cmbFont) — App font family
8. Tray Icon (cmbTrayIconStyle) — System tray icon style (Color/Symbolic/Outline/Accent)

**Alerts / Notifications:**
9. CPU Percent (spinCpuPercent) — CPU usage alert threshold
10. Memory Percent (spinMemoryPercent) — Memory usage alert threshold
11. Disk Percent (spinDiskPercent) — Disk usage alert threshold
12. Battery Health (spinBatteryHealthPercent) — Battery health alert threshold (hidden if no battery)
13. Disk Health Alert (checkDiskHealthAlert) — Enable/disable disk health alerts (hidden if no SMART data)

**Tools:**
14. Disk Analyzer (cmbDiskAnalyzer) — Preferred disk analyzer tool
15. Custom Executable Path (txtDiskAnalyzerCustomPath) — Custom analyzer path (shown only when "Custom..." is selected)

**Scheduled Cleaning:**
16. Enable automatic weekly cleaning (mChkQuickSetup) — Quick-setup toggle
17. Schedule summary label (mLblQuickSetupSummary) — Shows next scheduled run
18. Manage Schedules button (mBtnManageSchedules) — Opens schedule management dialog
19. View Cleaning History button (mBtnViewHistory) — Opens history dialog
20. Notify when junk exceeds N GB (mChkThresholdAlert + mSpnThresholdGB) — Threshold alert
21. Show notification after scheduled clean (mChkCleaningNotifications) — Post-clean notifications

**Footer:**
22. Nexis vX.X.X Luke Simpson (lblCreatedBy) — Version + author credit

## 2. Problems Identified

### P1: No visual grouping
All 21 settings are in a flat grid with no section containers. The only visual separator between "General" and "Alerts" is a thin vertical spacer (10px). Compare with GNOME Settings and Hardware Info pages, which use `QGroupBox` containers with titled, bordered, rounded cards (`@cardBg` background, `@borderColor` border, 12px radius).

### P2: Inconsistent column count
Rows use 4, 5, or 6 columns depending on the content. The "Toggles" row (7-8) uses 5 columns while the alert row (4-5) uses 4. The first row uses 5 real items + a spacer for 6 total.

### P3: Large implicit minimum width
Six side-by-side comboboxes with 150-200px minimum widths, plus margins (12px × 2) and spacing (12px × 5), creates ~1100px minimum content width. This is wider than many laptop screens and prevents meaningful window resizing.

### P4: Mixed UI/programmatic layout
Rows 0-10 are in the `.ui` file, rows 9-15 are built in C++. The initScheduledCleaning() function physically removes and re-adds the spacer and footer widgets to insert itself, a fragile pattern prone to row-numbering drift.

### P5: Row 2/3 collision
The Font label/combo shares rows with a vertical spacer and the Alert Messages title due to overlapping grid positions. Qt handles this by expanding cells, but the visual result is unpredictable spacing.

### P6: Poor discoverability on small screens
Settings below the fold (Autostart, App Quit Don't Ask, Disk Analyzer, Disk Health Alert, and the entire Scheduled Cleaning section) require scrolling on shorter displays, but the page has no `QScrollArea`.

### P7: Checkbox labels not descriptive
`checkAutostart` and `checkAppQuitDontAsk` have empty `text` properties — the label is in a separate QLabel above the checkbox. This is fine visually but differs from the Scheduled Cleaning section where checkboxes have inline labels. The inconsistency is minor but noticeable.

## 3. Comparison with Other Pages

### GNOME Settings Page (best reference)
- Uses `QGroupBox` containers with titled sections (Themes, Fonts, Interface, Clock & Status)
- Each group has a `QGridLayout` with label-in-col-0, control-in-col-1 pattern
- Scrollable via `QScrollArea`
- QSS gives groups: `border: 1px solid @borderColor; border-radius: 12; background-color: @cardBg;`
- Title styled: `font-size: 11pt; font-weight: bold; color: @color05;`

### Hardware Info Page (also uses groups)
- `QGroupBox` per hardware section (System, Processor, Graphics, etc.)
- Same QSS pattern: `@cardBg` background, `@borderColor` border, 12px radius
- Scrollable content area

Both pages look polished and organized. The Settings page looks like a first-draft grid with items randomly placed.

## 4. Recommended Redesign

### 4.1 Structure: Vertical QScrollArea with QGroupBox Sections

Replace the flat 6-column grid with a vertical layout inside a scroll area, containing 4 QGroupBox sections:

```
QScrollArea
  └─ QWidget (scrollContent)
     └─ QVBoxLayout
        ├─ QGroupBox "General"
        │   └─ QGridLayout (2 columns: label | control)
        │      ├─ Language        | [cmbLanguages]
        │      ├─ Start Page      | [cmbStartPage]
        │      ├─ Default Disk    | [cmbDisks]
        │      ├─ Autostart Nexis | [checkAutostart]  (inline label on checkbox)
        │      └─ Skip Quit Dialog| [checkAppQuitDontAsk] (inline label on checkbox)
        │
        ├─ QGroupBox "Appearance"
        │   └─ QGridLayout (2 columns)
        │      ├─ Color Scheme    | [cmbColorScheme]
        │      ├─ Font            | [cmbFont]
        │      └─ Tray Icon Style | [cmbTrayIconStyle]
        │
        ├─ QGroupBox "Alerts"
        │   └─ QGridLayout (2 columns)
        │      ├─ CPU Usage       | [spinCpuPercent] %
        │      ├─ Memory Usage    | [spinMemoryPercent] %
        │      ├─ Disk Usage      | [spinDiskPercent] %
        │      ├─ Battery Health  | [spinBatteryHealthPercent] % (hidden if no battery)
        │      └─ Disk Health     | [checkDiskHealthAlert] (hidden if no SMART)
        │
        ├─ QGroupBox "Tools"
        │   └─ QGridLayout (2 columns)
        │      ├─ Disk Analyzer   | [cmbDiskAnalyzer]
        │      └─ Custom Path     | [txtDiskAnalyzerCustomPath] (shown when Custom selected)
        │
        ├─ QGroupBox "Scheduled Cleaning"
        │   └─ QVBoxLayout
        │      ├─ [checkAutoWeekly] Enable automatic weekly cleaning   [summaryLabel]
        │      ├─ [Manage Schedules] [View History]  (button row)
        │      ├─ [checkThreshold] Notify when junk exceeds [spinGB] GB
        │      └─ [checkNotifications] Show notification after scheduled clean
        │
        └─ lblCreatedBy (footer, right-aligned)
```

### 4.2 Key Design Decisions

1. **2-column label/control grid within each group** — Matches GNOME Settings pattern. Labels left-aligned, controls right-aligned or left-aligned with consistent widths.

2. **QScrollArea wrapper** — Handles small screens gracefully. All other content-heavy pages (Hardware Info, GNOME Settings) already use one.

3. **Reuse existing QGroupBox QSS** — The `#HardwareInfoPage QGroupBox` and `#GnomeSettingsPage QGroupBox` rules are identical. We can either:
   - Add `#SettingsPage QGroupBox` rules matching the same pattern, or
   - Create a shared `.settings-card` class applied to all three pages.

4. **Move checkboxes to inline labels** — Instead of a separate label above a blank checkbox, use `checkAutostart->setText("Autostart Nexis")`. This is more accessible and matches the Scheduled Cleaning section's pattern.

5. **Controls get consistent max-width** — All comboboxes and spinboxes get `max-width: 250px` to prevent them stretching across the full group width. Labels stretch to fill remaining space.

6. **Group-level subtitle for Alerts** — The current "Alert messages (Show a warning after the specified percentage)" title becomes the QGroupBox title or a subtitle label inside the Alerts group.

### 4.3 Implementation Approach

**Option A: Full `.ui` rebuild** — Rebuild `settings_page.ui` in Qt Designer with the new structure (QScrollArea → QVBoxLayout → QGroupBoxes). Move all widgets from the flat grid into their respective groups. Scheduled Cleaning widgets would also be defined in the `.ui` file, eliminating the programmatic `initScheduledCleaning()` entirely.

**Option B: Programmatic rebuild** — Replace `setupUi()` with fully programmatic layout construction in `settings_page.cpp`. Delete most of the `.ui` file content. More flexible but harder to maintain with Qt Designer.

**Option C: Hybrid** — Keep the `.ui` file for widget declarations but restructure the layout programmatically in `init()`. Move widgets from the flat grid into newly created QGroupBoxes in code.

**Recommended: Option A** — Full `.ui` rebuild is cleanest. The `.ui` file is the standard way to define Qt layouts, and the current one is small enough that a rebuild is straightforward. The Scheduled Cleaning section can also move into the `.ui` file, simplifying `settings_page.cpp` significantly.

### 4.4 QSS Additions Needed

```qss
#SettingsPage QGroupBox {
    border: 1px solid @borderColor;
    border-radius: 12;
    margin-top: 12;
    padding: 12 12 6 12;
    background-color: @cardBg;
}

#SettingsPage QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    padding: 0 6;
    color: @color05;
    font-size: 11pt;
    font-weight: bold;
}
```

This is identical to the existing GNOME Settings and Hardware Info patterns.

### 4.5 Minimum Size Fix

With a 2-column layout inside a scroll area, the minimum width drops from ~1100px to ~450px (label ~200px + control ~250px). The scroll area handles vertical overflow. This should eliminate the minimum size restriction issue entirely.

## 5. Summary of Issues vs. Fixes

| Issue | Current | Fix |
|-------|---------|-----|
| P1: No visual grouping | Flat grid, all items at same level | QGroupBox sections with card styling |
| P2: Inconsistent columns | 4, 5, or 6 columns per row | Consistent 2-column label/control within each group |
| P3: Large minimum width | ~1100px implicit minimum | ~450px with 2-column groups in scroll area |
| P4: Mixed UI/programmatic | Fragile row manipulation in code | All widgets in `.ui` file |
| P5: Row collisions | Font/spacer/title overlap on rows 2-3 | Separate groups, no overlapping |
| P6: No scrolling | Settings below fold invisible | QScrollArea wrapper |
| P7: Checkbox label inconsistency | Mix of separate-label and inline-label | All checkboxes use inline text |
