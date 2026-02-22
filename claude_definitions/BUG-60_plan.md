# BUG-60 Plan: Fix "0 Bytes RAM" in Dashboard System Summary

## Task 1: Update `onMemoryUpdated()` to refresh system summary

- [x] **File:** `shared/nexis/Pages/Dashboard/dashboard_page.cpp`
- **Change:** In `onMemoryUpdated()`, after updating the memory tile, update `mSummaryRam` with the actual `total` value and call `refreshSummaryColors()` to redraw the summary label.
- **Guard:** Only update on the first call where `total > 0` to avoid unnecessary redraws every second. Use a simple check: if `mSummaryRam` starts with "0" (the broken state), update it.

```cpp
// Inside onMemoryUpdated(), after existing tile updates:
// Update system summary RAM if it was unavailable at init time
if (total > 0 && mSummaryRam.startsWith("0")) {
    mSummaryRam = FormatUtil::formatBytes(total) + " RAM";
    refreshSummaryColors();
}
```

## Task 2: Build verification

- [x] Run incremental build to confirm no compile errors.

## Task 3: Update tracking

- [x] Mark BUG-60 as `[x]` in BUGS.md with resolution note.
