# FR-48: Tray Icon Style Selector — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a Settings page combo to choose between 4 tray icon styles (Color, Symbolic, Outline, Accent), applied immediately and persisted across sessions.

**Architecture:** New `TrayIconStyle` setting in `SettingManager`, new `updateTrayIcon()` method in `AppManager` that maps style string → resource path → `QSystemTrayIcon::setIcon()`. Settings page gets a QComboBox in the `.ui` grid alongside the existing Appearance combo. Three new SVG files for the monochrome variants.

**Tech Stack:** C++/Qt6, QSettings, QSystemTrayIcon, QComboBox, SVG

**Design Doc:** `docs/plans/2026-02-22-FR-48-tray-icon-style-design.md`

---

### Task 1: Add TrayIconStyle to SettingManager

**Files:**
- Modify: `shared/nexis/Managers/setting_manager.h:32` (after `AppFont` key)
- Modify: `shared/nexis/Managers/setting_manager.cpp:273` (after `getAppFont()`)

**Step 1: Add the setting key**

In `shared/nexis/Managers/setting_manager.h`, add after line 32 (`const QString AppFont("AppFont");`):

```cpp
    const QString TrayIconStyle("TrayIconStyle");
```

**Step 2: Add getter/setter declarations**

In `shared/nexis/Managers/setting_manager.h`, add after line 115 (`QString getAppFont() const;`):

```cpp
    void setTrayIconStyle(const QString &value);
    QString getTrayIconStyle() const;
```

**Step 3: Implement getter/setter**

In `shared/nexis/Managers/setting_manager.cpp`, add after `getAppFont()` (after line 272):

```cpp
void SettingManager::setTrayIconStyle(const QString &value)
{
    mSettings->setValue(SettingKeys::TrayIconStyle, value);
}

QString SettingManager::getTrayIconStyle() const
{
    return mSettings->value(SettingKeys::TrayIconStyle, "color").toString();
}
```

**Step 4: Build to verify compilation**

Run: `cmake --build build -j$(sysctl -n hw.ncpu)`
Expected: Clean build, zero errors.

**Step 5: Commit**

```bash
git add shared/nexis/Managers/setting_manager.h shared/nexis/Managers/setting_manager.cpp
git commit -m "feat(settings): add TrayIconStyle persistence key (FR-48)"
```

---

### Task 2: Create SVG assets and register in QRC

**Files:**
- Create: `shared/nexis/static/tray-icon-symbolic.svg`
- Create: `shared/nexis/static/tray-icon-outline.svg`
- Create: `shared/nexis/static/tray-icon-accent.svg`
- Modify: `shared/nexis/static.qrc:43` (after `tray-icon.svg` entry)

**Step 1: Create the symbolic SVG**

Create `shared/nexis/static/tray-icon-symbolic.svg`:

```xml
<?xml version="1.0" encoding="utf-8"?>
<svg version="1.1" xmlns="http://www.w3.org/2000/svg" width="128" height="128" viewBox="0 0 128 128">
  <circle cx="64" cy="64" r="60" fill="#BEBEBE"/>
  <path d="M 42 92 L 42 36 L 52 36 L 78 72 L 78 36 L 88 36 L 88 92 L 78 92 L 52 56 L 52 92 Z" fill="#2A2A2A"/>
</svg>
```

**Step 2: Create the outline SVG**

Create `shared/nexis/static/tray-icon-outline.svg`:

```xml
<?xml version="1.0" encoding="utf-8"?>
<svg version="1.1" xmlns="http://www.w3.org/2000/svg" width="128" height="128" viewBox="0 0 128 128">
  <circle cx="64" cy="64" r="56" fill="none" stroke="#BEBEBE" stroke-width="5"/>
  <path d="M 44 90 L 44 38 L 54 38 L 80 74 L 80 38 L 86 38 L 86 90 L 76 90 L 50 54 L 50 90 Z" fill="none" stroke="#BEBEBE" stroke-width="4" stroke-linejoin="round"/>
</svg>
```

