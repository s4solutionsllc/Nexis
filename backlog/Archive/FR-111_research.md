# FR-111 Research — macOS Login Items & Background Tasks manager

## Goal

Extend the existing Startup Apps page on macOS to show all three LaunchAgent/LaunchDaemon
sources (not just `~/Library/LaunchAgents`) with read-only views for system-level items,
and display the plist file path per entry.

SMAppService (Ventura+ background tasks) is deferred — complexity/API instability makes
it better suited for a follow-up FR.

---

## Current Architecture

### Data layer — `StartupInfo` / `StartupInfoMacOS`

**Interface** (`shared/nexis-core/Info/startup_info.h`):
```cpp
struct StartupAppData {
    QString name;
    QString filePath;
    QString iconPath;
    bool enabled = true;
};
class StartupInfo {
    virtual QList<StartupAppData> getStartupApps() const = 0;
    virtual QString autostartPath() const = 0;
    virtual bool isAutostartDisabled() const = 0;
};
```

**macOS implementation** (`macos/nexis-core/Info/startup_info.cpp`):
- `autostartPath()` → `~/Library/LaunchAgents`
- `getStartupApps()` reads that single directory
- Skips `com.apple.*` identifiers
- Reads enabled state from plist `<key>Disabled</key>` (XML parse only — binary plists not handled)
- Derives display name from reverse-DNS identifier
- Extracts .app path from `ProgramArguments` for icon

### Service layer — `StartupService`

**`shared/nexis/Services/startup_service.cpp`**:
- Singleton wrapping `StartupInfo`
- `QFileSystemWatcher` on `autostartPath()` → emits `appsChanged()`
- Only watches the one user agents dir

### UI layer — `StartupAppsPage`

**`shared/nexis/Pages/StartupApps/startup_apps_page.cpp`**:
- Calls `mStartupService->getApps()` → populates `listWidgetStartup` with `StartupApp` widgets
- Each `StartupApp`: app icon, name, edit button, delete button, enable/disable checkbox
- Toggle: XML-edits the plist `Disabled` key via `FileUtil::writeFile`
- Delete: `QFile::remove(filePath)`
- No grouping / categorization

### Row widget — `StartupApp`

**`shared/nexis/Pages/StartupApps/startup_app.ui`**:
- Single-line QHBoxLayout: icon | name | spacer | edit | delete | checkbox
- No subtitle/path label
- Height: 45px

**`shared/nexis/Pages/StartupApps/startup_app.cpp`**:
- Constructor params: `name, enabled, filePath, iconPath`
- On macOS: reads .app bundle icon via `QFileIconProvider`

---

## System Data Sources

### 1. `~/Library/LaunchAgents` — User Agents (existing)
- Third-party per-user login items
- User has full read/write access
- Current code handles this

### 2. `/Library/LaunchAgents` — System Agents
- Per-user agents installed system-wide (run for every user)
- Examples: Google Keystone, Zoom updater, Microsoft AutoUpdate
- Readable without root; modifying requires root
- Plists can be XML or binary (`bplist00`)

### 3. `/Library/LaunchDaemons` — System Daemons
- System-wide daemons (run as root, not per-user)
- Examples: `com.google.keystone.daemon`, `com.privateinternetaccess.vpn.daemon`
- Readable without root; modifying requires root

### Enabled State via `launchctl`
```
launchctl print-disabled user/$(id -u)
```
Returns a list like:
```
"com.microsoft.update.agent" => enabled
"com.apple.Siri.agent" => disabled
```
This is the authoritative state regardless of the plist `Disabled` key, and handles binary
plists. Available for user domain only (no root needed).

For system agents/daemons, `launchctl print-disabled system/` requires root — so we read
the plist `Disabled` key as a best-effort fallback.

---

## Design Decisions

### Scope for this implementation
- ✅ Enumerate `/Library/LaunchAgents` (system agents) — read-only display
- ✅ Enumerate `/Library/LaunchDaemons` (system daemons) — read-only display
- ✅ Show plist file path as subtitle on every row
- ✅ Use `launchctl print-disabled user/$(id -u)` for accurate user agent enabled state
- ❌ SMAppService — deferred (macOS 13+ only, private API changes across versions)
- ❌ Signing identity / team ID — deferred (requires `codesign` subprocess; complexity without clear user value)
- ❌ Modify system-level items — read-only (requires root; AuthorizationExecuteWithPrivileges is deprecated; out of scope)

### UI approach
Add a `LoginItemCategory` enum and `readOnly` flag to `StartupAppData`. The
existing single `QListWidget` gets **section header separator rows** between categories.
No new page needed — this extends Startup Apps in-place.

Row widget gets an optional subtitle (`lblStartupAppPath`) that shows the file path.
In read-only mode, toggle/edit/delete controls are hidden and a lock icon or "(system)"
label indicates the item is not modifiable.

---

## Files to Change

| File | Change |
|------|--------|
| `shared/nexis-core/Info/startup_info.h` | Add `LoginItemCategory` enum + `readOnly`/`category` fields to `StartupAppData` |
| `macos/nexis-core/Info/startup_info.cpp` | Enumerate system agents + daemons; use `launchctl` for user enabled state |
| `macos/nexis-core/Info/startup_info_macos.h` | Add helper method declarations |
| `shared/nexis/Services/startup_service.cpp` | Watch all 3 dirs (macOS); plumb `getAllLoginItems()` |
| `shared/nexis/Services/startup_service.h` | Add `getAllLoginItems()` |
| `shared/nexis/Pages/StartupApps/startup_app.ui` | Add `lblStartupAppPath` subtitle label |
| `shared/nexis/Pages/StartupApps/startup_app.h` | Add `readOnly` ctor param and path setter |
| `shared/nexis/Pages/StartupApps/startup_app.cpp` | Read-only mode hides actions; shows path subtitle |
| `shared/nexis/Pages/StartupApps/startup_apps_page.cpp` | Group items by category; insert section headers |
| `shared/nexis/static/themes/default/style/style.qss` | `#startupSectionHeader` style |

---

## Linux Compatibility

All new fields (`category`, `readOnly`) default to `UserAgent` / `false` so the Linux
code path (`StartupInfoLinux`) requires no changes. `getAllLoginItems()` on Linux
delegates to the existing `getStartupApps()`.

---

## Key Risks

- **Binary plists**: `/Library/LaunchAgents` items can be binary (`bplist00`). Text
  parsing will silently fail to extract name/icon. Mitigation: fall back to filename-based
  name derivation and no icon — same as currently happens for malformed XML plists.
- **`launchctl print-disabled` parsing**: Output is plain text, not machine-parseable
  plist. Parse with simple line-based regex. If launchctl is unavailable (non-standard
  macOS) fall back to plist `Disabled` key.
- **Read permissions on `/Library/LaunchAgents`**: Readable for all users on standard
  macOS installs. If `opendir` fails (sandbox or unusual permissions), show empty section
  with a note.
