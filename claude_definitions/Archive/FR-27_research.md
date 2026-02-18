# FR-27 Research: Populate theme/font fields as dropdowns

## Problem

The GNOME Settings Appearance tab uses plain `QLineEdit` widgets for GTK Theme, Icon Theme, Cursor Theme, UI Font, Document Font, and Monospace Font. The WM tab uses a `QLineEdit` for Titlebar Font. Users must type exact theme/font names with no guidance on what's available on the system.

## Widgets to Convert

### Appearance Tab (`gnome_appearance_tab.ui` / `.cpp`)

| Widget Name | Label | Schema | Key | New Widget Type |
|---|---|---|---|---|
| `editGtkTheme` | GTK Theme | `INTERFACE` | `GTK_THEME` | Editable `QComboBox` |
| `editIconTheme` | Icon Theme | `INTERFACE` | `ICON_THEME` | Editable `QComboBox` |
| `editCursorTheme` | Cursor Theme | `INTERFACE` | `CURSOR_THEME` | Editable `QComboBox` |
| `editFont` | UI Font | `INTERFACE` | `FONT_NAME` | `QFontComboBox` + `QSpinBox` |
| `editDocFont` | Document Font | `INTERFACE` | `DOCUMENT_FONT` | `QFontComboBox` + `QSpinBox` |
| `editMonoFont` | Monospace Font | `INTERFACE` | `MONOSPACE_FONT` | `QFontComboBox` (monospace filter) + `QSpinBox` |

### WM Tab (`gnome_wm_tab.ui` / `.cpp`)

| Widget Name | Label | Schema | Key | New Widget Type |
|---|---|---|---|---|
| `editTitlebarFont` | Titlebar Font | `WM_PREFS` | `TITLEBAR_FONT` | `QFontComboBox` + `QSpinBox` |

## GNOME Font Value Format

GNOME stores font names as `"FontFamily Size"` (e.g., `"Ubuntu 11"`, `"Cantarell Bold 11"`). The value includes:
- Font family name (may contain spaces)
- Optional style modifier (e.g., "Bold", "Italic")
- Size as the last space-separated token

Parsing: split on last space — everything before is the family+style, the last token is the size.

## Theme Discovery (Linux)

### GTK Themes
- System-wide: `/usr/share/themes/*/gtk-3.0/` or `/usr/share/themes/*/gtk-4.0/`
- User: `~/.local/share/themes/*/gtk-3.0/` or `~/.local/share/themes/*/gtk-4.0/`
- The directory name is the theme name
- Validation: a valid GTK theme has a `gtk-3.0/` or `gtk-4.0/` subdirectory

### Icon Themes
- System-wide: `/usr/share/icons/*/index.theme`
- User: `~/.local/share/icons/*/index.theme`
- The directory name is the theme name
- Exclude: `default`, directories without `index.theme`

### Cursor Themes
- System-wide: `/usr/share/icons/*/cursors/` (cursor themes are typically within icon theme dirs)
- Also: `/usr/share/cursors/xorg-x11/` on some distros
- User: `~/.local/share/icons/*/cursors/`
- The directory name is the theme name

## Font Discovery

`QFontDatabase::families()` returns all installed font families. Available on both Linux and macOS via `Qt6::Gui` (already linked).

For monospace filtering: `QFontDatabase::isFixedPitch(family)` returns true for monospace fonts.

## Existing Combo Box Pattern

From `gnome_appearance_tab.cpp` (Color Scheme combo):

**Loading:**
```cpp
ui->cmbColorScheme->addItem(tr("Default"), "default");
// ...
QString colorScheme = GnomeSettingsTool::getS(...);
int csIdx = ui->cmbColorScheme->findData(colorScheme);
if (csIdx >= 0) ui->cmbColorScheme->setCurrentIndex(csIdx);
```

**Signal handling:**
```cpp
connect(ui->cmbColorScheme, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
    if (mLoading) return;
    QString prevVal = GnomeSettingsTool::getS(...);
    if (!GnomeSettingsTool::setS(..., ui->cmbColorScheme->currentData().toString())) {
        const QSignalBlocker blocker(ui->cmbColorScheme);
        int idx = ui->cmbColorScheme->findData(prevVal);
        if (idx >= 0) ui->cmbColorScheme->setCurrentIndex(idx);
        emit settingFailed(tr("Failed to apply Color Scheme"));
    }
});
```

For editable combo boxes, the signal pattern is different — use `currentTextChanged` or `editingFinished` on the line edit. For `QFontComboBox`, the signal is `currentFontChanged(QFont)`.

## UI Layout Considerations

Font fields currently occupy a single grid column (column 1). Converting to `QFontComboBox` + `QSpinBox` requires either:
1. **A `QHBoxLayout` in column 1** containing both widgets — keeps the grid structure intact.
2. **Expanding to 3 columns** — breaks the existing layout for all rows.

Option 1 is cleaner. Create a `QWidget` container with `QHBoxLayout` holding the font combo and size spin box.

## Platform Considerations

On macOS, the GNOME Settings page is already hidden when schemas don't exist (`schemaExists()` returns false). Theme scanning paths don't apply to macOS. The `QFontDatabase` API works cross-platform, but since the entire page is hidden on macOS, this is a non-issue.

## Files to Modify

1. `shared/nexis/Pages/GnomeSettings/gnome_appearance_tab.ui` — Replace 6 QLineEdit widgets
2. `shared/nexis/Pages/GnomeSettings/gnome_appearance_tab.h` — Add helper methods, possibly member pointers
3. `shared/nexis/Pages/GnomeSettings/gnome_appearance_tab.cpp` — Theme/font discovery, new signal connections, updated loadSettings
4. `shared/nexis/Pages/GnomeSettings/gnome_wm_tab.ui` — Replace 1 QLineEdit widget
5. `shared/nexis/Pages/GnomeSettings/gnome_wm_tab.h` — Add helper methods if needed
6. `shared/nexis/Pages/GnomeSettings/gnome_wm_tab.cpp` — Font combo + spin box, new connections, updated loadSettings
