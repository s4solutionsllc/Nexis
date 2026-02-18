Perfect! Now I have enough information. Let me create the comprehensive research document:

---

# FR-16: Scheduled / Automated Cleaning — Research Report

**Date:** February 18, 2026  
**Researcher:** Claude Code  
**Project:** Nexis  
**Scope:** Very thorough investigation of current System Cleaner architecture and scheduler infrastructure

---

## Executive Summary

Nexis currently has a robust manual System Cleaner with 6 cleaning categories (Package Cache, Crash Reports, Application Logs, Application Caches, Trash, Dev Tool Caches) and clean I/O operations. To implement scheduled/automated cleaning (FR-16), we must:

1. Extract core cleaning logic from `SystemCleanerPage` into a reusable, non-UI `CleanerService` class
2. Implement OS-native scheduling: launchd plists (macOS) and systemd timers (Linux)
3. Add headless CLI support (`--clean --schedule "name"`) to run without the GUI
4. Extend `SettingManager` with schedule persistence
5. Provide a Settings UI for creating/managing schedules

The current architecture makes this achievable without major refactoring — the cleaning operations are already isolated in `systemClean()` and use thread-safe patterns.

---

## 1. Current System Cleaner Architecture

### 1.1 Overview

**File:** `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/SystemCleaner/system_cleaner_page.h` and `.cpp`

The `SystemCleanerPage` class is a single, comprehensive monolith that handles:
- Category selection (6 checkboxes)
- Scan/preview (what would be cleaned)
- Clean execution (actual deletion)
- UI feedback (loading animations, tree widget display)

**Key structure:**
```cpp
class SystemCleanerPage : public QWidget {
    enum CleanCategories {
        PACKAGE_CACHE,
        CRASH_REPORTS,
        APPLICATION_LOGS,
        APPLICATION_CACHES,
        TRASH,
        DEV_TOOL_CACHES
    };
    
    // Scanning
    void systemScan();
    void onScanFinished();
    
    // Cleaning
    void systemClean();
    void onCleanFinished();
    
    // UI
    void on_btnScan_clicked();
    void on_btnClean_clicked();
};
```

### 1.2 The Scan Workflow (Lines 178–277)

**Current flow:**

1. User checks boxes for categories they want to scan
2. Clicks "Scan" button → `on_btnScan_clicked()` runs (main thread)
3. Main thread reads checkbox states into member variables:
   - `mScanPackageCache`, `mScanCrashReports`, `mScanAppLog`, `mScanAppCache`, `mScanTrash`, `mScanDevToolCache`
4. Worker thread launched via `QtConcurrent::run()` → calls `systemScan()`
5. Worker thread calls manager methods:
   - `tmr->getPackageCaches()` → (ToolManager)
   - `im->getCrashReports()` → (InfoManager)
   - `im->getAppLogs()` → (InfoManager)
   - `im->getAppCaches()` → (InfoManager)
   - `im->getDevToolCaches()` → (InfoManager)
6. Results stored in lists: `mPackageCaches`, `mCrashReports`, `mAppLogs`, `mAppCaches`, `mDevToolCaches`
7. Signal `scanFinishedS()` emitted
8. Main thread slot `onScanFinished()` runs:
   - Populates tree widget with results
   - Displays total size
   - For Dev Tool Caches, renames ambiguous "Cache"/"GPUCache" entries to "appName/Cache"
   - Clears scan result lists (BUG-10 fix)

**Key code snippet (lines 390–438):**
```cpp
void SystemCleanerPage::on_btnScan_clicked() {
    if (mScanInProgress || mCleanInProgress)
        return;
    
    // Read checkbox states on main thread
    mScanPackageCache = ui->checkPackageCache->isChecked();
    mScanCrashReports = ui->checkCrashReports->isChecked();
    // ... etc.
    
    // Pre-scan UI updates
    ui->btnScan->hide();
    mLoadingMovie->start();
    // ... disable checkboxes
    
    mScanInProgress = true;
    mWorkerFuture = QtConcurrent::run([this]() { systemScan(); });
}
```

### 1.3 The Clean Workflow (Lines 296–388)

**Current flow:**

1. User checks files/folders in the tree widget that they want to delete
2. Clicks "Clean" button → `on_btnClean_clicked()` runs (main thread)
3. Main thread validates that something is selected via `cleanValid()`
4. Main thread reads tree widget state and builds work lists:
   - `mFilesToDelete` — all selected files/dirs
   - `mChildrenToRemove` — tree indices for UI cleanup
   - `mCleanTrash` — whether trash itself is selected
5. Worker thread launched → calls `systemClean()`
6. Worker thread deletes files:
   - For Trash: calls `QDir::removeRecursively()` on `~/.Trash` (macOS) or `~/.local/share/Trash` (Linux)
   - For other categories:
     - Directories: empty contents but preserve the dir itself (BUG-02 fix)
     - Files: call `CommandUtil::sudoExec("rm", {"-rf", filesToDelete})`
7. Worker thread calculates total bytes freed
8. Signal `cleanFinishedS()` emitted
9. Main thread slot `onCleanFinished()` runs:
   - Removes cleaned items from tree widget
   - Updates category titles and sizes
   - Shows cleaned size label
   - Hides loading indicator

