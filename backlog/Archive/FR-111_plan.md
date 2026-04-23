# FR-111 Plan — macOS Login Items & Background Tasks manager

## Overview

Extend Startup Apps to enumerate `/Library/LaunchAgents` and `/Library/LaunchDaemons`
in addition to the existing `~/Library/LaunchAgents`. System items are read-only.
Each row gains a file path subtitle. User agent enabled state uses `launchctl print-disabled`.

---

## Tasks

### Phase 1 — Data model

- [ ] **1a. Extend `StartupAppData`** (`shared/nexis-core/Info/startup_info.h`)
  - Add `enum class LoginItemCategory { UserAgent, SystemAgent, SystemDaemon }`
  - Add `LoginItemCategory category = LoginItemCategory::UserAgent;`
  - Add `bool readOnly = false;`
  - Add `QString identifier;` (the reverse-DNS label, e.g. `com.google.keystone.agent`)

- [ ] **1b. Add `getAllLoginItems()` to `StartupInfo` interface** (`shared/nexis-core/Info/startup_info.h`)
  - `virtual QList<StartupAppData> getAllLoginItems() const` — default calls `getStartupApps()`

- [ ] **1c. Implement in `StartupInfoMacOS`** (`macos/nexis-core/Info/startup_info.cpp`, `startup_info_macos.h`)
  - Move existing user-agent logic to private `loadPlistDir(path, category, readOnly)`
  - Call `launchctl print-disabled user/<uid>` via QProcess; parse output for user-agent enabled state
  - Override `getAllLoginItems()` to call `loadPlistDir` for all 3 paths and combine results
  - Existing `getStartupApps()` remains unchanged (user agents only) for backward compat

- [ ] **1d. Update `StartupService`** (`shared/nexis/Services/startup_service.h/.cpp`)
  - Add `getAllLoginItems() const` method
  - On macOS: watch `/Library/LaunchAgents` and `/Library/LaunchDaemons` in addition to user dir
  - Plumb through to `mInfo->getAllLoginItems()`

- [ ] **Build check** after phase 1

### Phase 2 — Row widget

- [ ] **2a. Add path subtitle label to `startup_app.ui`**
  - Change inner layout from flat HBox to QVBoxLayout with two rows:
    - Row 1: icon | name | spacer | edit | delete | checkbox (existing)
    - Row 2: `lblStartupAppPath` — small, secondary color, full width
  - `lblStartupAppPath` hidden by default (`visible=false`)
  - Increase `minimumSize/height` from 45 to 60 to accommodate subtitle

- [ ] **2b. Update `StartupApp` constructor** (`startup_app.h/.cpp`)
  - Add `bool readOnly = false` parameter (after `iconPath`)
  - Add path label population: `ui->lblStartupAppPath->setText(filePath); ui->lblStartupAppPath->setVisible(true);`
  - In read-only mode: `ui->checkStartup->setEnabled(false)` + hide edit + delete buttons
  - Show a `(System)` tooltip or accessible name on the row

- [ ] **Build check** after phase 2

### Phase 3 — Page grouping

- [ ] **3a. Update `StartupAppsPage::loadApps()`** (`startup_apps_page.cpp`)
  - Call `mStartupService->getAllLoginItems()` instead of `getApps()`
  - Group results into 3 buckets by `category`
  - For each non-empty bucket: insert a section-header QListWidgetItem before the group
    - Use a simple `QLabel` widget with `objectName("startupSectionHeader")`
    - Item is not selectable and has no `sizeHint` padding
  - Pass `readOnly` flag when constructing `StartupApp`
  - Disconnect `editStartupAppS` / `deleteAppS` for read-only items

- [ ] **3b. Update `filterStartupApps()`** (`startup_apps_page.cpp`)
  - Section header items should remain visible if any sibling in their group is visible
  - Keep simple: hide header item when all items in its group are hidden by search

- [ ] **3c. Update item count label**
  - Show total count, or count per tab — keep existing `setAppCount()` using total

- [ ] **3d. QSS** (`style.qss`)
  ```qss
  #startupSectionHeader {
      color: @tertiaryText;
      font-size: 9pt;
      font-weight: 600;
      padding: @dp4 @dp8;
      background-color: transparent;
  }
  #lblStartupAppPath {
      color: @tertiaryText;
      font-size: 9pt;
  }
  ```

- [ ] **Build check** after phase 3

### Phase 4 — Verification and cleanup

- [ ] **4a. Full build** — `cmake --build build -j$(sysctl -n hw.ncpu)`
- [ ] **4b. Run full test suite** — `ctest --test-dir build --output-on-failure`
- [ ] **4c. Regenerate screenshot baselines** (startup_apps pages will change)
- [ ] **4d. Update docs** (`APPLICATION_OVERVIEW.md`, `ARCHITECTURE_REVIEW.md`)
- [ ] **4e. Update tracking** — mark FR-111 `[x]`, add resolution note + commit hash
- [ ] **4f. Commit** — `feat(macos): enumerate system LaunchAgents and LaunchDaemons in Startup Apps (FR-111)`
- [ ] **4g. Archive** research and plan files

---

## Acceptance Criteria

- On macOS, Startup Apps shows three groups: "User Agents", "System Agents", "System Daemons"
- System groups are visible without entering credentials
- Items in System Agents / System Daemons have toggle, edit, and delete controls hidden
- Each row shows the plist file path as a subtitle
- Enabled/disabled state for user agents matches `launchctl print-disabled user/<uid>` output
- User agents can still be toggled, edited, and deleted as before
- Linux build is unaffected — no compile errors on Linux code path
- All 28 tests pass after baseline regeneration

---

## Rollback

All changes are in the macOS-guarded code path (`#ifdef Q_OS_MAC`) or in
data-model additions that are backward-compatible. The Linux startup code is
unchanged. Reverting the 5 source files is sufficient.

## Not In Scope

- SMAppService / Login Items (Ventura+ API) — future FR
- Signing identity / team ID display — future FR
- Modifying system-level items (requires root auth) — future FR
