# Nexis UI Redesign — Implementation Plan (Concept A)

**Concept:** Bento Dashboard with Collapsible Sidebar
**Scope:** Collapsible sidebar with grouped sections, bento grid dashboard with metric tiles + sparklines, refined color palette, and supporting infrastructure changes.

> **Implementation strategy:** Build in layers. Each phase produces a buildable, testable intermediate. No phase breaks existing functionality — the old widgets are replaced only after their new counterparts are verified.

---

## Phase 1: Collapsible Sidebar Infrastructure

**Goal:** Replace the fixed 220px sidebar with a collapsible sidebar that toggles between 64px icon-rail and 220px expanded states, with grouped navigation sections.

### Task 1.1 — Add sidebar collapse state management
- [x] Add `sidebarCollapsed` bool to `SettingManager` with getter/setter and persistence in QSettings
- [x] Add `sigSidebarCollapseToggled(bool collapsed)` signal to `SignalMapper`
- [x] Add `Ctrl+B` shortcut in `App` to toggle sidebar collapsed state
- **Files:** `setting_manager.h`, `setting_manager.cpp`, `signal_mapper.h`, `app.cpp`
- **Acceptance:** Setting persists across app restarts. Signal fires on toggle.

### Task 1.2 — Restructure sidebar layout in app.ui
- [x] Replace the flat `QVBoxLayout` of buttons inside `#sidebar` with a programmatic layout (built in `app.cpp` init) instead of the .ui file
- [x] Move all sidebar button creation from `.ui` to code so we can dynamically insert section headers and control visibility
- [x] Add a collapse toggle button at the top of the sidebar (hamburger icon)
- [x] Add section header labels: "MONITOR", "MANAGE", "SYSTEM" as non-clickable `QLabel` widgets between button groups
- [x] Group buttons under sections:
  - **MONITOR:** Dashboard, Hardware Info, Resources
  - **MANAGE:** System Cleaner, Search, Processes, Services, Startup Apps, Uninstaller
  - **SYSTEM:** Docker*, Helpers, APT/Homebrew*, GNOME Settings*, Settings
  (*conditional — already hidden when unavailable)
- [x] Move Feedback out of the sidebar button list — it becomes a small icon button in the sidebar footer or settings page
- **Files:** `app.ui`, `app.cpp`, `app.h`
- **Acceptance:** All sidebar buttons visible with section headers. Feedback is no longer in the page list. App builds and runs.

### Task 1.3 — Implement sidebar collapse/expand animation
- [x] When collapsed: sidebar width animates from 220px → 64px. Section headers hide. Button text hides. Only icons visible. Collapse toggle button changes to expand icon.
- [x] When expanded: sidebar width animates from 64px → 220px. Section headers fade in. Button text fades in.
- [x] Use `QPropertyAnimation` on sidebar `maximumWidth` + `minimumWidth` properties (250ms, QEasingCurve::OutCubic)
- [x] During animation, set button text visibility via `setVisible()` on a per-button label (or simply let QSS text clipping handle it by setting `max-width` on the button text area)
- [x] Section header labels: set `setVisible(false)` when collapsed
- [x] Icon-only mode: buttons show centered icons at 20x20 with no text padding
- **Files:** `app.cpp`, `app.h`
- **Acceptance:** Smooth 250ms collapse/expand. No layout jumping. Icons remain centered in collapsed state.

### Task 1.4 — QSS updates for collapsible sidebar
- [x] Add new QSS tokens to `values.ini` (both themes):
  - `@sidebarCollapsedWidth` = 64
  - `@sidebarExpandedWidth` = 220
  - `@sectionHeaderColor` (use `@color06` / secondary text)
