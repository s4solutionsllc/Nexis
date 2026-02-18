# BUG-33 Research: Uninstaller "Purge" checkbox text invisible in dark mode

## Problem

The `chkPurge` QCheckBox ("Purge (also remove configuration files)") on the Uninstaller page has no visible text in dark mode. The text renders in a dark/black color against the dark background.

## Root Cause

In `shared/nexis/static/themes/default/style/style.qss`, the QCheckBox section (lines 137–170) defines:

1. **Generic `QCheckBox::indicator`** (lines 141–152): Sets custom toggle images for checked/unchecked states but **never sets a `color` property** on the `QCheckBox` base selector itself.

2. **`QCheckBox[accessibleName="circle"]`** (lines 154–157): Explicitly sets `color: @color06` — this is why the "circle" variant checkboxes (used for System Cleaner scan result toggles, etc.) display text correctly.

Since there is no `QCheckBox { color: ... }` rule, Qt falls back to the system palette default text color, which is typically black — invisible against the dark theme background.

## Affected Widget

- `chkPurge` in `shared/nexis/Pages/Uninstaller/uninstallerpage.ui` (lines 316–325)
- It is the **only** standard QCheckBox with visible text in the entire app
- All other checkboxes use `accessibleName="circle"` which has its own explicit color

## Fix

Add `QCheckBox { color: @color05; }` to the QSS before the `::indicator` rules. `@color05` is the primary text color (`#ffffff` in dark, `#241f31` in light).

## Insertion Point

Line 140 in style.qss — between the QCheckBox section header comment (line 137–139) and the `QCheckBox::indicator` rule (line 141).
