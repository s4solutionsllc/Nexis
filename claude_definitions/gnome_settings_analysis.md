# GNOME Settings Page — Deep Analysis

## 1. File Inventory (18 source files)

### Shared GUI (5 pages + 5 headers + 5 UI files)
| File | Purpose |
|------|---------|
| `shared/nexis/Pages/GnomeSettings/gnome_settings_page.h` | Main page header (tab container) |
| `shared/nexis/Pages/GnomeSettings/gnome_settings_page.cpp` | Tab container logic, schema-gated visibility |
| `shared/nexis/Pages/GnomeSettings/gnome_settings_page.ui` | 4-button tab bar + QStackedWidget |
| `shared/nexis/Pages/GnomeSettings/gnome_appearance_tab.h` | Appearance tab header |
| `shared/nexis/Pages/GnomeSettings/gnome_appearance_tab.cpp` | 17 settings via `org.gnome.desktop.interface` |
| `shared/nexis/Pages/GnomeSettings/gnome_appearance_tab.ui` | Flat grid layout, 17 label+widget rows |
| `shared/nexis/Pages/GnomeSettings/gnome_wm_tab.h` | Window Manager tab header |
| `shared/nexis/Pages/GnomeSettings/gnome_wm_tab.cpp` | 14 settings via `wm.preferences` + `mutter` |
| `shared/nexis/Pages/GnomeSettings/gnome_wm_tab.ui` | 2 QGroupBox sections |
| `shared/nexis/Pages/GnomeSettings/gnome_mouse_tab.h` | Mouse & Touchpad tab header |
| `shared/nexis/Pages/GnomeSettings/gnome_mouse_tab.cpp` | 10 settings via `peripherals.mouse` + `touchpad` |
| `shared/nexis/Pages/GnomeSettings/gnome_mouse_tab.ui` | 2 QGroupBox sections with sliders |
| `shared/nexis/Pages/GnomeSettings/gnome_desktop_tab.h` | Desktop tab header |
| `shared/nexis/Pages/GnomeSettings/gnome_desktop_tab.cpp` | 6 settings via `background` + `sound` |
| `shared/nexis/Pages/GnomeSettings/gnome_desktop_tab.ui` | 2 QGroupBox sections with browse buttons |

### Core Tool Layer (shared header + platform implementations)
| File | Purpose |
|------|---------|
| `shared/nexis-core/Tools/gnome_settings_tool.h` | Static API: `getS/setS`, `getB/setB`, `getI/setI`, `getD/setD` |
| `linux/nexis-core/Tools/gnome_settings_tool.cpp` | Linux impl: `gsettings get/set` via `CommandUtil::exec()` |
| `linux/nexis-core/Tools/gnome_settings_constants.h` | Maps to real GSettings schema paths + key names |
| `macos/nexis-core/Tools/gnome_settings_tool.cpp` | macOS impl: `defaults read/write` (skeleton, unused) |
| `macos/nexis-core/Tools/gnome_settings_constants.h` | Placeholder mappings to macOS `defaults` domains (incomplete) |

---

## 2. Architecture

```
App::init()
  │  checks ToolManager::checkGnomeSettings()
  │    Linux: gsettings available AND org.gnome.desktop.interface schema exists
  │    macOS: always false (page never shown)
  │
  ▼
GnomeSettingsPage (tab container)
  │  4 QPushButtons in QButtonGroup → QStackedWidget
  │  Hides tab buttons if their schemas are missing
  │
  ├─ [0] GnomeAppearanceTab    → org.gnome.desktop.interface (17 keys)
  ├─ [1] GnomeWmTab            → org.gnome.desktop.wm.preferences (9 keys)
  │                             → org.gnome.mutter (5 keys)
  ├─ [2] GnomeMouseTab         → org.gnome.desktop.peripherals.mouse (4 keys)
  │                             → org.gnome.desktop.peripherals.touchpad (6 keys)
  └─ [3] GnomeDesktopTab       → org.gnome.desktop.background (3 keys)
                                → org.gnome.desktop.sound (3 keys)
  │
  ▼ (each tab calls)
GnomeSettingsTool (static methods)
  │
  ▼
Linux: gsettings get/set <schema> <key> [value]
macOS: defaults read/write <domain> <key> [-type value]  (never reached at runtime)
```

### Key Design Patterns

1. **`mLoading` guard**: Every tab has a `bool mLoading` flag. During `loadSettings()`, the flag is `true` and all write callbacks early-return, preventing gsettings writes when populating the UI from current values.

2. **Immediate writes**: Every widget change results in an immediate `gsettings set` subprocess invocation. No batching, debouncing, or "Apply" button.

3. **Schema-gated visibility**: Each tab hides `QGroupBox` sections whose schemas are absent. The main page hides tab buttons whose schemas are absent. This gracefully handles partial GNOME installations.

4. **No `#ifdef` in GUI layer**: All platform divergence is in the core tool layer. The GUI compiles on both platforms but is only instantiated on Linux.

---

## 3. Complete Settings Inventory (47 keys across 7 schemas)