**Step 3: Create the accent SVG**

Create `shared/nexis/static/tray-icon-accent.svg`:

```xml
<?xml version="1.0" encoding="utf-8"?>
<svg version="1.1" xmlns="http://www.w3.org/2000/svg" width="128" height="128" viewBox="0 0 128 128">
  <circle cx="64" cy="64" r="60" fill="#E95420"/>
  <path d="M 42 92 L 42 36 L 52 36 L 78 72 L 78 36 L 88 36 L 88 92 L 78 92 L 52 56 L 52 92 Z" fill="#1A1C22"/>
</svg>
```

**Step 4: Register in static.qrc**

In `shared/nexis/static.qrc`, add after line 43 (`<file>static/tray-icon.svg</file>`):

```xml
        <file>static/tray-icon-symbolic.svg</file>
        <file>static/tray-icon-outline.svg</file>
        <file>static/tray-icon-accent.svg</file>
```

**Step 5: Build to verify resources compile**

Run: `cmake --build build -j$(sysctl -n hw.ncpu)`
Expected: Clean build. The new SVGs are compiled into the binary via `qrc_static.cpp`.

**Step 6: Commit**

```bash
git add shared/nexis/static/tray-icon-symbolic.svg shared/nexis/static/tray-icon-outline.svg shared/nexis/static/tray-icon-accent.svg shared/nexis/static.qrc
git commit -m "feat(assets): add 3 monochrome tray icon SVG variants (FR-48)"
```

---

### Task 3: Add updateTrayIcon() to AppManager

**Files:**
- Modify: `shared/nexis/Managers/app_manager.h:34` (after `getTrayIcon()`)
- Modify: `shared/nexis/Managers/app_manager.cpp:27` (constructor) and after `getTrayIcon()` (line 50)

**Step 1: Declare updateTrayIcon()**

In `shared/nexis/Managers/app_manager.h`, add after line 34 (`QSystemTrayIcon *getTrayIcon();`):

```cpp
    void updateTrayIcon();
```

**Step 2: Implement updateTrayIcon()**

In `shared/nexis/Managers/app_manager.cpp`, add after `getTrayIcon()` (after line 50):

```cpp
void AppManager::updateTrayIcon()
{
    QString style = mSettingManager->getTrayIconStyle();
    QString path;

    if (style == "symbolic")
        path = QStringLiteral(":/static/tray-icon-symbolic.svg");
    else if (style == "outline")
        path = QStringLiteral(":/static/tray-icon-outline.svg");
    else if (style == "accent")
        path = QStringLiteral(":/static/tray-icon-accent.svg");
    else
        path = QStringLiteral(":/static/tray-icon.svg");

    mTrayIcon->setIcon(QIcon(path));
}
```

**Step 3: Call updateTrayIcon() at startup instead of hardcoded path**

In `shared/nexis/Managers/app_manager.cpp`, change line 27 from:

```cpp
    mTrayIcon = new QSystemTrayIcon(QIcon(":/static/tray-icon.svg"));
```

To:

```cpp
    mTrayIcon = new QSystemTrayIcon();
    updateTrayIcon();
```

This applies the saved preference at startup instead of always using the color icon.

**Step 4: Build to verify**

Run: `cmake --build build -j$(sysctl -n hw.ncpu)`
Expected: Clean build.

**Step 5: Commit**

```bash
git add shared/nexis/Managers/app_manager.h shared/nexis/Managers/app_manager.cpp
git commit -m "feat(tray): add updateTrayIcon() with style-based icon selection (FR-48)"
```

---

### Task 4: Add Tray Icon combo to Settings page UI

**Files:**
- Modify: `shared/nexis/Pages/Settings/settings_page.ui`

**Step 1: Add label widget to grid**