**Key code snippet (lines 440–489):**
```cpp
void SystemCleanerPage::on_btnClean_clicked() {
    if (mScanInProgress || mCleanInProgress)
        return;
    
    if (!cleanValid())
        return;
    
    // Read tree state on main thread
    QStringList filesToDelete;
    QList<QPair<int,int>> childrenToRemove;
    bool cleanTrash = false;
    
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *it = tree->topLevelItem(i);
        CleanCategories cat = (CleanCategories) it->data(2, 0).toInt();
        
        if (cat != CleanCategories::TRASH) {
            for (int j = 0; j < it->childCount(); ++j) {
                if (it->child(j)->checkState(0) == Qt::Checked) {
                    mFilesToDelete << it->child(j)->data(2, 0).toString();
                    mChildrenToRemove.append(QPair<int,int>(i, j));
                }
            }
        } else if (cat == CleanCategories::TRASH) {
            if (it->checkState(0) == Qt::Checked) {
                mCleanTrash = true;
                // Set trash path
            }
        }
    }
    
    mCleanInProgress = true;
    mWorkerFuture = QtConcurrent::run([this]() { systemClean(); });
}
```

### 1.4 Critical Data Sources

**InfoManager** (`/shared/nexis/Managers/info_manager.h`):
```cpp
QFileInfoList getCrashReports() const;      // delegated to SystemInfo
QFileInfoList getAppLogs() const;           // delegated to SystemInfo
QFileInfoList getAppCaches() const;         // delegated to SystemInfo
QFileInfoList getDevToolCaches() const;     // delegated to SystemInfo
```

**ToolManager** (`/shared/nexis/Managers/tool_manager.h`):
```cpp
QFileInfoList getPackageCaches() const;
```

Both are singleton instances, accessed via `InfoManager::ins()` and `ToolManager::ins()`.

### 1.5 Threading & Safety

**Current approach:**
- `QtConcurrent::run()` for worker threads (light but not persistent)
- Main thread reads checkbox/tree state before launching worker
- Worker thread writes results to member lists
- Main thread reads member lists in slot after worker finishes
- Flag variables (`mScanInProgress`, `mCleanInProgress`) prevent overlapping operations (BUG-10 fix)
- Member `QFuture<void> mWorkerFuture` tracked and awaited on shutdown (BUG-05 fix)

**Safe pattern:**
```cpp
mScanInProgress = true;
mWorkerFuture = QtConcurrent::run([this]() { 
    systemScan();  // Worker: write to member lists
});
// Main thread continues; signal emitted by worker
connect(this, &SystemCleanerPage::scanFinishedS, 
        this, &SystemCleanerPage::onScanFinished);
// Signal handler: read member lists on main thread
```

---

## 2. Settings & Preferences Infrastructure

### 2.1 SettingManager Architecture

**File:** `/shared/nexis/Managers/setting_manager.h` and `.cpp`

**Storage mechanism:**
- QSettings-based INI file
- Location: `QStandardPaths::AppConfigLocation` (e.g., `~/.config/nexis/settings.ini` on Linux, `~/Library/Application Support/Nexis/settings.ini` on macOS)
- Singleton instance via `SettingManager::ins()`

**Current setting keys** (lines 7–27):
```cpp
namespace SettingKeys {
    const QString ThemeName("ThemeName");
    const QString Language("Language");
    const QString DiskName("DiskName");
    const QString StartPage("StartPage");
    const QString CPUAlertPercent("CPUAlertPercent");
    const QString MemoryAlertPercent("MemoryAlertPercent");
    const QString DiskAlertPercent("DiskAlertPercent");
    const QString AppQuitDialogDontAsk("AppQuitDialogDontAsk");
    const QString AppQuitDialogChoice("AppQuitDialogChoice");
    const QString ColorScheme("ColorScheme");
    const QString DiskAnalyzerTool("DiskAnalyzerTool");
    const QString DiskAnalyzerCustomPath("DiskAnalyzerCustomPath");
    const QString KioskMode("KioskMode");
    const QString TempSensorId("TempSensorId");
    const QString GpuDeviceId("GpuDeviceId");
    const QString BatteryAlertPercent("BatteryAlertPercent");
    const QString BatteryAlertLastHealth("BatteryAlertLastHealth");
    const QString BatteryAlertSnoozedUntil("BatteryAlertSnoozedUntil");
    const QString DiskHealthAlertEnabled("DiskHealthAlertEnabled");
}
```

**Interface pattern:**
```cpp
public:
    void setThemeName(const QString &value);
    QString getThemeName() const;
    
    void setKioskMode(bool value);
    bool getKioskMode() const;
```

**Implementation:**
```cpp
void SettingManager::setKioskMode(bool value) {
    mSettings->setValue(SettingKeys::KioskMode, value);
}

bool SettingManager::getKioskMode() const {
    return mSettings->value(SettingKeys::KioskMode, false).toBool();
}
```

**Key observations:**
- Simple key-value store (no complex nested structures yet)
- To store schedules, we'll need to add getter/setter methods for JSON arrays or indexed groups
- Settings persist immediately and survive app restart

### 2.2 Settings Page UI

**File:** `/shared/nexis/Pages/Settings/settings_page.h` and `.cpp`

Current settings include:
- Theme selection
- Language selection
- Disk selection
- CPU/Memory/Disk/Battery alert thresholds
- Auto-start checkbox
- Start page selection
- Color scheme (light/dark/auto)
- Disk analyzer tool selection
- Health alert toggles

**Pattern:** Settings page initializes UI from `SettingManager`, connects UI change signals to slots that call `SettingManager::set*()` methods.

