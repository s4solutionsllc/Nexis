# BUG-31 Implementation Plan: GNOME Settings error handling

## Overview

Fix the silent failure chain when `gsettings set` errors: make `GnomeSettingsTool::set*()` return success/failure, revert widgets on failure, and show a transient inline error message.

---

## Task 1: Make `GnomeSettingsTool::set*()` return bool

### 1.1 Update shared header
- [x] In `shared/nexis-core/Tools/gnome_settings_tool.h`, change all four set methods from `void` to `bool`:
  - `static bool setS(...)`, `static bool setB(...)`, `static bool setI(...)`, `static bool setD(...)`

### 1.2 Update Linux implementation
- [x] In `linux/nexis-core/Tools/gnome_settings_tool.cpp`:
  - `setS()`: After `CommandUtil::exec()`, read `process->exitCode()` to detect failure. Since `CommandUtil::exec()` doesn't expose exit codes, use a new approach: call `gsettings set` and then immediately call `gsettings get` to verify the value was applied. If the read-back value differs from what was written, return `false`. Otherwise return `true`. Catch block returns `false`.

  **Alternative (preferred):** Add a new static method `CommandUtil::execWithStatus()` that returns a struct `{QString output; int exitCode; QString error;}` instead of throwing. This avoids modifying the existing `exec()` signature and breaking other callers. Then `setS()` checks `exitCode != 0` and returns `false`.

### 1.3 Update macOS implementation
- [x] In `macos/nexis-core/Tools/gnome_settings_tool.cpp`: same changes as Linux, adapted for `defaults write`.

**Build verification:** Incremental build after this task.

---

## Task 2: Add `CommandUtil::execWithStatus()`

- [x] In `shared/nexis-core/Utils/command_util.h`:
  - Add a `struct ExecResult { QString output; QString error; int exitCode; };`
  - Add `static ExecResult execWithStatus(const QString &cmd, QStringList args = QStringList(), int timeoutMs = 30000);`

- [x] In `shared/nexis-core/Utils/command_util_shared.cpp`:
  - Implement `execWithStatus()` — same as `exec()` but reads both stdout and stderr, captures `process->exitCode()` and `process->exitStatus()`, and returns the struct instead of throwing.

**Acceptance criteria:** Existing `exec()` is untouched (zero risk to other callers). New method provides full subprocess result.

**Build verification:** Incremental build.

---

## Task 3: Add inline error status label to GnomeSettingsPage

### 3.1 Update the UI
- [x] In `shared/nexis/Pages/GnomeSettings/gnome_settings_page.ui`:
  - Add a `QLabel` named `lblStatus` between the tab bar row and the stacked widget. Hidden by default, styled for error display.

### 3.2 Add status display logic
- [x] In `shared/nexis/Pages/GnomeSettings/gnome_settings_page.h` and `.cpp`:
  - Add a public slot: `void showError(const QString &message)` that shows `lblStatus` with the error text and starts a `QTimer::singleShot(4000, ...)` to hide it after 4 seconds.
  - Style the label via QSS (red/warning text on a subtle background).

### 3.3 Add QSS styling for the status label
- [x] In `shared/nexis/static/themes/default/style/style.qss`:
  - Add selector for `#GnomeSettingsPage #lblStatus` — warning-colored text, small padding, appropriate font size.

**Acceptance criteria:** `showError("Failed to apply setting")` shows a transient inline message that auto-clears.

**Build verification:** Incremental build.

---

## Task 4: Update all tab lambdas to check return value and revert on failure

### 4.1 GnomeAppearanceTab (17 connections)
- [x] Update each lambda to:
  1. Capture the previous value before the `set*()` call.
  2. Call `set*()` and check the `bool` return.
  3. On failure: use `QSignalBlocker` to revert the widget, and call `showError()` on the parent page.

### 4.2 GnomeWmTab (14 connections)
- [x] Same pattern as 4.1.

### 4.3 GnomeMouseTab (10 connections)
- [x] Same pattern as 4.1. For slider connections, also revert the speed display label.

### 4.4 GnomeDesktopTab (8 connections)
- [x] Same pattern as 4.1. For browse button connections, also revert the line edit text.

### Implementation approach for tabs
Each tab needs access to the parent `GnomeSettingsPage` to call `showError()`. Pass the page pointer to each tab's constructor, or emit a signal from the tab that the page connects to. Signal approach is cleaner (avoids tight coupling):

- [x] Add `signal void settingFailed(const QString &message)` to each tab header.
- [x] In `GnomeSettingsPage` constructor, connect each tab's `settingFailed` signal to `showError()`.
- [x] In each lambda's failure path, emit `settingFailed(tr("Failed to apply %1").arg(settingName))`.

**Acceptance criteria:** Every setting change that fails reverts the widget to its previous value and shows an inline error message. The `mLoading` guard is never triggered during revert (using `QSignalBlocker` instead).

**Build verification:** Clean rebuild after all tabs are updated.

---

## Task 5: Update tracking files, commit, and push

- [x] Mark BUG-31 as `[x]` in `BUGS.md` with resolution note.
- [x] Commit and push all changes.
