Perfect! Now I have comprehensive understanding. Let me create the research document. Since this is a READ-ONLY task, I cannot create the file myself, but I will provide a comprehensive research report to the user.

## Comprehensive Research Report: FR-10 Startup App Customization Enhancements

I have completed a thorough investigation of the startup app management system in Nexis. Here's what I found:

---

### **1. CURRENT ARCHITECTURE OVERVIEW**

**Three-Layer System:**
- **Page Layer:** `StartupAppsPage` (shared) — Main UI showing list of apps
- **Item Layer:** `StartupApp` (shared) — Individual app widget with enable/disable toggle
- **Editor Layer:** `StartupAppEdit` (platform-specific) — Modal dialog to create/edit apps

**File I/O:** `FileUtil` class handles all file reading/writing via QFile

---

### **2. STARTUP APP MANAGEMENT FILES**

**Shared (Platform-Agnostic):**
- `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/StartupApps/startup_apps_page.h` — Page header (38 lines)
- `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/StartupApps/startup_apps_page.cpp` — Page logic (211 lines) — Loads .plist (macOS) or .desktop (Linux) files from their respective directories
- `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/StartupApps/startup_app.h` — Item header (52 lines)
- `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/StartupApps/startup_app.cpp` — Item logic (127 lines) — Handles enable/disable toggling, deletion, edit launch
- `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/StartupApps/startup_app_edit.h` — Editor header with regex patterns (59 lines)
- `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/StartupApps/startup_app_edit.ui` — Qt Designer UI (149 lines, 380×270 dialog)

**Platform-Specific Editor:**
- `/Users/luke/Documents/GitHub/Nexis/linux/nexis/Pages/StartupApps/startup_app_edit.cpp` — Linux .desktop file editor (146 lines)
- `/Users/luke/Documents/GitHub/Nexis/macos/nexis/Pages/StartupApps/startup_app_edit.cpp` — macOS plist editor (190 lines)

**UI Files:**
- `startup_apps_page.ui` — Main page layout (253 lines)
- `startup_app.ui` — Item card layout (190 lines)
- `startup_app_edit.ui` — Editor dialog (149 lines)

---

### **3. CURRENT CUSTOMIZATION OPTIONS**

**Implemented (across both platforms):**

1. **Application Name** (`Name=` on Linux, `Label` in plist on macOS)
2. **Description/Comment** (`Comment=` on Linux, derived from Label on macOS)
3. **Executable Command** (`Exec=` on Linux, `ProgramArguments` array on macOS)
4. **Startup Delay** (`X-GNOME-Autostart-Delay=` on Linux, shell-wrapped `/bin/bash -c "sleep N && cmd"` on macOS) — *Already implemented in FR-15*
5. **Enable/Disable Toggle** (via `Hidden=` on Linux, `Disabled=` key in plist on macOS)

**In Editor Dialog:**
- `txtStartupAppName` — QLineEdit for app name (required)
- `txtStartupAppComment` — QLineEdit for comment/description (required)
- `txtStartupAppCommand` — QLineEdit for full command path (required)
- `spnStartupDelay` — QSpinBox: 0-3600 seconds (FR-15 feature)
- Validation: All three text fields must be non-empty to save

---

### **4. DESKTOP ENTRY SPECIFICATION SUPPORT**

**Currently Implemented in Code (Regex Patterns):**
```cpp
#define NAME_REG QRegularExpression("^Name=.*")
#define COMMENT_REG QRegularExpression("^Comment=.*")
#define EXEC_REG QRegularExpression("^Exec=.*")
#define GNOME_ENABLED_REG QRegularExpression("^X-GNOME-Autostart-enabled=.*")
#define HIDDEN_REG QRegularExpression("^Hidden=.*")
#define DELAY_REG QRegularExpression("^X-GNOME-Autostart-Delay=.*")
```

**[Desktop Entry] Standard Fields NOT Yet Implemented:**

*Core Fields (Standard Spec):*
- `Type=` (Application, Link, Directory) — *ignored, hardcoded to "Application"*
- `Icon=` — Icon name/path
- `Terminal=` (true/false) — Run in terminal
- `MimeType=` — MIME types the app can handle
- `Categories=` — Application categories (Utility, Development, etc.)
- `Keywords=` — Search keywords
- `StartupNotify=` — EWMH startup notification protocol

*GNOME-Specific Extensions:*
- `X-GNOME-Autostart-Phase=` (Phase, Desktop, Applications) — Launch phase
- `X-GNOME-Autostart-Condition=` — Conditional autostart (e.g., `@DisplayServer ne wayland`)