- [x] Add QSS rules for collapsed sidebar state:
  ```css
  #sidebar[collapsed="true"] { min-width: 64; max-width: 64; }
  #sidebar[collapsed="true"] QPushButton { text-align: center; padding: 0; }
  #sidebar .sectionHeader { font-size: 8pt; font-weight: 600; text-transform: uppercase; color: @sectionHeaderColor; padding: 12px 12px 4px 12px; }
  #sidebar[collapsed="true"] .sectionHeader { max-height: 0; padding: 0; }
  ```
- [x] Use Qt dynamic property `collapsed` on the sidebar widget so QSS can target `#sidebar[collapsed="true"]`
- [x] Add collapse toggle button styles (`#btnSidebarToggle`)
- **Files:** `style.qss`, `values.ini` (dark), `values.ini` (light)
- **Acceptance:** Sidebar looks correct in both collapsed and expanded states. Both themes work.

### Task 1.5 — Remove pageTitle bar
- [x] Remove `#pageTitle` QLabel from `app.ui` / the page content area
- [x] The current page name will be implied by the selected sidebar button (which is highlighted orange)
- [x] In collapsed mode, show a small breadcrumb label at the top of the content area (optional — can be deferred)
- [x] Update `pageClick()` to no longer set `ui->pageTitle->setText()`
- [x] Update kiosk mode code that hides/shows `ui->pageTitle`
- **Files:** `app.ui`, `app.cpp`, `app.h`
- **Acceptance:** No page title bar visible. ~40px of vertical space recovered. Kiosk mode still works.

### Task 1.6 — Build verification
- [x] Clean rebuild: `rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) && cmake --build build -j$(sysctl -n hw.ncpu)`
- [x] Run `ctest --test-dir build --output-on-failure`
- [x] Manual verification: sidebar collapses/expands, all pages accessible, section headers visible when expanded, theme switching works
- **Acceptance:** Build succeeds. All tests pass. No regressions.

---

## Phase 2: New Color Palette & Theme Tokens

**Goal:** Refine the dark and light theme palettes to match the Nexis logo gradient more closely, add new semantic tokens needed for the bento dashboard.

### Task 2.1 — Update dark theme tokens
- [x] Update `values.ini` (dark theme) with refined colors:
  - `@pageContent` → `#1A1C22` (darker base)
  - `@sidebar` → `#222228`
  - `@cardBg` → `#2A2C32`
  - `@cardBgElevated` (new) → `#32343A` (hover/active cards)
  - `@borderColor` → `#3A3D4A`
  - `@accentColor` → `#FF6B1A` (brighter, matching logo 30% gradient stop)
  - `@accentHover` → `#E95420` (current accent becomes the darker hover)
  - `@color05` (primary text) → `#F0F2F5` (warm white)
  - `@color06` (secondary text) → `#9A9DA6`
  - `@tertiaryText` (new) → `#6B6E78`
  - `@destructiveColor` → `#E05454` (softer red)
  - `@infoColor` (new) → `#5B9BD5`
- [x] Add new tokens for gauge colors (each metric gets a named token):
  - `@cpuColor` → `#FF6B1A`
  - `@memoryColor` → `#FFB347`
  - `@diskColor` → `#E05454`
  - `@networkColor` → `#26A69A`
  - `@gpuColor` → `#813D9C`
  - `@tempColor` → `#5B9BD5`
  - `@batteryColor` → `#2EC27E`
  - `@diskHealthColor` → `#FF8C00`
- **Files:** `shared/nexis/static/themes/default/style/values.ini`
- **Acceptance:** Dark theme renders with the new palette. No missing tokens (AppManager validates at load).

### Task 2.2 — Update light theme tokens
- [x] Update `values.ini` (light theme) with warmer cream palette:
  - `@pageContent` → `#F5F0EB` (warm cream)
  - `@sidebar` → `#EDE7E0`
  - `@cardBg` → `#FFFFFF`
  - `@cardBgElevated` (new) → `#FFF8F2`
  - `@borderColor` → `#DED8D0`
  - Keep `@accentColor` → `#E95420` (darker orange for light bg readability)
  - Same gauge color tokens as dark theme (they work on both backgrounds)
