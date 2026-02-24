# FR-58 + FR-59: Per-Process Disk I/O & Network Bandwidth

**Date:** 2026-02-24
**Features:** FR-58 (Per-Process Disk I/O), FR-59 (Per-Process Network Bandwidth)
**Approach:** Combined implementation sharing infrastructure

## Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Scope | Combined FR-58 + FR-59 | Shared Process struct extension and column infrastructure avoids rework |
| Linux network | macOS-only MVP | No viable non-privileged per-process network API on Linux |
| Data model | Embed in Process struct | All process data travels together through existing signal; no signal chain changes |
| Delta tracking | Inside platform ProcessInfo classes | Keeps platform-specific code where it belongs; cleanest data flow |

## Data Model

Four new fields on the `Process` class:

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `diskReadRate` | `double` | `-1.0` | Bytes/sec disk read. -1 = unavailable |
| `diskWriteRate` | `double` | `-1.0` | Bytes/sec disk written. -1 = unavailable |
| `netDownRate` | `double` | `-1.0` | Bytes/sec network received. -1 = unavailable |
| `netUpRate` | `double` | `-1.0` | Bytes/sec network sent. -1 = unavailable |

The `-1.0` default distinguishes "unavailable" from "zero actual I/O". The UI displays `—` for -1, `0 B/s` for zero.

## Platform Implementations

### Disk I/O

**Linux:** After `ps` parse, iterate each PID and read `/proc/<pid>/io` for `read_bytes`/`write_bytes`. Store previous values in `QHash<pid_t, QPair<quint64,quint64>>`. Rate = `(current - previous) / elapsedSeconds`.

**macOS:** After `ps` parse, call `proc_pid_rusage(pid, RUSAGE_INFO_V3, ...)` for each PID. Extract `ri_diskio_bytesread`/`ri_diskio_byteswritten`. Same delta tracking.

### Network

**macOS:** Run `nettop -P -d -L 1 -J bytes_in,bytes_out` once per process tick. Parse CSV output into `QHash<pid_t, QPair<quint64,quint64>>`. Compute deltas against stored previous values.

**Linux:** Not implemented. Network fields remain at `-1.0` (displayed as `—`).

### Delta Tracking Storage

Each platform's `ProcessInfo` subclass gains:

```cpp
QHash<pid_t, QPair<quint64,quint64>> mPrevDiskIo;  // pid -> (prevRead, prevWrite)
QHash<pid_t, QPair<quint64,quint64>> mPrevNetIo;    // macOS only
QElapsedTimer mIoTimer;                               // actual interval measurement
```

Stale entries (PIDs no longer in process list) are pruned each tick.

## UI — Processes Page Columns

Four new columns inserted before the "Process" column (which shifts from index 12 to 16):

| Index | Header | Hidden by Default | Sort Value |
|-------|--------|-------------------|------------|
| 12 | Disk Read/s | Yes | Raw bytes/s (double) |
| 13 | Disk Write/s | Yes | Raw bytes/s (double) |
| 14 | Net Down/s | Yes | Raw bytes/s (double) |
| 15 | Net Up/s | Yes | Raw bytes/s (double) |
| 16 | Process | No (shifted) | Command string |

Display format: `FormatUtil::formatBytes(rate) + "/s"` (e.g., "1.2 MiB/s"). For `-1.0`: display `—`.

Filter column (`mHeaders.count() - 1`) auto-adapts to index 16 (still "Process").

## Signal Chain

**No changes.** `DataRefreshService::processesUpdated(QList<Process>, QString)` carries enriched Process objects. The existing flow: `onProcessTick()` -> `im->updateProcesses()` -> `emit processesUpdated(...)` remains intact.

## Interval Calculation

`QElapsedTimer` measures actual elapsed time between ticks for accurate rate computation: `rate = deltaBytes / (elapsedMs / 1000.0)`. The process refresh interval (1-10 seconds, from slider) determines the delta window.

## Error Handling

- `/proc/<pid>/io` read failure (permission, process death) -> rate = `-1.0`
- `proc_pid_rusage()` returns non-zero -> rate = `-1.0`
- `nettop` command fails or times out -> all network rates `-1.0`
- First tick after launch: no previous data -> rates = `0.0` (no delta)
- PID reuse: stale hash entries pruned each tick, reused PID starts fresh

## Files Modified

| File | Change |
|------|--------|
| `shared/nexis-core/Info/process.h` | Add 4 fields + getters/setters |
| `shared/nexis-core/Info/process.cpp` | Implement new getters/setters |
| `linux/nexis-core/Info/process_info.cpp` | `/proc/<pid>/io` reading + delta tracking |
| `macos/nexis-core/Info/process_info.cpp` | `proc_pid_rusage()` + `nettop` parsing + delta tracking |
| `shared/nexis/Pages/Processes/processes_page.cpp` | 4 new columns in header, createRow, hiddenHeaders |
| `shared/nexis/Pages/Processes/processes_page.h` | No new members needed |

No new files. No new signals. No CMakeLists changes.
