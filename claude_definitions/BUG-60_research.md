# BUG-60 Research: Dashboard system summary shows "0 Bytes RAM"

## Problem

The system summary card on the Dashboard page displays "0 Bytes RAM" instead of the actual system memory (e.g., "16.0 GB RAM").

## Root Cause

**Timing/initialization order issue.** The memory value is read before it has been populated.

### Call Chain

1. `DashboardPage::DashboardPage()` → calls `init()` (line 54)
2. `DashboardPage::init()` → calls `buildSystemSummary()` (line 242)
3. `buildSystemSummary()` → reads `im->getMemTotal()` (line 293)
4. At this point, `MemoryInfo::memTotal` is still 0 (initialized in constructor at `memory_info_shared.cpp:8`)
5. `DataRefreshService::start()` is called LATER in `app.cpp` (line ~366), which triggers the first `updateMemoryInfo()` and populates `memTotal`

### Why it stays 0

`onMemoryUpdated()` (line 382) receives the correct `total` value from `DataRefreshService` but only updates the memory *tile* — it never updates `mSummaryRam` or refreshes the summary label.

## Key Files

| File | Role |
|------|------|
| `shared/nexis/Pages/Dashboard/dashboard_page.cpp:293` | `buildSystemSummary()` reads memTotal (returns 0) |
| `shared/nexis/Pages/Dashboard/dashboard_page.cpp:382-414` | `onMemoryUpdated()` — updates tile but not summary |
| `shared/nexis/Pages/Dashboard/dashboard_page.cpp:308-318` | `refreshSummaryColors()` — rebuilds summary label from cached strings |
| `shared/nexis-core/Info/memory_info_shared.cpp:8` | `memTotal(0)` — constructor initializes to 0 |
| `shared/nexis/Managers/data_refresh_service.cpp:110` | Emits `memoryUpdated` signal with real values |

## Fix Approach

Update `mSummaryRam` inside `onMemoryUpdated()` when `total > 0` and call `refreshSummaryColors()` to redraw the label. This is the least invasive approach — no constructor reordering, no new signals, just updating the cached string when real data arrives.
