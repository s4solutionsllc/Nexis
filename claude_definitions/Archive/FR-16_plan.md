# FR-16 Implementation Plan — Scheduled / Automated Cleaning

## Summary

Add OS-native scheduled cleaning to Nexis. Users create named cleaning schedules (frequency, time, categories) from a new Settings section. Schedules are executed headlessly via launchd (macOS) or systemd timers (Linux), with zero background resource cost. Core cleaning logic is extracted from `SystemCleanerPage` into a reusable `CleanerService` class.

**Scope decisions (from Open Questions in user story):**
- **Multiple schedules:** Yes — power users need daily log cleans + weekly full cleans.
- **Threshold alerts:** Included as a lightweight addition (scan-only, notification, no auto-clean).
- **Headless notifications:** Use `QSystemTrayIcon::showMessage()` (brief app launch, works cross-platform).
- **Cleaning history format:** Plain text log (keep it simple; structured format is a future enhancement).
- **FR-18 (exclusion rules) dependency:** Deferred — FR-16 ships without per-path exclusion rules. The minimum-age filter (skip files < 24h old) provides basic safety. FR-18 can be layered on later.
- **Non-systemd Linux fallback:** Fall back to cron. Detection via `systemctl --user status` exit code.

---

## Phase 1: Extract `CleanerService` from `SystemCleanerPage`

Extract scan and clean I/O logic into a standalone, non-QWidget service class. `SystemCleanerPage` becomes a thin UI wrapper that calls `CleanerService`.

**New files:** `shared/nexis/Managers/cleaner_service.h`, `shared/nexis/Managers/cleaner_service.cpp`

- [x] **1.1** Create `CleanerService` header with:
  - Public enum `CleanCategory` mirroring the existing `CleanCategories` (PACKAGE_CACHE through DEV_TOOL_CACHES)
  - `struct ScanResult { QMap<CleanCategory, QFileInfoList> categoryFiles; quint64 totalSize; }`
  - `struct CleanResult { quint64 totalBytesFreed; int totalFilesRemoved; QMap<CleanCategory, quint64> categoryBreakdown; QDateTime timestamp; QString scheduleName; }`
  - `ScanResult scan(const QList<CleanCategory> &categories)` — synchronous, returns results
  - `CleanResult clean(const QList<CleanCategory> &categories, int minFileAgeSecs = 86400)` — synchronous, performs deletion
  - `CleanResult cleanSchedule(const QString &scheduleId)` — loads schedule from settings, runs clean
  - Singleton pattern: `static CleanerService *ins()`

- [x] **1.2** Implement `CleanerService::scan()`:
  - Extract the body of `SystemCleanerPage::systemScan()` — calls `ToolManager::ins()->getPackageCaches()`, `InfoManager::ins()->getCrashReports()`, etc.
  - Calculate total size by summing `QFileInfo::size()` across all results
  - Return `ScanResult`

- [x] **1.3** Implement `CleanerService::clean()`:
  - Extract the body of `SystemCleanerPage::systemClean()` — trash handling (platform-conditional), directory emptying (BUG-02 fix preserved), file deletion via `CommandUtil::sudoExec("rm", ...)`
  - Add minimum-age filter: skip files where `QFileInfo::lastModified()` is less than `minFileAgeSecs` ago
  - For headless mode: skip `sudoExec` for paths requiring elevation (user-space-only cleaning). Package caches that live in system dirs (e.g., `/var/cache/apt/`) are skipped when running unattended — the user can still clean them via the manual UI
  - Calculate and return `CleanResult`

- [x] **1.4** Implement `CleanerService::cleanSchedule()`:
  - Load schedule config from `SettingManager` by schedule ID
  - Extract category list, min-file-age, and schedule name
  - Call `clean()` with those parameters
  - Write results to cleaning history log (see Phase 5)
  - Return `CleanResult`

- [x] **1.5** Refactor `SystemCleanerPage` to use `CleanerService`:
  - `systemScan()` now calls `CleanerService::ins()->scan(categoriesList)` and stores the result
  - `systemClean()` now calls `CleanerService::ins()->clean(categoriesList, 0)` — min-age=0 for manual cleans (user already reviewed the scan)
  - Remove duplicated I/O logic from `SystemCleanerPage`; UI-only code (tree widget population, checkbox reading) stays
  - Verify all existing behavior is preserved: scan results display, clean results display, loading animations, BUG-02/BUG-05/BUG-10 fixes

- [x] **1.6** Build and verify no regressions