In `shared/nexis/Pages/Settings/settings_page.ui`, add a new `QLabel` at row 0, column 4. Insert before the closing `</layout>` tag (before line 627), or alongside the other row-0 labels. The label should match the pattern of `lblAppearance` (row 0, col 3):

```xml
   <item row="0" column="4">
    <widget class="QLabel" name="lblTrayIconStyle">
     <property name="sizePolicy">
      <sizepolicy hsizetype="Expanding" vsizetype="Preferred">
       <horstretch>0</horstretch>
       <verstretch>0</verstretch>
      </sizepolicy>
     </property>
     <property name="text">
      <string>Tray Icon</string>
     </property>
    </widget>
   </item>
```

**Step 2: Add combo widget to grid**

Add a `QComboBox` at row 1, column 4. Match the sizing of `cmbColorScheme` (min 150, max 200):

```xml
   <item row="1" column="4">
    <widget class="QComboBox" name="cmbTrayIconStyle">
     <property name="sizePolicy">
      <sizepolicy hsizetype="Expanding" vsizetype="Fixed">
       <horstretch>0</horstretch>
       <verstretch>0</verstretch>
      </sizepolicy>
     </property>
     <property name="minimumSize">
      <size>
       <width>150</width>
       <height>0</height>
      </size>
     </property>
     <property name="maximumSize">
      <size>
       <width>200</width>
       <height>16777215</height>
      </size>
     </property>
     <property name="cursor">
      <cursorShape>PointingHandCursor</cursorShape>
     </property>
     <property name="focusPolicy">
      <enum>Qt::NoFocus</enum>
     </property>
     <property name="sizeAdjustPolicy">
      <enum>QComboBox::AdjustToMinimumContentsLengthWithIcon</enum>
     </property>
    </widget>
   </item>
```

**Step 3: Build to verify UI compiles**

Run: `cmake --build build -j$(sysctl -n hw.ncpu)`
Expected: Clean build. `uic` generates the new widgets in `ui_settings_page.h`.

**Step 4: Commit**

```bash
git add shared/nexis/Pages/Settings/settings_page.ui
git commit -m "feat(settings-ui): add Tray Icon label and combo to grid (FR-48)"
```

---

### Task 5: Wire up combo in Settings page C++

**Files:**
- Modify: `shared/nexis/Pages/Settings/settings_page.h:48` (after `cmbFontChanged`)
- Modify: `shared/nexis/Pages/Settings/settings_page.cpp` (init, connects, handler)

**Step 1: Declare the slot**

In `shared/nexis/Pages/Settings/settings_page.h`, add after line 48 (`void cmbFontChanged(int index);`):

```cpp
    void cmbTrayIconStyleChanged(int index);
```

**Step 2: Populate the combo in init()**

In `shared/nexis/Pages/Settings/settings_page.cpp`, add after the font combo setup (after line 138, `ui->cmbFont->setCurrentIndex(...)`):

```cpp
    // tray icon style
    ui->cmbTrayIconStyle->addItem(tr("Color (Default)"), "color");
    ui->cmbTrayIconStyle->addItem(tr("Symbolic"), "symbolic");
    ui->cmbTrayIconStyle->addItem(tr("Outline"), "outline");
    ui->cmbTrayIconStyle->addItem(tr("Accent"), "accent");
    ui->cmbTrayIconStyle->setCurrentIndex(
        ui->cmbTrayIconStyle->findData(mSettingManager->getTrayIconStyle()));
```

**Step 3: Add to drop shadow list**

In `shared/nexis/Pages/Settings/settings_page.cpp`, at line 164-166 (the widgets list for `addDropShadow`), add `ui->cmbTrayIconStyle` to the list:

```cpp
    QList<QWidget*> widgets = {
        ui->cmbLanguages, ui->cmbDisks, ui->cmbStartPage, ui->cmbColorScheme,
        ui->cmbFont, ui->cmbTrayIconStyle, ui->spinCpuPercent, ui->spinMemoryPercent,
        ui->spinDiskPercent, ui->cmbDiskAnalyzer
    };
```

