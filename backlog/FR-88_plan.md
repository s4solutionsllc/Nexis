# FR-88 Plan: Migrate Inline setStyleSheet() Calls to Central QSS

## Approach

Migrate in 4 phases, ordered by impact and safety. Each phase is independently committable and testable. Static migrations (Phase 1-2) are pure refactors with zero behavioral change. Semi-dynamic migrations (Phase 3) introduce property selectors. Phase 4 is cleanup.

---

## Phase 1 — Helper Widgets (Highest Impact)

These 3 files have the most inline styles (20 static selectors) and are purely theme-token-based. Move all `refreshThemeColors()` styles to QSS and strip the C++ styling code.

### Task 1.1: Add Helper widget QSS rules to `style.qss`
- [ ] Add a `/* -- Helper Widgets (Firewall / Open Ports / Network Diagnostics) -- */` section
- [ ] Add rules for all firewall widgets:
  - `#fwTitle` — title label
  - `#fwStatusText` — status label
  - `#fwToggle` — QPushButton with `:hover`, `:disabled` states
  - `#fwDetailCard` — QFrame card
  - `QLabel[objectName="fwSecondary"]` or iterate via `#fwSecondary` — secondary labels
  - `#fwWarning` — warning label
  - `#fwHelpBtn` — QToolButton transparent with hover
  - `#fwRefresh` — QPushButton with `:hover` state
- [ ] Add rules for all open ports widgets:
  - `#portsTitle` — title label
  - `#portsSearch` — QLineEdit with `:focus`
  - `#portsListenToggle` — QPushButton with `:checked`, `:hover:!checked`
  - `#portsRefresh` — QPushButton with `:hover`, `:disabled`
  - `#portsTable` — QTableView with `::item`, `::item:selected`, `QHeaderView::section`
  - `QLabel[objectName="portsSecondary"]` or `#portsSecondary` — secondary labels
- [ ] Add rules for all network diagnostics widgets:
  - `#netDiagTitle` — title label
  - `#netDiagCard` — QFrame card
  - `#netDiagSubheader` — subheader label
  - `QLabel[objectName="netDiagSecondary"]` or `#netDiagSecondary` — secondary labels
  - `#netDiagRetest` — QPushButton with `:hover`, `:disabled`

### Task 1.2: Strip inline styles from `firewall_widget.cpp`
- [ ] Remove all `setStyleSheet()` calls in `refreshThemeColors()` for the 9 static widgets
- [ ] Remove theme token variable declarations that are no longer used
- [ ] Keep `refreshThemeColors()` if it still handles the semi-dynamic status dot; otherwise remove entirely and disconnect signal
- [ ] Keep the `onStatusFetched()` status dot styling (semi-dynamic — Phase 3)

### Task 1.3: Strip inline styles from `open_ports_widget.cpp`
- [ ] Remove all `setStyleSheet()` calls in `refreshThemeColors()` for the 6 static widgets
- [ ] Remove unused token variables
- [ ] Keep `onConnectionsFetched()` model foreground colors (must stay inline — QStandardItem, not QSS-targetable)

### Task 1.4: Strip inline styles from `network_diag_widget.cpp`
- [ ] Remove all `setStyleSheet()` calls in `refreshThemeColors()` for the 5 static widgets
- [ ] Remove unused token variables
- [ ] Keep `onDiagnosticsFinished()` dynamic result widget styling (semi-dynamic — Phase 3)

### Task 1.5: Build and visual verification
- [ ] `cmake --build build -j$(nproc)` — confirm clean compile
- [ ] Run app, open Helpers page, verify:
  - Firewall section renders identically
  - Open Ports section renders identically
  - Network Diagnostics section renders identically
  - Theme switching updates all widgets correctly

---

## Phase 2 — Other Static Migrations

### Task 2.1: Add Helpers Page power profile QSS
- [ ] Add to `style.qss`:
  - Power profile button group styling (checked/hover states)
  - `#powerProfileWidget` — card background
  - `#powerProfileLabel` — label color
  - Conflict warning label styling
- [ ] Strip `applyPowerProfileStyle()` inline styles from `helpers_page.cpp`
- [ ] Keep `applyPowerProfileStyle()` only if it handles dynamic state; otherwise remove and disconnect signal

### Task 2.2: Add Exclusion Manager dialog QSS
- [ ] Add `#lblExclusionNotice` rule to QSS
- [ ] Strip `setStyleSheet()` from `exclusion_manager_dialog.cpp::refreshThemeColors()`
- [ ] If `refreshThemeColors()` is now empty, remove it and disconnect signal

### Task 2.3: Add APT Source Repository Item QSS
- [ ] Add `#repoStatusBadge` base styling (border-radius, padding, font-size) — note: background-color is semi-dynamic (Phase 3)
- [ ] Move static `mLblDescription` color to QSS via existing `#repoDetailDescription` or a new selector
- [ ] Strip static `setStyleSheet()` calls from `apt_source_repository_item.cpp`

### Task 2.4: Build and visual verification
- [ ] Clean compile
- [ ] Verify Helpers power profile section, Exclusion Manager dialog, APT Source items

---

## Phase 3 — Semi-Dynamic Migrations (Property Selectors)

Convert runtime color choices to QSS dynamic property selectors. Each widget sets a property like `status` and QSS matches on it.