**Acceptance criteria:** Manual System Cleaner scan and clean work identically to before. `CleanerService` can be instantiated and called independently of any QWidget.

---

## Phase 2: `ScheduleManager` — Schedule CRUD and OS Integration

Create a manager class that handles schedule persistence (via `SettingManager`) and OS-native scheduler registration (launchd plists / systemd timers / cron fallback).

**New files:** `shared/nexis/Managers/schedule_manager.h`, `shared/nexis/Managers/schedule_manager.cpp`

- [x] **2.1** Create `ScheduleManager` header with:
  - `struct CleaningSchedule` containing: `id` (QString UUID), `name`, `enabled` (bool), `frequency` (enum: Daily, EveryNDays, Weekly, Monthly), `everyNDays` (int), `dayOfWeek` (int 0=Sun), `dayOfMonth` (int 1–31), `hour` (int 0–23), `minute` (int 0–59), `categories` (QList<int>), `minFileAgeSecs` (int, default 86400), `lastRun` (QDateTime), `lastBytesFreed` (quint64), `dryRunCompleted` (bool)
  - CRUD: `getAllSchedules()`, `getSchedule(id)`, `createSchedule(schedule)`, `updateSchedule(schedule)`, `deleteSchedule(id)`
  - `syncToOS()` — writes/updates/deletes all launchd plists or systemd timers to match current schedule state
  - `getNextRunTime(schedule)` — computes next execution time from schedule parameters
  - Singleton: `static ScheduleManager *ins()`

- [x] **2.2** Extend `SettingManager` for schedule persistence:
  - Add `SettingKeys::Schedules` ("Schedules") — stores JSON array of schedule objects
  - Add `SettingKeys::CleaningNotifications` ("CleaningNotificationsEnabled") — bool, default true
  - Add `SettingKeys::ThresholdAlertEnabled` ("ThresholdAlertEnabled") — bool, default false
  - Add `SettingKeys::ThresholdGB` ("ThresholdGB") — int, default 5
  - Add getter/setter methods for each new key
  - Schedule data is serialized/deserialized via `QJsonDocument`/`QJsonArray`

- [x] **2.3** Implement `ScheduleManager` persistence:
  - `loadSchedules()` — reads `SettingManager::getSchedules()`, deserializes JSON into `QList<CleaningSchedule>`
  - `saveSchedules()` — serializes `mSchedules` to JSON, calls `SettingManager::setSchedules()`
  - Each CRUD method calls `saveSchedules()` and `syncToOS()` to ensure the OS scheduler is always in sync

- [x] **2.4** Implement macOS launchd integration (`#ifdef Q_OS_MACOS`):
  - `createLaunchdPlist(schedule)` — writes `~/Library/LaunchAgents/com.nexis.clean.{id}.plist` with `StartCalendarInterval` matching the schedule's frequency/day/time, `ProgramArguments` pointing to `QCoreApplication::applicationFilePath()` with `--clean {id}`, `RunAtLoad=false`
  - `deleteLaunchdPlist(id)` — calls `launchctl unload` then `QFile::remove()`
  - `syncToOS()` — for each schedule: if enabled, create/update plist and `launchctl load`; if disabled or deleted, unload and remove
  - Use the existing autostart plist pattern in `settings_page.cpp` as reference

- [x] **2.5** Implement Linux systemd integration (`#ifndef Q_OS_MACOS`):
  - `detectScheduler()` — returns enum `Systemd` or `Cron` based on `systemctl --user status` exit code
  - `createSystemdTimer(schedule)` — writes `.timer` and `.service` files to `~/.config/systemd/user/nexis-clean-{id}.{timer,service}`, `OnCalendar` computed from frequency, `Persistent=true`, `Type=oneshot`
  - `deleteSystemdTimer(id)` — calls `systemctl --user disable --now`, then `QFile::remove()` both files
  - `createCronEntry(schedule)` — appends a cron line with `# nexis-schedule:{id}` comment marker for identification
  - `deleteCronEntry(id)` — reads crontab, filters out lines with matching marker, writes back
  - `syncToOS()` — detects scheduler type, delegates to appropriate method

- [x] **2.6** Build and verify

**Acceptance criteria:** Schedules can be created, read, updated, deleted via `ScheduleManager`. Each CRUD operation produces the correct launchd plist / systemd timer / cron entry on the filesystem.

---

## Phase 3: Headless CLI Mode in `main.cpp`

Add `--clean <schedule-id>` and `--check-threshold` command-line flags that run without the GUI.

**Modified file:** `shared/nexis/main.cpp`

