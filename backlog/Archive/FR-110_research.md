# FR-110 Research — Boot-time Analyzer

## Goal
Add a "Boot Analysis" page showing which services/processes slow down startup, ranked by impact.
Linux: `systemd-analyze blame`. macOS: `sysctl kern.boottime` total time (no per-service breakdown without root).

---

## Existing Codebase Patterns

### Sidebar Page Registration (app.cpp)
- Sidebar buttons created in `buildSidebar()` via `createSidebarButton(tooltip)` (~line 197)
- Page slots appended to `mPageSlots` as factory lambdas (lines 348–416)
- `mListSidebarButtons` list parallels mPageSlots for index-based page lookup (line 418–421)
- Click handlers use `navByTitle(tr("Page Title"))` lambda (lines 497–540)
- Icons set in `updateSidebarIcons()` per theme (~line 881)
- Page member pointers declared in `app.h` (~line 125)
- `#include "Pages/BootAnalysis/boot_analysis_page.h"` needed in `app.h`

### Platform-Specific Core Pattern
- Shared interface in `shared/nexis-core/Info/foo_info.h` (abstract base class)
- Linux impl in `linux/nexis-core/Info/foo_info_linux.h` + `foo_info.cpp`
- macOS impl in `macos/nexis-core/Info/foo_info_macos.h` + `foo_info.cpp`
- Both platforms share same `.cpp` filename; CMake picks the right directory
- CMakeLists.txt: added to `CORE_PLAT_SRCS`/`CORE_PLAT_HDRS` inside `if(APPLE)...else()` block (~lines 99–214)

### Async Execution
- `QtConcurrent::run([this]() { ... })` for non-blocking work
- `QFuture<T>` held as member; UI updated via `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` or signal
- `QAtomicInt` cancellation flag if long-running
- `QProcess::readAllStandardOutput()` used synchronously inside the worker thread (call `waitForFinished()`)

---

## Linux Data Source: `systemd-analyze`

### Total boot time
```
$ systemd-analyze
Startup finished in 1.234s (kernel) + 2.345s (initrd) + 5.678s (userspace) = 9.257s
```
Parse with regex: `(\d+\.?\d*)(ms|s) \(userspace\)` for userspace portion; `= (\d+\.?\d*)(ms|s)` for total.

### Per-service breakdown
```
$ systemd-analyze blame
   5.123s NetworkManager.service
   3.456s avahi-daemon.service
 500ms systemd-udevd.service
     ...
```
Parse with regex: `^\s*([\d.]+)(ms|s)\s+(.+)$`
Normalize to milliseconds. Sort descending.

### Availability check
`QProcess::execute("systemd-analyze", {"--version"})` returns 0 if available.
If not found (non-systemd distros, WSL without init), return `available=false` with `error` message.

---

## macOS Data Source: `sysctl kern.boottime`

```
$ sysctl kern.boottime
kern.boottime: { sec = 1745000000, usec = 123456 }
```
Parse: extract `sec` value. `totalBootMs = (QDateTime::currentSecsSinceEpoch() - sec) * 1000.0`.
Note: this is elapsed time since last boot (uptime), not the duration of the boot process itself.

No per-service breakdown without `sudo` on macOS. `entries` list returned empty.
Show a note: "Per-service boot timing is not available on macOS."

---

## Impact Thresholds
- **High**: ≥ 5000 ms
- **Medium**: 1000 – 4999 ms
- **Low**: < 1000 ms

---

## UI Design

Programmatic `QWidget` page (no .ui file — simpler than restructuring an existing .ui):

```
[Title: "Boot Analysis"]             [Refresh button]
[Subtitle: "Total boot time: 9.3s | 42 services"]
─────────────────────────────────────────────────
  Service Name          Duration    Impact
  NetworkManager.srv    5.1s        High
  avahi-daemon.service  3.5s        Medium
  ...
─────────────────────────────────────────────────
[Status: "Last analyzed: 12:34:05" / "Not available: ..."]
```

- `QTableWidget`: 3 columns (Name, Duration, Impact), read-only, `QHeaderView::Stretch` on Name
- Impact column: plain text "High" / "Medium" / "Low" — no hardcoded color
- Async: Refresh button triggers `QtConcurrent::run`; table populated on completion

---

## SVG Icons

Themes: `default` (white fill) and `light` (dark fill `#3d3846`).
New icon: clock/timer concept — simple SVG using same viewBox="0 0 16 16".

---

## Files to Create
- `shared/nexis-core/Info/boot_analysis_info.h`
- `linux/nexis-core/Info/boot_analysis_info_linux.h`
- `linux/nexis-core/Info/boot_analysis_info.cpp`
- `macos/nexis-core/Info/boot_analysis_info_macos.h`
- `macos/nexis-core/Info/boot_analysis_info.cpp`
- `shared/nexis/Pages/BootAnalysis/boot_analysis_page.h`
- `shared/nexis/Pages/BootAnalysis/boot_analysis_page.cpp`
- `shared/nexis/static/themes/default/img/sidebar-icons/boot-analysis.svg`
- `shared/nexis/static/themes/light/img/sidebar-icons/boot-analysis.svg`

## Files to Modify
- `CMakeLists.txt`
- `shared/nexis/app.h`
- `shared/nexis/app.cpp`