No existing "Scheduled Cleaning" section — we'll need to add this.

---

## 3. Timer & Background Task Infrastructure

### 3.1 Existing Timer Usage

**Found via grep:**
- `ProcessesPage` uses `QTimer` for periodic process list updates (every 1000ms)
- `ResourcesPage` uses `QTimer` for periodic CPU/memory/disk/network/GPU updates (every 1000ms)
- `DashboardPage` uses `QTimer` for temperature, GPU, battery, disk health updates (every 1000ms)

**Example from ResourcesPage:**
```cpp
mTimer(new QTimer(this))
// ...
connect(mTimer, &QTimer::timeout, this, &ResourcesPage::updateCpuChart);
// ...
mTimer->start(1000);
```

**Observation:** These are all in-process, UI-focused timers. They run continuously while the page is visible. **NOT suitable for scheduled background cleaning** because they require the app to be running.

### 3.2 No Existing Persistent Background Tasks

- No QThreadPool or thread persistence (uses `QtConcurrent::run` for one-off workers)
- No system tray integration for background tasks (tray icon exists but only for minimize/restore)
- No IPC (D-Bus, named pipes, etc.) for inter-process communication
- No launchd or systemd integration

---

## 4. Tray Icon & Background Operation Support

### 4.1 System Tray Icon Infrastructure

**File:** `/shared/nexis/Managers/app_manager.h` and `.cpp`

**Creation** (app_manager.cpp lines 25):
```cpp
mTrayIcon = new QSystemTrayIcon(QIcon(":/static/tray-icon.svg"));
```

**Access method:**
```cpp
QSystemTrayIcon *AppManager::getTrayIcon()
{
    return mTrayIcon;
}
```

**Current usage** (app.cpp lines 173–199):
```cpp
void App::createTrayActions()
{
    // Add action for each sidebar button
    for (QPushButton *button: mListSidebarButtons) {
        QString toolTip = button->toolTip();
        QAction *action = new QAction(toolTip, this);
        connect(action, &QAction::triggered, [=] {
            clickSidebarButton(toolTip, true);  // Open page when clicked
        });
        mTrayMenu->addAction(action);
    }
    
    // Click tray icon to show/restore window
    connect(mTrayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason) {
        setWindowState(windowState() & ~Qt::WindowMinimized);
        show();
        if (windowHandle())
            windowHandle()->requestActivate();
    });
    
    mTrayMenu->addSeparator();
    QAction *quitAction = new QAction(tr("Quit"), this);
    connect(quitAction, &QAction::triggered, [=] {qApp->quit();});
    mTrayMenu->addAction(quitAction);
    
    mTrayIcon->setContextMenu(mTrayMenu);
}
```

**Key property** (main.cpp line 63):
```cpp
app.setQuitOnLastWindowClosed(false);
```

This allows the app to continue running when the main window is closed — perfect for background cleaning.

### 4.2 Tray Icon for Notifications

The tray icon is fully functional for:
- Context menus (done)
- Click activation (done)
- Notifications (theoretically possible — `showMessage()` method exists on `QSystemTrayIcon`)

However, **no code currently sends tray notifications**. We'll need to add this.

---

## 5. Cleaning Operations & Utilities

### 5.1 CommandUtil for Privileged Deletion

**File:** `/shared/nexis-core/Utils/command_util.h`

```cpp
class CommandUtil {
public:
    static QString sudoExec(const QString &cmd, QStringList args = {}, QByteArray data = {});
    static QString exec(const QString &cmd, QStringList args = {}, QByteArray data = {}, int timeoutMs = 30000);
    static ExecResult execWithStatus(const QString &cmd, QStringList args = {}, int timeoutMs = 30000);
    static bool isExecutable(const QString &cmd);
};
```

**Usage in SystemCleanerPage** (line 345):
```cpp
CommandUtil::sudoExec("rm", QStringList() << "-rf" << filesToDelete);
```

This prompts for sudo password via the OS (standard security dialog). For scheduled cleaning, we may need to handle authentication differently.

### 5.2 File Utilities

**File:** `/shared/nexis-core/Utils/file_util.h`

Key methods:
- `getFileSize(path)` — returns `quint64`
- `readStringFromFile(path)` — returns QString

### 5.3 Format Utilities

**File:** `/shared/nexis-core/Utils/format_util.h`

Key method:
- `formatBytes(quint64 bytes)` — returns human-readable string (e.g., "2.1 GB")

---

## 6. Signal Infrastructure

### 6.1 SignalMapper (Global Signal Broadcaster)

**File:** `/shared/nexis/signal_mapper.h` and `.cpp`

```cpp
class SignalMapper : public QObject {
    Q_OBJECT
public:
    static SignalMapper *ins();
signals:
    void sigChangedAppTheme();
    void sigUninstallStarted();
    void sigUninstallFinished();
};
```

**Purpose:** Broadcast events app-wide. Used for theme changes, uninstall events.

**For FR-16:** We could add signals like:
- `sigScheduledCleanStarted(QString scheduleName)`
- `sigScheduledCleanFinished(CleanResult result)`

---

## 7. Platform-Specific Code Structure

### 7.1 Directory Layout

```
/Nexis/
  /shared/
    /nexis/              ← Cross-platform Qt code
    /nexis-core/         ← Cross-platform core libraries
  /macos/
    /nexis/Managers/     ← macOS-specific ToolManager
  /linux/
    /nexis/Managers/     ← Linux-specific ToolManager
```

