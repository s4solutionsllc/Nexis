---
title: "Disk Tools"
description: "Find large and old files, detect duplicates, and reclaim disk space with built-in scanning tools."
order: 5.5
icon: "hard-drive"
---

# Disk Tools

The Disk Tools page gives you two powerful scanning modes for reclaiming disk space without leaving Nexis. Instead of launching an external disk analyzer, you can find oversized files, forgotten downloads, and duplicate copies right here — review the results, and move what you don't need to the trash.

A mode selector at the top of the page lets you switch between **Large & Old Files** and **Duplicate Finder**.

## Large & Old Files

This mode scans a directory tree for files that are unusually large, haven't been accessed in a long time, or both. It's the fastest way to answer "what's eating my disk?" without installing a separate tool.

### Setting Up a Scan

Before scanning, configure three controls:

**Directory** — Choose the root directory to search. Click the browse button to pick a folder, or type a path directly. The scan covers everything inside this directory recursively.

**Size threshold** — Set the minimum file size to include in results. Files smaller than this are ignored. For example, setting it to 100 MB limits results to files 100 MB and larger.

**Age threshold** — Set the minimum number of days since a file was last accessed. Files accessed more recently than this are excluded. For example, 90 days means only files untouched for three months or longer appear.

**Filter mode** — Choose how the two thresholds combine:

| Mode | Behavior |
|------|----------|
| **Either** | A file appears if it exceeds the size threshold *or* the age threshold (or both) |
| **Large only** | Only the size threshold is applied; age is ignored |
| **Old only** | Only the age threshold is applied; size is ignored |

### Running the Scan

Click **Scan** to start. A progress indicator shows the scan is running. You can click **Cancel** at any time to stop the scan early — results found so far are kept.

> **Tip:** Scanning your entire home directory is usually fast (under a minute). Scanning from `/` takes longer because it includes system directories. Start with your home folder for the quickest wins.

### Reviewing Results

Results appear in a sortable tree with columns for file path, size, and last accessed date. Click any column header to sort.

- **Sort by size** (largest first) to find the biggest space consumers.
- **Sort by date** (oldest first) to find long-forgotten files.

Each row has a checkbox. Check the files you want to remove, or use the header checkbox to select all.

### Deleting Files

Click **Move to Trash** to send the checked files to your system's trash. This is a safe operation — files are not permanently deleted and can be recovered from the trash if needed.

A confirmation dialog shows how many files will be moved and their total size before proceeding.

---

## Duplicate Finder

This mode identifies files that have identical content, even if their names differ. Duplicates waste disk space and can accumulate over time from backups, downloads, and file reorganization.

### How It Works

The Duplicate Finder uses a three-stage pipeline to efficiently detect duplicates without reading every byte of every file:

1. **Size grouping** — Files are grouped by size. Files with a unique size cannot be duplicates, so they are eliminated immediately. This stage is very fast.
2. **Partial hash** — For each size group, Nexis reads the first 4 KB of each file and computes a SHA-256 hash. Files with unique partial hashes are eliminated. This catches most non-duplicates without reading full files.
3. **Full hash** — For remaining candidates, the entire file is hashed with SHA-256. Only files with identical full hashes are confirmed as duplicates.

This approach is fast even on large directories because most files are eliminated in stages 1 and 2 without ever reading their full contents.

### Running a Scan

Choose a directory and click **Scan**. A progress bar shows which stage is active and how many files have been processed. Like the Large & Old Files mode, you can cancel at any time.

### Reviewing Results

Duplicates are displayed in a grouped tree. Each group represents a set of identical files — expand a group to see every copy. The group header shows the file size and how many copies exist.

Within each group:

- The **first file** is unchecked by default — this is the copy Nexis suggests you keep.
- **All other copies** are pre-checked, suggesting they are the ones to remove.

You can adjust the checkboxes however you like. For example, if the "keeper" is in the wrong location, uncheck it and check a different copy instead.

> **Tip:** Look at the file paths to decide which copy to keep. A file in `~/Documents/` is probably the "real" one, while a copy in `~/Downloads/` or a backup folder is likely the duplicate.

### Deleting Duplicates

Click **Move to Trash** to send the checked files to the trash. A confirmation dialog shows the number of files and total space that will be freed.

> **Tip:** Start with your Downloads folder — it's a common source of duplicate files from repeated downloads. Then expand to your home directory for a more thorough sweep.

## What's Next

Learn how to find files across your filesystem in the [Search](./06-search) guide.
