# BUG-38: HardwareInfoPage table styling — alternating rows illegible in dark mode

## Problem

The HardwareInfoPage tables have light-coloured alternating row backgrounds with white text, making rows illegible in dark mode. Every other row shows light text on a light (near-white) background.

## Root Cause Analysis

Two issues combine to cause this:

### 1. `alternatingRowColors` is enabled but no dark `alternate-background-color` is defined

All 7 `QTableWidget` instances in `hardware_info_page.ui` set `<property name="alternatingRowColors"><bool>true</bool></property>`. When `alternatingRowColors` is enabled, Qt uses its built-in `alternate-background-color` palette role for even/odd rows. Since neither the global QSS nor the page-specific QSS defines `alternate-background-color`, Qt falls back to the **system palette** — which on macOS is a light grey/white colour. This creates light-background rows with `@color05` (white) text = illegible.

No other page in the app uses `alternatingRowColors`. The Helpers page `tableViewHosts` does NOT have it set.

### 2. The page-specific QSS item rule uses `background-color: transparent`

The current HardwareInfoPage QSS sets:
```css
#HardwareInfoPage QTableWidget::item {
    color: @color05;
    background-color: transparent;
    padding: 4 8;
}
```

With `background-color: transparent`, non-alternating rows correctly show through to the `@cardBg` parent. But alternating rows still get the system palette colour because Qt's alternating row painting happens **beneath** the item delegate — `transparent` on the item doesn't override the alternate-row colour; it lets it show through.

### 3. Comparison: How the Helpers page works

The Helpers page uses a `QTableView` (not `QTableWidget`) and relies entirely on the **global** QSS rules:

```css
QTableView {
    background-color: transparent;
    color: @color05;
    font-size: 10pt;
    gridline-color: @borderColor;
    border-radius: 6;
}

QTableView::item {
    font-size: 10pt;
    color: @color05;
    padding: 8 6;
    background-color: @color01;
}

QTableView::item:selected {
    background-color: @color02;
}

QHeaderView::section {
    background-color: @color02;
    border-width: 0 0 1 0;
    border-style: solid;
    border-color: @borderColor;
    font-size: 10pt;
    color: @accentColor;
    padding: 8 12;
}
```

Key points:
- `alternatingRowColors` is **not** enabled
- Every item gets `background-color: @color01` (dark: `#36363a`) — an opaque dark background
- Text is `color: @color05` (white) — always legible
- The table itself is `background-color: transparent`
- Headers use `@color02` background with `@accentColor` text
- No `frameShape` override — it inherits globally

## Plan

### Step 1: Remove `alternatingRowColors` from all QTableWidget instances in `hardware_info_page.ui`

Remove the `<property name="alternatingRowColors"><bool>true</bool></property>` line from all 7 table widgets (`tblSystem`, `tblProcessor`, `tblGraphics`, `tblMemory`, `tblStorage`, `tblNetwork`, `tblThermal`).

This matches the Helpers page approach where no table uses alternating row colours.

### Step 2: Update QSS to match the global QTableView item styling

Replace the current `#HardwareInfoPage QTableWidget::item` rule to use an **opaque** dark background instead of `transparent`:

```css
#HardwareInfoPage QTableWidget::item {
    font-size: 10pt;
    color: @color05;
    padding: 8 6;
    background-color: @color01;
}
```

This matches the global `QTableView::item` rule exactly — same font-size, color, padding, and opaque `@color01` background. Every row will have a consistent dark background with white text.

### Step 3: Ensure header sections inherit proper global styling

The current page-specific `QHeaderView::section` override only sets `background-color` and `color`, missing `border`, `font-size`, and `padding` from the global rule. Remove the page-specific override so it inherits the complete global `QHeaderView::section` rule. The global rule already provides:
- `background-color: @color02`
- `border-width: 0 0 1 0; border-style: solid; border-color: @borderColor`
- `font-size: 10pt`
- `color: @accentColor`
- `padding: 8 12`

### Step 4: Add `frameShape: NoFrame` to table widgets in the `.ui` file

The Helpers page `tableViewHosts` explicitly sets `<property name="frameShape"><enum>QFrame::NoFrame</enum></property>`. The HardwareInfoPage tables don't have this, which may cause a visible border frame. Add this property to all 7 table widgets for visual consistency.

### Step 5: Build and verify

Incremental build.

## Summary of Changes

| File | Change |
|------|--------|
| `hardware_info_page.ui` | Remove `alternatingRowColors` from all 7 tables; add `frameShape: NoFrame` to all 7 tables |
| `style.qss` | Update `#HardwareInfoPage QTableWidget::item` to use opaque `@color01` bg, `10pt` font, `8 6` padding; remove `#HardwareInfoPage QHeaderView::section` override |
