# BUG-36 Research: System Cleaner "Total Size" label text invisible in dark mode

## Problem

The "Total size: X.XX MB" label on the System Cleaner scan results page is invisible in dark mode — dark text on dark background.

## Root Cause

`lblTotalBytes` (QLabel at row 4, column 3 in system_cleaner_page.ui, lines 706–719) has:
- No inline stylesheet in the .ui file
- No QSS rule targeting `#lblTotalBytes` in style.qss

Without explicit styling, Qt falls back to the system palette default text color, which is dark/black — invisible against the dark theme background.

## Comparison with Nearby Elements

In the System Cleaner QSS section (style.qss lines 548–649):

- `#cleanerCategories QLabel` → `color: @color05` (white in dark mode) ✅
- `#checkSelectAllSystemScan` → `color: @color05` ✅
- `#treeWidgetScanResult::item` → `color: @color11` ✅
- `#lblRemovedTotalSize` → `color: @successColor` (green) ✅
- `#lblTotalBytes` → **no rule** ❌

## Fix

Add `#lblTotalBytes { font-size: 11pt; color: @color05; }` to style.qss, immediately after the `#lblRemovedTotalSize` rule (line 649). Uses `@color05` (primary text color) and `11pt` font-size to match the style of neighbouring labels.

## Files

- `shared/nexis/static/themes/default/style/style.qss` — insert new rule after line 649