- [x] **3.1** Add new CLI options:
  - `--clean <schedule-id>` — runs the specified schedule's clean headlessly
  - `--check-threshold` — runs a scan-only pass and sends a tray notification if cleanable junk exceeds the user's threshold
  - Parse these BEFORE the `QLockFile` check and `App` construction

- [x] **3.2** Implement headless `--clean` branch:
  - Skip the `QLockFile` entirely (the headless process is transient, not a second GUI instance)
  - Initialize core managers only: `SettingManager`, `InfoManager`, `ToolManager`, `CleanerService`, `ScheduleManager`
  - Call `CleanerService::ins()->cleanSchedule(scheduleId)`
  - If `SettingManager::getCleaningNotificationsEnabled()`:
    - Create a `QSystemTrayIcon`, call `showMessage()` with results summary
    - Use `QTimer::singleShot(5000, qApp, &QApplication::quit)` to allow notification to display before exiting
  - If notifications disabled, exit immediately via `return 0`

- [x] **3.3** Implement headless `--check-threshold` branch:
  - Initialize same core managers
  - Call `CleanerService::ins()->scan(allCategories)` to get total cleanable size
  - If `totalSize >= SettingManager::getThresholdGB() * 1073741824` (bytes):
    - Send tray notification: "Nexis found X.X GB of cleanable files. Open Nexis to review."
  - Exit

- [x] **3.4** Implement dry-run first-clean safety:
  - In `CleanerService::cleanSchedule()`, check `schedule.dryRunCompleted`
  - If false: run `scan()` instead of `clean()`, send notification "Your schedule '{name}' would clean X.X GB across N files. The first automatic clean will run next time.", set `dryRunCompleted = true` in the schedule, save via `ScheduleManager`
  - If true: proceed with actual `clean()`

- [x] **3.5** Build and test headless mode:
  - `./build/output/nexis.app/Contents/MacOS/nexis --clean test-id` (expected: runs clean or dry-run, shows notification, exits)
  - `./build/output/nexis.app/Contents/MacOS/nexis --check-threshold` (expected: scans, potentially notifies, exits)

**Acceptance criteria:** `nexis --clean <id>` performs a scheduled clean and sends a notification without showing any GUI. `nexis --check-threshold` scans and conditionally notifies.

---

## Phase 4: Schedule Editor Dialog

Create the UI for creating and editing individual cleaning schedules.

**New files:** `shared/nexis/Pages/SystemCleaner/schedule_editor_dialog.h`, `schedule_editor_dialog.cpp`, `schedule_editor_dialog.ui`

- [x] **4.1** Design the `.ui` file:
  - QDialog, min size ~450×420
  - `txtScheduleName` QLineEdit (default: "Weekly Cleanup")
  - Frequency: QButtonGroup with 4 QRadioButtons (Daily, Every N Days, Weekly, Monthly)
  - `spnEveryNDays` QSpinBox (1–90, visible only when "Every N Days" selected)
  - `cmbDayOfWeek` QComboBox (Sunday–Saturday, visible only when "Weekly" selected)
  - `spnDayOfMonth` QSpinBox (1–31, visible only when "Monthly" selected)
  - `spnHour` QSpinBox (0–23), `spnMinute` QSpinBox (0–59), default 03:00
  - 6 category QCheckBoxes mirroring the System Cleaner page (Package Caches, Crash Reports, App Logs, App Caches, Trash, Dev Tool Caches)
  - `chkSkipRecent` QCheckBox ("Skip files newer than"), `spnMinFileAge` QSpinBox (1–168 hours, default 24)
  - `btnSave` QPushButton, `btnCancel` QPushButton
  - `lblErrorMsg` QLabel (hidden by default)

- [x] **4.2** Implement dialog class:
  - Constructor accepts optional `CleaningSchedule` for edit mode; populates all fields if provided
  - `on_btnSave_clicked()`: validates (name not empty, at least one category checked), builds `CleaningSchedule` struct, emits `scheduleCreated(CleaningSchedule)` or `scheduleUpdated(CleaningSchedule)` signal
  - Frequency radio buttons toggle visibility of conditional widgets (Every N Days spinner, day-of-week combo, day-of-month spinner)
  - Trash category has a warning label: "⚠ Trash is permanently deleted and cannot be recovered"

- [x] **4.3** Add `CMAKE_AUTOUIC_SEARCH_PATHS` entry (if not already covered by the existing SystemCleaner path — it should be, since the dialog `.ui` lives alongside the page `.ui`)

- [x] **4.4** Build and verify dialog opens and saves correctly

