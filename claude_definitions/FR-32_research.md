# FR-32 Research: QSS Token Validation in Theme System

> Deep research on the Nexis theme system to inform implementation of runtime token validation.
> Date: 2026-02-20

---

## Table of Contents

1. [updateStylesheet() Flow](#1-updatestylesheet-flow)
2. [Class Interface (app_manager.h)](#2-class-interface)
3. [Token Inventory: values.ini](#3-token-inventory-valuesini)
4. [Token Inventory: style.qss](#4-token-inventory-styleqss)
5. [Cross-Reference: Mismatches](#5-cross-reference-mismatches)
6. [The @dpN Token Pattern](#6-the-dpn-token-pattern)
7. [Third Theme Check](#7-third-theme-check)
8. [Consumers of getStyleValues()](#8-consumers-of-getstylevalues)
9. [Past Theme Bugs Analysis](#9-past-theme-bugs-analysis)
10. [Existing Validation / Warnings](#10-existing-validation--warnings)
11. [Inline setStyleSheet() Bypasses](#11-inline-setstylesheet-bypasses)
12. [Key Technical Details](#12-key-technical-details)
13. [Insertion Point for Validation Code](#13-insertion-point-for-validation-code)

---

## 1. updateStylesheet() Flow

**File:** `shared/nexis/Managers/app_manager.cpp`, lines 88-121

The method executes in four phases:

### Phase 1: Resolve Theme Name (line 90)

```cpp
QString themeName = resolveThemeName();
```

`resolveThemeName()` (lines 73-86) maps the user's color scheme preference:
- `"light"` -> returns `"light"`
- `"dark"` -> returns `"default"`
- `"auto"` -> queries `QGuiApplication::styleHints()->colorScheme()` on Qt 6.5+; returns `"light"` if the system reports Light, otherwise `"default"`

### Phase 2: Load Theme Values (lines 93-96)

```cpp
delete mStyleValues;
mStyleValues = new QSettings(
    QString(":/static/themes/%1/style/values.ini").arg(themeName),
    QSettings::IniFormat);
```

- Deletes the previous `QSettings` object (handles theme switching)
- Loads values from the QRC-embedded INI file for the resolved theme
- The INI files have **no section headers** -- all keys are in the implicit `General` group
- `QSettings::allKeys()` returns keys WITH the `@` prefix (e.g. `"@color01"`, `"@accentColor"`)
- This is confirmed by consumers like `history_chart.cpp:72` which calls `value("@chartLabelColor")`

### Phase 3: Load QSS Template and Replace Color Tokens (lines 98-105)

```cpp
mStylesheetFileContent = FileUtil::readStringFromFile(
    QStringLiteral(":/static/themes/default/style/style.qss"));

for (const QString &key : mStyleValues->allKeys()) {
    mStylesheetFileContent.replace(key, mStyleValues->value(key).toString());
}
```

**Critical design decision:** The QSS template is **always** loaded from `default/style/style.qss`, regardless of the active theme. Only the `values.ini` changes per theme. This means there is a single source of truth for the stylesheet structure.

**How replacement works:**
- `allKeys()` returns `["@themeName", "@pageContent", "@sidebar", "@color01", ..., "@destructiveColor"]`
- For each key, `QString::replace(key, value)` does a global string substitution
- Example: all occurrences of `@color01` in the QSS become `#36363a` (dark) or `#ffffff` (light)
- The `@themeName` token is special: it appears inside `url()` paths (e.g. `url(:/static/themes/@themeName/img/clean.png)`) and gets replaced with `"default"` or `"light"` to select theme-specific images

**Replacement order is undefined:** `QSettings::allKeys()` returns keys in an unspecified order. This is safe because:
- No token name is a substring of another token name (e.g. there's no `@color0` that could partial-match `@color01`)
- All tokens use distinct names
- `QString::replace` is a global substitution, so order between independent tokens doesn't matter

**Potential hazard with replacement order:** If a token *value* contained text that looked like another token (e.g. `@color03=@accentColor`), the replacement could chain. Currently no values contain `@` characters (they're all hex colors or the word `default`/`light`), so this is safe.

### Phase 4: Replace DPI Tokens (lines 107-116)

```cpp
static const QRegularExpression dpRx(QStringLiteral("@dp(\\d+)"));
QRegularExpressionMatch m;
qsizetype offset = 0;
while ((m = dpRx.match(mStylesheetFileContent, offset)).hasMatch()) {
    int base = m.captured(1).toInt();
    QString scaled = QString::number(Dpi::scale(base));
    mStylesheetFileContent.replace(m.capturedStart(), m.capturedLength(), scaled);
    offset = m.capturedStart() + scaled.length();
}
```

See [Section 6](#6-the-dpn-token-pattern) for full analysis.

### Phase 5: Apply and Notify (lines 118-120)

```cpp
qApp->setStyleSheet(mStylesheetFileContent);
emit SignalMapper::ins()->sigChangedAppTheme();
```

- Sets the fully-resolved stylesheet on the entire application
- Emits `sigChangedAppTheme()` so widgets that read style values programmatically (CircleBar, HistoryChart, DiskUsageLauncher) can update their non-QSS styling

### When updateStylesheet() Is Called

1. **At startup:** Called by `App::init()` (not shown in AppManager, but happens during app initialization)
2. **On system color scheme change (Qt 6.5+):** The constructor (lines 34-42) connects `QStyleHints::colorSchemeChanged` to call `updateStylesheet()` when the user's color scheme setting is `"auto"`
3. **On manual theme change:** When the user changes the color scheme in Settings, the settings page calls `updateStylesheet()`

---

## 2. Class Interface

**File:** `shared/nexis/Managers/app_manager.h`

```cpp
class AppManager
{
public:
    static AppManager *ins();                         // Singleton accessor

    void updateStylesheet();                          // Phase 1-5 above
    QString getStylesheetFileContent() const;         // Returns fully-resolved QSS string
    QSettings *getStyleValues() const;                // Returns current theme's values.ini as QSettings

    QSystemTrayIcon *getTrayIcon();
    QMap<QString, QString> getLanguageList() const;
    void loadLanguageList();
    QString resolveThemeName() const;                 // "default" or "light"

private:
    static AppManager *instance;
    AppManager();

    QTranslator mTranslator;
    QSystemTrayIcon *mTrayIcon;
    QSettings *mStyleValues;                          // Current theme's values.ini
    QMap<QString, QString> mLanguageList;
    QString mStylesheetFileContent;                   // Fully-resolved QSS after token replacement
    SettingManager *mSettingManager;
};
```

**Key observation for FR-32:** `mStyleValues` is a raw pointer that is `delete`d and re-`new`d on every `updateStylesheet()` call. The validation code will run after `mStyleValues` is loaded but before (or during) the replacement loop. The `mStylesheetFileContent` string is available as a member variable, so we can scan it for unreplaced tokens after the replacement loop too.

---

## 3. Token Inventory: values.ini

### Dark Theme (default)

**File:** `shared/nexis/static/themes/default/style/values.ini` (31 lines)

| # | Token | Value | Purpose |
|---|-------|-------|---------|
| 1 | `@themeName` | `default` | Theme directory name for image URLs |
| 2 | `@pageContent` | `#222226` | Main content area background |
| 3 | `@sidebar` | `#2e2e32` | Sidebar background |
| 4 | `@circleChartBackgroundColor` | `#2e2e32` | Dashboard circle chart bg (used in C++) |
| 5 | `@historyChartBackgroundColor` | `#2e2e32` | Resources history chart bg (used in C++) |
| 6 | `@chartLabelColor` | `#9a9996` | Chart axis label color (used in C++) |
| 7 | `@chartGridColor` | `#5e5c64` | Chart grid line color (used in C++) |
| 8 | `@color01` | `#36363a` | Surface / card background |
| 9 | `@color02` | `#3d3846` | Hover / alternate background |
| 10 | `@color03` | `#E95420` | Accent (same as accentColor) |
| 11 | `@color04` | `#9a9996` | Subtle text / hover states |
| 12 | `@color05` | `#ffffff` | Primary text |
| 13 | `@color06` | `#9a9996` | Secondary / dimmed text |
| 14 | `@color07` | `#ffffff` | Text on accent / inverse text |
| 15 | `@color08` | `#222226` | Deep background (same as pageContent) |
| 16 | `@color09` | `#e01b24` | Destructive hover |
| 17 | `@color10` | `#c64516` | Accent hover (same as accentHover) |
| 18 | `@color11` | `#deddda` | Title text / medium emphasis |
| 19 | `@color12` | `#c0bfbc` | Label text / low emphasis |
| 20 | `@color13` | `#3d3846` | Border alternate (same as color02) |
| 21 | `@color14` | `#5e5c64` | Border (same as borderColor) |
| 22 | `@color15` | `#2ec27e` | Success green |
| 23 | `@color16` | `#E95420` | Accent (same as accentColor) |
| 24 | `@accentColor` | `#E95420` | Primary accent (Nexis orange) |
| 25 | `@accentHover` | `#c64516` | Accent hover state |
| 26 | `@cardBg` | `#36363a` | Card backgrounds (same as color01) |
| 27 | `@borderColor` | `#5e5c64` | Default border color |
| 28 | `@successColor` | `#2ec27e` | Success status |
| 29 | `@warningColor` | `#e5a50a` | Warning status |
| 30 | `@destructiveColor` | `#e01b24` | Destructive / error status |

**Total: 30 tokens**

### Light Theme

**File:** `shared/nexis/static/themes/light/style/values.ini` (31 lines)

| # | Token | Value | Purpose |
|---|-------|-------|---------|
| 1 | `@themeName` | `light` | Theme directory name for image URLs |
| 2 | `@pageContent` | `#fafafb` | Main content area background |
| 3 | `@sidebar` | `#ebebed` | Sidebar background |
| 4 | `@circleChartBackgroundColor` | `#ffffff` | Dashboard circle chart bg |
| 5 | `@historyChartBackgroundColor` | `#ffffff` | Resources history chart bg |
| 6 | `@chartLabelColor` | `#5e5c64` | Chart axis label color |
| 7 | `@chartGridColor` | `#deddda` | Chart grid line color |
| 8 | `@color01` | `#ffffff` | Surface / card background |
| 9 | `@color02` | `#f6f5f4` | Hover / alternate background |
| 10 | `@color03` | `#E95420` | Accent (identical) |
| 11 | `@color04` | `#5e5c64` | Subtle text / hover states |
| 12 | `@color05` | `#241f31` | Primary text (dark on light) |
| 13 | `@color06` | `#77767b` | Secondary / dimmed text |
| 14 | `@color07` | `#ffffff` | Text on accent (identical) |
| 15 | `@color08` | `#fafafb` | Deep background |
| 16 | `@color09` | `#e01b24` | Destructive hover (identical) |
| 17 | `@color10` | `#c64516` | Accent hover (identical) |
| 18 | `@color11` | `#3d3846` | Title text / medium emphasis |
| 19 | `@color12` | `#5e5c64` | Label text / low emphasis |
| 20 | `@color13` | `#deddda` | Border alternate |
| 21 | `@color14` | `#c0bfbc` | Border |
| 22 | `@color15` | `#26a269` | Success green (darker variant) |
| 23 | `@color16` | `#E95420` | Accent (identical) |
| 24 | `@accentColor` | `#E95420` | Primary accent (identical) |
| 25 | `@accentHover` | `#c64516` | Accent hover (identical) |
| 26 | `@cardBg` | `#ffffff` | Card backgrounds |
| 27 | `@borderColor` | `#deddda` | Default border color |
| 28 | `@successColor` | `#26a269` | Success status (darker variant) |
| 29 | `@warningColor` | `#cd9309` | Warning status (darker variant) |
| 30 | `@destructiveColor` | `#c01c28` | Destructive / error (slightly different) |

**Total: 30 tokens**

### Differences Between Themes

Both themes define **exactly the same 30 token names** -- perfect parity. The differences are in the values:

- **Identical across both themes (7):** `@color03`, `@color07`, `@color09`, `@color10`, `@color16`, `@accentColor`, `@accentHover`
- **Light/dark inversions (23):** All other tokens swap light and dark values appropriately
- **Semantic aliases exist:** `@color03` = `@color16` = `@accentColor`, `@color10` = `@accentHover`, `@color15` = `@successColor` -- these are legacy from the original Stacer codebase; the semantic names (`@accentColor`, `@successColor`, etc.) were added later

---

## 4. Token Inventory: style.qss

**File:** `shared/nexis/static/themes/default/style/style.qss` (1285 lines)

### Color Tokens Used (non-@dp)

Extracted by scanning for `@[a-zA-Z]` patterns excluding `@dp` prefixed tokens:

| Token | Usage Count | Sample Line |
|-------|------------|-------------|
| `@color01` | 21 | Line 72: `background-color: @color01;` |
| `@color02` | 14 | Line 332: `background-color: @color02;` |
| `@color04` | 4 | Line 24: `background-color: @color04;` |
| `@color05` | 33 | Line 76: `color: @color05;` |
| `@color06` | 13 | Line 18: `background-color: @color06;` |
| `@color07` | 9 | Line 110: `color: @color07;` |
| `@color09` | 1 | Line 398: `background-color: @color09;` |
| `@color11` | 7 | Line 667: `color: @color11;` |
| `@color12` | 7 | Line 131: `color: @color12;` |
| `@accentColor` | 13 | Line 109: `background-color: @accentColor;` |
| `@accentHover` | 4 | Line 408: `background-color: @accentHover;` |
| `@borderColor` | 27 | Line 96: `border: 1px solid @borderColor;` |
| `@cardBg` | 9 | Line 476: `background-color: @cardBg;` |
| `@pageContent` | 5 | Line 466: `background-color: @pageContent;` |
| `@sidebar` | 1 | Line 429: `background-color: @sidebar;` |
| `@destructiveColor` | 2 | Line 392: `background-color: @destructiveColor;` |
| `@successColor` | 2 | Line 703: `color: @successColor;` |
| `@warningColor` | 1 | Line 1058: `color: @warningColor;` |
| `@themeName` | 11 | Line 638: `url(:/static/themes/@themeName/img/clean.png)` |

### Tokens NOT Used in QSS

The following tokens are defined in `values.ini` but are **never referenced** in `style.qss`:

| Token | Why It Exists |
|-------|---------------|
| `@color03` | Alias for `@accentColor`; QSS uses `@accentColor` instead |
| `@color08` | Alias for `@pageContent`; QSS uses `@pageContent` instead |
| `@color10` | Alias for `@accentHover`; QSS uses `@accentHover` instead |
| `@color13` | Alias for `@color02`; not used anywhere |
| `@color14` | Alias for `@borderColor`; QSS uses `@borderColor` instead |
| `@color15` | Alias for `@successColor`; QSS uses `@successColor` instead |
| `@color16` | Alias for `@accentColor`; QSS uses `@accentColor` instead |
| `@circleChartBackgroundColor` | Used in C++ only (`circlebar.cpp:61`) |
| `@historyChartBackgroundColor` | Used in C++ only (`history_chart.cpp:74`) |
| `@chartLabelColor` | Used in C++ only (`history_chart.cpp:72`) |
| `@chartGridColor` | Used in C++ only (`history_chart.cpp:73`) |

**11 tokens defined but unused in QSS.** Of these:
- 7 are legacy numbered aliases (`@color03`, `@color08`, `@color10`, `@color13`, `@color14`, `@color15`, `@color16`) -- they exist for backward compatibility but the QSS now uses semantic names
- 4 are chart tokens consumed only in C++ code via `getStyleValues()->value("@token")`

### Tokens Used in QSS But Not in values.ini

**None.** Every `@token` in the QSS has a corresponding entry in both `values.ini` files. As of this analysis, the system is in sync.

---

## 5. Cross-Reference: Mismatches

### Current State: No Mismatches

All 19 distinct non-`@dp` tokens used in `style.qss` are defined in both `values.ini` files. All 30 tokens in both `values.ini` files use the same token names. The system is currently healthy.

### Historical Mismatches (see Section 9)

Past bugs were not caused by missing tokens in `values.ini`. They were caused by:
1. **Missing QSS rules** -- widgets that had no QSS selector at all, falling back to system palette
2. **Missing object names** -- programmatic widgets without `setObjectName()`, so QSS selectors didn't match
3. **Hardcoded inline colors** -- `setStyleSheet("color: gray")` bypassing the token system

Token validation would catch category (1) only if it also verified that tokens resolve to valid CSS values. It would NOT directly catch categories (2) and (3), but it would catch the most dangerous failure: a new token added to QSS without a corresponding `values.ini` entry.

---

## 6. The @dpN Token Pattern

### How It Works

**File:** `shared/nexis/Managers/app_manager.cpp`, lines 107-116

```cpp
static const QRegularExpression dpRx(QStringLiteral("@dp(\\d+)"));
QRegularExpressionMatch m;
qsizetype offset = 0;
while ((m = dpRx.match(mStylesheetFileContent, offset)).hasMatch()) {
    int base = m.captured(1).toInt();
    QString scaled = QString::number(Dpi::scale(base));
    mStylesheetFileContent.replace(m.capturedStart(), m.capturedLength(), scaled);
    offset = m.capturedStart() + scaled.length();
}
```

The regex `@dp(\d+)` matches tokens like `@dp8`, `@dp14px`, `@dp100`, etc. -- any `@dp` followed by one or more digits. The captured group `(\d+)` extracts only the numeric portion.

**DPI Scaling:** `Dpi::scale()` (from `shared/nexis/dpi.h`) multiplies the base pixel value by the device pixel ratio, but ONLY when Qt's built-in HiDPI scaling is explicitly disabled via `QT_ENABLE_HIGHDPI_SCALING=0`. In normal Qt6 operation, `Dpi::scale()` returns the input unchanged (factor = 1.0).

### @dp Tokens Found in style.qss

The following `@dpN` patterns appear in the QSS (values are the base pixel sizes before DPI scaling):

| Token | Base px | Usage Count | Example |
|-------|---------|-------------|---------|
| `@dp2` | 2 | 2 | margin, padding |
| `@dp4` | 4 | 7 | border-radius, padding |
| `@dp6` | 6 | 17 | border-radius, padding, margin |
| `@dp8` | 8 | 17 | width, height, padding, border-radius |
| `@dp10` | 10 | 1 | padding |
| `@dp12` | 12 | 9 | margin, padding |
| `@dp14` | 14 | 6 | indicator width/height |
| `@dp15` | 15 | 1 | indicator width/height |
| `@dp16` | 16 | 11 | width, height, indicator |
| `@dp18` | 18 | 2 | min-height, indicator |
| `@dp20` | 20 | 1 | width |
| `@dp22` | 22 | 1 | max-height |
| `@dp24` | 24 | 3 | padding, indicator |
| `@dp26` | 26 | 1 | width/height |
| `@dp30` | 30 | 1 | min-height/min-width |
| `@dp36` | 36 | 2 | height, min-height |
| `@dp44` | 44 | 1 | width |
| `@dp100` | 100 | 1 | min-width |
| `@dp180` | 180 | 1 | width |

Some tokens include `px` suffix in the QSS (e.g. `@dp6px`, `@dp30px`). The regex captures only the digits, so `@dp6px` matches with captured group `6`. After replacement, `@dp6px` becomes `6px` (the `px` is literal text after the match).

### Why @dp Tokens Must Be Excluded from Validation

`@dpN` tokens are **not** defined in `values.ini`. They are handled by a separate regex-based replacement pass. The validation code must exclude any token matching `@dp\d+` from the "missing from values.ini" check. The Architecture Review's suggested code uses `if (token.startsWith("dp")) continue;` which is correct but could be made more precise with a regex check.

---

## 7. Third Theme Check

There are **only two theme directories** with `values.ini` files:
- `shared/nexis/static/themes/default/style/values.ini` (dark)
- `shared/nexis/static/themes/light/style/values.ini` (light)

No `nexis/style/values.ini` or third theme directory exists. The `resolveThemeName()` method only returns `"default"` or `"light"`, confirming these are the only two themes.

Both theme directories also contain matching image subdirectories (`img/`) with theme-specific PNGs and SVGs referenced by the `@themeName` token in QSS `url()` paths. Both have the same set of image files.

---

## 8. Consumers of getStyleValues()

The `getStyleValues()` method exposes the current theme's `QSettings` to C++ code that needs theme colors outside of QSS. These are the consumers:

### circlebar.cpp (line 60-63)

```cpp
QSettings *styleValues = AppManager::ins()->getStyleValues();
mChartView->setBackgroundBrush(QColor(styleValues->value("@circleChartBackgroundColor").toString()));
mSeries->slices().last()->setColor(styleValues->value("@pageContent").toString());
```

Tokens used: `@circleChartBackgroundColor`, `@pageContent`

### history_chart.cpp (lines 72-74)

```cpp
QString chartLabelColor = AppManager::ins()->getStyleValues()->value("@chartLabelColor").toString();
QString chartGridColor = AppManager::ins()->getStyleValues()->value("@chartGridColor").toString();
QString historyChartBackground = AppManager::ins()->getStyleValues()->value("@historyChartBackgroundColor").toString();
```

Tokens used: `@chartLabelColor`, `@chartGridColor`, `@historyChartBackgroundColor`

### disk_usage_launcher_widget.cpp (lines 523-529)

```cpp
QSettings *sv = AppManager::ins()->getStyleValues();
QString textColor = sv->value("@color12").toString();
mToolNameLabel->setStyleSheet(QString("color: %1;").arg(textColor));
mDescriptionLabel->setStyleSheet(QString("color: %1;").arg(textColor));
```

Token used: `@color12`

**Implication for validation:** These C++ consumers would silently get empty strings if a token they depend on were removed from `values.ini`. The validation system should also verify that C++-consumed tokens exist, but this is harder to automate (would need code scanning, not just QSS scanning). For FR-32, focusing on QSS validation is the primary scope.

---

## 9. Past Theme Bugs Analysis

### BUG-21: Homebrew tree view white background in dark mode

**Root cause:** Programmatic `QTreeWidget` had no `setObjectName()`, so QSS selector `#treeWidgetPackages` never matched. The widget used the system palette (white background).

**Would token validation catch it?** No. The tokens were defined; the QSS rules existed. The problem was that the widget had no object name to match the selectors.

### BUG-33: Purge checkbox text invisible in dark mode

**Root cause:** `QCheckBox` had no QSS `color` rule. The `QCheckBox::indicator` was styled (custom toggle images), but the text color was unstyled, falling back to the system palette (dark text on dark background).

**Would token validation catch it?** No. This was a missing QSS rule, not a missing token. No `@token` was involved -- the rule simply didn't exist.

### BUG-36: System Cleaner "Total Size" label invisible in dark mode

**Root cause:** `#lblTotalBytes` had no QSS color rule. Nearby labels had explicit rules, but this one was missed. System palette fallback produced dark text on dark background.

**Would token validation catch it?** No. Same as BUG-33 -- a missing QSS rule, not a missing token.

### BUG-38: Hardware Info table rows illegible in dark mode

**Root cause:** `alternatingRowColors` was enabled on QTableWidgets, but no `alternate-background-color` was defined in QSS. Qt's system palette provided light alternating rows with white text.

**Would token validation catch it?** No. This was a QWidget property + missing QSS property, not a token issue.

### BUG-40: FR-16 UI regressions -- hardcoded colors

**Root cause:** Programmatically created widgets used hardcoded inline styles (`"color: gray"`, `rgba(128,128,128,30)`) instead of `@token`-based QSS. These didn't adapt to theme changes.

**Would token validation catch it?** No. The hardcoded colors bypass the token system entirely.

### Summary of Theme Bug Patterns

| Bug | Category | Token Validation Would Catch? |
|-----|----------|-------------------------------|
| BUG-21 | Missing object name | No |
| BUG-33 | Missing QSS rule | No |
| BUG-36 | Missing QSS rule | No |
| BUG-38 | Missing QSS property | No |
| BUG-40 | Hardcoded inline colors | No |

**None of these past bugs would have been caught by token validation.** They all represent a different failure class: widgets or properties that aren't covered by the QSS at all, or that bypass it.

**What token validation WILL catch:**
1. **Future typos:** A developer adds `@accentColr` (typo) to the QSS -- validation warns immediately
2. **Forgotten values.ini entries:** A new semantic token `@hoverBg` is added to QSS but not to `values.ini` -- warning at runtime
3. **values.ini parity drift:** Token added to `default/values.ini` but not `light/values.ini` -- validation per-theme catches it
4. **Invalid color format:** Token value `1e1e1e` (missing `#`) or `#ZZZZZZ` (invalid hex) -- format validation catches it
5. **Stale tokens in values.ini:** Tokens that exist in `values.ini` but are no longer used anywhere (QSS or C++) -- reverse validation catches it

These are **prevention** measures for future regressions, not retroactive fixes for past bugs.

---

## 10. Existing Validation / Warnings

**There is ZERO validation or warning code in the theme system.** Specifically:

- `app_manager.cpp` has no `qWarning()`, `qCritical()`, or `qDebug()` calls related to tokens
- No check for unreplaced `@` tokens after the replacement loop
- No check that color values are valid hex format
- No check that `values.ini` loaded successfully
- No check that `style.qss` loaded successfully
- `QString::replace()` silently does nothing if the search string isn't found

The method is a pure "happy path" implementation with no error handling or diagnostics.

---

## 11. Inline setStyleSheet() Bypasses

The following code bypasses the token system by using hardcoded colors in inline `setStyleSheet()` calls:

### disk_usage_launcher_widget.cpp (17 occurrences)

```cpp
// Lines 258-373: Multiple status label color assignments
mStatusLabel->setStyleSheet("color: #2ec27e; font-weight: bold;");  // 8 occurrences (success green)
mStatusLabel->setStyleSheet("color: #77767b; font-weight: bold;");  // 7 occurrences (dimmed text)

// Lines 528-529: Theme-aware but inline
mToolNameLabel->setStyleSheet(QString("color: %1;").arg(textColor));
mDescriptionLabel->setStyleSheet(QString("color: %1;").arg(textColor));
```

Note: Lines 528-529 read from `getStyleValues()` so they're theme-aware, but the 15 other occurrences use hardcoded hex colors.

### app.cpp (line 446-452): Kiosk overlay

```cpp
overlay->setStyleSheet(
    "background-color: rgba(0, 0, 0, 160);"
    "color: white;"
    "font-size: 14pt;"
    "padding: 16px 32px;"
    "border-radius: 8px;"
);
```

This is arguably acceptable since it's a transient overlay that fades out after 3.5 seconds.

### settings_page.cpp (lines 455, 457): Scroll area transparency

```cpp
scrollArea->setStyleSheet("QScrollArea{background-color:transparent;}");
scrollWidget->setStyleSheet("background-color:transparent;");
```

This is a workaround for the Qt QSS/QPalette timing issue (documented in CLAUDE.md's "QScrollArea Viewport in Programmatic Dialogs" section) and is acceptable.

---

## 12. Key Technical Details

### QSettings::allKeys() Behavior with @ Prefix

The `values.ini` files use `@` as part of the key name (e.g., `@color01=...`). `QSettings::IniFormat` preserves the `@` as part of the key. `allKeys()` returns `["@themeName", "@pageContent", ...]` with the `@` prefix intact.

Important: since the INI file has no section header, all keys end up in the implicit `General` group. However, `allKeys()` returns them without the `General/` prefix, so the keys are directly usable as search strings in `QString::replace()`.

### Token Name Character Set

From the current `values.ini` files, all token names follow the pattern `@[a-zA-Z][a-zA-Z0-9]*`:
- Start with `@` followed by a letter
- Contain only letters and digits (no underscores, hyphens, or other characters)
- Case-sensitive: `@cardBg` is different from `@cardbg`

The Architecture Review suggests the regex `@([a-zA-Z][a-zA-Z0-9_]*)` (including underscores). Currently no token uses underscores, but including them future-proofs the regex.

### DPI Token Disambiguation

The `@dp` prefix creates an overlap: `@dp` could theoretically conflict with a color token starting with `dp` (e.g., `@dpSpecialColor`). In practice this doesn't happen because:
- Color tokens use camelCase names like `@color01`, `@accentColor`
- DPI tokens always have digits immediately after `@dp` (e.g., `@dp8`, `@dp100`)
- The validation regex should use `@dp\d+` to match DPI tokens precisely

### The @themeName Token Is Special

`@themeName` is unique among tokens because it's used inside `url()` paths rather than as a CSS value:

```css
background: url(:/static/themes/@themeName/img/clean.png) no-repeat center;
```

It gets replaced with `"default"` or `"light"`, selecting theme-specific image resources. Token validation should handle it the same as color tokens (it's in `values.ini` and gets replaced by the same loop), but format validation should NOT check it for hex color format.

---

## 13. Insertion Point for Validation Code

### Recommended Location

**File:** `shared/nexis/Managers/app_manager.cpp`, `updateStylesheet()` method

The validation should be inserted at **two points:**

#### Point A: Before the replacement loop (lines 102-105)

Scan the raw QSS template for all `@token` references and verify each exists in `mStyleValues`. This catches missing tokens early, before replacement.

```
Line 100: (QSS loaded)
Line 101: (blank line)
>>> INSERT VALIDATION HERE: scan QSS for @tokens, check against mStyleValues
Line 102: (blank line -- currently doesn't exist, replace loop starts at 103)
Line 103: for (const QString &key : mStyleValues->allKeys()) {
```

Specifically, between line 101 and line 103 (after loading QSS, before replacing tokens).

#### Point B: After the DPI replacement loop (line 116), before setStyleSheet (line 118)

Scan the fully-resolved stylesheet for any remaining `@token` patterns that weren't replaced. This is a safety net that catches any token that slipped through.

```
Line 116: }  // end of DPI replacement loop
>>> INSERT POST-VALIDATION HERE: scan for unreplaced @tokens
Line 118: qApp->setStyleSheet(mStylesheetFileContent);
```

#### Optional Point C: Hex color format validation

During the replacement loop (Point A), for each token that is NOT `@themeName`, validate that the value is a valid CSS hex color (`#[0-9a-fA-F]{3,8}`).

### Specific Code Changes Needed

Only one file needs modification: `shared/nexis/Managers/app_manager.cpp`

The `#include <QRegularExpression>` is already present (line 4), which is needed for the validation regex.

### Build Impact

No new files, no CMakeLists.txt changes, no new dependencies. The validation is pure logic added to an existing method in an existing file.

---

## Appendix A: Complete Token Cross-Reference Table

| Token | In default values.ini | In light values.ini | In style.qss | In C++ Code | Notes |
|-------|:--------------------:|:-------------------:|:------------:|:-----------:|-------|
| `@themeName` | Yes | Yes | Yes (11x) | No | Image URL paths |
| `@pageContent` | Yes | Yes | Yes (5x) | Yes (circlebar.cpp) | |
| `@sidebar` | Yes | Yes | Yes (1x) | No | |
| `@circleChartBackgroundColor` | Yes | Yes | No | Yes (circlebar.cpp) | C++ only |
| `@historyChartBackgroundColor` | Yes | Yes | No | Yes (history_chart.cpp) | C++ only |
| `@chartLabelColor` | Yes | Yes | No | Yes (history_chart.cpp) | C++ only |
| `@chartGridColor` | Yes | Yes | No | Yes (history_chart.cpp) | C++ only |
| `@color01` | Yes | Yes | Yes (21x) | No | |
| `@color02` | Yes | Yes | Yes (14x) | No | |
| `@color03` | Yes | Yes | No | No | Legacy alias for @accentColor |
| `@color04` | Yes | Yes | Yes (4x) | No | |
| `@color05` | Yes | Yes | Yes (33x) | No | Most-used token |
| `@color06` | Yes | Yes | Yes (13x) | No | |
| `@color07` | Yes | Yes | Yes (9x) | No | |
| `@color08` | Yes | Yes | No | No | Legacy alias for @pageContent |
| `@color09` | Yes | Yes | Yes (1x) | No | |
| `@color10` | Yes | Yes | No | No | Legacy alias for @accentHover |
| `@color11` | Yes | Yes | Yes (7x) | No | |
| `@color12` | Yes | Yes | Yes (7x) | Yes (disk_usage_launcher) | |
| `@color13` | Yes | Yes | No | No | Legacy alias for @color02 |
| `@color14` | Yes | Yes | No | No | Legacy alias for @borderColor |
| `@color15` | Yes | Yes | No | No | Legacy alias for @successColor |
| `@color16` | Yes | Yes | No | No | Legacy alias for @accentColor |
| `@accentColor` | Yes | Yes | Yes (13x) | No | |
| `@accentHover` | Yes | Yes | Yes (4x) | No | |
| `@cardBg` | Yes | Yes | Yes (9x) | No | |
| `@borderColor` | Yes | Yes | Yes (27x) | No | Second most-used |
| `@successColor` | Yes | Yes | Yes (2x) | No | |
| `@warningColor` | Yes | Yes | Yes (1x) | No | |
| `@destructiveColor` | Yes | Yes | Yes (2x) | No | |

**Summary:**
- **30 tokens** defined in each values.ini
- **19 tokens** used in style.qss
- **6 tokens** used in C++ code (via `getStyleValues()`)
- **7 tokens** unused anywhere (legacy numbered aliases: `@color03`, `@color08`, `@color10`, `@color13`, `@color14`, `@color15`, `@color16`)
- **4 tokens** used in C++ only, not in QSS (`@circleChartBackgroundColor`, `@historyChartBackgroundColor`, `@chartLabelColor`, `@chartGridColor`)
- **0 tokens** in QSS that are missing from values.ini (system is currently in sync)