### Appearance Tab — `org.gnome.desktop.interface` (17 keys)

| UI Widget | GSettings Key | Type | Control | Notes |
|-----------|--------------|------|---------|-------|
| `cmbColorScheme` | `color-scheme` | String | QComboBox | "default", "prefer-dark", "prefer-light" |
| `editGtkTheme` | `gtk-theme` | String | QLineEdit | e.g. "Adwaita" |
| `editIconTheme` | `icon-theme` | String | QLineEdit | e.g. "Adwaita" |
| `editCursorTheme` | `cursor-theme` | String | QLineEdit | e.g. "Adwaita" |
| `spinCursorSize` | `cursor-size` | Int | QSpinBox | 1–96 |
| `editFont` | `font-name` | String | QLineEdit | e.g. "Cantarell 11" |
| `editDocFont` | `document-font-name` | String | QLineEdit | |
| `editMonoFont` | `monospace-font-name` | String | QLineEdit | |
| `spinTextScaling` | `text-scaling-factor` | Double | QDoubleSpinBox | 0.5–3.0, step 0.1 |
| `cmbAntialiasing` | `font-antialiasing` | String | QComboBox | "none", "grayscale", "rgba" |
| `cmbHinting` | `font-hinting` | String | QComboBox | "none", "slight", "medium", "full" |
| `chkAnimations` | `enable-animations` | Bool | QCheckBox | |
| `chkHotCorners` | `enable-hot-corners` | Bool | QCheckBox | |
| `cmbClockFormat` | `clock-format` | String | QComboBox | "12h", "24h" |
| `chkClockSeconds` | `clock-show-seconds` | Bool | QCheckBox | |
| `chkClockWeekday` | `clock-show-weekday` | Bool | QCheckBox | |
| `chkBatteryPct` | `show-battery-percentage` | Bool | QCheckBox | |

### Window Manager Tab — `org.gnome.desktop.wm.preferences` (9 keys)

| UI Widget | GSettings Key | Type | Control | Notes |
|-----------|--------------|------|---------|-------|
| `editButtonLayout` | `button-layout` | String | QLineEdit | e.g. "appmenu:close" |
| `cmbFocusMode` | `focus-mode` | String | QComboBox | "click", "sloppy", "mouse" |
| `editTitlebarFont` | `titlebar-font` | String | QLineEdit | |
| `spinWorkspaces` | `num-workspaces` | Int | QSpinBox | 1–36 |
| `cmbDblClick` | `action-double-click-titlebar` | String | QComboBox | toggle-maximize, minimize, lower, menu, none |
| `cmbMidClick` | `action-middle-click-titlebar` | String | QComboBox | same options |
| `cmbRightClick` | `action-right-click-titlebar` | String | QComboBox | same options |
| `chkAutoRaise` | `auto-raise` | Bool | QCheckBox | |
| `chkRaiseOnClick` | `raise-on-click` | Bool | QCheckBox | |

### Window Manager Tab — `org.gnome.mutter` (5 keys)

| UI Widget | GSettings Key | Type | Control |
|-----------|--------------|------|---------|
| `chkDynamicWorkspaces` | `dynamic-workspaces` | Bool | QCheckBox |
| `chkEdgeTiling` | `edge-tiling` | Bool | QCheckBox |
| `chkAutoMaximize` | `auto-maximize` | Bool | QCheckBox |
| `chkCenterNewWindows` | `center-new-windows` | Bool | QCheckBox |
| `chkWorkspacesPrimary` | `workspaces-only-on-primary` | Bool | QCheckBox |

### Mouse Tab — `org.gnome.desktop.peripherals.mouse` (4 keys)

| UI Widget | GSettings Key | Type | Control | Notes |
|-----------|--------------|------|---------|-------|
| `chkMouseNatural` | `natural-scroll` | Bool | QCheckBox | |
| `sliderMouseSpeed` | `speed` | Double | QSlider | -1.0..1.0 mapped to -100..100 |
| `cmbAccelProfile` | `accel-profile` | String | QComboBox | "default", "flat", "adaptive" |
| `chkLeftHanded` | `left-handed` | Bool | QCheckBox | |

### Mouse Tab — `org.gnome.desktop.peripherals.touchpad` (6 keys)

| UI Widget | GSettings Key | Type | Control | Notes |
|-----------|--------------|------|---------|-------|
| `chkTapToClick` | `tap-to-click` | Bool | QCheckBox | |
| `chkTouchpadNatural` | `natural-scroll` | Bool | QCheckBox | |
| `sliderTouchpadSpeed` | `speed` | Double | QSlider | -1.0..1.0 mapped to -100..100 |
| `chkTwoFingerScroll` | `two-finger-scrolling-enabled` | Bool | QCheckBox | |
| `chkEdgeScrolling` | `edge-scrolling-enabled` | Bool | QCheckBox | |
| `chkDisableTyping` | `disable-while-typing` | Bool | QCheckBox | |

### Desktop Tab — `org.gnome.desktop.background` (3 keys)