**Acceptance criteria:** The schedule editor dialog can create and edit schedules with full parameter control. All frequency options correctly show/hide conditional fields.

---

## Phase 5: Settings Page — Scheduled Cleaning Section

Add a "Scheduled Cleaning" section to the Settings page with schedule list, quick-setup toggle, threshold alert config, and cleaning history access.

**Modified files:** `settings_page.ui` (both platform copies if they differ), `settings_page.h`, `settings_page.cpp`

- [x] **5.1** Add "Scheduled Cleaning" group to Settings page UI:
  - New `QGroupBox` or section header after the existing "Disk Analyzer" row
  - `chkQuickSetup` QCheckBox: "Enable automatic weekly cleaning (recommended)" — one-toggle quick setup
  - `lblQuickSetupSummary` QLabel: shows next scheduled time when enabled
  - `btnManageSchedules` QPushButton: "Manage Schedules..." — opens the schedule manager dialog

- [x] **5.2** Add threshold alert controls:
  - `chkThresholdAlert` QCheckBox: "Notify me when cleanable junk exceeds"
  - `spnThresholdGB` QSpinBox: 1–100 GB, default 5
  - When enabled, `ScheduleManager` creates a daily `--check-threshold` OS schedule

- [x] **5.3** Wire up quick-setup toggle:
  - On enable: `ScheduleManager::ins()->createSchedule(defaultWeeklySchedule)` — Weekly, Sunday, 03:00, all categories except Trash, minAge=24h
  - On disable: `ScheduleManager::ins()->deleteSchedule(quickSetupId)`
  - Update summary label with next run time

- [x] **5.4** Create Schedule Manager dialog (list view):
  - New `QDialog` showing all schedules as a scrollable list of cards
  - Each card shows: enable/disable toggle, name, frequency summary, categories, last run info, Edit button, Delete button
  - "Add Schedule" button at the bottom opens `ScheduleEditorDialog`
  - Edit button opens `ScheduleEditorDialog` pre-populated
  - Delete button confirms then calls `ScheduleManager::ins()->deleteSchedule()`
  - All changes sync to OS immediately via `ScheduleManager`

- [x] **5.5** Add cleaning history log writer to `CleanerService`:
  - Path: `SettingManager::ins()->getConfigPath() + "/clean_history.log"`
  - Format: `[YYYY-MM-DD HH:MM:SS] Schedule: {name} | Categories: {list} | Cleaned: {size} ({count} files) | Duration: {secs}s`
  - Called at the end of `cleanSchedule()`

- [x] **5.6** Add "View Cleaning History" button:
  - Opens a simple `QDialog` with a `QPlainTextEdit` (read-only) displaying the last 50 lines of the log file
  - "Clear History" button at the bottom

- [x] **5.7** Build and verify all Settings interactions

**Acceptance criteria:** Quick-setup toggle creates/removes a default weekly schedule. Schedule list shows all schedules with full CRUD. Threshold alert creates a daily check schedule. Cleaning history is viewable.

---

## Phase 6: System Cleaner Page — Schedule Indicator

Add a subtle schedule indicator panel to the System Cleaner categories page showing the next scheduled clean.

**Modified files:** `system_cleaner_page.ui`, `system_cleaner_page.h`, `system_cleaner_page.cpp`

- [x] **6.1** Add indicator widget to the categories page (page 0 of the stacked widget):
  - Small panel below the category checkboxes, above the Scan button
  - Shows: 📅 icon + "Next: {schedule name} — {day} {time}" + "Last: {date} — cleaned {size}"
  - Hidden when no schedules exist
  - "Manage" link/button that opens the schedule manager dialog (same as Settings)

- [x] **6.2** Connect to `ScheduleManager` to refresh the indicator:
  - On page show / schedule change, query `ScheduleManager::ins()->getAllSchedules()`
  - Find the next upcoming schedule, display its info
  - If no enabled schedules, hide the panel

- [x] **6.3** Build and verify

**Acceptance criteria:** The System Cleaner page shows upcoming schedule info when schedules exist.

---

## Phase 7: Signal Infrastructure and Notifications

Wire up app-wide signals for scheduled clean events and implement tray notifications.

**Modified files:** `signal_mapper.h`, `app.cpp`

- [x] **7.1** Add signals to `SignalMapper`:
  - `sigScheduledCleanStarted(QString scheduleName)`
  - `sigScheduledCleanFinished(quint64 bytesFreed, int fileCount)`

