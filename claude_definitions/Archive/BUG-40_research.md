# BUG-40 Research — FR-16 UI Regressions: Layout Position and Theme Compliance

## Summary

The FR-16 (Scheduled/Automated Cleaning) implementation introduced UI elements to the Settings page, System Cleaner page, and three new dialogs. These additions have two categories of issues:

1. **Layout position:** The "Scheduled Cleaning" section is appended at rows 11–15 in the Settings page grid, which places it **below** the `lblCreatedBy` footer (row 10). The footer should always be the last visual element.
2. **Theme compliance:** All programmatically created dialogs and widgets use hardcoded colors (`"color: gray"`, `"color: #d4a017"`, `"color: red"`, `rgba(128,128,128,30)`) instead of the app's `@token`-based theme system. These hardcoded values break Auto/Light/Dark mode.

---

## Inventory of All UI Changes Introduced by FR-16

### 1. Settings Page — `initScheduledCleaning()` (settings_page.cpp:305–378)

**What was added (programmatically to the QGridLayout):**
- Row 11: `QLabel "Scheduled Cleaning"` section title (accessibleName="title")
- Row 12, cols 0-1: `QCheckBox "Enable automatic weekly cleaning"` (mChkQuickSetup)
- Row 12, cols 2-3: `QLabel` summary text (mLblQuickSetupSummary)
- Row 13, col 0: `QPushButton "Manage Schedules..."` (mBtnManageSchedules)
- Row 13, col 1: `QPushButton "View Cleaning History"` (mBtnViewHistory)
- Row 14, cols 0-1: `QCheckBox "Notify when junk exceeds"` (mChkThresholdAlert)
- Row 14, col 2: `QSpinBox` with " GB" suffix (mSpnThresholdGB)
- Row 15, cols 0-2: `QCheckBox "Show notification after scheduled clean"` (mChkCleaningNotifications)

**Issues found:**
| # | Issue | Severity |
|---|-------|----------|
| 1 | All widgets are placed at rows 11–15, **below** the `lblCreatedBy` footer at row 10. The footer ("Nexis v1.1.2 Luke Simpson") should be the last element on the page. The Scheduled Cleaning section should be inserted **above** the footer/spacer. | HIGH |
| 2 | `mLblQuickSetupSummary->setStyleSheet("color: gray; font-size: 11px;")` — hardcoded "gray" doesn't adapt to theme. Should use `@color06` (which is `#9a9996` in dark, `#77767b` in light). | MEDIUM |
| 3 | No drop shadow applied to new widgets (`mBtnManageSchedules`, `mBtnViewHistory`, `mSpnThresholdGB`) unlike existing Settings widgets which get `Utilities::addDropShadow(widgets, 50)` at line 150. | LOW |
| 4 | The new `QPushButton`s have no `accessibleName` property set (existing buttons in the app use `accessibleName="primary"` or similar for styled variants). They render as unstyled grey buttons. | LOW |

### 2. Manage Schedules Dialog — `onManageSchedules()` (settings_page.cpp:427–543)

**What was added:**
- `QDialog` with title "Manage Cleaning Schedules", min size 550×400
- `QScrollArea` containing schedule cards
- Each card: `QGroupBox` with `QCheckBox` (enable), `QLabel` name (bold), `QLabel` frequency, `QLabel` last run, `QPushButton "Edit"`, `QPushButton "Delete"`
- "Add Schedule" and "Close" buttons at bottom

**Issues found:**
| # | Issue | Severity |
|---|-------|----------|
| 5 | `QDialog` created with `QDialog dialog(this)` — no explicit background color set. The dialog background **does** inherit from the app stylesheet since `qApp->setStyleSheet()` is used. However, the QSS has no `QDialog { background-color: ... }` rule, so it uses the platform default. On macOS this is fine (native dialog chrome), but on Linux with some DEs, the dialog background may not match. | LOW |
| 6 | `freqLabel->setStyleSheet("color: gray;")` at line 464 — hardcoded color. | MEDIUM |
| 7 | `lastLabel->setStyleSheet("color: gray; font-size: 11px;")` at line 475 — hardcoded color. | MEDIUM |
| 8 | `QGroupBox` cards have no `objectName`, so they only get the generic `QGroupBox::title` QSS styling (bold gray). The card body itself has no explicit background — it inherits from the dialog. This is acceptable but the cards lack visual structure. | LOW |

