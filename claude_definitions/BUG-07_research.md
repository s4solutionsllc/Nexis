# BUG-07 Research: HiDPI / 4K Scaling Issues

## Bug Summary

QWidget UI doesn't scale properly on HiDPI displays. Text truncation, garbled service lists on 4K monitors. Full fix requires QML migration. Upstream: [oguzhaninan/Stacer#111](https://github.com/oguzhaninan/Stacer/issues/111), [#482](https://github.com/oguzhaninan/Stacer/issues/482).

## Current DPI Handling

**The app does nothing explicit.** There is no DPI setup in `main.cpp` or `app.cpp`. Qt6 enables high-DPI scaling automatically (the old `AA_EnableHighDpiScaling` attribute was removed from the API — it's always on). No environment variables (`QT_SCALE_FACTOR`, `QT_AUTO_SCREEN_SCALE_FACTOR`, etc.) are set in code.

**What Qt6 scales automatically:**
- Window geometry and widget positions (via layout managers)
- Font rendering when using `pointsize` (which the app does — correct)
- System palette colours

**What Qt6 does NOT scale:**
- Hardcoded pixel values in C++ (`setFixedSize(64, 64)`, `pixmap(48, 48)`)
- Hardcoded pixel values in `.ui` files (`<width>220</width>`, `<height>45</height>`)
- Hardcoded pixel values in QSS (`width: 44px;`, `height: 24px;`)

The core problem is that the codebase is saturated with hardcoded pixel values that bypass Qt's automatic scaling.

## Hardcoded Pixel Sizes in C++

### setFixedSize / setFixedHeight calls

| File | Line | Code | Impact |
|---|---|---|---|
| `system_cleaner_page.cpp` | 50, 67 | `lbl->setFixedSize(64, 64)` | Cleaner category icons don't scale |
| `disk_usage_launcher_widget.cpp` | 51 | `iconLabel->setFixedSize(48, 48)` | Disk analyzer icon doesn't scale |
| `system_cleaner_page.cpp` | 83 | `header()->setFixedHeight(30)` | Tree header too small on 4K |
| `host_manage.cpp` | 57 | `header()->setFixedHeight(32)` | Table header too small on 4K |
| `processes_page.cpp` | 49 | `header()->setFixedHeight(36)` | Table header too small on 4K |
| `uninstaller_page.cpp` | 27 | `header()->setFixedHeight(30)` | Tree header too small on 4K |
| `apt_source_manager_page.cpp` | 52 | `header()->setFixedHeight(30)` | Tree header too small on 4K |
| `search_page.cpp` | 40 | `header()->setFixedHeight(32)` | Table header too small on 4K |
| `hardware_info_page.cpp` | 58 | `table->setFixedHeight(height)` | Calculated, but based on row count × fixed row height |

### pixmap() with fixed sizes

| File | Line | Code |
|---|---|---|
| `system_cleaner_page.cpp` | 47, 64 | `icon.pixmap(QSize(64, 64))` |
| `disk_usage_launcher_widget.cpp` | 98 | `toolIcon.pixmap(48, 48)` |
| `apt_source_manager_page.cpp` | 58 | `setIconSize(QSize(20, 20))` |

These rasterize scalable SVGs at fixed pixel counts, then display in fixed-size labels. On a 2× HiDPI display, they appear at half the expected size.

### Column widths

| File | Line | Code |
|---|---|---|
| `system_cleaner_page.cpp` | 79 | `setColumnWidth(0, 600)` |
| `host_manage.cpp` | 51-52 | `resizeSection(0, 195); resizeSection(1, 195)` |

## Hardcoded Pixel Sizes in .ui Files

**115 instances** of `fixedWidth`, `fixedHeight`, `minimumSize`, or `maximumSize` across **20 .ui files**.

Key offenders:

| File | Element | Size | Impact |
|---|---|---|---|
| `app.ui` | Sidebar | 220px wide | Too narrow on 4K |
| `app.ui` | Sidebar button icons | 28×28px | Icons tiny on HiDPI |
| `service_item.ui` | Row height | 45px min/max | Text truncation at high DPI |
| `service_item.ui` | Service icon | 25×25px | Misaligned with scaled text |
| `system_cleaner_page.ui` | 20 size constraints | Various | Multiple elements don't scale |
| `search_page.ui` | 18 size constraints | Various | Search results don't scale |
| `settings_page.ui` | 11 size constraints | Various | Settings layout cramped |

## Hardcoded Pixel Sizes in QSS

`shared/nexis/static/themes/default/style/style.qss` contains ~90 hardcoded pixel values:

- Scrollbar widths: `width: 8;` (unitless = pixels)
- Checkbox toggle: `width: 44px; height: 24px;`
- Checkbox indicator: `width: 18px; height: 18px;`
- Slider handle: `width: 16px; height: 16px;`
- Sidebar width: `min-width: 200; max-width: 200;`
- Sidebar button height: `height: 36;`
- Sidebar button icon size: `width: 26; height: 26;`
- ComboBox dropdown: `min-width: 100;`
- Many padding/margin values: `padding: 6 24;`, `padding: 8 6;`, etc.

Qt's QSS engine does not apply DPI scaling to pixel values. They are rendered at absolute device pixels.

## Font Handling

Fonts use `pointsize` in .ui files (e.g., `pointsize 12`), which is correct — point sizes are DPI-aware. However, the surrounding containers have fixed pixel heights, so on HiDPI the font grows but its container doesn't, causing truncation.

The Ubuntu font is bundled and loaded at `main.cpp:121`. This is orthogonal to HiDPI but relates to FR-26.

## The Architectural Problem

The app uses QWidgets throughout. QWidget's sizing model is fundamentally pixel-based. While Qt6 applies a global scale factor, it only affects "logical pixels" → "device pixels" mapping. All the hardcoded values above are in logical pixels, and Qt does NOT upscale them further — they stay at the specified logical pixel count.

On a 4K display at 200% scaling:
- A `setFixedSize(64, 64)` widget renders at 128 device pixels (correct density) but occupies 64 logical pixels of screen space — which is half the expected logical size compared to a standard 96-DPI display.
- Font `pointsize 12` correctly renders larger, but the widget around it stays at 64 logical pixels, causing overflow.

**QML solves this** because its coordinate system is density-independent by default. All dimensions are specified in device-independent pixels that scale automatically.

## Assessment: Feasibility Without QML Migration

A full QML migration is a massive undertaking. However, significant improvement is possible within QWidgets:

### What can be fixed without QML:
1. Replace hardcoded pixel sizes with DPI-scaled values using `QFontMetrics` or `logicalDpiX()`/`logicalDpiY()`
2. Remove `setFixedSize()` / `setFixedHeight()` calls where possible, relying on layouts instead
3. Use `QIcon::pixmap(size * devicePixelRatio)` with `setDevicePixelRatio()` for crisp icons
4. Convert fixed `.ui` sizes to minimum sizes with expanding policies
5. Use `em`-based or font-metric-based sizing in QSS where possible (limited support)

### What fundamentally can't be fixed in QWidgets:
1. QSS doesn't support `em` units for most properties — pixel sizes can't auto-scale
2. Some QWidget components (scrollbar widths, checkbox indicators) have no DPI-aware styling mechanism
3. Complex layouts with many interdependent fixed sizes are brittle to change

## Recommendation

**Severity: LOW** — The bug is real but the upstream project acknowledged it requires QML migration. Pragmatic improvements (DPI-aware C++ sizes, removing unnecessary fixed sizes from .ui files) can help, but a full fix is architectural.

The best near-term approach would be:
1. Replace C++ `setFixedSize()` calls with DPI-scaled equivalents using a helper function
2. Remove the most impactful `.ui` fixed sizes (sidebar, service items)
3. Accept that QSS-driven sizes (scrollbars, checkboxes) won't fully scale
4. Document the limitation and recommend `QT_SCALE_FACTOR=1` as a workaround for users