- **Files:** `shared/nexis/static/themes/light/style/values.ini`
- **Acceptance:** Light theme renders correctly with warm cream tones.

### Task 2.3 — Update QSS references to use new tokens
- [x] Update CircleBar colors in `dashboard_page.cpp` to use the named tokens (read from style values at runtime) instead of hardcoded hex strings
- [x] Update any QSS rules that reference the old color values
- [x] Verify `@cardBgElevated` is used for hover states on cards
- **Files:** `dashboard_page.cpp`, `style.qss`
- **Acceptance:** All gauge colors consistent with the research document assignments. Theme switching works.

### Task 2.4 — Build verification
- [x] Incremental build + test
- [x] Visual verification: both themes look correct, no missing or garbled colors
- **Acceptance:** Build succeeds. Both themes render correctly.

---

## Phase 3: MetricTile Widget (Replaces CircleBar on Dashboard)

**Goal:** Create a new `MetricTile` widget that shows value + sparkline + trend indicator in a compact rectangular card, then swap it in for the CircleBar gauges on the Dashboard.

### Task 3.1 — Create MetricTile widget
- [x] Create new files:
  - `shared/nexis/Pages/Dashboard/metric_tile.h`
  - `shared/nexis/Pages/Dashboard/metric_tile.cpp`
  - `shared/nexis/Pages/Dashboard/metric_tile.ui`
- [x] MetricTile layout (top to bottom):
  1. Header row: `QLabel` title (left) + `QLabel` value (right, large font)
  2. Gauge: thin horizontal progress bar or mini arc gauge (configurable)
  3. Sparkline: `QChartView` with `QLineSeries`, no axes, transparent background, 60-data-point rolling window
  4. Footer row: `QLabel` subtitle (left) + trend indicator icon (right) — optional quick-action button
- [x] Constructor: `MetricTile(const QString &title, const QColor &color, QWidget *parent = nullptr)`
- [x] Public API:
  - `void setValue(int percent, const QString &valueText)` — updates gauge + value label
  - `void addDataPoint(double value)` — appends to sparkline ring buffer and redraws
  - `void setSubtitle(const QString &text)` — sets footer label (e.g., "2.83 GHz", "Core 3: 31%")
  - `void setTrendDirection(TrendDirection dir)` — enum { Rising, Falling, Stable }
  - `void setQuickAction(const QString &text, std::function<void()> callback)` — optional chip button
- [x] Sparkline implementation:
  - Ring buffer of 60 `double` values (one per second, matching DataRefreshService fast tick)
  - `QLineSeries` updated each tick
  - Chart: no axes, no legend, no margins, transparent background
  - Line color matches the metric color
  - Area under curve filled with 10% opacity of metric color
- [x] Trend calculation: compare last 5 values average vs previous 5 values average
  - If rising > 5%: `Rising` (↑ arrow)
  - If falling > 5%: `Falling` (↓ arrow)
  - Otherwise: `Stable` (→ arrow)
- [x] Add files to `GUI_SHARED_SRCS` and `GUI_SHARED_HDRS` in `CMakeLists.txt`
- [x] Add `Pages/Dashboard` to `CMAKE_AUTOUIC_SEARCH_PATHS` if not already present
- **Files:** New: `metric_tile.h`, `metric_tile.cpp`, `metric_tile.ui`. Modified: `CMakeLists.txt`
- **Acceptance:** MetricTile compiles. Can be instantiated standalone with test data.

### Task 3.2 — Create NetworkTile widget (specialized MetricTile)
- [x] Create:
  - `shared/nexis/Pages/Dashboard/network_tile.h`
  - `shared/nexis/Pages/Dashboard/network_tile.cpp`
  - `shared/nexis/Pages/Dashboard/network_tile.ui`
- [x] NetworkTile layout:
  1. Header: "Network"
  2. Two-line value: "↓ 28.7 KB/s" and "↑ 8.0 KB/s" with separate colors
  3. Dual sparkline: download (teal solid) + upload (teal dashed) overlaid
  4. Footer: "Total: ↓9.2 GB  ↑25.8 GB"