| UI Widget | GSettings Key | Type | Control | Notes |
|-----------|--------------|------|---------|-------|
| `editWallpaper` + `btnBrowseWallpaper` | `picture-uri` | String | QLineEdit + QPushButton | Prepends `file://` |
| `editWallpaperDark` + `btnBrowseWallpaperDark` | `picture-uri-dark` | String | QLineEdit + QPushButton | Prepends `file://` |
| `cmbPictureOptions` | `picture-options` | String | QComboBox | none, wallpaper, centered, scaled, stretched, zoom, spanned |

### Desktop Tab — `org.gnome.desktop.sound` (3 keys)

| UI Widget | GSettings Key | Type | Control |
|-----------|--------------|------|---------|
| `chkEventSounds` | `event-sounds` | Bool | QCheckBox |
| `chkInputFeedback` | `input-feedback-sounds` | Bool | QCheckBox |
| `chkVolumeOver100` | `allow-volume-above-100-percent` | Bool | QCheckBox |

---

## 4. QSS Styling

All GNOME Settings styling is in `shared/nexis/static/themes/default/style/style.qss` lines 1041–1104. No separate light theme QSS — light theme uses the same rules with different `values.ini` color tokens.

| Selector | What it styles |
|----------|---------------|
| `#GnomeSettingsPage QLabel` | Font 10pt, color `@color12` |
| `#GnomeSettingsPage QGroupBox` | Card style: 1px border, 12px radius, `@cardBg` background |
| `#GnomeSettingsPage QGroupBox::title` | Bold 11pt, `@color05`, positioned top-left |
| `#GnomeSettingsPage QPushButton[accessibleName="navBtn"]` | Pill-shaped tab buttons, accent color when checked |
| `#btnBrowseWallpaper`, `#btnBrowseWallpaperDark` | Browse button styling with border + hover |
| `#GnomeSettingsPage #lblMouseSpeedVal`, `#lblTouchpadSpeedVal` | Bold speed value labels |

---

## 5. Sidebar Integration

- **Sidebar button:** `btnGnomeSettings` in `app.ui` (QPushButton, checkable, 28×28 icon, part of `sidebarBtnGroup`)
- **Label:** Set at runtime: `ui->btnGnomeSettings->setText(tr("GNOME Settings"))` (app.cpp line 103)
- **Icon:** `gnome-settings.svg` (both default and light theme variants in QRC)
- **Click handler:** `App::on_btnGnomeSettings_clicked()` → `pageClick(gnomeSettingsPage)`
- **Conditional visibility:** Button hidden if `ToolManager::checkGnomeSettings()` returns false

---

## 6. Build System

- All GUI files live under `shared/` — compiled on **both** platforms via `GLOB_RECURSE`
- CMakeLists.txt includes `GnomeSettings` in `CMAKE_AUTOUIC_SEARCH_PATHS` (line 104) and `target_include_directories` (line 144)
- Platform gating is **runtime-only** via `ToolManager::checkGnomeSettings()`
- macOS compiles all 5 `.cpp` GUI files into `.o` objects but never instantiates them

---

## 7. macOS Status

The macOS core layer (`macos/nexis-core/Tools/`) has a skeleton implementation:
- `gnome_settings_tool.cpp` uses `defaults read/write` commands
- `gnome_settings_constants.h` has **placeholder mappings** — many distinct GNOME keys map to the same macOS key (e.g., `GTK_THEME`, `ICON_THEME`, `CURSOR_THEME`, `CURSOR_SIZE`, `FONT_NAME` all map to `AppleInterfaceStyle`)
- `ToolManager::checkGnomeSettings()` **hard-returns `false`** on macOS
- The page is compiled but never shown

---

## 8. Translation Coverage

~91 translatable strings across the GNOME Settings feature, found in all 26 `.ts` files. Strings include all UI labels, tab names, combo box options, group box titles, and the sidebar label.

---

## 9. Observations and Potential Issues

1. **No error feedback to user**: If `gsettings set` fails (wrong value format, permission denied), the error is logged via `qCritical()` but the UI widget still shows the new value. The user sees no indication that the change didn't apply.

2. **Subprocess per setting change**: Every widget change spawns a new `gsettings set` process. Rapidly changing a slider fires many subprocesses. No debouncing on sliders.

3. **Theme/font fields are plain QLineEdit**: Users must know the exact theme/font name. No browse dialog or autocomplete for GTK themes, icon themes, or fonts. Could use `QFontDialog` for font fields and scan `/usr/share/themes/` for theme names.

4. **macOS implementation is incomplete**: The constants file maps many unrelated GNOME keys to the same macOS defaults key. The feature is gated off on macOS but the code compiles, adding ~500 lines of dead object code to the macOS binary.

5. **Speed slider has no debounce**: The mouse/touchpad speed sliders call `gsettings set` on every `valueChanged` signal (every pixel of slider movement). This could spawn hundreds of subprocesses during a single drag. A `QTimer`-based debounce (e.g., 200ms) would be more efficient.

6. **Wallpaper path validation**: The wallpaper browse button accepts any image file but doesn't validate that the path is accessible to the GNOME desktop (e.g., paths on removable media or restricted directories).