### Task 3.1: Define status property QSS rules
- [ ] Add generic status-colored selectors to `style.qss`:
  ```qss
  /* Status-based color selectors for dynamic widgets */
  QLabel[status="success"] { color: @successColor; }
  QLabel[status="warning"] { color: @warningColor; }
  QLabel[status="error"]   { color: @destructiveColor; }
  QLabel[status="info"]    { color: @color05; }
  ```

### Task 3.2: Migrate Firewall status dot
- [ ] In `firewall_widget.cpp::onStatusFetched()`: replace `setStyleSheet()` with `setProperty("status", "success"/"error")` + unpolish/polish
- [ ] Add QSS rule for `#fwStatusDot[status="success"]` and `[status="error"]` with font-size

### Task 3.3: Migrate Disk Tile health labels
- [ ] In `disk_tile.cpp::setDriveHealth()` and `refreshThemeColors()`: replace `setStyleSheet()` with `setProperty("status", "success"/"error")` + unpolish/polish
- [ ] Add QSS rule for status labels with font-size and font-weight

### Task 3.4: Migrate Maintenance Wizard step icons
- [ ] In `maintenance_wizard_dialog.cpp::setStepStatus()`: replace `setStyleSheet()` with `setProperty("status", ...)` + unpolish/polish
- [ ] Reuse generic status QSS rules from Task 3.1

### Task 3.5: Migrate Verify Disk status label
- [ ] In `helpers_page.cpp::onVerifyDisk()`: replace `setStyleSheet()` with `setProperty("status", ...)` + unpolish/polish

### Task 3.6: Migrate Repo Detail Panel status badge
- [ ] In `repo_detail_panel.cpp::showRepo()`: replace `setStyleSheet()` on `mLblStatusBadge` with `setProperty("repoStatus", "healthy"/"warning"/"error"/"unknown")` + unpolish/polish
- [ ] Add QSS rules:
  ```qss
  #repoStatusBadge[repoStatus="healthy"] { background-color: @successColor; }
  #repoStatusBadge[repoStatus="warning"] { background-color: @warningColor; }
  #repoStatusBadge[repoStatus="error"]   { background-color: @destructiveColor; }
  #repoStatusBadge[repoStatus="unknown"] { background-color: @tertiaryText; }
  ```

### Task 3.7: Migrate APT Source Repository Item status
- [ ] In `apt_source_repository_item.cpp::updateStatusIndicator()`: replace inline styles with `setProperty("repoStatus", ...)` + unpolish/polish
- [ ] Add QSS rules for status dot color and left border color based on `[repoStatus=...]`

### Task 3.8: Evaluate Network Diagnostics result widgets
- [ ] These are created dynamically per test result in `onDiagnosticsFinished()`. They CAN use QSS since objectNames are set and QSS is global.
- [ ] Set `setProperty("status", "success"/"error")` on icon and value labels
- [ ] Reuse generic status QSS rules

### Task 3.9: Build and full visual verification
- [ ] Clean compile
- [ ] Test all semi-dynamic scenarios:
  - Firewall enabled/disabled status display
  - Disk health good/bad colors
  - Maintenance wizard step states
  - Repo health badge all 4 states
  - Network diagnostics pass/fail results
  - Theme switching in all states

---

## Phase 4 — Cleanup

### Task 4.1: Remove empty `refreshThemeColors()` methods
- [ ] For each migrated file: if `refreshThemeColors()` is now empty or only handles fully-dynamic styles that were already inline, remove the method and disconnect from `sigChangedAppTheme`
- [ ] Note: Some files will still need `refreshThemeColors()` for Category C (fully dynamic) styles — only remove if truly empty

### Task 4.2: Remove unused includes
- [ ] Remove `#include "Managers/app_manager.h"` from files that no longer call `getStyleValues()` (only if no other AppManager usage)
- [ ] Remove unused `QSettings` includes

### Task 4.3: Update documentation
- [ ] Update `docs/ARCHITECTURE_REVIEW.md` — note that styling is now centralized in QSS
- [ ] Update `docs/APPLICATION_OVERVIEW.md` if inline style count stats are mentioned

### Task 4.4: Final build + test
- [ ] Full clean rebuild
- [ ] `ctest --test-dir build --output-on-failure`
- [ ] Manual verification: cycle through all pages, switch themes, verify no regressions

---

## Out of Scope

These **must remain inline** and should not be migrated:
- `health_score_tile.cpp` — per-score threshold colors + paintEvent component bars
- `metric_tile.cpp` — per-instance progress bar chunk color from mColorToken
- `metric_tile_base.cpp` — per-instance action button color
- `network_tile.cpp` — per-instance download/upload colors
- `ring_tile.cpp` — per-instance progress bar color
- `vumeter_tile.cpp` — per-instance segment colors (paintEvent)
- `open_ports_widget.cpp` — QStandardItem foreground (model data, not QSS)
- QScrollArea viewport transparent hacks (QSS can't target viewport widget)

## Acceptance Criteria

1. All static inline styles moved to `style.qss` with no visual change
2. Semi-dynamic styles use `setProperty()` + QSS property selectors
3. No hardcoded hex colors remain in migrated code
4. Theme switching works correctly for all migrated widgets
5. Clean build, all tests pass
6. No regression in any page's appearance