- [x] Public API:
  - `void setValues(quint64 rxDelta, quint64 txDelta, quint64 rxTotal, quint64 txTotal)`
- [x] Add to CMakeLists.txt
- **Files:** New: `network_tile.h`, `network_tile.cpp`, `network_tile.ui`. Modified: `CMakeLists.txt`
- **Acceptance:** NetworkTile compiles and renders correctly with test data.

### Task 3.3 — QSS styles for MetricTile and NetworkTile
- [x] Add QSS rules for the new widgets:
  ```css
  #metricTile { background-color: @cardBg; border-radius: 12; border: 1px solid @borderColor; }
  #metricTile:hover { background-color: @cardBgElevated; border-color: @accentColor; }
  #metricTileTitle { color: @color06; font-size: 9pt; font-weight: 600; text-transform: uppercase; }
  #metricTileValue { color: @color05; font-size: 18pt; font-weight: 700; }
  #metricTileSubtitle { color: @tertiaryText; font-size: 9pt; }
  #metricTileProgress { background-color: @color02; border: 0; border-radius: 2; max-height: 4; }
  #metricTileProgress::chunk { border-radius: 2; }
  ```
- **Files:** `style.qss`
- **Acceptance:** MetricTile looks correct in both themes.

### Task 3.4 — Build verification
- [x] Incremental build
- [x] Verify new widgets compile without errors
- **Acceptance:** Build succeeds.

---

## Phase 4: Bento Grid Dashboard Layout

**Goal:** Replace the current dashboard layout (row of CircleBars + row of secondary widgets) with a bento grid of MetricTiles.

### Task 4.1 — Redesign dashboard_page.ui
- [x] Replace the current `QGridLayout` structure with a new bento-style layout:
  - Row 0: CPU tile (2-col span, "hero" size), Disk tile (1 col), Network tile (1 col)
  - Row 1: GPU tile, Temperature tile, Battery tile, Disk Health tile
  - Row 2: Quick Actions bar (full width) — optional, can be Phase 6
  - Row 3: Update bar (keep existing)
- [x] Each tile occupies a cell in a `QGridLayout` with appropriate `rowSpan`/`colSpan`
- [x] CPU hero tile: larger card with sparkline for each core (or aggregate), value + clock speed
- [x] Graceful degradation: hide tiles for unavailable hardware (same conditional logic as current)
- [x] Make layout responsive: tiles should have minimum sizes but stretch proportionally
- **Files:** `dashboard_page.ui`
- **Acceptance:** Layout matches the bento mockup. Tiles arranged correctly.

### Task 4.2 — Wire MetricTiles into DashboardPage
- [x] Replace `CircleBar*` members with `MetricTile*` members in `dashboard_page.h`:
  - `mCpuBar` → `mCpuTile` (MetricTile)
  - `mMemBar` → `mMemTile` (MetricTile)
  - `mDiskBar` → `mDiskTile` (MetricTile)
  - `mTempBar` → `mTempTile` (MetricTile)
  - `mGpuBar` → `mGpuTile` (MetricTile)
  - `mBatteryBar` → `mBatteryTile` (MetricTile)
  - `mDiskHealthBar` → `mDiskHealthTile` (MetricTile)
  - `mDownloadBar` + `mUploadBar` → `mNetworkTile` (NetworkTile)
