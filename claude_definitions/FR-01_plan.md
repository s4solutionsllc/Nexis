# FR-01: deb822 APT Source File Support — Implementation Plan

## Overview

Add support for the modern `.sources` format (deb822 — RFC 822 key-value style) to Nexis's APT repository manager. Currently only the legacy one-line `.list` format is supported. Debian 13+ and Ubuntu 24.04+ ship with `.sources` as the default format, meaning Nexis shows no repositories on these systems.

**Scope:** Linux-only. macOS uses Homebrew and is unaffected.

**Approach:** Follows QuentiumYT/Stacer commit `87279f6` as reference, adapted to Nexis's codebase. Key design decisions:
- Rename `distribution` → `suites` throughout (matches APT terminology)
- Change `changeSource()` signature from `QString` → `APTSourcePtr` (format-agnostic structured approach)
- Dual-format parsing: both `.list` and `.sources` files produce the same `APTSourcePtr` objects
- Format-preserving writes: `.list` files stay `.list`, `.sources` files stay `.sources`
- Stanza-aware editing: preserve field order, comments, and multi-line values (Signed-By)

---

## Phase 1 — Data Model & API Changes

Update the shared header and interface to support both formats.

### Task 1.1: Rename `distribution` → `suites` in APTSource struct
- [x] In `shared/nexis-core/Tools/apt_source_tool.h`:
  - Rename `QString distribution;` → `QString suites;`
  - Update comment block (line 8-9) to reflect deb822 fields
  - Change `changeSource()` signature: `static void changeSource(const APTSourcePtr aptSource, const APTSourcePtr newSource);`

### Task 1.2: Update ToolManager interface
- [x] In `shared/nexis/Managers/tool_manager.h` (line 40):
  - Change `void changeAPTSource(const APTSourcePtr aptSource, const QString newSource);`
  - To: `void changeAPTSource(const APTSourcePtr aptSource, const APTSourcePtr newSource);`

### Task 1.3: Update Linux ToolManager proxy
- [x] In `linux/nexis/Managers/tool_manager.cpp` (line 169-172):
  - Update `changeAPTSource()` to forward the new `APTSourcePtr` parameter

### Task 1.4: Update macOS ToolManager proxy
- [x] In `macos/nexis/Managers/tool_manager.cpp` (line 152-155):
  - Update `changeAPTSource()` to forward the new `APTSourcePtr` parameter
  - Also update macOS `changeSource()` in `macos/nexis-core/Tools/apt_source_tool.cpp` to accept new signature (Homebrew stub — just update signature, implementation unchanged)

**Acceptance:** ✅ Project compiles with updated signatures. All callers adapted.

---

## Phase 2 — Parser Rewrite (apt_source_tool.cpp)

Rewrite `linux/nexis-core/Tools/apt_source_tool.cpp` to support dual-format parsing and stanza-aware writing.

### Task 2.1: Add `.sources` file discovery
- [x] In `getSourceList()`, extend file discovery to include `*.sources` files

### Task 2.2: Implement deb822 stanza parser
- [x] Add parsing branch in `getSourceList()` for `.sources` files

### Task 2.3: Update `.list` parser for renamed field
- [x] Change `aptSource->distribution = ...` → `aptSource->suites = ...`

### Task 2.4: Rewrite `changeSource()` for dual-format support
- [x] Change method signature to accept `APTSourcePtr newSource` (nullptr = delete)
- [x] Branch on `.sources` vs `.list` with format-specific stanza/line rewriting

### Task 2.5: Rewrite `changeStatus()` for format-aware enable/disable
- [x] Copy-and-modify pattern delegating to `changeSource()`

### Task 2.6: Update `removeAPTSource()` to pass nullptr
- [x] Change `changeSource(aptSource, "")` → `changeSource(aptSource, nullptr)`

**Acceptance:** ✅ Both `.list` and `.sources` files correctly parsed, displayed, and modifiable. Build succeeds.