**Step 4: Connect the signal**

In `shared/nexis/Pages/Settings/settings_page.cpp`, add after line 176 (`connect(ui->cmbFont, ...)`):

```cpp
    connect(ui->cmbTrayIconStyle, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsPage::cmbTrayIconStyleChanged);
```

**Step 5: Implement the handler**

Add the handler method at the end of `settings_page.cpp` (or alongside the other `cmb*Changed` handlers):

```cpp
void SettingsPage::cmbTrayIconStyleChanged(int index)
{
    QString style = ui->cmbTrayIconStyle->itemData(index).toString();
    mSettingManager->setTrayIconStyle(style);
    apm->updateTrayIcon();
}
```

**Step 6: Build and verify**

Run: `cmake --build build -j$(sysctl -n hw.ncpu)`
Expected: Clean build, zero warnings.

**Step 7: Commit**

```bash
git add shared/nexis/Pages/Settings/settings_page.h shared/nexis/Pages/Settings/settings_page.cpp
git commit -m "feat(settings): wire tray icon style combo with live preview (FR-48)"
```

---

### Task 6: Run tests and verify

**Step 1: Run all existing tests**

Run: `ctest --test-dir build --output-on-failure`
Expected: All 7 test suites pass (FormatUtil, FileUtil, CommandUtil, DiskHealth, Schedule, ThemeToken, Screenshot).

**Step 2: Manual smoke test**

Launch the app: `./build/nexis/nexis`

Verify:
1. Settings page shows "Tray Icon" label and combo in the Appearance row (next to Appearance/Font)
2. Combo defaults to "Color (Default)"
3. Selecting "Symbolic" immediately changes the system tray icon to the gray circle
4. Selecting "Outline" shows the stroke-only variant
5. Selecting "Accent" shows the orange circle variant
6. Selecting "Color (Default)" restores the original colorful icon
7. Quit and relaunch — the last selected style is remembered

**Step 3: Verify no regressions**

Run: `cmake --build build -j$(sysctl -n hw.ncpu) && ctest --test-dir build --output-on-failure`
Expected: Clean build, all tests pass.

---

### Task 7: Update tracking files and documentation

**Files:**
- Modify: `FEATURE_REQUESTS.md:103` (FR-48 entry)
- Modify: `docs/APPLICATION_OVERVIEW.md` (Settings page section)

**Step 1: Mark FR-48 as done**

In `FEATURE_REQUESTS.md`, change FR-48 from `[ ]` to `[x]` and add resolution note:

```markdown
- [x] **FR-48: Monochrome app icon option** — Add an option to choose between the normal (colorful) and a monochrome app icon. Targeted at Linux users for better visual integration with system trays on GNOME and similar desktop environments where monochrome tray icons are the convention. Add a setting on the Settings page to toggle between "Color" and "Monochrome" icon styles, and update the tray icon accordingly at runtime. Requires creating a monochrome variant of the tray SVG. Issue [#5](https://github.com/lsimpsonsfdc/Nexis/issues/5) by @Vai0Lou.
  - **Resolved:** Added "Tray Icon" QComboBox to Settings page with 4 styles (Color, Symbolic, Outline, Accent). Three simplified monochrome SVGs created. `AppManager::updateTrayIcon()` applies selection at startup and on change. Persisted via `SettingManager::TrayIconStyle`.
```

**Step 2: Update APPLICATION_OVERVIEW.md**

Add a bullet to the Settings page section mentioning the new tray icon style selector.

**Step 3: Commit**

```bash
git add FEATURE_REQUESTS.md docs/APPLICATION_OVERVIEW.md
git commit -m "docs: mark FR-48 complete, update application overview"
```

---

### Task 8: Final commit and push

**Step 1: Push all commits**

```bash
git push
```