### 7.2 Platform Conditionals

**Macro:** `Q_OS_MAC` (or `Q_OS_MACOS` on newer Qt)

**Examples:**
```cpp
#ifdef Q_OS_MACOS
    // macOS-specific code
    mTrashPath = QDir::homePath() + "/.Trash/";
#else
    // Linux code
    mTrashPath = QDir::homePath() + "/.local/share/Trash/";
#endif
```

**For scheduling:**
- macOS: Use `Q_OS_MAC` to write launchd plists
- Linux: Use `#ifndef Q_OS_MAC` to write systemd units

---

## 8. Application Entry Point & CLI Support

### 8.1 Current main.cpp (Shared)

**File:** `/shared/nexis/main.cpp`

**Current CLI options** (lines 83–91):
```cpp
QCommandLineOption hideOption("hide", "Hide Nexis while launching.");
QCommandLineOption noSplashOption("nosplash", "Hide splash screen while launching.");

QCommandLineParser parser;
parser.addVersionOption();
parser.addHelpOption();
parser.addOption(hideOption);
parser.addOption(noSplashOption);
parser.process(app);
```

**Usage:**
```bash
nexis --hide           # Launch hidden
nexis --nosplash       # Launch without splash
```

**For FR-16, we'd add:**
```bash
nexis --clean --schedule "Weekly Cleanup"   # Run headless cleaning
nexis --check-threshold                      # Check if junk exceeds threshold
```

**Key property** (line 63):
```cpp
app.setQuitOnLastWindowClosed(false);
```

This allows headless mode — if we launch with `--clean` and no GUI, the app can still run and exit.

---

## 9. Platform-Specific Scheduling Mechanisms

### 9.1 macOS launchd

**User agent plist location:** `~/Library/LaunchAgents/`

**Example plist for weekly cleaning at 3 AM on Sundays:**
```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.nexis.clean.weekly</string>
    
    <key>ProgramArguments</key>
    <array>
        <string>/path/to/nexis</string>
        <string>--clean</string>
        <string>--schedule</string>
        <string>Weekly Cleanup</string>
    </array>
    
    <key>StartCalendarInterval</key>
    <dict>
        <key>Hour</key>
        <integer>3</integer>
        <key>Minute</key>
        <integer>0</integer>
        <key>Weekday</key>
        <integer>0</integer>
    </dict>
    
    <key>RunAtLoad</key>
    <false/>
    
    <key>StandardErrorPath</key>
    <string>/var/log/nexis-clean-weekly.log</string>
    
    <key>StandardOutPath</key>
    <string>/var/log/nexis-clean-weekly.log</string>
</dict>
</plist>
```

**Key properties:**
- `Label` — unique identifier (use `com.nexis.clean.{schedule-id}`)
- `ProgramArguments` — command and args (must be absolute path)
- `StartCalendarInterval` — when to run (Hour 0–23, Minute 0–59, Weekday 0=Sunday)
- `RunAtLoad` — should it run on launchd startup (no, we want specific times)
- Supports `Persistent=true` equivalent via `KeepAlive` for continuous tasks (not needed here)

**Qt considerations:** Qt on macOS can generate plist XML directly using QSettings:
```cpp
QSettings plist(path, QSettings::NativeFormat);  // Native = plist on macOS
plist.setValue("Label", "com.nexis.clean.weekly");
plist.setValue("ProgramArguments/0", "/path/to/nexis");
// ... etc
```

### 9.2 Linux systemd User Timers

**User timer location:** `~/.config/systemd/user/`

**Example timer file: `nexis-clean-weekly.timer`:**
```ini
[Unit]
Description=Nexis Weekly System Cleaning
After=network-online.target

[Timer]
OnCalendar=Sun *-*-* 03:00:00
Persistent=true
Unit=nexis-clean-weekly.service

[Install]
WantedBy=timers.target
```

**Example service file: `nexis-clean-weekly.service`:**
```ini
[Unit]
Description=Nexis System Cleaning Service
After=network-online.target

[Service]
Type=oneshot
ExecStart=/path/to/nexis --clean --schedule "Weekly Cleanup"
StandardOutput=journal
StandardError=journal
```

**Key properties:**
- `OnCalendar` — when to run (cron-like syntax: `DayOfWeek HH:MM:SS`)
- `Persistent=true` — if missed, run on next boot
- `Type=oneshot` — run once, don't keep alive
- Timer and service are separate files (linked by `Unit=`)

**Qt considerations:** We generate these as plain text files. Qt has no native systemd support, but writing INI text is trivial:
```cpp
QFile timer(timerPath);
timer.open(QIODevice::WriteOnly | QIODevice::Truncate);
QTextStream stream(&timer);
stream << "[Unit]\nDescription=Nexis Weekly System Cleaning\n";
stream << "[Timer]\nOnCalendar=Sun *-*-* 03:00:00\n";
stream << "Persistent=true\n";
stream << "Unit=nexis-clean-weekly.service\n";
timer.close();

// Enable: `systemctl --user enable nexis-clean-weekly.timer`
// Disable: `systemctl --user disable nexis-clean-weekly.timer`
```

### 9.3 Linux Fallback: cron

If systemd is not available (Alpine, Void, Devuan, etc.), we can fall back to cron.

**User cron location:** `~/.local/share/cron/tabs/` (non-standard) or direct crontab edit

