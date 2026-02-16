# FR-27 Implementation Plan: Populate theme/font fields as dropdowns

## Overview

Replace plain `QLineEdit` fields for GNOME themes and fonts with proper dropdowns:
- **Theme fields** (GTK Theme, Icon Theme, Cursor Theme) → editable `QComboBox` populated by scanning filesystem directories
- **Font fields** (UI Font, Document Font, Monospace Font, Titlebar Font) → `QFontComboBox` + `QSpinBox` pair, since GNOME stores fonts as `"FontFamily Size"`

7 widgets across 2 tabs (6 in Appearance, 1 in WM).

---

## Task 1: Update Appearance Tab UI — Theme Combo Boxes

- [x] In `gnome_appearance_tab.ui`, replace 3 `QLineEdit` widgets with `QComboBox`:
  - Row 1: `editGtkTheme` → `cmbGtkTheme` (QComboBox, editable, minWidth 200, PointingHandCursor)
  - Row 2: `editIconTheme` → `cmbIconTheme` (QComboBox, editable, minWidth 200, PointingHandCursor)
  - Row 3: `editCursorTheme` → `cmbCursorTheme` (QComboBox, editable, minWidth 200, PointingHandCursor)

---

## Task 2: Update Appearance Tab UI — Font Combo + Size Widgets

- [x] In `gnome_appearance_tab.ui`, replace 3 `QLineEdit` font widgets with composite widgets:
  - Row 5: `editFont` → `QWidget` container with `QHBoxLayout` holding `fontFont` (QFontComboBox) + `spinFontSize` (QSpinBox, range 6–72, default 11)
  - Row 6: `editDocFont` → container with `fontDocFont` (QFontComboBox) + `spinDocFontSize` (QSpinBox)
  - Row 7: `editMonoFont` → container with `fontMonoFont` (QFontComboBox) + `spinMonoFontSize` (QSpinBox)
  - All QFontComboBox widgets: minWidth 200, PointingHandCursor
  - All QSpinBox widgets: range 6–72, suffix `" pt"`, ClickFocus

**Note:** The monospace font combo will be filtered at runtime in Task 4 (using `QFontComboBox::setFontFilters(QFontComboBox::MonospacedFonts)`).

---

## Task 3: Update WM Tab UI — Titlebar Font

- [x] In `gnome_wm_tab.ui`, replace `editTitlebarFont` (row 2, column 1) with:
  - `QWidget` container with `QHBoxLayout` holding `fontTitlebarFont` (QFontComboBox) + `spinTitlebarFontSize` (QSpinBox, range 6–72)

---

## Task 4: Implement theme discovery and appearance tab logic

- [x] In `gnome_appearance_tab.h`:
  - Add `#include <QFontComboBox>` forward declaration if needed
  - Add private helper methods:
    - `QStringList discoverGtkThemes()`
    - `QStringList discoverIconThemes()`
    - `QStringList discoverCursorThemes()`
  - Add private helper: `static void parseFontValue(const QString &value, QString &family, int &size)` — splits GNOME `"FontFamily Size"` string into family and size components

- [x] In `gnome_appearance_tab.cpp`:
  - **Theme discovery methods:**
    - `discoverGtkThemes()`: scan `/usr/share/themes/` and `~/.local/share/themes/` for dirs containing `gtk-3.0/` or `gtk-4.0/` subdirectory. Return sorted unique list.
    - `discoverIconThemes()`: scan `/usr/share/icons/` and `~/.local/share/icons/` for dirs containing `index.theme`. Exclude `default`. Return sorted unique list.
    - `discoverCursorThemes()`: scan `/usr/share/icons/` and `~/.local/share/icons/` for dirs containing `cursors/` subdirectory. Return sorted unique list.
  - **`parseFontValue()`:** split on last space — everything before is family, last token parsed as int is size. If no valid size found, default to 11.
  - **Update `loadSettings()`:**
    - Theme combos: call discovery methods, `addItems()` to populate, then `setCurrentText()` with gsettings value. If the current value isn't in the list, editable combo handles it gracefully (shows typed text).
    - Font combos: `setCurrentFont(QFont(family))`, spin boxes: `setValue(size)` — parsed from gsettings value via `parseFontValue()`.
    - Monospace combo: `fontMonoFont->setFontFilters(QFontComboBox::MonospacedFonts)` before population.
  - **Update signal connections:**
    - Theme combos: replace `QLineEdit::editingFinished` with `QComboBox::currentTextChanged(QString)`. The lambda sends `currentText()` (not `currentData()`) to `setS()` since these are editable. On failure, revert with `QSignalBlocker` + `setCurrentText(prev)`.
    - Font combos: connect both `QFontComboBox::currentFontChanged` and `QSpinBox::valueChanged` to a shared lambda (or individual lambdas) that composes `"FontFamily Size"` and calls `setS()`. On failure, revert both widgets.

---

## Task 5: Implement WM tab titlebar font logic

- [x] In `gnome_wm_tab.cpp`:
  - Add `#include <QFontComboBox>` and `#include <QFontDatabase>` if needed
  - Reuse `parseFontValue()` — either duplicate as a static local or extract to a tiny shared header. Since it's only 2 call sites, a static free function in each `.cpp` is simplest.
  - **Update `loadSettings()`:** parse titlebar font value, set combo + spin box.
  - **Update signal connection:** replace `QLineEdit::editingFinished` with font combo + spin box signals that compose and write `"FontFamily Size"`.

---

## Task 6: Build verification and tracking

- [x] Incremental build to verify compilation.
- [x] Mark FR-27 as `[x]` in `FEATURE_REQUESTS.md` with resolution note.
- [x] Commit and push.
