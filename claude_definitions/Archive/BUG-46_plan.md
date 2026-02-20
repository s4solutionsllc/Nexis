# BUG-46 Plan: Fix kiosk overlay centering

## Summary

The "Press ESC to exit kiosk mode" overlay is positioned using `mSlidingStacked`'s local coordinates in the wrong coordinate space, and `showFullScreen()` hasn't completed by the time positioning runs. Fix by centering on the actual screen geometry instead.

## Tasks

### Task 1: Replace positioning logic in `showKioskOverlay()`

- [x] **File:** `shared/nexis/app.cpp`, lines 455-458
- **Change:** Replace the `mSlidingStacked`-relative positioning with `QScreen::geometry()` centering.

**Before:**
```cpp
QWidget *target = mSlidingStacked;
int x = target->x() + (target->width() - overlay->width()) / 2;
int y = target->y() + (target->height() - overlay->height()) / 2;
overlay->move(x, y);
```

**After:**
```cpp
QScreen *screen = windowHandle() ? windowHandle()->screen() : qApp->primaryScreen();
if (screen) {
    int x = (screen->geometry().width() - overlay->width()) / 2;
    int y = (screen->geometry().height() - overlay->height()) / 2;
    overlay->move(x, y);
}
```

**Why this works:**
- `QScreen::geometry()` returns the screen's physical dimensions immediately — no dependency on the async `showFullScreen()` completing.
- In fullscreen mode, the window fills the screen exactly, so screen-relative and window-relative child coordinates are equivalent.
- `windowHandle()->screen()` handles multi-monitor correctly (uses whichever screen the window is on).
- Falls back to `primaryScreen()` if `windowHandle()` isn't ready yet.

### Task 2: Build verification

- [x] Clean build to confirm no compilation errors.

## Acceptance Criteria

1. Overlay appears centered on the monitor when entering kiosk mode.
2. Multi-monitor: overlay centers on the screen the app window is on.
3. No build warnings or errors introduced.
4. Fade-out animation continues to work as before.