**Cron syntax for weekly at Sunday 3 AM:**
```
0 3 * * 0 /path/to/nexis --clean --schedule "Weekly Cleanup"
```

**Qt approach:**
```cpp
// Read user crontab
QString crontab = CommandUtil::exec("crontab", {"-l"});
// Append our line
crontab += "\n0 3 * * 0 /path/to/nexis --clean --schedule \"Weekly Cleanup\"\n";
// Write back
CommandUtil::exec("crontab", {"-"}, crontab.toUtf8());
```

---

## 10. Key Files & Code Locations Summary

| File Path | Class/Purpose | Key Functions |
|-----------|---------------|---|
| `shared/nexis/Pages/SystemCleaner/system_cleaner_page.h/.cpp` | SystemCleanerPage | `systemScan()`, `systemClean()`, UI handlers |
| `shared/nexis-core/Utils/file_util.h/.cpp` | File operations | `getFileSize()` |
| `shared/nexis-core/Utils/command_util.h/.cpp` | Command execution | `sudoExec()`, `exec()` |
| `shared/nexis-core/Utils/format_util.h/.cpp` | Formatting | `formatBytes()` |
| `shared/nexis/Managers/info_manager.h/.cpp` | System info | `getCrashReports()`, `getAppLogs()`, `getAppCaches()`, `getDevToolCaches()` |
| `shared/nexis/Managers/tool_manager.h/.cpp` | Tools | `getPackageCaches()` |
| `shared/nexis/Managers/setting_manager.h/.cpp` | Settings persistence | `SettingManager::ins()`, get/set methods |
| `shared/nexis/Managers/app_manager.h/.cpp` | App-wide config | `getTrayIcon()`, theme management |
| `shared/nexis/Pages/Settings/settings_page.h/.cpp` | Settings UI | Settings form, signal handlers |
| `shared/nexis/app.h/.cpp` | Main window | Tray actions, page routing |
| `shared/nexis/main.cpp` | Entry point | CLI args parsing, app init |
| `shared/nexis/signal_mapper.h/.cpp` | Global signals | App-wide event broadcaster |

---

## 11. Scan/Clean Data Flow Details

### 11.1 Data Sources (InfoManager & ToolManager)

**Package Caches** (ToolManager):
- Platform-specific implementations in `macos/nexis/Managers/tool_manager.cpp` and `linux/nexis/Managers/tool_manager.cpp`
- Returns list of cache directories for installed packages (Homebrew on macOS, apt/pacman on Linux)

**Crash Reports** (InfoManager → SystemInfo):
- macOS: `~/Library/Logs/DiagnosticMessages/`
- Linux: typically in app-specific directories (e.g., `~/.cache/*/crashes`)

**Application Logs** (InfoManager → SystemInfo):
- macOS: `~/Library/Logs/`
- Linux: `~/.local/share/application-logs/` and app-specific log dirs

**Application Caches** (InfoManager → SystemInfo):
- macOS: `~/Library/Caches/`
- Linux: `~/.cache/`

**Dev Tool Caches** (InfoManager → SystemInfo):
- Node: `node_modules/`, `.npm/`, `.yarn/cache/`
- Python: `.cache/pip/`, `.cache/pipenv/`, `.tox/`
- Rust: `.cargo/registry/cache/`, `.rustup/`
- Java: `.gradle/caches/`, `.m2/repository/`
- Go: `$GOPATH/pkg/`, `~/go/pkg/`
- Kotlin: `.konan/cache/`
- Ruby: `.gem/`

### 11.2 Deletion Pattern

**Current code** (systemClean, lines 296–349):

1. **Trash handling:**
   ```cpp
   if (mCleanTrash) {
       #ifdef Q_OS_MACOS
           // Flat dir: remove all contents
           QDir trashDir(mTrashPath);
           for (const QFileInfo &entry : trashDir.entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot)) {
               if (entry.isDir())
                   QDir(entry.absoluteFilePath()).removeRecursively();
               else
                   QFile::remove(entry.absoluteFilePath());
           }
       #else
           // FreeDesktop: structure with files/ and info/
           QDir(mTrashPath + "/files").removeRecursively();
           QDir(mTrashPath + "/info").removeRecursively();
       #endif
   }
   ```

2. **Other categories (package caches, logs, caches, dev tools):**
   ```cpp
   for (const QString &path : mFilesToDelete) {
       QFileInfo fi(path);
       if (fi.isDir()) {
           // Empty directory contents but preserve the directory itself (BUG-02)
           QDir dir(path);
           for (const QFileInfo &entry : dir.entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot)) {
               if (entry.isDir())
                   QDir(entry.absoluteFilePath()).removeRecursively();
               else
                   QFile::remove(entry.absoluteFilePath());
           }
       } else {
           filesToRemove << path;
       }
   }
   
   if (!filesToRemove.isEmpty()) {
       CommandUtil::sudoExec("rm", QStringList() << "-rf" << filesToRemove);
   }
   ```

**Key behavior:**
- Directories are emptied, not removed (preserves structure for apps that depend on them)
- Trash is special: `files/` and `info/` subdirs are removed on Linux; all contents on macOS
- Uses `sudoExec` for rm command (will prompt for password)
- Calculates size before deletion for reporting

---

## 12. UI Components for Cleaning

### 12.1 SystemCleanerPage UI (system_cleaner_page.ui)

