# BUG-57 Research: Disk selector shows virtual filesystems, snapshots, and loopbacks

## Problem

`DiskInfo::updateDiskInfo()` in `disk_info_shared.cpp` passes every `QStorageInfo` entry with `isValid() == true` into the disk list. The gear menu and Settings combobox show everything Qt returns.

## What Qt returns

`QStorageInfo::mountedVolumes()` docs say it excludes "pseudo filesystems", but in practice it returns far more than physical disks:

### macOS observed entries
- `/dev/disk3s1s1` → `Macintosh HD` (APFS boot volume) ✅
- `/dev/disk3s5` → `Macintosh HD - Data` (APFS data container) — same physical disk as above
- `/dev/disk3s4` → `VM` (APFS swap/VM volume)
- `/dev/disk1s1` → `Preboot`, `Recovery` (hidden system volumes)
- `/dev/disk5s1` → mounted `.dmg` installer images
- Time Machine local snapshot mounts
- Network shares (NFS, SMB)

### Linux observed entries
- `/dev/sda1`, `/dev/nvme0n1p2` → real partitions ✅
- `/dev/loop0..N` → Snap packages (20+ on Ubuntu with Snap)
- `tmpfs` → `/run`, `/dev/shm`, `/run/user/1000`
- `overlay` → Docker overlayfs
- squashfs → Flatpak/AppImage
- `devtmpfs` → `/dev`
- sysfs, proc, cgroup — pseudo-filesystems (Qt claims to filter these but doesn't always)

## Available QStorageInfo filtering signals

| Method | What it tells us |
|--------|-----------------|
| `device()` | Device path — `/dev/disk3s5`, `/dev/loop0`, `tmpfs`, `overlay` |
| `fileSystemType()` | Filesystem — `apfs`, `ext4`, `tmpfs`, `squashfs`, `overlay`, `devtmpfs` |
| `rootPath()` | Mount point — `/`, `/home`, `/snap/firefox/5432` |
| `isReadOnly()` | Read-only flag — true for Snap squashfs, recovery volumes |
| `bytesTotal()` | Total size — 0 for some pseudo-FS; useful as a sanity filter |

## Filtering Strategy

### Filesystem type exclusion list (cross-platform)
These filesystem types should always be excluded:
```
tmpfs, devtmpfs, devfs, sysfs, procfs, cgroup, cgroup2,
squashfs, overlay, fuse.snapfuse, autofs,
nullfs, fdescfs, linprocfs, linsysfs, map
```

### Device path filters
- **Linux:** Exclude `/dev/loop*` (Snap loopbacks)
- **macOS:** Exclude device paths where the device string doesn't start with `/dev/` (non-block devices)

### Mount path filters
- Exclude paths starting with `/snap/` (Snap package mounts)
- Exclude paths starting with `/proc`, `/sys`, `/dev` (pseudo-FS mount points)
- Exclude paths starting with `/run` (runtime mounts)

### Size filter
- Exclude entries where `bytesTotal() == 0` (invalid or pseudo-FS)

### macOS-specific: APFS volume role deduplication
On macOS, a single physical SSD appears as multiple APFS "roles":
- `Macintosh HD` (System role) — the boot snapshot, read-only
- `Macintosh HD - Data` (Data role) — the writable data volume
- `Preboot`, `Recovery`, `VM` — system volumes

Users care about the Data volume (where their files live). Filter by:
- Exclude entries where `rootPath()` is `/System/Volumes/Preboot`, `/System/Volumes/Recovery`, `/System/Volumes/VM`, `/System/Volumes/Update`
- The root `/` mount (Macintosh HD) and `/System/Volumes/Data` (Macintosh HD - Data) typically share the same APFS container — keep both since they show different usage stats

## Consumers of the disk list

1. **Dashboard disk tile** (`dashboard_page.cpp:416-474`) — gear menu + donut chart
2. **Settings disk combobox** (`settings_page.cpp:69-79`) — disk alert threshold config
3. **`DiskInfo::devices()`** and **`DiskInfo::fileSystemTypes()`** — helper functions (also unfiltered)

All consumers use `DiskInfo::getDisks()` or `DiskInfo::updateDiskInfo()`, so filtering in `updateDiskInfo()` fixes all call sites at once.

## Files to modify

| File | Change |
|------|--------|
| `shared/nexis-core/Info/disk_info_shared.cpp` | Add filtering logic to `updateDiskInfo()` and helper functions |
