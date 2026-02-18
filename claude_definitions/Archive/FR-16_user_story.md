# FR-16: Scheduled / Automated Cleaning — User Story

**Feature Request:** FR-16
**Issue:** [#449](https://github.com/oguzhaninan/Stacer/issues/449)
**Date:** February 2026

---

## 1. User Story

**As a** Nexis user who accumulates gigabytes of system junk, package caches, and developer tool caches over time,
**I want** to configure automatic cleaning schedules so that my system stays optimized without manual intervention,
**so that** I can reclaim disk space consistently, avoid performance degradation from cache bloat, and spend less time on routine maintenance.

---

## 2. Background & Competitive Context

Scheduled/automated cleaning is a **table-stakes feature** among system optimizers. Both CleanMyMac and CCleaner offer variants of this capability, though they approach it very differently. Understanding their implementations informs what Nexis should build — and where Nexis can do better.

### 2.1 CleanMyMac X Approach

CleanMyMac X's consumer product does **not** auto-clean. It uses a **reminder-based model**:

- A background "Health Monitor" daemon continuously tracks CPU, RAM, disk space, battery, and junk accumulation.
- When junk exceeds a user-configurable threshold, a macOS notification nudges the user to open the app and run a scan.
- Trash size thresholds trigger separate alerts.
- Users must still click "Scan" then "Clean" manually. The consumer product never deletes files autonomously.
- Background malware scanning runs independently but is detection-only — removal requires manual confirmation.
- Maintenance scripts (flush DNS, reindex Spotlight, repair permissions, free RAM) are entirely manual with no scheduling option.

CleanMyMac Business (enterprise tier) is the only product with true scheduled automation:
- Admins set a start date/time and repeat interval (7, 14, or 30 days — these are the only options).
- A forced-execution fallback runs the task if the user hasn't acted on pre-execution reminders (0, 1, or 2 reminders configurable).
- Schedule parameters are immutable after creation — changing them requires deleting and recreating the task.

**Key insight:** CleanMyMac deliberately avoids autonomous deletion in its consumer product. The reminder model respects user agency but adds friction. Their Safety Database (a curated whitelist of protected files) mitigates risk but is proprietary and opaque.

### 2.2 CCleaner Approach

CCleaner offers **two distinct automation systems** (Professional tier only):

**Scheduled Cleaning** (calendar-driven):
- Intervals: daily, weekly, every 2 weeks, at startup, monthly.
- Users choose day-of-week and time-of-day.
- Runs the user's saved "Custom Clean" profile or the one-click "Health Check" profile.
- Executes silently via Windows Task Scheduler (the app runs with a `/auto` flag, no UI shown).
- Post-clean notification appears above the system tray showing results.

**Smart Cleaning** (event/threshold-driven):
- A persistent background agent monitors junk accumulation in real time.
- When junk exceeds a threshold (~500MB–1GB, adjustable in Pro), a system tray popup offers "Click here to clean."
- Browser monitoring sub-feature: automatically cleans browser data when browsers close.
- Pro users can enable fully silent auto-cleaning (no popup, just cleans when threshold is hit).

**Key insight:** CCleaner's dual model (calendar + threshold) gives users flexibility, but the background agent is resource-intensive and users frequently complain about it. The v7 rewrite temporarily removed Smart Cleaning entirely, causing significant user backlash — proving the feature is valued but the implementation was fragile.

### 2.3 What Neither Competitor Does Well

| Gap | Description | Nexis Opportunity |
|-----|-------------|-------------------|
| **Developer cache awareness** | Neither tool understands dev tool caches (node_modules, .gradle, .cargo, etc.) | Nexis already scans 8+ dev cache types; scheduling these cleans is unique |
| **Cross-platform scheduling** | CCleaner uses Windows Task Scheduler; CleanMyMac uses proprietary daemon | Nexis can use launchd (macOS) + systemd timers (Linux) natively |
| **Transparency** | Both tools use opaque "safety databases" or hidden rules | Nexis can show exactly what will be cleaned before and after, with full audit trail |
| **Exclusion UX** | CCleaner's exclude is buried in Options; CleanMyMac requires right-click during scan | Nexis can offer first-class exclusion rules integrated into the scheduling UI |
| **Lightweight operation** | CCleaner's background agent is resource-heavy; CleanMyMac's daemon is always-on | Nexis can use OS-native scheduling (launchd/systemd) — zero background resource cost |

---

## 3. Personas

### 3.1 Developer Dana
- **Profile:** Full-stack developer, uses macOS and Linux daily. Has 15+ Node projects, several Rust crates, and a few Gradle-based Android projects.
- **Pain point:** `node_modules`, `.gradle/caches`, `.cargo/registry`, and Homebrew caches routinely consume 20–40GB. Manually running the System Cleaner every few weeks is tedious.
- **Need:** A weekly automatic clean of dev tool caches and package caches, with a notification showing how much space was recovered. Wants to exclude active project caches.

### 3.2 Sysadmin Sam
- **Profile:** Manages a fleet of 20 Linux workstations in a small IT department. Nexis is installed on all of them.
- **Pain point:** Log files and crash reports fill up `/var/log` and cause disk alerts. Currently uses cron scripts but wants a GUI solution non-technical staff can configure.
- **Need:** Scheduled cleaning of application logs and crash reports every 3 days. Wants it to run silently without user interaction. Needs confidence that critical logs won't be deleted.

### 3.3 Casual User Casey
- **Profile:** macOS user, moderate technical comfort. Uses their Mac for web browsing, email, and light creative work.
- **Pain point:** "My Mac is slow and the disk is full" — doesn't know what to clean or how often. Currently ignores the problem until forced to deal with it.
- **Need:** A simple "set and forget" option — ideally one toggle that enables sensible defaults. Wants a notification when cleaning happens so they know the app is working. Doesn't want to think about categories or exclusions.

### 3.4 Privacy-Conscious Pat
- **Profile:** Security researcher, uses Linux. Chose Nexis specifically because it's open-source.
- **Pain point:** Wants browser caches and application logs cleaned frequently for privacy, but doesn't trust proprietary tools.
- **Need:** Fine-grained control over what gets cleaned and when. Wants to review the first scheduled clean before trusting it to run unattended. Values transparency and audit logs over convenience.

---

## 4. Functional Requirements

### 4.1 Schedule Configuration

| ID | Requirement | Priority |
|----|-------------|----------|
| SC-01 | User can create one or more named cleaning schedules | Must |
| SC-02 | Each schedule has a frequency: daily, every N days (1–90), weekly (pick day), monthly (pick day-of-month) | Must |
| SC-03 | Each schedule has a preferred time-of-day (hour picker, 24h format) | Must |
| SC-04 | Each schedule selects one or more cleaning categories (same 6 as manual clean: Package Cache, Crash Reports, App Logs, App Caches, Trash, Dev Tool Caches) | Must |
| SC-05 | Schedules are persisted across app restarts via `SettingManager` / QSettings | Must |
| SC-06 | User can enable/disable individual schedules without deleting them | Must |
| SC-07 | User can edit all parameters of an existing schedule (unlike CleanMyMac Business, which locks schedule parameters after creation) | Must |
| SC-08 | User can delete a schedule | Must |

### 4.2 Execution

| ID | Requirement | Priority |
|----|-------------|----------|
| EX-01 | Scheduled cleans run via OS-native mechanisms: launchd (macOS) / systemd timers (Linux) — NOT an in-app QTimer | Must |
| EX-02 | Scheduled cleans execute without the GUI being open (headless mode via `--clean` CLI flag) | Must |
| EX-03 | If the app IS open when a scheduled clean triggers, run it in-process and update the UI | Should |
| EX-04 | If the system was asleep/off at the scheduled time, run the clean on next wake/boot (launchd `StartCalendarInterval` handles this natively; systemd `Persistent=true`) | Must |
| EX-05 | Cleaning logic is extracted from `SystemCleanerPage` into a reusable, non-UI `CleanerService` class | Must |
| EX-06 | Only files matching the selected categories are cleaned — no scope creep beyond what the user configured | Must |

### 4.3 Exclusion Rules (ties into FR-18)

| ID | Requirement | Priority |
|----|-------------|----------|
| XR-01 | User can define exclusion rules: specific files, directories, or glob patterns (e.g., `~/Library/Caches/my-critical-app/**`) | Must |
| XR-02 | Exclusion rules apply to both manual and scheduled cleans | Must |
| XR-03 | Exclusion rules are configurable per-schedule OR globally | Should |
| XR-04 | Default exclusions protect known-critical directories (e.g., active Homebrew downloads, currently-running process caches) | Should |

### 4.4 Notifications

| ID | Requirement | Priority |
|----|-------------|----------|
| NT-01 | After a scheduled clean completes, show a system tray notification with: categories cleaned, total space freed, number of files removed | Must |
| NT-02 | If the app is open, also display results in a non-modal banner or the dashboard | Should |
| NT-03 | User can disable post-clean notifications in Settings | Must |
| NT-04 | Notification is clickable — opens Nexis to a cleaning summary/log view | Should |

### 4.5 Threshold-Based Alerts (Inspired by CCleaner Smart Cleaning)

| ID | Requirement | Priority |
|----|-------------|----------|
| TH-01 | User can set a junk accumulation threshold (e.g., "notify me when cleanable junk exceeds 5GB") | Should |
| TH-02 | Threshold check runs periodically (e.g., daily) via the same OS-native scheduling mechanism | Should |
| TH-03 | When threshold is exceeded, show a tray notification: "Nexis found X GB of cleanable files. Click to review." | Should |
| TH-04 | Threshold alerts are notification-only — they do NOT auto-clean (following CleanMyMac's consumer model of respecting user agency) | Must |
| TH-05 | User can optionally enable auto-clean on threshold (advanced setting, disabled by default) | Could |

### 4.6 Safety & Transparency

| ID | Requirement | Priority |
|----|-------------|----------|
| SF-01 | First scheduled clean for a new schedule runs a dry-run scan and shows a preview notification: "Scheduled clean would remove X files (Y GB). Approve to enable automatic cleaning." | Must |
| SF-02 | Cleaning log is written to `~/.config/nexis/clean_history.log` (Linux) or `~/Library/Application Support/Nexis/clean_history.log` (macOS) with timestamp, categories, files removed, bytes freed | Must |
| SF-03 | Log history is viewable from within the app (new "Cleaning History" section or tab) | Should |
| SF-04 | Maximum age policy: never delete files newer than N hours (configurable, default 24h) — mirrors CCleaner's "only delete files older than 24 hours" safety net | Should |
| SF-05 | Directories are emptied, not deleted (preserve existing BUG-02 fix behavior) | Must |

### 4.7 Settings Integration

| ID | Requirement | Priority |
|----|-------------|----------|
| ST-01 | New "Scheduled Cleaning" section in Settings page, or a dedicated sub-page accessible from System Cleaner | Must |
| ST-02 | Quick-setup option: "Enable weekly cleaning with recommended settings" — one toggle for users who don't want to configure details (serves Casual User Casey persona) | Should |
| ST-03 | All schedule settings stored in existing `settings.ini` via `SettingManager` | Must |
| ST-04 | Schedule state synced to OS scheduling mechanisms on every settings change (create/update/delete launchd plist or systemd timer) | Must |

---

## 5. Non-Functional Requirements

| ID | Requirement |
|----|-------------|
| NF-01 | **Zero background resource cost** when using OS-native scheduling. No persistent daemon, no polling QTimer, no background agent. This is a key differentiator vs. CCleaner's resource-heavy approach. |
| NF-02 | **Headless clean completes in < 60 seconds** for typical workloads (< 10,000 files). |
| NF-03 | **Schedule creation/modification takes effect immediately** — no app restart required. |
| NF-04 | **Graceful degradation**: if launchd/systemd is unavailable (e.g., non-systemd Linux), fall back to in-app QTimer scheduling (requires app to be running). |
| NF-05 | **No root/sudo required for scheduling** — launchd user agents and systemd user timers run in user space. Cleaning itself may require elevation for some paths (e.g., `/var/log`), handled by existing `CommandUtil::sudoExec` pattern. |

---

## 6. User Flows

### 6.1 Creating a New Schedule

```
1. User navigates to System Cleaner page (or Settings > Scheduled Cleaning)
2. User clicks "Add Schedule" button
3. Dialog appears with:
   a. Schedule name (text field, default: "Weekly Cleanup")
   b. Frequency selector (Daily / Every N days / Weekly / Monthly)
   c. Day-of-week or day-of-month picker (contextual)
   d. Time-of-day picker (hour, defaults to 03:00)
   e. Category checkboxes (all 6, mirroring manual scan UI)
   f. "Manage Exclusions" link/button (opens exclusion rule editor)
4. User configures and clicks "Save"
5. Nexis writes schedule to settings.ini
6. Nexis creates/updates launchd plist or systemd timer unit
7. First-run dry preview: Nexis immediately runs a scan (not clean) for selected
   categories and shows a tray notification:
   "Your new schedule 'Weekly Cleanup' would clean 3.2 GB across 847 files.
    The first automatic clean will run on [next scheduled date]."
8. Schedule appears in the schedule list as "Enabled"
```

### 6.2 Scheduled Clean Executes (App Closed)

```
1. OS scheduler (launchd/systemd) triggers at configured time
2. Nexis launches in headless mode: `nexis --clean --schedule "Weekly Cleanup"`
3. CleanerService loads schedule config from settings.ini
4. CleanerService runs scan for configured categories
5. CleanerService applies exclusion rules, filters out protected files
6. CleanerService applies minimum-age filter (skip files < 24h old)
7. CleanerService deletes matching files/empties matching directories
8. CleanerService writes results to clean_history.log
9. CleanerService sends OS notification via tray icon or native notification API:
   "Nexis cleaned 2.1 GB (612 files) — Package Caches, Dev Tool Caches, Trash"
10. Nexis process exits
```

### 6.3 Scheduled Clean Executes (App Open)

```
1. OS scheduler triggers; Nexis detects it's already running (single-instance)
2. Instead of launching a new process, the running instance receives the
   schedule trigger (via IPC — named pipe, D-Bus, or file-based signal)
3. CleanerService runs the clean in a background thread
4. Dashboard shows a brief inline banner: "Scheduled clean running..."
5. On completion, banner updates: "Cleaned 2.1 GB — View Details"
6. Tray notification also fires (unless disabled)
7. If the user is currently on the System Cleaner page, results are NOT
   auto-applied to the tree (to avoid disrupting manual work). Instead,
   a subtle indicator appears: "Scheduled clean completed. Re-scan to see
   updated results."
```

### 6.4 Threshold Alert Flow

```
1. OS scheduler runs daily threshold check: `nexis --check-threshold`
2. CleanerService runs a scan-only pass (no deletion) for all categories
3. If total cleanable size > user's threshold (e.g., 5 GB):
   a. Send tray notification: "Nexis found 7.3 GB of cleanable files. Click to review."
   b. Clicking notification opens Nexis to the System Cleaner page with
      scan results pre-populated
4. If total cleanable size < threshold: exit silently, no notification
```

### 6.5 Quick Setup Flow (One-Toggle)

```
1. User opens Settings page
2. User sees "Scheduled Cleaning" section with a prominent toggle:
   "Enable automatic weekly cleaning (recommended settings)"
3. User enables toggle
4. Nexis creates a default schedule:
   - Name: "Weekly Auto-Clean"
   - Frequency: Weekly, Sunday
   - Time: 03:00
   - Categories: All except Trash (Trash is excluded by default to prevent
     accidental data loss — user can add it manually)
   - Exclusions: Default safe exclusions
5. First-run dry preview fires (same as flow 6.1, step 7)
6. Toggle shows summary: "Next clean: Sunday 3:00 AM — All categories except Trash"
7. "Customize..." link opens the full schedule editor pre-populated with these settings
```

---

## 7. UI Wireframe (Text-Based)

### 7.1 System Cleaner Page — Schedule Indicator

```
┌─────────────────────────────────────────────────────────────┐
│  System Cleaner                                    [⏰ Schedules (2)] │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ☐ Package Caches    ☐ Crash Reports    ☐ Application Logs  │
│  ☐ Application Caches  ☐ Trash          ☐ Dev Tool Caches   │
│                                                             │
│  ☐ Select All                              [ 🔍 Scan ]     │
│                                                             │
│  ┌─ Next Scheduled Clean ──────────────────────────────┐   │
│  │  📅 "Weekly Cleanup" — Sunday 3:00 AM               │   │
│  │  Categories: Package Caches, App Caches, Dev Tools   │   │
│  │  Last run: Feb 14 — cleaned 2.1 GB                  │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### 7.2 Schedule Manager Dialog

```
┌─────────────── Scheduled Cleaning ────────────────────────┐
│                                                            │
│  ┌────────────────────────────────────────────────────┐   │
│  │ ✅ Weekly Cleanup          Weekly, Sun 3:00 AM      │   │
│  │    Package Caches, App Caches, Dev Tool Caches      │   │
│  │    Last: Feb 14 (2.1 GB)    Next: Feb 21            │   │
│  │                                    [ Edit ] [ ✕ ]   │   │
│  ├────────────────────────────────────────────────────┤   │
│  │ ✅ Daily Logs Cleanup       Daily, 4:00 AM          │   │
│  │    Crash Reports, Application Logs                  │   │
│  │    Last: Feb 16 (340 MB)    Next: Feb 17            │   │
│  │                                    [ Edit ] [ ✕ ]   │   │
│  └────────────────────────────────────────────────────┘   │
│                                                            │
│                                    [ + Add Schedule ]      │
│                                                            │
│  ─── Threshold Alert ─────────────────────────────────    │
│  ☐ Notify me when cleanable junk exceeds [ 5 ] GB        │
│                                                            │
│  ─── History ─────────────────────────────────────────    │
│  [ View Cleaning History ]                                 │
│                                                            │
└───────────────────────────────────────────────────────────┘
```

### 7.3 Schedule Editor Dialog

```
┌──────────────── Edit Schedule ────────────────────────────┐
│                                                            │
│  Name:  [ Weekly Cleanup                              ]   │
│                                                            │
│  Frequency:  ( ) Daily                                     │
│              ( ) Every [ 3 ] days                          │
│              (•) Weekly    Day: [ Sunday      ▾ ]         │
│              ( ) Monthly   Day: [ 1st        ▾ ]          │
│                                                            │
│  Time:  [ 03 ] : [ 00 ]                                  │
│                                                            │
│  Categories:                                               │
│  ☑ Package Caches       ☐ Crash Reports                   │
│  ☐ Application Logs     ☑ Application Caches              │
│  ☐ Trash                ☑ Dev Tool Caches                 │
│                                                            │
│  Safety:                                                   │
│  ☑ Skip files newer than [ 24 ] hours                    │
│                                                            │
│  [ Manage Exclusion Rules... ]                             │
│                                                            │
│                          [ Cancel ]  [ Save Schedule ]    │
└───────────────────────────────────────────────────────────┘
```

---

## 8. Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC-01 | User can create, edit, enable/disable, and delete cleaning schedules from the UI |
| AC-02 | Schedules persist across app restarts and are written to OS-native scheduling (launchd plist on macOS, systemd timer on Linux) |
| AC-03 | A scheduled clean runs at the configured time WITHOUT the Nexis GUI being open |
| AC-04 | A scheduled clean only cleans the categories selected in that schedule |
| AC-05 | Exclusion rules prevent specified files/directories from being cleaned |
| AC-06 | A system tray notification appears after each scheduled clean showing space freed |
| AC-07 | A cleaning history log is written and viewable from the app |
| AC-08 | The first run of a new schedule performs a dry-run preview, not an actual clean |
| AC-09 | Files newer than the minimum-age threshold (default 24h) are not deleted |
| AC-10 | If the system was asleep at the scheduled time, the clean runs on next wake |
| AC-11 | The quick-setup toggle in Settings enables a sensible default schedule with one click |
| AC-12 | Disabling or deleting a schedule removes the corresponding launchd plist / systemd timer |
| AC-13 | The app builds and runs correctly on both macOS and Linux after implementation |

---

## 9. Technical Architecture Notes

### 9.1 Key Refactoring Required

The current `SystemCleanerPage` is monolithic — scan logic, clean logic, and UI are tightly coupled. Scheduled cleaning requires:

1. **Extract `CleanerService`** — a non-QWidget class containing:
   - `scan(QList<CleanCategory> categories)` → returns scan results
   - `clean(QList<CleanCategory> categories, ExclusionRules rules)` → performs deletion
   - `dryRun(QList<CleanCategory> categories)` → returns what would be cleaned
   - Signal: `cleanCompleted(CleanResult result)` with bytes freed, file count, per-category breakdown

2. **Headless entry point** — `main.cpp` detects `--clean` / `--check-threshold` flags and runs `CleanerService` without creating the GUI window.

3. **OS scheduler integration**:
   - macOS: Write/update/delete `~/Library/LaunchAgents/com.nexis.clean.{schedule-id}.plist` with `StartCalendarInterval`
   - Linux: Write/update/delete `~/.config/systemd/user/nexis-clean-{schedule-id}.timer` and `.service` units

4. **SettingManager additions**: New QSettings keys for schedule data (JSON array or indexed key groups).

### 9.2 Nexis Advantages Over Competitors' Approaches

| Aspect | CCleaner | CleanMyMac | Nexis (Proposed) |
|--------|----------|------------|------------------|
| Scheduling mechanism | Windows Task Scheduler + background agent | Proprietary daemon (consumer: reminder only) | OS-native: launchd / systemd (zero idle cost) |
| Resource usage when idle | High (persistent background process) | Medium (Health Monitor daemon) | None (OS handles scheduling) |
| Headless operation | Yes (`/auto` flag) | No (consumer requires GUI interaction) | Yes (`--clean` flag) |
| First-run safety | None — cleans immediately | N/A (doesn't auto-clean) | Dry-run preview before first real clean |
| Audit trail | Minimal (post-clean popup) | Cumulative "space freed" stat | Full per-clean log with file-level detail |
| Dev cache awareness | None | None | Full (8+ cache types with per-project granularity) |
| Cross-platform | Windows only | macOS only | Linux + macOS with native OS integration on both |

---

## 10. Open Questions

| # | Question | Impact |
|---|----------|--------|
| OQ-01 | Should we support multiple schedules or just one? Multiple adds complexity but serves power users (e.g., daily log clean + weekly full clean). | UX complexity |
| OQ-02 | Should the threshold alert feature be part of FR-16 or a separate feature request? | Scope |
| OQ-03 | For the headless `--clean` mode, should Nexis send notifications via `QSystemTrayIcon` (requires brief app launch) or native `notify-send` (Linux) / `osascript` (macOS)? | Implementation |
| OQ-04 | Should the cleaning history log use a structured format (JSON/SQLite) or plain text? Structured enables future analytics. | Technical debt |
| OQ-05 | FR-18 (exclusion rules) is a dependency. Should it be implemented as part of FR-16 or separately first? | Sequencing |
| OQ-06 | On non-systemd Linux (e.g., Alpine, Void, Devuan), should we fall back to cron, an in-app QTimer, or simply not support scheduling? | Platform coverage |
