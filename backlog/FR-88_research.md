# FR-88 Research: Migrate Inline setStyleSheet() Calls to Central QSS

## Problem Statement

The Nexis codebase has ~60 inline `setStyleSheet()` calls spread across 11+ C++ files. While all use theme tokens (BUG-47 compliant), the styling is split between the central `style.qss` and per-file C++ code, creating two parallel styling systems. This makes visual changes require editing C++ files and recompiling, rather than just editing the QSS.

## Token Replacement Architecture

`AppManager::updateStylesheet()` (app_manager.cpp:115-196):
1. Loads `style.qss` from QRC
2. Loads `values.ini` for the active theme (dark or light)
3. Sorts tokens by descending length (prevents substring collisions, per BUG-49)
4. String-replaces all `@tokens` with hex values
5. Replaces `@fontFamily` and `@dp<N>` scaling tokens
6. Calls `qApp->setStyleSheet()` globally

All 73 tokens from `values.ini` are available in QSS. New tokens can be added to both `default/style/values.ini` and `light/style/values.ini`.

## Classification of All Inline Styles

### Category A: STATIC (38 instances)

These use only theme tokens with no runtime logic. Direct QSS migration candidates.

**Firewall Widget** (`firewall_widget.cpp` — `refreshThemeColors()`):
- `#fwTitle` — `color: @color05; font-size: 16px; font-weight: bold;`
- `#fwStatusText` — `color: @color05; font-size: 14px; font-weight: bold;`
- `#fwToggle` — QPushButton with normal/hover/disabled states using @accentColor, @cardBg, @color04, @color07
- `#fwDetailCard` — `QFrame { background-color: @cardBg; border: 1px solid @borderColor; border-radius: 8px; }`
- `fwSecondary` labels — `color: @color04; font-size: 12px;`
- `#fwWarning` — `color: @warningColor; font-size: 13px;`
- `#fwHelpBtn` — QToolButton transparent with hover @color04
- `#fwRefresh` — QPushButton with normal/hover states using @cardBg, @color05, @borderColor, @accentColor, @color04

**Open Ports Widget** (`open_ports_widget.cpp` — `refreshThemeColors()`):
- `#portsTitle` — `color: @color05; font-size: 16px; font-weight: bold;`
- `#portsSearch` — QLineEdit with focus state using @cardBg, @color05, @borderColor, @accentColor
- `#portsListenToggle` — QPushButton with checked/hover states using @cardBg, @color04, @borderColor, @accentColor, @color07
- `#portsRefresh` — QPushButton with hover/disabled using @accentColor, @cardBg, @color04, @color07
- `#portsTable` — QTableView full styling using @cardBg, @color05, @borderColor, @accentColor, @color04, @color07
- `portsSecondary` labels — `color: @color04; font-size: 12px;`

**Network Diagnostics Widget** (`network_diag_widget.cpp` — `refreshThemeColors()`):
- `#netDiagTitle` — `color: @color05; font-size: 16px; font-weight: bold;`
- `#netDiagCard` — `QFrame { background-color: @cardBg; border: 1px solid @borderColor; border-radius: 8px; }`
- `#netDiagSubheader` — `color: @color05; font-size: 14px; font-weight: bold;`
- `netDiagSecondary` labels — `color: @color04; font-size: 12px/13px;`
- `#netDiagRetest` — QPushButton with hover/disabled using @accentColor, @cardBg, @color04, @color07

**Helpers Page** (`helpers_page.cpp`):
- Power profile buttons — QPushButton with checked/hover using @cardBg, @color04, @borderColor, @accentColor, @color07
- `#powerProfileWidget` — `background-color: @cardBg; border-radius: 6px;`
- `#powerProfileLabel` — `color: @color05; font-size: 12px;`
- Conflict warning label — `color: @warningColor; font-size: 11px;`

**Exclusion Manager Dialog** (`exclusion_manager_dialog.cpp`):
- `#lblExclusionNotice` — `color: @color05; padding: 6px 0;`

**Repo Detail Panel** (`repo_detail_panel.cpp`):
- QScrollArea / container — `background-color: transparent;` (viewport fix)

**Settings Page** (`settings_page.cpp`):
- QScrollArea / container — `background-color: transparent;` (viewport fix)

**App** (`app.cpp`):
- Kiosk overlay — `background-color: @overlayBackground; color: @overlayText; font-size: 14pt;`

### Category B: SEMI-DYNAMIC (9 instances)

Fixed CSS structure but runtime color choice (success vs error). Can use QSS dynamic property selectors.