### 3. Cleaning History Dialog — `onViewCleaningHistory()` (settings_page.cpp:546–591)

**What was added:**
- `QDialog` with title "Cleaning History", min size 600×400
- `QPlainTextEdit` (read-only) showing log contents
- "Clear History" and "Close" buttons

**Issues found:**
| # | Issue | Severity |
|---|-------|----------|
| 9 | Same `QDialog` background concern as #5 above. | LOW |
| 10 | `QPlainTextEdit` has no `objectName` — it inherits the global QSS for `QPlainTextEdit` (border, border-radius, background-color, font-size, color). This **is** theme-aware thanks to the global rules — no issue here. | N/A |

### 4. Schedule Editor Dialog — `schedule_editor_dialog.cpp` (entire file, 307 lines)

**What was added:**
- `QDialog` subclass with programmatic UI
- `QLineEdit` for schedule name
- `QGroupBox "Frequency"` with 4 `QRadioButton`s and conditional fields
- `QSpinBox` × 5 (every N days, day of month, hour, minute, min file age)
- `QComboBox` for day of week
- `QGroupBox "Categories to Clean"` with 6 `QCheckBox`es
- `QLabel` trash warning
- `QCheckBox` "Skip files newer than"
- `QLabel` error message
- `QPushButton` Save/Cancel

**Issues found:**
| # | Issue | Severity |
|---|-------|----------|
| 11 | `mLblTrashWarning->setStyleSheet("color: #d4a017; font-size: 11px;")` at line 144 — hardcoded gold color. This is a warning color that should ideally reference `@warningColor` (which is `#e5a50a` in the QSS values). The hardcoded `#d4a017` is close but not identical. | MEDIUM |
| 12 | `mLblError->setStyleSheet("color: red;")` at line 168 — hardcoded red. Should use `@destructiveColor` (`#e01b24` in dark, `#c01c28` in light) from the QSS token system. | MEDIUM |
| 13 | No `objectName` set on the dialog — QSS cannot target it specifically. Widget children (QLineEdit, QSpinBox, QComboBox, QRadioButton, QCheckBox, QPushButton) all inherit from global QSS rules and **are themed correctly** for text colors, borders, and backgrounds. | LOW |

### 5. System Cleaner — Schedule Indicator — `initScheduleIndicator()` (system_cleaner_page.cpp:504–538)

**What was added:**
- `QFrame` indicator panel with rounded corners
- Calendar emoji icon
- Two `QLabel`s: next schedule and last schedule

**Issues found:**
| # | Issue | Severity |
|---|-------|----------|
| 14 | `mScheduleIndicator->setStyleSheet("QFrame { background: rgba(128,128,128,30); border-radius: 6px; padding: 8px; }")` at line 512 — hardcoded semi-transparent gray. This is the same absolute RGBA in both dark and light mode. On dark backgrounds it's barely visible; on light backgrounds it creates a different visual weight. Should use a theme-aware color like `@cardBg` or a themed RGBA. | MEDIUM |
| 15 | `mLblLastSchedule->setStyleSheet("color: gray; font-size: 11px;")` at line 524 — hardcoded "gray". | MEDIUM |
| 16 | `mLblNextSchedule` has no explicit color — inherits from the page's QSS context. On the System Cleaner page, labels under `#cleanerCategories` have `color: @color05` in the QSS, but since this label is added programmatically outside the `#cleanerCategories` widget, it falls back to the `QLabel` default. On some themes this may be invisible. | MEDIUM |
| 17 | Calendar emoji (📅 U+1F4C5) rendering depends on platform emoji support. On Linux without emoji fonts, this renders as a missing glyph box. Should use a theme icon or bundled SVG instead. | LOW |

