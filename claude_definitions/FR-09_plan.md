# FR-09: APT-RPM Support — Implementation Plan

## Overview

Add support for APT-RPM, a package management system used by ALT Linux, PCLinuxOS, and Vine Linux. APT-RPM provides APT's frontend (`apt-get`, `apt-cache`) with RPM as the backend package format. This requires:

1. A new `APT_RPM` enum value and detection logic
2. Replacing hardcoded `"deb"`/`"deb-src"` strings with dynamic type functions in the source tool
3. Routing `APT_RPM` to the correct package listing/removal functions in ToolManager
4. Supporting `apt-repo` (ALT Linux's repository management tool) as an alternative to `add-apt-repository`

**Reference:** QuentiumYT/Stacer PR #31 (merged), +69/-24 lines across 7 files.

---

## Phase 1 — Enum & Detection

### Task 1.1: Add `APT_RPM` enum value
- [x] In `shared/nexis-core/Tools/package_tool_shared.h`: Added `APT_RPM` after `APT`

### Task 1.2: Update detection logic
- [x] In `linux/nexis-core/Tools/package_tool.cpp`: Added APT-RPM detection BEFORE the APT check (`apt-get` + `rpm` present, `dpkg` absent)

---

## Phase 2 — Source Tool Updates

### Task 2.1: Add helper functions
- [x] Added `isAptRpm()`, `binaryType()`, `sourceType()` static helpers in `apt_source_tool.cpp`

### Task 2.2: Update `getSourceList()` — .list format parsing
- [x] Replaced hardcoded `"deb"` in filter regex, `sourceColumns.first()` comparisons

### Task 2.3: Update `getSourceList()` — deb822 format parsing
- [x] Replaced `types.contains("deb")`, `types.contains("deb-src")`, and type string construction

### Task 2.4: Update `changeSource()` — deb822 stanza editing
- [x] Replaced both `isSource ? "deb-src" : "deb"` patterns

### Task 2.5: Update `changeSource()` — .list line reconstruction
- [x] Replaced `isSource ? "deb-src" : "deb"` in line reconstruction

### Task 2.6: Update `addRepository()` for APT-RPM
- [x] Rewritten to support `apt-repo add` with fallback to `add-apt-repository`

### Task 2.7: Update `removeAPTSource()` for APT-RPM
- [x] Rewritten to support `apt-repo rm` with fallback to direct file editing

### Task 2.8: Add `#include` for PackageTool enum
- [x] Added `#include "package_tool.h"` at top of `apt_source_tool.cpp`

---

## Phase 3 — ToolManager Routing

### Task 3.1: Update Linux ToolManager switch statements
- [x] Added `case APT_RPM:` to all 4 switch blocks in `tool_manager.cpp`:
  - `getPackages()` → `getRpmPackages()`
  - `dryRunRemovePackages()` → `dpkgDryRunRemove()`
  - `getPackageCaches()` → `getDpkgPackageCaches()`
  - `uninstallPackages()` → `dpkgRemovePackages()`

### Task 3.2: Update disk usage launcher widget
- [x] Added `case APT_RPM:` fall-through to `case APT:` in `disk_usage_launcher_widget.cpp`

---

## Phase 4 — UI Updates

### Task 4.1: Update placeholder text for APT-RPM
- [x] Added conditional placeholder text showing RPM source example for APT-RPM systems
- [x] Moved `#include <Tools/package_tool.h>` out of `#ifdef Q_OS_MAC` block

### Task 4.2: Hide edit button for APT-RPM (optional)
- [x] Decision: keep the edit button visible — it performs direct file editing which works regardless of `apt-repo`

---

## Phase 5 — Build & Verify

### Task 5.1: Build verification
- [x] Incremental build succeeds on macOS with zero errors, zero warnings

### Task 5.2: Code review checklist
- [x] `APT_RPM` enum value added and detection works
- [x] No remaining hardcoded `"deb"` in `apt_source_tool.cpp` — all replaced with `binaryType()`/`sourceType()`
- [x] All ToolManager switch statements handle `APT_RPM`
- [x] `apt-repo` support with proper fallback
- [x] Disk usage launcher widget handles `APT_RPM`
- [x] No regressions for Debian/Ubuntu APT (dpkg present → still detects as `APT`)

### Task 5.3: Update tracking files
- [x] Mark FR-09 as `[x]` done in FEATURE_REQUESTS.md
- [x] Add CHANGELOG.md entry
- [x] Commit and push

---

## Files Modified

| File | Change Scope | Description |
|------|-------------|-------------|
| `shared/nexis-core/Tools/package_tool_shared.h` | Trivial | Added `APT_RPM` enum value |
| `linux/nexis-core/Tools/package_tool.cpp` | Minor | Added APT-RPM detection before APT check |
| `linux/nexis-core/Tools/apt_source_tool.cpp` | Moderate | Added helper functions, replaced hardcoded `"deb"`/`"deb-src"`, updated `addRepository()`/`removeAPTSource()` for `apt-repo` |
| `linux/nexis/Managers/tool_manager.cpp` | Minor | Added `case APT_RPM:` to 4 switch statements |
| `shared/nexis/Pages/Resources/disk_usage_launcher_widget.cpp` | Trivial | Added `case APT_RPM:` fall-through to APT |
| `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp` | Minor | Conditional placeholder text for APT-RPM |
| `FEATURE_REQUESTS.md` | Minor | Marked FR-09 done |
| `CHANGELOG.md` | Minor | Added entry |
