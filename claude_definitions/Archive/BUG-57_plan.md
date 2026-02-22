# BUG-57 Plan: Filter disk selector to only show real volumes

## Approach

Add a static helper `shouldIncludeDisk()` in `disk_info_shared.cpp` that checks filesystem type, device path, mount path, and size. Apply it in `updateDiskInfo()`, `devices()`, and `fileSystemTypes()`.

## Tasks

### Task 1: Add filtering logic to `disk_info_shared.cpp`

- [x] **File:** `shared/nexis-core/Info/disk_info_shared.cpp`

Add a static `shouldIncludeDisk(const QStorageInfo &info)` function with these filters:

1. **Size check:** Skip if `bytesTotal() == 0`
2. **Filesystem type exclusion:** Skip if `fileSystemType()` is in:
   `tmpfs, devtmpfs, devfs, sysfs, procfs, cgroup, cgroup2, squashfs, overlay, fuse.snapfuse, autofs, nullfs, fdescfs, linprocfs, linsysfs, map`
3. **Device path filters:**
   - Skip if device starts with `/dev/loop` (Snap loopbacks on Linux)
   - Skip if device is literally `tmpfs`, `devtmpfs`, `overlay`, `none`, or empty
4. **Mount path filters:**
   - Skip if rootPath starts with `/snap/` (Snap package mounts)
   - Skip if rootPath is exactly `/dev`, `/proc`, `/sys`, `/run`
5. **macOS system volume exclusion:**
   - Skip if rootPath starts with `/System/Volumes/Preboot`, `/System/Volumes/Recovery`, `/System/Volumes/VM`, `/System/Volumes/Update`

Apply `shouldIncludeDisk()` in:
- `updateDiskInfo()` — the main loop
- `devices()` — the device list helper
- `fileSystemTypes()` — the FS type helper

### Task 2: Build and verify

- [x] Run incremental build to confirm no compilation errors.

### Task 3: Update tracking and docs

- [x] Mark BUG-57 as `[x]` in BUGS.md with resolution notes.
- [x] Update `docs/APPLICATION_OVERVIEW.md` if the disk tile description mentions showing "all mounted volumes".

## Acceptance Criteria

- On macOS: only physical volumes appear (e.g., `Macintosh HD`, `Macintosh HD - Data`, external drives). No `Preboot`, `Recovery`, `VM`, or mounted DMGs.
- On Linux: only real partitions appear (e.g., `/dev/sda1`, `/dev/nvme0n1p2`). No `/dev/loop*`, `tmpfs`, `overlay`, or `squashfs`.
- Settings page disk combobox is also filtered (same data source).
- External drives and USB sticks still appear correctly.