- [x] Update `dashboard_page.cpp` constructor to create MetricTiles with appropriate colors (reading from style values)
- [x] Update `init()` to add tiles to the new grid layout
- [x] Update all `on*Updated()` slots:
  - `onCpuUpdated()`: call `mCpuTile->setValue()` and `mCpuTile->addDataPoint()`
  - `onMemoryUpdated()`: call `mMemTile->setValue()` and `mMemTile->addDataPoint()`
  - `onDiskUsageUpdated()`: call `mDiskTile->setValue()`
  - `onNetworkUpdated()`: call `mNetworkTile->setValues()`
  - `onGpuUpdated()`: call `mGpuTile->setValue()` and `mGpuTile->addDataPoint()`
  - `updateTempBar()` → `updateTempTile()`: call `mTempTile->setValue()` and `addDataPoint()`
  - `onBatteryUpdated()`: call `mBatteryTile->setValue()`
  - `onDiskHealthUpdated()`: call `mDiskHealthTile->setValue()`
- [x] Add quick-action callbacks to tiles:
  - CPU tile: "View Processes" → navigate to ProcessesPage
  - Disk tile: "Clean" → navigate to SystemCleanerPage
  - Network tile: "Details →" → navigate to ResourcesPage
- [x] Keep alert notification logic unchanged (it only uses values, not widgets)
- **Files:** `dashboard_page.h`, `dashboard_page.cpp`
- **Acceptance:** Dashboard shows metric tiles with live data, sparklines update in real-time, quick actions work.

### Task 4.3 — System Summary Card (bottom of dashboard)
- [x] Create a simple `QWidget` (not a separate class — just a styled container) below the tile grid
- [x] Shows: hostname, OS name, kernel, uptime, last clean date/size, scheduled clean info
- [x] Data sources: `InfoManager::getSystemInfo()` for hostname/OS/kernel, `SettingManager` for last clean info
- [x] Style with `@cardBg` background, `@tertiaryText` labels, single-line compact layout
- **Files:** `dashboard_page.cpp`, `dashboard_page.ui`, `style.qss`
- **Acceptance:** Summary card displays correct system info.

### Task 4.4 — Remove old CircleBar and LineBar from Dashboard
- [x] Once MetricTiles are verified working, remove CircleBar/LineBar includes and usage from DashboardPage
- [x] **Do NOT delete** the CircleBar/LineBar source files yet — they may be used elsewhere or kept for reference
- [x] Clean up the old `.ui` layout elements (circleBars container, lineBars container, tempContainer, gpuContainer, batteryContainer)
- **Files:** `dashboard_page.h`, `dashboard_page.cpp`, `dashboard_page.ui`
- **Acceptance:** No references to CircleBar/LineBar in DashboardPage. Old layout containers removed.

### Task 4.5 — Build verification
- [x] Clean rebuild
- [x] Run tests
- [x] Manual verification: all dashboard tiles show live data, sparklines animate, quick actions navigate to correct pages
- **Acceptance:** Build succeeds. Tests pass. Dashboard is fully functional.

---

## Phase 5: Sidebar Icon Updates & Polish

**Goal:** Update sidebar icons to match the refined visual language, polish hover states, and ensure the sidebar feels cohesive with the new dashboard.

### Task 5.1 — Update sidebar SVG icons
- [x] Create/update SVG icons in `static/themes/nexis/img/sidebar-icons/` (and `default/` for dark theme) to use a consistent line-icon style:
  - Thinner stroke (1.5px), rounded caps, 20x20 viewBox
  - Ensure they are legible at both 20x20 (expanded) and 16x16 (collapsed icon rail)
- [x] Icons needed:
  - `dash.svg` — grid/bento icon (replacing gauge icon)
  - `hardware-info.svg` — circuit board / chip
  - `resources.svg` — activity/chart line
  - `cleaner.svg` — broom/sparkle
  - `search.svg` — magnifying glass
  - `process.svg` — list/activity
  - `services.svg` — gear/cog
  - `startup-apps.svg` — rocket/play
  - `uninstaller.svg` — package-minus / trash
  - `docker.svg` — whale (keep)
  - `helpers.svg` — wrench
  - `ppa-manager.svg` — package/box
  - `gnome-settings.svg` — desktop
  - `settings.svg` — sliders
  - `sidebar-collapse.svg` (new) — chevron-left
  - `sidebar-expand.svg` (new) — chevron-right / hamburger