**Structure:**
- QStackedWidget with 2 pages:
  1. **Categories page** (index 0):
     - 6 category checkboxes (Package Caches, Crash Reports, App Logs, App Caches, Trash, Dev Tool Caches)
     - Icons for each category (loaded from theme, fallback SVGs)
     - "Select All" checkbox
     - "Scan" button
  2. **Results page** (index 1):
     - "Back" button
     - QTreeWidget showing scan results (hierarchical: category → files/dirs with sizes)
     - "Sort By" combobox (Name A–Z, Name Z–A, Size Small–Large, Size Large–Small)
     - "Select All" checkbox for results
     - "Clean" button (large, prominent)
     - Loading indicator
     - Results label ("X size files cleaned")

### 12.2 ByteTreeWidget (Custom Widget)

**File:** `system_cleaner_page.h` references `byte_tree_widget.h`

```cpp
class ByteTreeWidget : public QTreeWidgetItem {
public:
    void setValues(const QString &text, const quint64 &size, const QVariant &data);
    virtual bool operator<(const QTreeWidgetItem &other) const;
};
```

**Purpose:** Custom tree item that sorts by byte size correctly (numeric, not string).

---

## 13. Architecture for Refactoring (For FR-16)

### 13.1 Proposed New CleanerService Class

To support scheduled cleaning, we need to extract core logic into a non-UI service:

```cpp
// NEW FILE: shared/nexis-core/Services/cleaner_service.h

#include <QObject>
#include <QList>
#include <QString>

struct CleanResult {
    quint64 totalBytesFreed;
    int totalFilesRemoved;
    int totalDirsEmptied;
    QMap<QString, quint64> categoryBreakdown;  // "Package Caches" → bytes
    QDateTime timestamp;
};

class CleanerService : public QObject {
    Q_OBJECT
public:
    static CleanerService *ins();
    
    // Scan-only (no deletion)
    QMap<QString, QFileInfoList> scan(const QList<int> &categories);  // int = CleanCategory enum
    
    // Dry-run (scan + estimate)
    CleanResult dryRun(const QList<int> &categories);
    
    // Actual cleaning
    CleanResult clean(const QList<int> &categories);
    
    // Headless entry point
    CleanResult cleanSchedule(const QString &scheduleName);
    
signals:
    void cleaningStarted(QString scheduleName);
    void cleaningProgress(int filesProcessed, quint64 bytesFreed);
    void cleaningFinished(CleanResult result);
    
private:
    CleanerService();
    
    // Extracted from SystemCleanerPage::systemScan()
    QMap<QString, QFileInfoList> scanPackageCaches();
    QMap<QString, QFileInfoList> scanCrashReports();
    QMap<QString, QFileInfoList> scanAppLogs();
    QMap<QString, QFileInfoList> scanAppCaches();
    QMap<QString, QFileInfoList> scanDevToolCaches();
    QMap<QString, QFileInfoList> scanTrash();
    
    // Extracted from SystemCleanerPage::systemClean()
    quint64 deleteFiles(const QStringList &paths);
    quint64 deleteTrash();
};
```

### 13.2 Proposed New ScheduleManager Class

To manage schedule persistence and OS-level scheduling:

```cpp
// NEW FILE: shared/nexis/Managers/schedule_manager.h

#include <QString>
#include <QList>
#include <QSettings>

struct CleaningSchedule {
    QString id;               // UUID or "weekly-cleanup"
    QString name;             // "Weekly Cleanup"
    bool enabled;             // true/false
    QString frequency;        // "daily", "weekly", "every_N_days", "monthly"
    int everyNDays;          // for "every_N_days" frequency
    int dayOfWeek;           // 0=Sunday ... 6=Saturday (for "weekly")
    int dayOfMonth;          // 1-31 (for "monthly")
    int hour;                // 0-23
    int minute;              // 0-59
    QList<int> categories;   // list of CleanCategory enums
    QList<QString> exclusionRules;
    int minFileAgeSecs;      // skip files newer than this (default 86400 = 24h)
    QDateTime lastRun;
    quint64 lastBytesFreed;
};

class ScheduleManager {
public:
    static ScheduleManager *ins();
    
    // CRUD
    QList<CleaningSchedule> getAllSchedules() const;
    CleaningSchedule getSchedule(const QString &id) const;
    void createSchedule(const CleaningSchedule &schedule);
    void updateSchedule(const CleaningSchedule &schedule);
    void deleteSchedule(const QString &id);
    
    // Persistence to SettingManager + OS
    void persistSchedules();  // Write to settings.ini
    void syncOSSchedulers();  // Create/update launchd/systemd
    
    // Quick setup
    void enableQuickSetup();  // Create default weekly schedule
    void disableQuickSetup(); // Delete auto-created schedule
    
    // Next scheduled clean
    CleaningSchedule getNextScheduledClean() const;
    QDateTime getNextRunTime(const CleaningSchedule &schedule) const;
    
private:
    ScheduleManager();
    
    void createLaunchdSchedule(const CleaningSchedule &schedule);   // macOS
    void deleteLaunchdSchedule(const QString &id);                   // macOS
    
    void createSystemdSchedule(const CleaningSchedule &schedule);   // Linux
    void deleteSystemdSchedule(const QString &id);                   // Linux
    
    void createCronSchedule(const CleaningSchedule &schedule);       // Linux fallback
    void deleteCronSchedule(const QString &id);                      // Linux fallback
    
    QList<CleaningSchedule> mSchedules;
};
```

