# FR-10 Startup App Customization Enhancements — Research Report

## 1. Current Architecture

### File Layout (10 source files, 3-layer architecture)

| Layer | Shared | Linux | macOS |
|-------|--------|-------|-------|
| Page (list view) | `shared/nexis/Pages/StartupApps/startup_apps_page.{h,cpp,ui}` | — | — |
| Item (app card) | `shared/nexis/Pages/StartupApps/startup_app.{h,cpp,ui}` | — | — |
| Editor (dialog) | `shared/nexis/Pages/StartupApps/startup_app_edit.{h,ui}` | `linux/nexis/Pages/StartupApps/startup_app_edit.cpp` | `macos/nexis/Pages/StartupApps/startup_app_edit.cpp` |

### Discovery

- **Linux:** Scans `~/.config/autostart/*.desktop`
- **macOS:** Scans `~/Library/LaunchAgents/*.plist` (filters `com.apple.*`)

### Existing Features

1. View all startup apps in a QListWidget with icon, name, edit/delete buttons, enable/disable checkbox
2. Enable/disable toggle — writes `Hidden=true/false` or `X-GNOME-Autostart-enabled=` (Linux), `<key>Disabled</key>` (macOS)
3. Delete — removes the underlying file
4. Add new — creates `.desktop` or `.plist` from template
5. Edit existing — parses file, populates Name/Comment/Command fields, overwrites on save
6. Startup delay (FR-15) — `X-GNOME-Autostart-Delay=N` on Linux, `sleep N && cmd` shell wrapper on macOS
7. Auto-refresh via QFileSystemWatcher on the autostart directory

### Editor Dialog Fields (current)

Name, Comment, Command, Delay (spinbox 0–3600s)

---

## 2. QuentiumYT v1.5.0 Changes (Commit `77a9928`)

### New Fields Added

| Field | Regex | .desktop Key | Status in Nexis |
|-------|-------|-------------|-----------------|
| `GenericName` | `^GenericName=.*` | `GenericName=` | Missing |
| `Icon` | `^Icon=.*` | `Icon=` | Missing |

### `getDesktopValue()` Bug Fix

**Bug:** `split("=")` + `directive.last()` breaks when values contain `=` (e.g. `Exec=env QT_QPA_PLATFORM=xcb /usr/bin/app`).

**Fix:** Changed to `directive.section('=', 1).trimmed()` which returns everything after the first `=`.

**Nexis status:** Bug is present in `shared/nexis/utilities.h` lines 29–40.

### Updated New-App Template

QuentiumYT's template includes `GenericName=`, `Icon=`, `Hidden=false`, `X-GNOME-Autostart-enabled=true`. Nexis's Linux template omits GenericName, Icon, and X-GNOME-Autostart-enabled.

---

## 3. Feature Gaps vs. Competitors

| Feature | GNOME Tweaks | KDE Autostart | Nexis | Priority |
|---------|-------------|---------------|-------|----------|
| Search/filter | ✅ | ✅ | ❌ | Medium |
| GenericName field | N/A | N/A | ❌ | Medium |
| Icon field / display | ✅ | ✅ | ❌ | Medium |
| System autostart (`/etc/xdg/autostart/`) visibility | ✅ | ✅ | ❌ | Low |
| `OnlyShowIn`/`NotShowIn` DE conditionals | N/A | N/A | ❌ | Low |
| Non-GNOME delay fallback (`sleep` in Exec) | N/A | N/A | ❌ | Low |

---

## 4. Bug: `getDesktopValue()` truncates values containing `=`

**File:** `shared/nexis/utilities.h:29-40`

```cpp
static QString getDesktopValue(const QRegularExpression &val, const QStringList &lines)
{
    QStringList filteredList = lines.filter(val);
    if (filteredList.count() > 0) {
        QStringList directive = filteredList.first().trimmed().split("=");
        if (directive.count() > 1) {
            return directive.last().trimmed();  // BUG: loses everything before last =
        }
    }
    return QString("");
}
```

**Impact:** `Exec=env VAR=val /usr/bin/app` → returns `val /usr/bin/app` instead of `env VAR=val /usr/bin/app`.

**Fix:** `return filteredList.first().trimmed().section('=', 1).trimmed();`

---

## 5. Key Code Locations

### Editor regex defines (`startup_app_edit.h:12-17`)
```cpp
#define NAME_REG QRegularExpression("^Name=.*")
#define COMMENT_REG QRegularExpression("^Comment=.*")
#define EXEC_REG QRegularExpression("^Exec=.*")
#define GNOME_ENABLED_REG QRegularExpression("^X-GNOME-Autostart-enabled=.*")
#define HIDDEN_REG QRegularExpression("^Hidden=.*")
#define DELAY_REG QRegularExpression("^X-GNOME-Autostart-Delay=.*")
```

### Linux new-app template (`linux/nexis/Pages/StartupApps/startup_app_edit.cpp:107-124`)
```cpp
QString appContent = QString(
    "[Desktop Entry]\n"
    "Name=%1\n"
    "Comment=%2\n"
    "Exec=%3\n"
    "Type=Application\n"
    "Terminal=false\n"
    "Hidden=false\n").arg(appName, appComment, appCommand);
```

### macOS plist builder (`macos/nexis/Pages/StartupApps/startup_app_edit.cpp:118-149`)
Generates full XML plist with Label, ProgramArguments, RunAtLoad. Delay wraps in `/bin/bash -c "sleep N && cmd"`.

### Page load flow (`startup_apps_page.cpp`)
- Linux: lines 147–184 — scans `*.desktop`, parses `Name=`, `Hidden=`, `X-GNOME-Autostart-enabled=`
- macOS: lines 83–143 — scans `*.plist`, derives name from reverse-DNS basename, checks `Disabled` key

### Signal/slot chain
```
loadApps() → foreach file → create StartupApp widget
  → StartupApp::deleteAppS → loadApps() (reload)
  → StartupApp::editStartupAppS(filePath) → openStartupAppEdit(filePath)
btnAddStartupApp::clicked → openStartupAppEdit("") → new app mode
StartupAppEdit::startupAppAdded → loadApps() (reload)
QFileSystemWatcher::directoryChanged → loadApps() (auto-refresh)
```

### QSS styling (`style.qss:683-734`)
- Card style: `#widgetStartupApp { border-radius: 12; background-color: @cardBg; }` with hover
- App icon: `url(:/static/themes/@themeName/img/app.png)` (generic, no per-app icon)
- Edit/Delete buttons: `edit.png` / `trash.png`, borderless