- [x] Update `updateSidebarIcons()` in `app.cpp` to load new icons
- **Files:** SVG files in `static/themes/*/img/sidebar-icons/`, `app.cpp`
- **Acceptance:** All sidebar icons render correctly in both collapsed and expanded states, both themes.

### Task 5.2 — Sidebar hover and active states polish
- [x] Update QSS for improved sidebar button states:
  - Checked (active): use accent color with slight gradient or left-edge indicator bar
  - Hover: subtle background lightening + left-edge highlight
  - Collapsed mode: checked button shows a small colored dot or left bar indicator instead of full background
- [x] Section headers: when expanded, show in uppercase with tertiary text color. Add subtle top border/padding for visual separation between groups.
- **Files:** `style.qss`
- **Acceptance:** Sidebar feels polished and modern. Active/hover states are clear.

### Task 5.3 — Build verification
- [x] Incremental build + visual check
- **Acceptance:** Sidebar looks cohesive. No icon rendering issues.

---

## Phase 6: Quick Actions Bar & Command Palette (Stretch Goals)

> These features enhance the experience but are not required for the core redesign. They can be implemented after Phases 1-5 are stable.

### Task 6.1 — Quick Actions Bar on Dashboard
- [x] Add a horizontal bar below the bento grid with action chips:
  - "Clean System" → SystemCleanerPage
  - "View Processes" → ProcessesPage
  - "Check Updates" → trigger checkUpdate()
  - Recent activity indicator: "Last clean: 2h ago — freed 2.3 GB"
- [x] Style as a low-profile card with pill-shaped buttons
- **Files:** `dashboard_page.cpp`, `dashboard_page.ui`, `style.qss`

### Task 6.2 — Command Palette (Ctrl+K)
- [x] Create new dialog: `shared/nexis/command_palette.h` / `.cpp`
- [x] Overlay popup (not a full dialog — maybe `QFrame` with `Qt::Popup` flag) centered in the window
- [x] Contains a `QLineEdit` search box + `QListWidget` results list
- [x] Indexes:
  - All page names → navigate to page
  - Actions: "Quick Clean", "Toggle Theme", "Toggle Sidebar", "Kiosk Mode"
  - Settings shortcuts
- [x] Fuzzy matching on the search text
- [x] Up/Down arrow keys navigate results, Enter activates
- [x] Escape closes
- [x] Add to CMakeLists.txt
- **Files:** New: `command_palette.h`, `command_palette.cpp`. Modified: `app.cpp`, `app.h`, `CMakeLists.txt`, `style.qss`

### Task 6.3 — Build verification
- [x] Build and test
- **Acceptance:** Quick actions bar works. Command palette opens, searches, and navigates.

---

## File Change Summary

### New Files
| File | Purpose |
|------|---------|
| `shared/nexis/Pages/Dashboard/metric_tile.h` | MetricTile widget header |
| `shared/nexis/Pages/Dashboard/metric_tile.cpp` | MetricTile widget implementation |
| `shared/nexis/Pages/Dashboard/metric_tile.ui` | MetricTile widget UI definition |
| `shared/nexis/Pages/Dashboard/network_tile.h` | NetworkTile widget header |
| `shared/nexis/Pages/Dashboard/network_tile.cpp` | NetworkTile widget implementation |
| `shared/nexis/Pages/Dashboard/network_tile.ui` | NetworkTile widget UI definition |
| `shared/nexis/command_palette.h` | Command palette header (Phase 6) |
| `shared/nexis/command_palette.cpp` | Command palette implementation (Phase 6) |
| SVG icons for sidebar collapse/expand | New sidebar control icons |