### 13.3 Proposed Settings Extensions

Add to `SettingManager`:
```cpp
public:
    // Schedule data stored as JSON array in one key
    void setSchedules(const QJsonArray &schedules);
    QJsonArray getSchedules() const;
    
    void setQuickSetupEnabled(bool enabled);
    bool getQuickSetupEnabled() const;
    
    void setCleaningNotificationsEnabled(bool enabled);
    bool getCleaningNotificationsEnabled() const;
    
    void setThresholdAlertEnabled(bool enabled);
    bool getThresholdAlertEnabled() const;
    
    void setThresholdGB(int gb);
    int getThresholdGB() const;
```

---

## 14. Headless Operation & CLI Integration

### 14.1 Main.cpp Extensions

Current main.cpp would need:

```cpp
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // ... existing setup ...
    
    // NEW: Parse extended CLI options
    QCommandLineOption cleanOption("clean", "Run scheduled cleaning", "schedule-name");
    QCommandLineOption checkThresholdOption("check-threshold", "Check junk threshold");
    QCommandLineOption noGuiOption("no-gui", "Don't show GUI");
    
    parser.addOption(cleanOption);
    parser.addOption(checkThresholdOption);
    parser.addOption(noGuiOption);
    parser.process(app);
    
    bool shouldRunHeadless = parser.isSet(cleanOption) || parser.isSet(checkThresholdOption);
    
    if (shouldRunHeadless) {
        // Run cleaning in headless mode
        CleanerService *cleaner = CleanerService::ins();
        
        if (parser.isSet(cleanOption)) {
            QString scheduleName = parser.value(cleanOption);
            CleanResult result = cleaner->cleanSchedule(scheduleName);
            
            // Send notification
            QSystemTrayIcon tray;
            tray.setIcon(QIcon(":/static/tray-icon.svg"));
            tray.show();
            tray.showMessage("Nexis", "Cleaned " + FormatUtil::formatBytes(result.totalBytesFreed));
            
            // Optionally wait for user to see notification
            QTimer::singleShot(3000, qApp, &QApplication::quit);
            return app.exec();
        }
        
        if (parser.isSet(checkThresholdOption)) {
            // Check threshold and notify if needed
            // ...
            return 0;  // Exit immediately if no notification
        }
    }
    
    // ... existing GUI code ...
    
    return app.exec();
}
```

### 14.2 Executable Path Resolution

For launchd/systemd, we need the absolute path to the nexis executable:

```cpp
QString appPath = QCoreApplication::applicationFilePath();
// On macOS: /usr/local/bin/nexis or /opt/homebrew/bin/nexis
// On Linux: /usr/bin/nexis or /usr/local/bin/nexis

// For launchd plist:
plist.setValue("ProgramArguments/0", appPath);

// For systemd service:
stream << "ExecStart=" << appPath << " --clean --schedule Weekly\n";
```

---

## 15. Notification Infrastructure

### 15.1 QSystemTrayIcon Notifications

```cpp
QSystemTrayIcon *tray = AppManager::ins()->getTrayIcon();
tray->showMessage("Nexis",
    "Cleaned 2.1 GB (612 files)\nPackage Caches, Dev Tool Caches, Trash",
    QSystemTrayIcon::Information,
    5000);  // 5 second timeout
```

### 15.2 Native Notifications (Alternative)

For better UX, we could use:

**macOS:** AppleScript via `QProcess`:
```cpp
QString script = "display notification \"Cleaned 2.1 GB\" with title \"Nexis\"";
QProcess::execute("osascript", {"-e", script});
```

**Linux:** `notify-send` command:
```cpp
QProcess::execute("notify-send", {"Nexis", "Cleaned 2.1 GB (612 files)"});
```

---

## 16. File Permissions & Elevation Handling

### 16.1 Current sudoExec Pattern

Some files to clean require elevated permissions:
- `/var/log/*` (Linux system logs)
- Some Homebrew cache paths (macOS)

Current code uses `CommandUtil::sudoExec()`, which prompts via OS password dialog.

**Problem for scheduled cleaning:** Can't prompt for password during unattended execution.

**Solutions:**
1. **Use sudoers NOPASSWD rule** (admin configures once)
   ```
   nexis ALL = NOPASSWD: /bin/rm -rf /var/log/*
   ```
   Then scheduled clean can call sudo without password.

2. **Clean only user-space paths** (safer, default)
   - Don't try to clean `/var/log/` in scheduled mode
   - Document that user must run a manual System Cleaner scan for system logs

3. **Hybrid:** Offer user choice
   - "Safe mode" (default): clean only user paths
   - "Full mode": requires sudoers setup

---

## 17. Logging & Audit Trail

### 17.1 Cleaning History Log

**Location:**
- macOS: `~/Library/Application Support/Nexis/clean_history.log`
- Linux: `~/.config/nexis/clean_history.log`

**Format example:**
```
[2026-02-18 03:00:00] Schedule: Weekly Cleanup
[2026-02-18 03:00:00] Categories: Package Caches, App Caches, Dev Tools
[2026-02-18 03:00:02] Scanned: 2,847 files, 3.2 GB
[2026-02-18 03:00:05] Deleted: 847 files, 2.1 GB
[2026-02-18 03:00:05] Failed: 0 files
[2026-02-18 03:00:05] Duration: 5 seconds
[2026-02-18 03:00:05] --- end log entry ---

[2026-02-25 03:00:00] Schedule: Weekly Cleanup
...
```