---

## Root Cause Analysis

### Layout Issue (Settings Page)

The `.ui` file defines rows 0–10, with `lblCreatedBy` at row 10. The `initScheduledCleaning()` method uses:
```cpp
grid->addWidget(lblTitle, 11, 0, 1, 6);
```
Row 11+ is **after** the footer. The correct approach is to insert before row 9 (the vertical spacer before the footer) and shift the spacer and footer down, OR to add the section into the `.ui` file directly.

### Theme Issue (Hardcoded Colors)

The app uses a sophisticated `@token` system in `style.qss` where color placeholders like `@color05`, `@color06`, `@color12`, `@accentColor` etc. are replaced at runtime by values from `values.ini` (dark) or light `values.ini`. The global stylesheet applied via `qApp->setStyleSheet()` ensures all standard widgets pick up themed colors.

However, when code calls `widget->setStyleSheet("color: gray")`, this **overrides** the global stylesheet for that widget. The local stylesheet takes priority, locking the color to the literal value regardless of theme.

**The correct patterns used elsewhere in the codebase:**
- Use `QLabel`'s QSS selectors like `#SettingsPage QLabel { color: @color12; }` for page-level styling
- Use `accessibleName` properties for styled button variants (`"primary"`, `"danger"`)
- Set `objectName` on programmatic widgets so they can be targeted by existing QSS rules
- Avoid inline `setStyleSheet()` calls with color values; if needed, use the app's color tokens programmatically

### Affected Theme Token Mapping

| Hardcoded Value | Should Be | Token | Dark Value | Light Value |
|----------------|-----------|-------|------------|-------------|
| `"color: gray"` | Dimmed text | `@color06` | `#9a9996` | `#77767b` |
| `"color: #d4a017"` | Warning | `@warningColor` | `#e5a50a` | `#e5a50a` |
| `"color: red"` | Error/destructive | `@destructiveColor` | `#e01b24` | `#c01c28` |
| `rgba(128,128,128,30)` | Card background | `@cardBg` | `#36363a` | `#ffffff` |

---

## Summary of All Issues (17 total)

| # | Location | Issue | Severity |
|---|----------|-------|----------|
| 1 | Settings page | Scheduled Cleaning section placed below footer (rows 11-15 vs footer at row 10) | HIGH |
| 2 | Settings page | Quick setup summary label hardcoded "gray" | MEDIUM |
| 3 | Settings page | No drop shadow on new widgets | LOW |
| 4 | Settings page | New buttons have no accessibleName | LOW |
| 5 | Manage Schedules dialog | No QDialog background rule in QSS | LOW |
| 6 | Manage Schedules dialog | Frequency label hardcoded "gray" | MEDIUM |
| 7 | Manage Schedules dialog | Last run label hardcoded "gray" | MEDIUM |
| 8 | Manage Schedules dialog | Schedule cards lack visual structure | LOW |
| 9 | Cleaning History dialog | No QDialog background rule in QSS | LOW |
| 10 | Cleaning History dialog | QPlainTextEdit correctly themed (no issue) | N/A |
| 11 | Schedule Editor dialog | Trash warning hardcoded "#d4a017" | MEDIUM |
| 12 | Schedule Editor dialog | Error label hardcoded "red" | MEDIUM |
| 13 | Schedule Editor dialog | No objectName on dialog | LOW |
| 14 | System Cleaner indicator | Background hardcoded rgba(128,128,128,30) | MEDIUM |
| 15 | System Cleaner indicator | Last schedule label hardcoded "gray" | MEDIUM |
| 16 | System Cleaner indicator | Next schedule label may lack themed color | MEDIUM |
| 17 | System Cleaner indicator | Emoji icon may not render on Linux | LOW |

**Critical (must fix):** #1 (layout position)
**Medium (theme compliance):** #2, #6, #7, #11, #12, #14, #15, #16
**Low (polish):** #3, #4, #5, #8, #9, #13, #17
