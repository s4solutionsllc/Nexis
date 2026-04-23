# FR-129 Research — Design System Alignment

## Overview

Six remaining sub-items after the initial QSS quick-wins committed in ffb4c8c. This document covers findings for each.

---

## FR-129g — Inter SemiBold (600) Not Bundled

### Current state
`main.cpp:336-339` registers four fonts at startup:
```cpp
QFontDatabase::addApplicationFont(":/static/font/Ubuntu-R.ttf");
QFontDatabase::addApplicationFont(":/static/font/Inter-Regular.ttf");
QFontDatabase::addApplicationFont(":/static/font/Inter-Bold.ttf");
QFontDatabase::addApplicationFont(":/static/font/JetBrainsMono-Regular.ttf");
```

`static.qrc` lists the same four files. `shared/nexis/static/font/` contains only these four.

### Weight-600 usage in QSS (style.qss)
These QSS rules specify `font-weight: 600` and are affected:
- Line 475: `#sidebar QPushButton:checked { font-weight: 600; }` — nav active label
- Line 488: `#sectionToggle { font-weight: 600; }` — sidebar eyebrows
- Line 734: `#metricTileTitle { font-weight: 600; }` — dashboard tile label
- Line 790: `#diskTileTitle { font-weight: 600; }`
- Line 800: `#diskHealthStatus { font-weight: 600; }`
- Line 835: `#networkTileTitle { font-weight: 600; }`
- Line 847: `#networkTileLabel { font-weight: 600; }`
- Line 877: `#editToolbarLabel { font-weight: 600; }`
- Line 940: `#summaryLabel { font-weight: 600; }`

Without Inter-SemiBold.ttf, Qt font matching uses the nearest registered weight → Inter-Bold (700), making labels visually heavier than the design spec intends.

### Fix
- Acquire `Inter-SemiBold.ttf` (Inter v4, OFL 1.1 licensed, available from rsms.me/inter)
- Add to `shared/nexis/static/font/`
- Register in `main.cpp` after the existing Inter-Bold registration
- Add to `static.qrc`

---

## FR-129h — Process Name Column Not in Mono Font

### Current state
`processes_page.cpp:309`:
```cpp
QStandardItem *cmd_i = new QStandardItem(proc.getCmd());
```
This is the last item in `createRow()` (column index 18, "Process"). All `QStandardItem` cells inherit the global `@fontFamily`. QSS cannot target individual columns of a `QTableView`, so the token approach used for service/startup/APT names cannot work here.

`updateRow()` at line 322 also sets column 18 via `setCell()`:
```cpp
setCell(18, proc.getCmd(), proc.getCmd(), ...);
```

### Fix
Set `Qt::FontRole` directly on the `QStandardItem` for the process-name column in both `createRow()` and `updateRow()`:

In `createRow()`:
```cpp
cmd_i->setFont(QFont(QStringLiteral("JetBrains Mono")));
```

In `updateRow()`, `setCell()` doesn't touch the font role; add a separate line after the `setCell(18, ...)` call:
```cpp
if (auto *item = mItemModel->item(row, 18))
    item->setFont(QFont(QStringLiteral("JetBrains Mono")));
```

This is simpler and more reliable than a `QStyledItemDelegate` since it doesn't require reimplementing paint logic.

---

## FR-129i — Command Palette Keyboard Hint Footer

### Current state
`command_palette.cpp` `buildLayout()` builds:
- `mSearchBox` (`QLineEdit`)
- `mResultsList` (`QListWidget`, max 320px)
- No footer

The design spec (kit.css `.nx-palette-foot`) specifies:
```css
.nx-palette-foot { display:flex; gap: 14px; padding: 8px 16px; border-top: 1px solid var(--border);
                   color: var(--fg3); font-size: 11px; background: var(--surface-01); }
```
Showing hint labels: `↑↓ navigate  ↵ select  esc close`

### Fix
Add a footer `QWidget` inside `buildLayout()`:
- `QHBoxLayout` with 3 `QLabel` widgets styled in mono font, small size, tertiary color
- `setObjectName("commandPaletteFooter")` on the widget
- New QSS rules for `#commandPaletteFooter` and `#commandPaletteFooter QLabel`
- Use the existing `@monoFontFamily` token for the hint text font

Key detail: the hints use Unicode glyphs (↑↓, ↵, esc as plain text) as functional indicators, not decorative emoji — consistent with the design spec.

---

## FR-129j — Service Description Label Not in Mono Font