**Implementation:**
```cpp
void CleanerService::logCleanResult(const CleanResult &result, const QString &scheduleName) {
    QString logPath = SettingManager::ins()->getConfigPath() + "/clean_history.log";
    QFile logFile(logPath);
    logFile.open(QIODevice::Append | QIODevice::Text);
    QTextStream stream(&logFile);
    
    stream << QString("[%1] Schedule: %2\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"), scheduleName);
    stream << QString("[%1] Categories: %2\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"), categoryList.join(", "));
    // ... etc
}
```

### 17.2 Cleaning History Viewer

New UI in Settings or System Cleaner:
- Show last 20 cleaning runs
- Per-schedule details
- Total space freed over time

---

## 18. Cross-Platform Challenges & Solutions

| Challenge | macOS | Linux | Solution |
|-----------|-------|-------|----------|
| Scheduler type | launchd | systemd/cron | Conditional via `Q_OS_MAC` |
| Path to schedules | `~/Library/LaunchAgents/` | `~/.config/systemd/user/` | Conditional paths |
| Notification API | AppleScript or native | notify-send | Conditional QProcess calls |
| Trash location | `~/.Trash/` | `~/.local/share/Trash/` | Already handled in SystemCleanerPage |
| Package cache paths | Homebrew | apt/pacman | Already handled in ToolManager |
| Dev tool caches | Similar but paths differ | Similar but paths differ | Already handled in InfoManager |
| Elevated file access | prompt via sudo | prompt via sudo | Existing CommandUtil::sudoExec |

---

## 19. Critical Design Decisions (Summary)

1. **Use OS-native scheduling, not in-app QTimer**
   - Zero resource cost when idle
   - Survives app restart/reboot
   - Aligns with system practices

2. **Extract cleaning logic into CleanerService**
   - Reusable by UI and headless modes
   - Enables unit testing
   - Clear separation of concerns

3. **First scheduled run is a dry-run with user approval**
   - Prevents accidental mass deletion
   - Follows CleanMyMac's safety model
   - Gives user confidence

4. **Exclusion rules (FR-18) are a hard dependency**
   - Users need granular control before auto-cleanup
   - Should be implemented before or alongside FR-16

5. **Default minimum age = 24 hours**
   - Prevents deletion of actively-used caches
   - Mirrors CCleaner's safety default

6. **Trash is opt-in for automated cleaning**
   - Default schedule excludes Trash
   - User must explicitly enable it
   - Reduces accidental data loss risk

7. **Full audit trail in clean_history.log**
   - Transparency for users who care
   - Foundation for future analytics
   - Helps debug issues

---

## 20. Estimated Complexity & Dependencies

**Hard dependencies:**
- FR-18 (Exclusion Rules) — must be implemented before or with FR-16
- Qt 6 (already in use)

**Soft dependencies:**
- Settings UI extension (needs dedicated new section)
- Notification system (already have tray icon)
- Logging infrastructure (straightforward)

**Estimated effort:**
- CleanerService extraction: 1–2 days
- ScheduleManager (OS integration): 2–3 days
- Settings UI: 1 day
- Headless mode + CLI: 1 day
- Testing & cross-platform validation: 2 days
- **Total: 7–9 days for core feature**

---

## 21. Recommended Implementation Sequence

1. **Phase 1:** Extract `CleanerService` from `SystemCleanerPage`
   - Decouple scan/clean logic from UI
   - Add signals for progress/completion
   - Ensure backward compatibility with UI

2. **Phase 2:** Extend `SettingManager` for schedule persistence
   - Add JSON array getter/setter for schedules
   - Add boolean flags (quick-setup, notifications-enabled, etc.)

3. **Phase 3:** Implement `ScheduleManager` with OS integration
   - Launchd plist generation/deletion (macOS)
   - Systemd timer generation/deletion (Linux)
   - Cron fallback (Linux non-systemd)

4. **Phase 4:** Add headless CLI support to main.cpp
   - Parse `--clean --schedule` and `--check-threshold` flags
   - Suppress GUI initialization when headless
   - Implement notification sending in headless mode

5. **Phase 5:** Build Settings UI for schedule management
   - Schedule list view
   - Schedule editor dialog
   - Quick-setup toggle
   - History viewer

6. **Phase 6:** Integration, testing, documentation
   - End-to-end testing on macOS and Linux
   - Performance validation
   - User documentation

---

## Conclusion

Nexis has a **solid foundation** for implementing scheduled cleaning:

- **Robust cleaning logic** already exists and handles platform differences well
- **Strong settings infrastructure** (SettingManager + QSettings) for persistence
- **Good tray icon integration** for background operation and notifications
- **Support for both platforms** via conditional compilation
- **Flexible command-line parsing** in main.cpp for headless operation

The **main refactoring needed** is extracting core cleaning logic from `SystemCleanerPage` into a non-UI `CleanerService` class. Once done, adding OS-level scheduling via launchd (macOS) and systemd (Linux) becomes straightforward.

The architecture described in this report aligns with **competitive best practices** (CleanMyMac, CCleaner) while offering **unique advantages:**
- Zero idle resource cost (vs. CCleaner's heavy background agent)
- Full transparency and audit trail (vs. CleanMyMac's opaque safety database)
- Native support for dev tool caches (neither competitor does this well)
- Cross-platform OS integration (launchd + systemd)

---

**Research completed:** February 18, 2026
