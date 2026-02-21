# BUG-47 Research: Theme Not Fully Applied When Switching

## Theme System Architecture

The app uses a **token-based QSS template** system:
- One `style.qss` template (1,647 lines) with `@tokenName` placeholders
- Per-theme `values.ini` files with color token definitions (44 tokens each)
- `AppManager::updateStylesheet()` resolves tokens → applies via `qApp->setStyleSheet()`
- `SignalMapper::sigChangedAppTheme` emitted after every theme switch
- Two themes: `default` (dark), `light`

### Theme Switch Call Chain
```
User selects theme → SettingsPage::cmbColorSchemeChanged()
  → SettingManager::setColorScheme() (persists to settings.ini)
  → AppManager::updateStylesheet()
    → resolveThemeName() (maps "light"/"dark"/"auto" → folder name)
    → Load values.ini from theme folder
    → Load style.qss template from default folder
    → Validate tokens (warn on mismatches)
    → Replace @tokens with color values
    → Replace @dpN with DPI-scaled pixel values
    → qApp->setStyleSheet(compiled)
    → emit sigChangedAppTheme()
```

### Available Token Reading Pattern
```cpp
QSettings *sv = AppManager::ins()->getStyleValues();
QString color = sv->value("@tokenName").toString();
```

---

## Affected Widgets — Detailed Analysis

### 1. MetricTile (`shared/nexis/Pages/Dashboard/metric_tile.h/.cpp`)

**Constructor:** Takes `QColor color` — resolved once from `colorFromStyle("@cpuColor")` in `dashboard_page.cpp:41-48`.

**Baked colors (never updated):**
- Line 57: `mSeries->setPen(QPen(mColor, 1.5))` — sparkline line
- Lines 64-66: `mAreaSeries->setBrush(fillColor)` — area fill (mColor @ 10% alpha)
- Lines 110-111: `mProgressBar->setStyleSheet(chunkStyle)` — progress bar chunk
- Lines 133-146: `mBtnAction->setStyleSheet(...)` — action button border/text/hover (includes hardcoded `#ffffff`)

**Existing listener (lines 18-22):** Only updates `mChart->setBackgroundBrush(QColor(cardBg))`.

**Missing from listener:** Series pen, area fill, progress bar, action button.

---

### 2. NetworkTile (`shared/nexis/Pages/Dashboard/network_tile.h/.cpp`)

**Constructor:** Takes `QColor color`. Upload color hardcoded: `mUploadColor("#E05454")` (line 11).

**Baked colors:**
- Line 44: `mLblDownLabel->setStyleSheet(QString("color: %1;").arg(mColor.name()))` — download label
- Line 57: `mRxSeries->setPen(QPen(mColor, 1.5))` — download sparkline
- Lines 64-66: `mRxAreaSeries->setBrush(rxFill)` — download area fill
- Line 111: `mLblUpLabel->setStyleSheet(QString("color: %1;").arg(mUploadColor.name()))` — upload label
- Line 124: `mTxSeries->setPen(QPen(mUploadColor, 1.5))` — upload sparkline
- Lines 131-133: `mTxAreaSeries->setBrush(txFill)` — upload area fill
- Lines 193-206: `mBtnAction->setStyleSheet(...)` — action button (includes hardcoded `#ffffff`)

**Existing listener (lines 17-22):** Only updates two chart backgrounds.

---

### 3. DiskTile (`shared/nexis/Pages/Dashboard/disk_tile.h/.cpp`)

**Constructor:** Takes `QColor arcColor, QColor trackColor` — resolved once.

**Baked colors:**
- `mArcColor` and `mTrackColor` used in `paintEvent()` for QPainter donut drawing
- Lines 63-64: `setDriveHealth()` uses hardcoded `"#2EC27E"` (healthy green) and `"#c01c28"` (bad red)

**NO theme listener at all.** Zero connection to `sigChangedAppTheme`.

**Note:** Center text uses `palette().color(QPalette::WindowText)` which IS theme-aware ✓.

---

### 4. DashboardPage (`shared/nexis/Pages/Dashboard/dashboard_page.cpp`)

- Lines 23-29: `colorFromStyle()` static helper reads tokens once at construction — passes resolved QColor to tile constructors
- Line 317: Hardcoded `#6B6E78` gray in system summary HTML span

---

### 5. DiskUsageLauncherWidget (`shared/nexis/Pages/Resources/disk_usage_launcher_widget.cpp`)

- 9× `#2ec27e` (green, "Installed") in `updateUi()` switch cases
- 8× `#77767b` (gray, "Not installed") in `updateUi()` switch cases
- Existing `applyThemeColors()` (lines 522-531) only updates tool name and description labels — NOT the status label
- Already connected to `sigChangedAppTheme` (line 114)

---

### 6. HardwareInfoPage (`shared/nexis/Pages/HardwareInfo/hardware_info_page.cpp`)

- Line 378: `valueItem->setForeground(QColor("#2ec27e"))` — "Good" health
- Line 380: `valueItem->setForeground(QColor("#e5a50a"))` — "Caution" health
- Line 382: `valueItem->setForeground(QColor("#e01b24"))` — "Critical" health
- No theme listener for these items

---

### 7. SettingsPage (`shared/nexis/Pages/Settings/settings_page.cpp`)

- Lines 39-44: Hardcoded `#E95420` for credit link underline color in HTML
- No theme listener to update link color on switch

---

### 8. CommandPalette (`shared/nexis/command_palette.cpp`)

- Line 45: `shadow->setColor(QColor(0, 0, 0, 100))` — always black shadow
- No connection to `sigChangedAppTheme`

---

## CircleBar Status

`CircleBar` is defined in `circlebar.h/.cpp/.ui` but **not instantiated anywhere** in the current codebase. It's a dead widget from the old dashboard. No fix needed.

## HistoryChart Status

Series colors (20 hardcoded hex values) are used to distinguish data lines (CPU cores, etc.) — these are NOT semantic theme colors. The existing listener correctly updates background, label, and grid colors. Low priority / acceptable as-is.

---

## Summary of Required Fixes

| Widget | Has Listener? | Listener Completeness | Hardcoded Colors |
|--------|--------------|----------------------|------------------|
| MetricTile | ✓ | ~20% (bg only) | QPen, QBrush, setStyleSheet ×2, #ffffff |
| NetworkTile | ✓ | ~20% (bg only) | #E05454, QPen ×2, QBrush ×2, setStyleSheet ×3, #ffffff |
| DiskTile | ✗ | N/A | QPainter ×2, #2EC27E, #c01c28 |
| DashboardPage | ✗ (for summary) | N/A | #6B6E78, #888888 fallback |
| DiskUsageLauncher | ✓ | ~50% (text only) | #2ec27e ×9, #77767b ×8 |
| HardwareInfoPage | ✗ | N/A | #2ec27e, #e5a50a, #e01b24 |
| SettingsPage | ✗ | N/A | #E95420 |
| CommandPalette | ✗ | N/A | QColor(0,0,0,100) |