| Widget | File | Runtime States | Approach |
|--------|------|---------------|----------|
| Firewall status dot | `firewall_widget.cpp` | enabled → @successColor, disabled → @destructiveColor | `setProperty("status", "success"/"error")` + QSS `[status="success"]` |
| Port state colors | `open_ports_widget.cpp` | LISTEN → @successColor, ESTABLISHED → @warningColor, CLOSE* → @destructiveColor | QStandardItem foreground — must stay inline (model data, not widget) |
| Diag result icon | `network_diag_widget.cpp` | pass → @successColor, fail → @destructiveColor | `setProperty("status", ...)` + QSS — but widgets are created dynamically per result |
| Diag result value | `network_diag_widget.cpp` | success → @color04, fail → @destructiveColor | Same as above |
| Verify disk status | `helpers_page.cpp` | success → @successColor, fail → @destructiveColor | `setProperty("status", ...)` + QSS |
| Repo status badge | `repo_detail_panel.cpp` | Healthy/Warning/Error/Unknown → 4 different colors | `setProperty("repoStatus", ...)` + QSS |
| Repo item border | `apt_source_repository_item.cpp` | Healthy/Warning/Error/Unknown → left border color | `setProperty("repoStatus", ...)` + QSS |
| Drive health label | `disk_tile.cpp` | healthy → @successColor, unhealthy → @destructiveColor | `setProperty("status", ...)` + QSS |
| Wizard step icon | `maintenance_wizard_dialog.cpp` | running/ok/warning/error → 4 colors | `setProperty("status", ...)` + QSS |

### Category C: FULLY DYNAMIC (13 instances)

Per-instance colors (e.g., each metric tile has a different color). Must stay inline.

- `health_score_tile.cpp` — score-based color (threshold: ≥75/≥40/<40)
- `health_score_tile.cpp` — paintBreakdownBars per-component colors
- `metric_tile.cpp` — progress bar chunk color from per-instance mColorToken
- `metric_tile_base.cpp` — action button color from per-instance metric color
- `network_tile.cpp` — download/upload label colors from per-instance tokens
- `ring_tile.cpp` — progress bar chunk color from per-instance mMetricColor
- `vumeter_tile.cpp` — segment colors from range thresholds (paint, not stylesheet)

**These must remain inline and are out of scope.**

## Existing QSS Patterns to Follow

**Page-scoped selectors:**
```qss
#HardwareInfoPage QGroupBox { ... }
#SystemCleanerPage #btnScan { ... }
#DiskToolsPage #btnTrash { ... }
```

**Property-based selectors:**
```qss
QPushButton[accessibleName="primary"] { ... }
QPushButton[accessibleName="danger"] { ... }
QLabel[accessibleName="dimmed"] { ... }
#sidebar[collapsed="true"] QPushButton { ... }
```

**State selectors:**
```qss
QPushButton:hover { ... }
QPushButton:checked { ... }
QPushButton:disabled { ... }
QLineEdit:focus { ... }
```

## Key Constraints

1. **Token substring safety (BUG-49):** New tokens must not be substrings of existing tokens.
2. **Re-polish requirement (BUG-56):** After `setProperty()` on a parent, child widgets need explicit `unpolish()`/`polish()`.
3. **QScrollArea viewport fix:** Must remain inline — QSS doesn't penetrate the viewport widget properly.
4. **QStandardItem colors:** Model-level data (e.g., port state foreground colors) cannot use QSS; must stay inline.
5. **Dynamic widgets:** Widgets created at runtime (diagnostic results, issue cards) can use QSS if they have objectNames, since QSS is global.

## Files Affected

| File | Static | Semi-Dynamic | Fully Dynamic |
|------|--------|--------------|---------------|
| `firewall_widget.cpp` | 9 | 1 | 0 |
| `open_ports_widget.cpp` | 6 | 1 (model) | 0 |
| `network_diag_widget.cpp` | 5 | 2 | 0 |
| `helpers_page.cpp` | 4 | 1 | 0 |
| `repo_detail_panel.cpp` | 2 | 2 | 0 |
| `apt_source_repository_item.cpp` | 1 | 2 | 0 |
| `exclusion_manager_dialog.cpp` | 1 | 0 | 0 |
| `maintenance_wizard_dialog.cpp` | 0 | 1 | 0 |
| `disk_tile.cpp` | 0 | 1 | 0 |
| `health_score_tile.cpp` | 0 | 0 | 2 |
| `metric_tile.cpp` | 0 | 0 | 1 |
| `metric_tile_base.cpp` | 0 | 0 | 1 |
| `network_tile.cpp` | 0 | 0 | 3 |
| `ring_tile.cpp` | 0 | 0 | 1 |
| `vumeter_tile.cpp` | 0 | 0 | 2 |
| `settings_page.cpp` | 2 | 0 | 0 |
| `app.cpp` | 1 | 0 | 0 |
| **Totals** | **31** | **11** | **10** |