### Modified Files
| File | Changes |
|------|---------|
| `CMakeLists.txt` | Add new source files to `GUI_SHARED_SRCS`/`GUI_SHARED_HDRS` |
| `app.ui` | Remove `pageTitle`, restructure sidebar to allow programmatic sections |
| `app.h` | Add sidebar collapse state, remove `pageTitle` references |
| `app.cpp` | Sidebar collapse/expand logic, section headers, remove pageTitle updates, Ctrl+B shortcut |
| `dashboard_page.h` | Replace CircleBar/LineBar members with MetricTile/NetworkTile |
| `dashboard_page.cpp` | New init, new update slots, quick actions, system summary card |
| `dashboard_page.ui` | Bento grid layout replacing old grid |
| `style.qss` | New sidebar states, MetricTile styles, section headers, collapse rules |
| `values.ini` (dark) | Updated palette colors, new gauge/semantic tokens |
| `values.ini` (light) | Updated palette colors, new gauge/semantic tokens |
| `signal_mapper.h` | Add `sigSidebarCollapseToggled` signal |
| `setting_manager.h` / `.cpp` | Add sidebar collapse persistence |

### Files NOT Modified (preserved)
| File | Reason |
|------|--------|
| `circlebar.h/.cpp/.ui` | Kept for potential reuse; removed from Dashboard only |
| `linebar.h/.cpp/.ui` | Kept for potential reuse; removed from Dashboard only |
| All non-Dashboard pages | No changes in Phases 1-5 |
| `data_refresh_service.h/.cpp` | Signal interface unchanged — dashboard just consumes them differently |
| `sliding_stacked_widget.h/.cpp` | Page switching mechanism unchanged |

---

## Implementation Order & Dependencies

```
Phase 1 (Sidebar) ─────────────────────────> Phase 5 (Icons & Polish)
                                                       │
Phase 2 (Palette) ──┐                                 │
                     ├──> Phase 3 (MetricTile) ──> Phase 4 (Bento Dashboard)
Phase 1 ────────────┘                                  │
                                                       v
                                              Phase 6 (Stretch Goals)
```

- **Phases 1 & 2** are independent and can be developed in parallel.
- **Phase 3** depends on Phase 2 (needs new color tokens for gauge colors).
- **Phase 4** depends on Phases 1 + 3 (needs collapsible sidebar + MetricTile widget).
- **Phase 5** depends on Phase 1 (needs sidebar restructure complete).
- **Phase 6** depends on Phase 4 (needs bento dashboard).

---

## Risk Assessment

| Risk | Mitigation |
|------|------------|
| CircleBar uses QtCharts; MetricTile's sparkline also needs QtCharts | QtCharts already linked. No new dependency. |
| Sidebar animation may cause layout thrashing | Use `QPropertyAnimation` on min/max width. Set `SizeConstraint::SetFixedSize` during animation. |
| Removing pageTitle breaks `clickSidebarButton()` / tray actions | Tray actions use `windowTitle()` matching on pages, not `pageTitle` label. Just remove the label update call. |
| Kiosk mode hides sidebar — collapse state must be preserved | Save pre-kiosk collapse state, restore on kiosk exit. |
| Conditional pages (Docker, GNOME, APT) insert at dynamic indices | Index insertion logic in `init()` remains identical — only the layout within the sidebar changes. |
| QSS dynamic property `collapsed` requires `unpolish()`/`polish()` cycle | Call `style()->unpolish(sidebar); style()->polish(sidebar);` after toggling the property. |

---

## Estimated Scope

| Phase | New Code (LOC est.) | Modified Code | Effort |
|-------|-------------------|---------------|--------|
| Phase 1: Sidebar | ~300 | ~200 modified | Medium |
| Phase 2: Palette | ~0 code | ~60 token changes | Low |
| Phase 3: MetricTile | ~400 new | ~20 CMake | Medium |
| Phase 4: Bento Dashboard | ~200 new | ~350 modified | High |
| Phase 5: Icons & Polish | ~50 | ~50 QSS | Low |
| Phase 6: Stretch Goals | ~300 new | ~50 | Medium |
| **Total** | **~1,250 new** | **~730 modified** | |