*KDE-Specific Extensions:*
- `X-KDE-StartupNotify=` — KDE startup notification

*Platform Conditionals:*
- `OnlyShowIn=` — Only run on specific DEs (GNOME, KDE, XFCE, etc.)
- `NotShowIn=` — Never run on specific DEs
- `TryExec=` — Only if executable exists at this path
- `DBusActivatable=` — Activatable via D-Bus

*Execution Modifiers:*
- `Path=` — Working directory for execution
- `Environment=` — Environment variables to set (not in standard spec, but used by GNOME)

*Advanced Features:*
- `Actions=` — Custom menu actions (submenu items in launcher)
- `SingleMainWindow=` — Reuse existing window instead of launching new instance

---

### **5. MACOS LAUNCHD PLIST SUPPORT**

**Current Template (macOS):**
```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>%1</string>          <!-- App name (reverse-DNS recommended) -->
    <key>ProgramArguments</key>
    <array>
        <string>%3</string>      <!-- Command or /bin/bash for delayed launch -->
    </array>
    <key>RunAtLoad</key>
    <true/>                       <!-- Always true for LaunchAgents -->
</dict>
</plist>
```

**Features:**
- Delay implemented via: `/bin/bash -c "sleep N && <cmd>"`
- App detection: Reverse-DNS identifier (e.g., `com.jetbrains.toolbox`) parsed and beautified
- Disabled state: `<key>Disabled</key><true/>` added/removed
- Comment field: Not standard in launchd; currently set to same as Label

**Plist Keys NOT Yet Supported:**
- `EnvironmentVariables` (dict) — Set env vars for the process
- `StandardOutPath` / `StandardErrorPath` — Redirect output/error
- `StandardInPath` — Input file
- `GroupName` / `UserName` — Run as specific user
- `ProcessType` (Adaptive, Background, Interactive) — Process priority
- `StartInterval` / `StartCalendarInterval` — Periodic execution instead of once at load
- `Throttle` — Launchd throttling on repeated failures
- `AbandonProcessGroup` — Don't terminate child processes
- `WorkingDirectory` — Working directory for execution
- `RootDirectory` — Chroot environment
- `Umask` — File creation mask
- `WatchPaths` / `QueueDirectories` — Inotify-like file watching

---

### **6. PLATFORM DIFFERENCES**

| Feature | Linux (.desktop) | macOS (plist) |
|---------|------------------|---------------|
| **Storage** | `~/.config/autostart/*.desktop` | `~/Library/LaunchAgents/*.plist` |
| **Enable/Disable** | `Hidden=false/true` OR `X-GNOME-Autostart-enabled=false/true` | `<key>Disabled</key><true/>` |
| **Delay** | `X-GNOME-Autostart-Delay=<seconds>` (GNOME-specific) | `/bin/bash -c "sleep N && cmd"` wrapper |
| **Description** | `Comment=...` (standard) | Not standard in launchd; mimicked with Label |
| **File Format** | Plain text key=value with `[Desktop Entry]` header | XML plist format |
| **Env Variables** | Not parsed; passed in Exec= shell expansion | `EnvironmentVariables` dict (not implemented) |
| **Desktop Detection** | `OnlyShowIn=` / `NotShowIn=` (GNOME, KDE, XFCE) | None (always runs) |
| **Terminal Launch** | `Terminal=true` opens in terminal | Would need shell wrapper |
| **Conditional Execution** | `X-GNOME-Autostart-Condition=` (GNOME-specific) | None (always loads at login) |

---

### **7. DATA FLOW & SIGNAL CONNECTIONS**

**Loading Sequence:**
1. `StartupAppsPage::init()` → Sets up file system watcher on autostart directory
2. `StartupAppsPage::loadApps()` → Scans directory for `.plist` (macOS) or `.desktop` (Linux) files
3. For each file:
   - Create `StartupApp` widget with name, enabled state, file path
   - Connect: `StartupApp::deleteAppS` → reload list
   - Connect: `StartupApp::editStartupAppS(filePath)` → `openStartupAppEdit(filePath)`
4. File watcher detects changes → auto-reload via `directoryChanged` signal

**Editing Sequence:**
1. User clicks edit or "Add" button → `StartupAppsPage::openStartupAppEdit(filePath)`
2. Sets static `StartupAppEdit::selectedFilePath`
3. Opens dialog via `StartupAppEdit::show()`
   - If editing: Parse existing file into UI fields
   - If creating: Clear all fields
