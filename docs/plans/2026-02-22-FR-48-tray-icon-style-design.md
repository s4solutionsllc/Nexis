# FR-48: Tray Icon Style Selector — Design Document

**Date:** 2026-02-22
**Feature Request:** [FR-48](https://github.com/lsimpsonsfdc/Nexis/issues/5) by @Vai0Lou
**Status:** Approved

## Problem

GNOME and similar Linux desktop environments use monochrome symbolic icons in the system tray for visual consistency. The Nexis tray icon is a colorful multi-tone SVG (grays + orange/red/yellow gradient fills) that stands out from the rest of the panel. Users want the option to switch to a monochrome variant that blends with their DE's tray icon convention.

## Solution

Add a "Tray Icon" QComboBox to the Settings page offering 4 icon styles:

| Style | Data Value | Resource Path | Description |
|-------|-----------|---------------|-------------|
| Color (Default) | `"color"` | `:/static/tray-icon.svg` | Original colorful icon (existing) |
| Symbolic | `"symbolic"` | `:/static/tray-icon-symbolic.svg` | Flat `#BEBEBE` filled circle + dark N cutout |
| Outline | `"outline"` | `:/static/tray-icon-outline.svg` | `#BEBEBE` stroke circle + stroke N, no fills |
| Accent | `"accent"` | `:/static/tray-icon-accent.svg` | `#E95420` filled circle + dark `#1A1C22` N cutout |

Selection is persisted via `SettingManager` and applied immediately at runtime.

## Architecture

### Data Flow

```
User selects combo item
  → cmbTrayIconStyleChanged(int index)
    → SettingManager::setTrayIconStyle(style)
    → AppManager::updateTrayIcon()
      → reads SettingManager::getTrayIconStyle()
      → constructs resource path
      → mTrayIcon->setIcon(QIcon(path))
```

No new SignalMapper signal needed. The combo handler calls `updateTrayIcon()` directly, matching the pattern used by `cmbColorSchemeChanged` → `updateStylesheet()`.

`AppManager::updateTrayIcon()` is also called once during `AppManager` construction (after `mTrayIcon` creation) so the saved preference is applied at startup.

### Settings Page Placement

The new label + combo pair goes in the Appearance column of the Settings grid:

- **Row 0, Column 4:** `lblTrayIconStyle` (QLabel, text: "Tray Icon")
- **Row 1, Column 4:** `cmbTrayIconStyle` (QComboBox, same sizing as `cmbColorScheme`)

This places it adjacent to the Appearance/Color Scheme combo (row 0-1, col 3), keeping all visual customization settings grouped together.

### Persistence

New `SettingManager` key and methods:

```cpp
// setting_manager.h — in SettingKeys namespace
const QString TrayIconStyle("TrayIconStyle");

// setting_manager.h — in SettingManager class
void setTrayIconStyle(const QString &value);
QString getTrayIconStyle() const;  // default: "color"
```

### SVG Assets

Three new simplified SVGs in `shared/nexis/static/`. The original `tray-icon.svg` (62KB, dozens of path groups) is too detailed for monochrome rendering at 16-22px. The new variants use a simplified N lettermark silhouette optimized for small sizes.

Mockups created at `docs/plans/FR-48-icon-examples/`:
- `option-a-symbolic.svg` — GNOME symbolic convention
- `option-b-outline.svg` — stroke-based minimal
- `option-c-accent.svg` — branded orange

Final production SVGs will be traced from the actual tray icon shape for accuracy.

## Files Changed

| File | Change |
|------|--------|
| `shared/nexis/Managers/setting_manager.h` | Add `TrayIconStyle` key + getter/setter declarations |
| `shared/nexis/Managers/setting_manager.cpp` | Implement `getTrayIconStyle()` / `setTrayIconStyle()` |
| `shared/nexis/Managers/app_manager.h` | Add `updateTrayIcon()` public method |
| `shared/nexis/Managers/app_manager.cpp` | Implement `updateTrayIcon()`, call after `mTrayIcon` construction |
| `shared/nexis/Pages/Settings/settings_page.ui` | Add `lblTrayIconStyle` (row 0, col 4) + `cmbTrayIconStyle` (row 1, col 4) |
| `shared/nexis/Pages/Settings/settings_page.h` | Add `cmbTrayIconStyleChanged(int)` slot declaration |
| `shared/nexis/Pages/Settings/settings_page.cpp` | Populate combo items, connect signal, implement handler, add to drop shadow list |
| `shared/nexis/static.qrc` | Register 3 new SVG files |
| `shared/nexis/static/tray-icon-symbolic.svg` | New: flat gray circle + dark N |
| `shared/nexis/static/tray-icon-outline.svg` | New: stroke circle + stroke N |
| `shared/nexis/static/tray-icon-accent.svg` | New: orange circle + dark N |

## Scope Boundaries

This feature does **not**:
- Change the application window icon (taskbar/dock icon) — only the system tray icon
- Integrate with GNOME's `-symbolic` icon theme naming convention (that requires installing to `/usr/share/icons/`, outside app scope)
- Affect the sidebar logo SVG
- Add theme-aware tray icon switching (e.g., auto-switching between light/dark monochrome) — the user's explicit choice is always respected
