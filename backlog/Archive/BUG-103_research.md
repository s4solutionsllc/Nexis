# BUG-103 Research: Schedule indicator white square on initial load

## Symptoms

When navigating to the System Cleaner page for the first time, the schedule indicator (`#scheduleIndicator`) renders as a small white/blank rectangle at the top-left corner of the page. Resizing the window triggers `resizeEvent()` which correctly repositions it to the bottom.

## Root Cause

`initScheduleIndicator()` is called at the end of the `SystemCleanerPage` constructor (line 154). It calls `updateScheduleIndicator()` which calls `mScheduleIndicator->show()` and then `repositionScheduleIndicator()`.

`repositionScheduleIndicator()` (line 639-654) computes the indicator's geometry based on `this->width()` and `this->height()`:
```cpp
int w = width() - outerMarginLR * 2;
int x = outerMarginLR;
int y = height() - indicatorH - outerMarginBottom;
```

At constructor time, the widget hasn't been laid out yet, so `width()`/`height()` return default or zero values. The indicator gets placed at an incorrect position (top-left, small size), producing the white square visible in the screenshot.

The `resizeEvent()` override correctly calls `repositionScheduleIndicator()`, which is why resizing the window fixes the display.

## Missing: `showEvent()` override

There is no `showEvent()` override in `SystemCleanerPage`. The first time the page becomes visible (when the user navigates to it), there's no trigger to reposition the indicator with valid geometry. Only `resizeEvent()` does this, but the initial show doesn't always fire a resize.

## Fix

Add a `showEvent()` override that calls `repositionScheduleIndicator()` and `repositionExclusionsButton()`. This ensures correct positioning when the page first becomes visible (valid geometry is available at show time).

## Files

- `shared/nexis/Pages/SystemCleaner/system_cleaner_page.cpp` — add `showEvent()`
- `shared/nexis/Pages/SystemCleaner/system_cleaner_page.h` — declare `showEvent()`
