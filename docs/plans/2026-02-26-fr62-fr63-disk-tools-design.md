# FR-62 & FR-63: Disk Tools Page Design

**Date:** 2026-02-26
**Features:** FR-62 (Large and Old File Scanner), FR-63 (Duplicate File Finder)

## Summary

A single "Disk Tools" sidebar page under MANAGE (after System Cleaner) combining two modes via a segmented button bar:

1. **Large & Old Files** — finds files exceeding a size threshold or not accessed within a time window
2. **Duplicate Finder** — identifies duplicate files using a 3-stage size → partial-hash → full-hash pipeline

Both modes share a directory picker, results tree with checkable items, and a "Move to Trash" action bar.

## Architecture

### Approach

- **FR-62:** Reuse existing `FileSearchService` / `FileSearchTool` which wraps the system `find` command with size and time filters. Enrich results with `QFileInfo` for display metadata.
- **FR-63:** New `DuplicateFinderService` singleton using `QDirIterator` + `QCryptographicHash` (SHA-256). Custom pipeline since hashing is fundamentally different from `find`.

### New Files

| File | Purpose |
|------|---------|
| `shared/nexis/Pages/DiskTools/disk_tools_page.h` | Page header |
| `shared/nexis/Pages/DiskTools/disk_tools_page.cpp` | Page implementation |
| `shared/nexis/Pages/DiskTools/disk_tools_page.ui` | UI layout |
| `shared/nexis/Services/duplicate_finder_service.h` | Duplicate finder service header |
| `shared/nexis/Services/duplicate_finder_service.cpp` | Duplicate finder service implementation |

### Integration Points

- **Sidebar:** New entry in `app.cpp` `buildSidebar()` MANAGE section, after System Cleaner
- **Page registration:** Added to `mListPages` / `mListSidebarButtons` in `app.cpp`
- **CMakeLists.txt:** Sources, headers, AUTOUIC path, include directory
- **Theme:** All colors from `values.ini` tokens, `refreshThemeColors()` connected to `sigChangedAppTheme`

## FR-62: Large & Old Files Mode

### Filter Controls

- **Size threshold:** QSpinBox + QComboBox unit (MB/GB), default 100 MB
- **Age threshold:** QSpinBox + QComboBox unit (days/months/years), default 180 days not accessed
- **Mode toggle:** "Large files" / "Old files" / "Both" (default: Both — files matching either criterion)

### Scan Flow

1. User configures filters and selects directories
2. Click "Scan" → build `FileSearchParams` per directory with size/time criteria
3. `FileSearchService` runs `find` in QtConcurrent worker
4. On completion, `QFileInfo` each result for size + access/modify times
5. Populate tree: flat list sorted by size descending

### Results Tree Columns

| Column | Content |
|--------|---------|
| Name | File name (basename) |
| Path | Parent directory |
| Size | Human-readable (FormatUtil::formatBytes) |
| Last Accessed | Date/time |
| Last Modified | Date/time |

- Sortable columns (click header)
- Checkbox per row for selective deletion
- Select All / Deselect All buttons

## FR-63: Duplicate Finder Mode

### Filter Controls

- **Minimum file size:** QSpinBox + QComboBox unit, default 1 MB (skips tiny config files)
- **File type filter:** QLineEdit with glob pattern (e.g., `*.jpg`, `*.mp4`) or empty for all files

### 3-Stage Scan Pipeline

**Stage 1 — Size grouping:**
Recursively traverse selected directories with `QDirIterator`. Group files by exact size. Discard groups with only 1 file.

**Stage 2 — Partial hash:**
For each size group, read first 4 KB of each file, compute SHA-256 of that prefix. Sub-group by partial hash. Discard sub-groups with only 1 file.

**Stage 3 — Full hash:**
For remaining candidates, compute full-file SHA-256. Group by full hash. These are confirmed duplicates.

### Progress Reporting

Signals emitted at each stage:
- "Stage 1: Scanning files... 45,230 found"
- "Stage 2: Hashing candidates... 1,204 / 3,500"
- "Stage 3: Verifying... 89 / 204"

### Results Tree (Grouped)

- **Parent items:** Duplicate groups — file count + total wasted space (e.g., "3 duplicates — 450 MB wasted")
- **Child items:** Individual files — path, size, last modified
- **Default selection:** First file in each group unchecked (keep one copy), remaining files pre-checked (suggest deletion)
- User can manually adjust which file to keep

### DuplicateFinderService

```
class DuplicateFinderService : public QObject
    static DuplicateFinderService *ins()

    struct DuplicateGroup {
        QList<QFileInfo> files;
        quint64 fileSize;
        QByteArray hash;
    };

    void scan(QStringList directories, qint64 minSize, QString globFilter)
    void cancel()

signals:
    void progressUpdated(int stage, int current, int total, QString message)
    void scanFinished(QList<DuplicateGroup> results)
```

- Runs in QtConcurrent worker thread
- Cancellable via `QAtomicInt` flag checked between stages and between file hashes
- `waitForFinished()` in destructor

## Shared Components

### Directory Picker Panel

- `QListWidget` showing scan directories
- "Add Directory" button → `QFileDialog::getExistingDirectory()`
- "Remove" button to remove selected entry
- Pre-populated defaults: Home, ~/Downloads, ~/Documents

### Action Bar

- "Move to Trash" button (disabled when nothing selected)
- Selected count + total size label: "3 files selected (2.4 GB)"
- Uses `QFile::moveToTrash()` with confirmation dialog before acting
- After deletion, removed items are cleared from the tree

### Theme Integration

- All colors from `values.ini` tokens (`@cardBg`, `@color03` accent, `@color05` text, etc.)
- `refreshThemeColors()` method connected to `SignalMapper::sigChangedAppTheme`
- Drop shadows via `Utilities::addDropShadow()`

## Sidebar Icon

Reuse or derive from existing Nexis icon set. Candidate: a disk/folder with magnifying glass, consistent with the Search page icon style.

## Deletion Behavior

Move to system trash only (`QFile::moveToTrash()`). Confirmation dialog shows file count and total size before proceeding. No permanent delete option.
