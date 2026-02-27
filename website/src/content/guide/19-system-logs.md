---
title: "System Logs"
description: "Browse and filter system log entries with severity color-coding and platform-native backends."
order: 13.5
icon: "file-text"
---

# System Logs

The System Logs page is a read-only log viewer that lets you browse your operating system's log stream without opening a terminal. Entries are displayed in a color-coded table with filtering controls so you can quickly narrow down to the messages that matter.

## Viewing Logs

When you navigate to the System Logs page, Nexis queries your system's log backend and populates a table with recent entries. Each row shows:

| Column | Description |
|--------|-------------|
| **Timestamp** | When the log entry was recorded |
| **Severity** | The log level (e.g., Emergency, Error, Warning, Notice, Info, Debug) |
| **Source** | The process, service, or subsystem that generated the entry |
| **Message** | The log message text |

Rows are color-coded by severity so critical entries stand out visually:

| Severity | Color |
|----------|-------|
| Emergency / Critical / Alert | Red |
| Error | Orange-red |
| Warning | Yellow |
| Notice / Info | Default text color |
| Debug | Dimmed / muted |

## Filtering

Two filter controls sit above the table:

### Severity Filter

Select a minimum severity level to display. For example, choosing **Warning** shows only Warning, Error, Critical, Alert, and Emergency entries — hiding the high-volume Info and Debug messages. This is the fastest way to cut through noise and find problems.

### Text Search

Type a keyword or phrase to filter entries whose message or source contains the search text. This works in combination with the severity filter, so you can search for "disk" while showing only errors, for example.

> **Tip:** Combine both filters for targeted investigation. Set severity to Error, then search for the name of a service you are troubleshooting. This shows only error-level messages from that specific service.

## Platform Differences

Nexis uses each platform's native log system, so the data source and available detail differ:

### Linux (journalctl)

On Linux systems with systemd, Nexis reads logs from the systemd journal using `journalctl --output=json`. This provides structured data including:

- Precise timestamps with microsecond resolution
- The systemd unit (service) that generated each entry
- Standard syslog severity levels

> **Note:** Some log entries from system services may require elevated privileges to read. If you notice missing entries, try running Nexis as root or adding your user to the `systemd-journal` group.

### macOS (log show)

On macOS, Nexis queries the unified logging system using `log show` with predicate-based filtering. This provides:

- Subsystem and category labels for each entry
- Standard os_log severity levels
- Entries from system services, applications, and kernel

> **Note:** macOS's unified log can be very verbose. The severity filter is especially useful here to focus on meaningful entries.

## Common Use Cases

### Diagnosing a Service Failure

Set the severity filter to **Error** and search for the name of the failing service. The filtered results show you exactly what went wrong and when.

### Reviewing Boot Messages

After a restart, check the logs to see if any services failed to start or reported errors during boot. Filter by severity to skip the routine startup messages.

### Monitoring Disk or Hardware Events

Search for keywords like "disk", "SMART", "thermal", or "battery" to find hardware-related log entries. These can provide early warning of developing issues.

## What's Next

If you are running GNOME on Linux, see how Nexis can help you tweak your desktop in [GNOME Settings](./14-gnome-settings). Otherwise, skip ahead to [Settings](./15-settings) to configure Nexis itself.