- [x] **7.2** Emit signals from `CleanerService::cleanSchedule()`:
  - Emit `sigScheduledCleanStarted` before cleaning
  - Emit `sigScheduledCleanFinished` after cleaning

- [x] **7.3** Connect in `App` to show tray notification (when app IS running):
  - Listen for `sigScheduledCleanFinished`
  - Show `AppManager::ins()->getTrayIcon()->showMessage(...)` with results summary
  - This handles the case where the app is open and a QTimer-based in-process trigger fires (stretch goal; primary mechanism is OS scheduler launching a separate headless process)

- [x] **7.4** Build and verify

**Acceptance criteria:** Tray notifications appear after scheduled cleans. Signals are available for any UI component to listen to.

---

## Phase 8: Documentation and Tracking

- [x] **8.1** Mark FR-16 as `[x]` in `FEATURE_REQUESTS.md` with resolution summary
- [x] **8.2** Mark all plan tasks `[x]` in this file
- [x] **8.3** Final full build verification on macOS
- [x] **8.4** Commit and push

**Acceptance criteria:** All tracking files updated. Clean build.

---

## File Change Summary

### New Files (7)
| File | Purpose |
|------|---------|
| `shared/nexis/Managers/cleaner_service.h` | Non-UI scan/clean service (extracted from SystemCleanerPage) |
| `shared/nexis/Managers/cleaner_service.cpp` | Implementation |
| `shared/nexis/Managers/schedule_manager.h` | Schedule CRUD + OS scheduler integration |
| `shared/nexis/Managers/schedule_manager.cpp` | Implementation |
| `shared/nexis/Pages/SystemCleaner/schedule_editor_dialog.h` | Schedule create/edit dialog |
| `shared/nexis/Pages/SystemCleaner/schedule_editor_dialog.cpp` | Implementation |
| `shared/nexis/Pages/SystemCleaner/schedule_editor_dialog.ui` | Dialog layout |

### Modified Files (~12)
| File | Changes |
|------|---------|
| `shared/nexis/Pages/SystemCleaner/system_cleaner_page.h` | Remove duplicated I/O logic, add schedule indicator slot |
| `shared/nexis/Pages/SystemCleaner/system_cleaner_page.cpp` | Delegate to CleanerService, add indicator widget |
| `shared/nexis/Pages/SystemCleaner/system_cleaner_page.ui` | Add schedule indicator panel |
| `shared/nexis/Managers/setting_manager.h` | Add schedule/threshold/notification keys and methods |
| `shared/nexis/Managers/setting_manager.cpp` | Implement new getters/setters |
| `shared/nexis/Pages/Settings/settings_page.h` | Add scheduled cleaning section slots |
| `shared/nexis/Pages/Settings/settings_page.cpp` | Wire up new settings section |
| `shared/nexis/Pages/Settings/settings_page.ui` | Add Scheduled Cleaning group |
| `shared/nexis/main.cpp` | Add --clean and --check-threshold headless branches |
| `shared/nexis/signal_mapper.h` | Add scheduled clean signals |
| `shared/nexis/app.cpp` | Connect scheduled clean signals to tray notifications |
| `CMakeLists.txt` | (Only if AUTOUIC path needed — likely no change since SystemCleaner path exists) |

---

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| `sudoExec` can't prompt in headless mode | Headless mode only cleans user-space paths (skip system dirs). Document this limitation. |
| Single-instance lock blocks headless process | Headless mode skips `QLockFile` check (uses different code path before lock acquisition) |
| Cron fallback on non-systemd Linux | Feature-detect systemd; gracefully fall back to cron with clear user messaging |
| Accidental mass deletion | Dry-run on first scheduled execution; 24h minimum-age filter; Trash excluded from default quick-setup |
| Large refactor risk on SystemCleanerPage | Phase 1 is isolated — extract then verify before proceeding. Each subsequent phase builds on verified foundation. |

---

## Task Summary

| Phase | Tasks | Effort | Scope |
|-------|-------|--------|-------|
| 1. Extract CleanerService | 6 | High | Core refactoring |
| 2. ScheduleManager + OS integration | 6 | High | New infrastructure |
| 3. Headless CLI mode | 5 | Medium | main.cpp + CleanerService |
| 4. Schedule Editor Dialog | 4 | Medium | New UI |
| 5. Settings page integration | 7 | Medium | Existing UI extension |
| 6. System Cleaner indicator | 3 | Low | UI polish |
| 7. Signals and notifications | 4 | Low | Infrastructure |
| 8. Documentation and tracking | 4 | Trivial | Housekeeping |
| **Total** | **39** | | |
