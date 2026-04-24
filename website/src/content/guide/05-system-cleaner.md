---
title: "System Cleaner"
description: "Scan for junk files, review what takes up space, and set up automated cleaning schedules."
order: 5
icon: "trash-2"
---

# System Cleaner

The System Cleaner helps you reclaim disk space by finding and removing files you no longer need -- old package caches, crash dumps, application logs, and more. You can run a scan manually, pick exactly which categories to clean, and even set up automated schedules so cleaning happens in the background without you thinking about it.

![System Cleaner page](/Nexis/images/guide/system-cleaner.png)

## Scan Categories

Nexis scans eight categories of cleanable files. Each category is displayed as a **card** with a title, a description of what it targets, and a size estimate populated after a scan.

| Category | What It Finds |
|----------|---------------|
| **Package Cache** | Downloaded package files from your package manager (APT, DNF, Pacman, or Homebrew) |
| **Crash Reports** | System and application crash dumps |
| **Application Logs** | Log files in `/var/log` and user-level log directories |
| **Application Caches** | Cached data from your installed apps |
| **Trash** | Files sitting in your trash bin that have not been permanently deleted |
| **Dev Tool Caches** | Caches from development tools like npm, cargo, gradle, pip, and Electron apps |
| **Broken Symlinks** | Dead symbolic links that point to files or directories that no longer exist |
| **Browser Privacy** | Browser caches, session data, and recent file history from Chrome, Firefox, Safari, Edge, Brave, and other browsers |

> **Linux:** Application caches are scanned from `~/.cache`. Package caches depend on your distro's package manager. Browser privacy artifacts are scanned from `~/.config/` and `~/.cache/` for Chromium-based browsers, `~/.mozilla/firefox/` for Firefox, and `~/.local/share/recently-used.xbel` for recent file history.

> **macOS:** Application caches are scanned from `~/Library/Caches`. Package caches come from Homebrew's download directory. Browser privacy artifacts are scanned from `~/Library/Caches/` and `~/Library/Application Support/` for each browser, including Safari and the system's recent file list.

## Running a Scan

Click the **Scan** button to search all eight categories. Nexis displays a progress indicator while it works. When the scan completes, each category card updates to show the total size found. A persistent footer at the bottom of the page shows the estimated total recoverable space across all selected categories.

## Reviewing Results

### Selecting Categories

Each category card has a checkbox. Check the categories you want to clean. The footer total updates live as you change your selection. Uncheck any category you want to skip.

For a detailed look at what was found in a specific category, click **View scan results →** to open a file tree showing every file discovered in that category. This is useful when you want to verify the contents before cleaning.

### Scan Trend Sparklines

Below each category checkbox, a small sparkline chart shows how the category's size has trended across your last 20 scans. Hover over the sparkline to see the delta compared to the previous scan (e.g., "+120 MB since last scan"). This helps you spot which categories are actually growing and worth cleaning regularly versus those that stay small.

## Cleaning

Once you are happy with your selection, click **Clean selected** in the footer to delete the files in all checked categories. Nexis removes them permanently -- they do not go to the trash.

> **Tip:** If you are unsure about a category, uncheck it and clean everything else first. You can always run another scan later.

## Scheduled Cleaning

Instead of remembering to clean manually, you can set up automated schedules that run in the background.

### Setting Up a Schedule

Open the schedule manager from the System Cleaner page (or from **Settings > Scheduled Cleaning**). For each schedule, you can configure:

- **Frequency** -- Daily, every N days, weekly, or monthly
- **Categories** -- Which of the eight scan categories to include, plus **Old Downloads** (see below)
- **Minimum file age** -- Only clean files older than a certain number of days
- **Threshold alerts** -- Get a tray notification when cleanable files exceed a size threshold (e.g., alert me when there is more than 5 GB to clean)

The schedule indicator on the System Cleaner page shows when the next automated clean will run and when the last one completed.

> **Tip:** Browser Privacy is unchecked by default in new schedules. Since browser cleaning can remove active session data, enable it only if you are comfortable clearing caches and session files on a schedule.

### Old Downloads

Schedules can include an **Old Downloads** category that moves files older than a configurable age from a configurable folder (default: your platform's Downloads directory) to the Trash -- recoverable, not permanently deleted. The age threshold and folder path are configured under **Settings > Scheduled Cleaning**.

### Pre-Clean Snapshots

Before any clean runs -- manual or scheduled -- Nexis can automatically create a system restore point. Enable **Create restore point before cleaning** under **Settings > Scheduled Cleaning**:

> **Linux:** Creates a Timeshift snapshot via `pkexec timeshift --create`. The toggle is hidden if Timeshift is not installed.

> **macOS:** Creates a local Time Machine snapshot via `tmutil localsnapshot`. No elevation prompt is needed.

If snapshot creation fails, the clean proceeds anyway -- the snapshot is a safety net, not a prerequisite.

### How Scheduling Works

Nexis uses your operating system's native task scheduler to trigger cleans at the right time:

> **macOS:** Schedules are stored as `launchd` plist files in `~/Library/LaunchAgents/`.

> **Linux:** Schedules use systemd timers or cron entries, whichever your system supports.

### Headless CLI Mode

Scheduled cleans run without opening the GUI. Nexis supports two command-line flags for this:

```bash
# Run a specific cleaning schedule
./nexis --clean <schedule-id>

# Check if any threshold alerts should fire
./nexis --check-threshold
```

This means cleaning can happen even if you are not logged into a desktop session, which is useful for servers and headless setups.

## What's Next

Find large files and duplicates with the built-in [Disk Tools](./18-disk-tools), or search for files across your filesystem in the [Search](./06-search) guide.