4. User fills Name, Comment, Command, optionally Delay
5. Click Save → `on_btnSave_clicked()` validates and writes file
   - For existing: Parse + modify + write back
   - For new: Build from template + write

**Enable/Disable:**
1. User clicks checkbox on `StartupApp` → `on_checkStartup_clicked()`
2. Read file, find `Hidden=` (Linux) or `Disabled=` (macOS)
3. Modify the single line
4. Write file back immediately (no dialog)

---

### **8. QUENTIUMYT V1.5.0 ENHANCEMENTS**

From `FEATURE_REQUESTS.md` line 17:
```
- [ ] **FR-10: Startup app customization enhancements** — Expanded options for managing auto-start applications. QuentiumYT v1.5.0.
```

The reference indicates QuentiumYT fork v1.5.0 has feature ideas, but the codebase doesn't currently mention what those are. The git history doesn't show upstream QuentiumYT commits directly. Based on the conversation context mentioning "expanded options," likely candidates from the general landscape:
- Environment variable support
- Conditional execution (desktop/session detection)
- Run-as-user / privilege elevation
- Working directory specification
- Terminal/GUI mode toggle
- Multiple argument support (not just single command string)

---

### **9. PROPOSED CUSTOMIZATION OPTIONS FOR FR-10**

**Tier 1 — Low-Effort Additions (Quick Wins):**
1. **Terminal Mode** (`Terminal=true/false`)
   - Linux: Already in .desktop spec; add checkbox to UI
   - macOS: Wrap command in `open -a Terminal` or use `osascript`

2. **Icon Selection** (`Icon=`)
   - Add icon picker button; let user browse system icons or app icons
   - Store icon name/path in file

3. **Working Directory** (`Path=` on Linux, `WorkingDirectory` idea)
   - Add directory picker line edit
   - Set as CWD when process launches

4. **OnlyShowIn / NotShowIn** (Linux-only, but valuable)
   - Desktop detection dropdown: All, GNOME, KDE, XFCE, etc.
   - Only show/run on selected desktops

**Tier 2 — Moderate-Effort Features:**
5. **Environment Variables** (Advanced mode)
   - Key=Value table widget with Add/Remove buttons
   - Linux: Append `Environment=KEY=VALUE` lines
   - macOS: Add `<key>EnvironmentVariables</key><dict>...</dict>` to plist

6. **Conditional Execution** (GNOME-specific)
   - Checkbox: "Only run if..." with conditions like `@DisplayServer ne wayland`
   - Maps to `X-GNOME-Autostart-Condition=`

7. **Arguments/Parameters** (Better multi-arg support)
   - Currently single `Exec=` or `ProgramArguments` string
   - Could parse into command + separate arguments fields
   - Or provide escaping help for quoted arguments

8. **Run-as-User** (macOS plist `UserName` key)
   - Dropdown to run as different user (for system-level tools)
   - macOS: Requires elevated privileges; would need sudo or launchctl

**Tier 3 — Complex Features (Major Refactor):**
9. **Periodic/Scheduled Execution** (macOS `StartInterval`, `StartCalendarInterval`)
   - Time picker for hourly/daily/weekly runs
   - Different from "autostart on login" — needs different UI mode

10. **File Watching / Triggered Execution** (macOS `WatchPaths`, `QueueDirectories`)
    - Monitor directory for changes; run on modification
    - File path picker(s)

11. **Standard Input/Output Redirection** (macOS `StandardInPath`, `StandardOutPath`)
    - File pickers for stdin/stdout/stderr
    - For scripting/logging workflows

12. **Process Priority & Resource Control** (macOS `ProcessType`, `Umask`, `Throttle`)
    - Advanced dropdown for Background/Interactive process types
    - File creation mask (octal) spinner

---

### **10. ARCHITECTURAL CONSIDERATIONS**

**Current Patterns:**
- Platform-specific `startup_app_edit.cpp` in `linux/` and `macos/` folders
- Shared header allows different implementations via preprocessor `#ifdef Q_OS_MACOS`
- UI is shared; platform-specific code handles serialization only

**For New Features:**
- Simple additions (Terminal, Icon) can be cross-platform in shared UI
- Linux-specific fields (OnlyShowIn) use conditional blocks in editor
- Environment variables: Abstraction layer to handle both `.desktop` and plist format differences
- Conditional execution: GNOME-specific; could be conditionally visible