### Current state
`service_item.cpp:22`: `ui->lblServiceDescription->setText("- " + description)` — service descriptions are systemd unit descriptions/paths like `"OpenSSH Daemon"`.

QSS rule at `style.qss:1270`:
```qss
#ServiceItem #lblServiceDescription {
    font-size: 9pt;
    color: @color06;
}
```
No `font-family` specified — inherits global `@fontFamily`.

The design spec treats service description text as a mono/path-style string (`.nx-list-desc { font-family: var(--font-mono) }`).

### Fix
Add `font-family: @monoFontFamily;` to the existing QSS rule — one line.

---

## FR-129k — Logo SVG Font Family

### Current state
Four SVG files use `font-family="Helvetica Neue, Arial, sans-serif"`:

| File | Note |
|------|------|
| `shared/nexis/static/themes/default/img/sidebar-icons/sidebar-logo.svg` | Full NEXIS wordmark, dark theme |
| `shared/nexis/static/themes/default/img/sidebar-icons/sidebar-logo-collapsed.svg` | Single "N" glyph, dark |
| `shared/nexis/static/themes/light/img/sidebar-icons/sidebar-logo.svg` | Full wordmark, light theme |
| `shared/nexis/static/themes/light/img/sidebar-icons/sidebar-logo-collapsed.svg` | Single "N", light |

Qt renders SVG text using the system font engine and `QFontDatabase`. Since Inter is registered via `addApplicationFont`, listing it first in `font-family` will use the registered Inter font for the wordmark.

### Fix
Change `font-family="Helvetica Neue, Arial, sans-serif"` → `font-family="Inter, Helvetica Neue, Arial, sans-serif"` in all four SVGs.

Note: the branding SVGs under `shared/nexis/static/branding/` use `SF Pro Display / Helvetica Neue / Segoe UI` — those are for external/marketing use and not rendered in the app, so they are out of scope here.

---

## FR-129l — Process Table Font-Size Inconsistency

### Current state
Global rule `style.qss`:
```qss
QTableView::item {
    font-size: 10pt;
    ...
}
```
10pt = ~13.3px. Design spec `.nx-table-row { font-size: 12px; }` = ~9pt.

The denser 9pt size matches the design's intent for data-heavy tables. The `#HardwareInfoPage QTableWidget::item` is separately specified at `10pt` and should stay there (hardware info tables are wider, less dense).

The processes table uses `QTableView` with object name `tableProcess` (from `processes_page.ui`). It currently inherits the global 10pt.

### Fix
Add a targeted override:
```qss
#tableProcess QTableView::item,
#tableProcess::item {
    font-size: 9pt;
}
```
This narrows the change to the processes page only, leaving `HardwareInfoPage` and other `QTableView` instances untouched. Re-evaluate for `#portsTable` (open ports) separately.

---

## Files to Modify

| File | Sub-items |
|------|-----------|
| `shared/nexis/static/font/Inter-SemiBold.ttf` | g (new file) |
| `shared/nexis/static.qrc` | g |
| `shared/nexis/main.cpp` | g |
| `shared/nexis/Pages/Processes/processes_page.cpp` | h |
| `shared/nexis/command_palette.cpp` | i |
| `shared/nexis/command_palette.h` | i |
| `shared/nexis/static/themes/default/style/style.qss` | i, j, l |
| `shared/nexis/static/themes/default/img/sidebar-icons/sidebar-logo.svg` | k |
| `shared/nexis/static/themes/default/img/sidebar-icons/sidebar-logo-collapsed.svg` | k |
| `shared/nexis/static/themes/light/img/sidebar-icons/sidebar-logo.svg` | k |
| `shared/nexis/static/themes/light/img/sidebar-icons/sidebar-logo-collapsed.svg` | k |

## Platform Notes
All changes are cross-platform (shared/ code only). No macOS- or Linux-specific code paths involved. Font rendering for weight-600 may be slightly more visible improvement on macOS (CoreText renders intermediate weights better than FreeType on Linux) but the font file addition benefits both platforms.

## Screenshot Baseline Impact
Changes that will cause screenshot test mismatches:
- FR-129h: Process name column will render in mono font → `processes` baselines need regeneration
- FR-129i: Command palette footer (only if a screenshot includes the open palette — currently none in the reference set)
- FR-129j: Service description font change → `services` baselines need regeneration
- FR-129l: Process table item font-size reduction → `processes` baselines need regeneration
