# FR-88 Plan: Migrate Inline setStyleSheet() Calls to Central QSS

## Approach

Migrate in 4 phases, ordered by impact and safety. Each phase is independently committable and testable. Static migrations (Phase 1-2) are pure refactors with zero behavioral change. Semi-dynamic migrations (Phase 3) introduce property selectors. Phase 4 is cleanup.

---

## Phase 1 — Helper Widgets (Highest Impact)

These 3 files have the most inline styles (20 static selectors) and are purely theme-token-based. Move all `refreshThemeColors()` styles to QSS and strip the C++ styling code.

### Task 1.1: Add Helper widget QSS rules to `style.qss`
- [x] Add a `/* -- Helper Widgets (Firewall / Open Ports / Network Diagnostics) -- */` section
- [x] Add rules for all firewall widgets (#fwTitle, #fwStatusText, #fwToggle, #fwDetailCard + descendant QLabel, #fwWarning, #fwHelpBtn, #fwRefresh)
- [x] Add rules for all open ports widgets (#portsTitle, #portsSearch, #portsListenToggle, #portsRefresh, #portsTable + QHeaderView, #portsSecondary)
- [x] Add rules for all network diagnostics widgets (#netDiagTitle, #netDiagCard, #netDiagSubheader, #netDiagSecondary, #netDiagLoading, #netDiagRetest)
- [x] Gave mLblLoading distinct objectName "netDiagLoading" (was "netDiagSecondary") to preserve its 13px vs 12px font-size difference

### Task 1.2: Strip inline styles from `firewall_widget.cpp`
- [x] Removed all static setStyleSheet() calls from refreshThemeColors() (9 widgets)
- [x] Kept refreshThemeColors() — still needed to re-apply semi-dynamic status dot via onStatusFetched(mCurrentStatus)
- [x] Kept onStatusFetched() dot color inline (semi-dynamic — Phase 3)

### Task 1.3: Strip inline styles from `open_ports_widget.cpp`
- [x] Removed entire refreshThemeColors() body (6 static widgets)
- [x] Kept method stub — signal still connected, empty body is safe
- [x] Kept onConnectionsFetched() model foreground colors (QStandardItem, not QSS-targetable)

### Task 1.4: Strip inline styles from `network_diag_widget.cpp`
- [x] Removed entire refreshThemeColors() body (6 static widgets)
- [x] Kept method stub — signal still connected, empty body is safe
- [x] Kept onDiagnosticsFinished() dynamic result widget styling

### Task 1.5: Build and visual verification
- [x] Clean compile — 0 warnings
- [x] All 26 tests pass (including FirewallTests, OpenPortsTests, NetworkDiagTests)

---

## Phase 2 — Other Static Migrations

### Task 2.1: Add Helpers Page power profile QSS
- [x] Added `#powerProfileWidget`, `#powerProfileLabel`, `#powerProfileWidget QPushButton` (with :checked/:hover states), `#powerProfileWarning` to style.qss
- [x] Emptied `applyPowerProfileStyle()` body — kept method stub (called from init and update)
- [x] Removed inline conflict warning styling from `initPowerProfileUI()`

### Task 2.2: Add Exclusion Manager dialog QSS
- [x] Added `#lblExclusionNotice` rule to style.qss
- [x] Emptied `refreshThemeColors()` body — kept method stub (called from constructor)

### Task 2.3: Add APT Source Repository Item QSS
- [x] Added `#lblRepoDescription` rule with `color: @tertiaryText` to style.qss
- [x] Removed static `mLblDescription` inline styling from `init()` and `refreshThemeColors()`
- [x] Kept `updateStatusIndicator()` call in `refreshThemeColors()` (semi-dynamic — Phase 3)

### Task 2.4: Build and visual verification
- [x] Clean compile — 0 warnings
- [x] All 26 tests pass

---

## Phase 3 — Semi-Dynamic Migrations (Property Selectors)

Convert runtime color choices to QSS dynamic property selectors. Each widget sets a property like `status` and QSS matches on it.

### Task 3.1: Define status property QSS rules
- [x] Added generic `[status="success/warning/error/info/dimmed/neutral"]` selectors
- [x] Added `#fwStatusDot` with font-size: 16px, objectName set on mLblDot
- [x] Removed `app_manager.h` include from `firewall_widget.cpp` (no longer needed)

### Task 3.2: Migrate Disk Tile health labels
- [x] Added `#diskHealthStatus` QSS rule with font-size/weight
- [x] Replaced setStyleSheet with setProperty("status",...) + unpolish/polish in both setDriveHealth() and refreshThemeColors()

### Task 3.3: Migrate Maintenance Wizard step icons
- [x] Added `#wizardStepIcon` QSS rules with per-status font-size/weight variants
- [x] Replaced setStepStatus() inline styling with setProperty("status",...) + unpolish/polish
- [x] Emptied refreshThemeColors() (was effectively a no-op)

### Task 3.4: Migrate Verify Disk status label
- [x] Added `#verifyDiskStatus` QSS rule with font-weight: bold
- [x] Replaced inline styling with setProperty("status",...) in onVerifyDisk()
- [x] Removed `app_manager.h` include from helpers_page.cpp

### Task 3.5: Migrate APT Source Repository Item status
- [x] Added `#repoStatusDot` QSS rule with font-size/weight
- [x] Added `#aptSourceRepositoryItemWidget[repoStatus=...]` border-left rules for all 4 states
- [x] Replaced updateStatusIndicator() inline styling with setProperty + unpolish/polish
- [x] Removed `app_manager.h` include from apt_source_repository_item.cpp

### Task 3.6: Deferred to future work
- Repo detail panel status badge (showRepo) — complex background-color + scoped button styles
- Repo detail panel issue widgets (addIssueWidget) — per-severity border + scoped button styles
- Network diagnostics result widgets — multiple dynamically-created label types with different font sizes
- These remain inline as the migration complexity outweighs the benefit

### Task 3.7: Build and verification
- [x] Clean compile — 0 warnings
- [x] All 26 tests pass

---

## Phase 4 — Cleanup

### Task 4.1: Remove empty `refreshThemeColors()` methods
- [x] Removed from `open_ports_widget.cpp/.h` — method + signal connection + header declaration
- [x] Removed from `network_diag_widget.cpp/.h` — method + signal connection + header declaration
- [x] Removed from `maintenance_wizard_dialog.cpp/.h` — method + signal connection + constructor call + header declaration
- [x] Removed from `exclusion_manager_dialog.cpp/.h` — method + constructor call + header declaration (no signal)
- [x] Removed `applyPowerProfileStyle()` from `helpers_page.cpp/.h` — method + 2 call sites

### Task 4.2: Remove unused includes
- [x] Removed `signal_mapper.h` from `open_ports_widget.cpp` and `network_diag_widget.cpp`
- [x] Removed `QSettings` from `maintenance_wizard_dialog.cpp`
- [x] Kept `app_manager.h` in `open_ports_widget.cpp` (model foreground), `network_diag_widget.cpp` (diagnostic result colors), `exclusion_manager_dialog.cpp` (constructor param)

### Task 4.3: Update documentation
- [x] Updated `docs/ARCHITECTURE_REVIEW.md` — noted FR-88 migration, `[status="..."]` property selectors, and remaining FR-89 inline styles

### Task 4.4: Final build + test
- [x] Clean compile — 0 warnings
- [x] All 26 tests pass

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