**Constraints:**
- File format is text-based (easy to parse), but whitespace-sensitive on macOS (XML plist)
- Validation: Name must be unique per file in directory
- File permissions: LaunchAgents on macOS typically 644; .desktop files 644

---

### **11. CODE QUALITY OBSERVATIONS**

**Strengths:**
- Clean separation of platform-specific logic
- Signal/slot connections well-organized (reload on change, file watcher)
- Regex patterns defined as constants for reusability
- FileUtil provides consistent file I/O

**Limitations:**
- Delay feature (FR-15) uses shell wrapper on macOS; fragile with special characters in command
- No escaping of command arguments; if user includes quotes/ampersands, plist/desktop file may be corrupted
- Plist parsing uses line-by-line regex; doesn't use proper XML parser (fragile to formatting changes)
- macOS plist "Comment" field not editable (just derived from Label); inconsistent with Linux
- No validation that executable actually exists (`TryExec` concept)
- No error handling if file write fails (fails silently)

**Testing Needs:**
- Commands with spaces, quotes, semicolons, pipes
- File paths with spaces in delay wrapper
- Special characters in app names (unicode, punctuation)
- macOS: Verify plist XML is valid (indent/format consistency)
- Concurrent edits (file system watcher vs in-dialog edits)

---

### **12. FILES REQUIRING MODIFICATION FOR EACH FEATURE**

**For any UI addition to editor:**
1. `/shared/nexis/Pages/StartupApps/startup_app_edit.ui` — Add widgets (Qt Designer)
2. `/shared/nexis/Pages/StartupApps/startup_app_edit.h` — Add member variables, regex patterns if needed
3. `/linux/nexis/Pages/StartupApps/startup_app_edit.cpp` — Parse/write Linux .desktop format
4. `/macos/nexis/Pages/StartupApps/startup_app_edit.cpp` — Parse/write macOS plist format

**For list view enhancements:**
1. `/shared/nexis/Pages/StartupApps/startup_app.ui` — Add new label/icon/display element
2. `/shared/nexis/Pages/StartupApps/startup_app.cpp` — Populate new display field
3. `/shared/nexis/Pages/StartupApps/startup_app.h` — Add member variable if needed

**For page-level changes:**
1. `/shared/nexis/Pages/StartupApps/startup_apps_page.cpp` — Load additional fields when parsing files

---

### **13. RELATED FEATURE/BUG TRACKING**

- **FR-15** (Resolved): Startup delay — Already implemented with spinbox in editor
- **BUG-07** (Resolved): HiDPI scaling — May affect UI layout of expanded editor dialog
- **FR-23, FR-24**: Disk usage analyzer — Unrelated
- **No existing Startup Apps bugs** in `BUGS.md` (good sign of stability)

---

### **20. SUMMARY TABLE: Enhancement Opportunities**

| Enhancement | Difficulty | Cross-Platform? | Linux Spec | macOS Spec | UI Impact |
|-------------|------------|-----------------|-----------|-----------|-----------|
| Terminal Mode | Low | Yes | `Terminal=` | osascript/open | 1 checkbox |
| Icon Picker | Low | Yes | `Icon=` | Plist `Icon` idea | 1 button + picker |
| Working Directory | Low | Yes | `Path=` | Plist idea | 1 line edit + picker |
| OnlyShowIn/NotShowIn | Low | Linux-only | `OnlyShowIn=` | N/A | 1 dropdown |
| Env Variables | Medium | Yes | `Environment=` lines | `EnvironmentVariables` dict | Table widget |
| Conditional Execution | Medium | Linux-only | `X-GNOME-Autostart-Condition=` | N/A | 1 dropdown |
| Better Arguments | Medium | Yes | Exec parsing | ProgramArguments array | Multiple inputs or table |
| Run-as-User | Medium | macOS-only | N/A (via sudo in Exec?) | `UserName` key | 1 dropdown |
| Periodic Execution | High | macOS-only | N/A (cron outside scope) | `StartInterval` / `StartCalendarInterval` | Time picker, separate mode |
| File Watching | High | macOS-only | N/A | `WatchPaths` | Path table widget |
| I/O Redirection | High | macOS-only | N/A | StandardInPath, etc. | File pickers |
| Process Priority | High | macOS-only | N/A | ProcessType, Umask | Dropdowns |

---

This completes the comprehensive research for FR-10. The system is well-structured for incremental enhancement. The next phase would be to prioritize which features to implement and create a detailed plan document.