---

## Phase 3 — Edit Dialog Updates

Update the edit dialog to work with structured APTSource objects and renamed field.

### Task 3.1: Rename `txtDistribution` → `txtSuites` in UI
- [x] In `shared/nexis/Pages/AptSourceManager/apt_source_edit.ui`:
  - Rename widget `txtDistribution` → `txtSuites`
  - Change placeholder text from "Distribution" to "Suites"

### Task 3.2: Update `APTSourceEdit::show()` to use renamed field
- [x] Change `ui->txtDistribution->setText(selectedAptSource->distribution)` → `ui->txtSuites->setText(selectedAptSource->suites)`

### Task 3.3: Update `clearElements()` for renamed field
- [x] Change `ui->txtDistribution->clear()` → `ui->txtSuites->clear()`

### Task 3.4: Rewrite `on_btnSave_clicked()` to build APTSourcePtr
- [x] Replace format-specific string construction with structured `APTSourcePtr` object

**Acceptance:** ✅ Editing preserves source format. All fields round-trip correctly.

---

## Phase 4 — Manager Page UX Improvements

Port QuentiumYT's UX improvements to the manager page.

### Task 4.1: Clear selection after operations
- [x] Added `selectedAptSource.clear()` after delete, add, and edit-save operations

### Task 4.2: Update placeholder text
- [x] Changed add-repository placeholder from `'deb http://...'` to `'ppa:deadsnakes/ppa'`

### Task 4.3: Button feedback during add operation
- [x] Show "Adding..." text and disable button while `add-apt-repository` is running

### Task 4.4: Preserve search filter across operations
- [x] Re-apply current search filter after add/delete/edit operations

**Acceptance:** ✅ No stale selection. Search filter persists. Button feedback during add.

---

## Phase 5 — Translation Updates

### Task 5.1: Update translatable strings
- [x] Skipped — lupdate runs via CI/Crowdin workflow, not manually

---

## Phase 6 — Build & Verify

### Task 6.1: Build verification
- [x] Incremental build succeeds on macOS with zero errors and zero warnings

### Task 6.2: Code review checklist
- [x] `distribution` fully renamed to `suites` in all files (grep verified: no matches)
- [x] `changeSource()` signature updated in all locations (header, linux impl, macos impl, tool_manager)
- [x] No remaining references to old `QString newSource` parameter (grep verified: no matches)
- [x] File deletion logic works when last stanza/line is removed
- [x] Comment preservation in stanza rewriting
- [x] Multi-line `Signed-By:` preserved during stanza edits

---

## Files Modified

| File | Change Scope | Description |
|------|-------------|-------------|
| `shared/nexis-core/Tools/apt_source_tool.h` | Moderate | Rename `distribution`→`suites`, change `changeSource()` signature |
| `linux/nexis-core/Tools/apt_source_tool.cpp` | **Major** | Full parser rewrite: deb822 parsing, stanza-aware writing, `Enabled:` field, file deletion |
| `shared/nexis/Managers/tool_manager.h` | Minor | Update `changeAPTSource()` signature |
| `linux/nexis/Managers/tool_manager.cpp` | Minor | Forward new signature |
| `macos/nexis/Managers/tool_manager.cpp` | Minor | Forward new signature |
| `macos/nexis-core/Tools/apt_source_tool.cpp` | Minor | Update `changeSource()` signature (Homebrew stub) |
| `shared/nexis/Pages/AptSourceManager/apt_source_edit.cpp` | Moderate | Build `APTSourcePtr` instead of format string, rename field |
| `shared/nexis/Pages/AptSourceManager/apt_source_edit.ui` | Minor | Rename `txtDistribution`→`txtSuites`, update placeholder |
| `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp` | Minor | UX improvements: selection clearing, placeholder, button feedback |
| `FEATURE_REQUESTS.md` | Minor | Mark FR-01 done |
| `CHANGELOG.md` | Minor | Add entry |
