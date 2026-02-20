# BUG-46 Research: Kiosk mode "Press ESC" overlay not centered on monitor

## Widget Hierarchy

```
App (QMainWindow)  ← overlay parent (`new QLabel(this)`)
  └── centralwidget (QWidget)
        └── horizontalLayout (QHBoxLayout, 0 margins, 0 spacing)
              ├── sidebar (QWidget, fixed 220px) ← hidden in kiosk mode
              └── pageContent (QWidget, expanding)
                    └── pageContentLayout (QVBoxLayout, 0 margins)
                          ├── pageTitle (QLabel) ← hidden in kiosk mode
                          └── mSlidingStacked (SlidingStackedWidget) ← added at app.cpp:63
```

## Current Positioning Code (`app.cpp:440-475`)

```cpp
void App::showKioskOverlay()
{
    QLabel *overlay = new QLabel(this);          // parented to App (QMainWindow)
    // ... styling ...
    overlay->adjustSize();

    QWidget *target = mSlidingStacked;
    int x = target->x() + (target->width() - overlay->width()) / 2;
    int y = target->y() + (target->height() - overlay->height()) / 2;
    overlay->move(x, y);
    // ... fade animation ...
}
```

## Root Cause

Two compounding issues:

### 1. Wrong coordinate space
The overlay is a child of `App` (QMainWindow), so `overlay->move(x, y)` positions it in the QMainWindow's coordinate space. But `mSlidingStacked->x()` / `->y()` return coordinates relative to `mSlidingStacked`'s **parent** (`pageContent`), not relative to `App`. The coordinate chain is:

```
App → centralwidget → horizontalLayout → pageContent → pageContentLayout → mSlidingStacked
```

Using `target->x()` / `target->y()` gives mSlidingStacked's offset within `pageContent` (which is ~0 since it fills the layout), but misses the offset from `pageContent` itself within the window. To correctly map, you'd need `mSlidingStacked->mapTo(this, QPoint(0,0))`.

### 2. Timing: `showFullScreen()` is asynchronous
The call order in `applyKioskMode(true)` (app.cpp:419-438):

```cpp
ui->sidebar->hide();        // 1. hide sidebar
ui->pageTitle->hide();      // 2. hide title
pageClick(dashboardPage);   // 3. switch to dashboard
showFullScreen();           // 4. request fullscreen (async!)
showKioskOverlay();         // 5. position overlay (runs immediately)
```

`showFullScreen()` sends a request to the window manager — the window geometry doesn't update until the event loop processes the WM response. At the moment `showKioskOverlay()` executes, `this->width()` / `this->height()` and all child widget geometries still reflect the **pre-fullscreen** window size.

This means even centering on `this->rect()` would use stale dimensions.

### Combined Effect
The overlay ends up positioned using pre-fullscreen stacked-widget-relative coordinates, offset from the true screen center in both X and Y.

## Correct Fix Approach

Use `QScreen::geometry()` — the screen dimensions are immediately known (they don't change with the fullscreen transition) and in fullscreen mode the window exactly matches the screen.

```cpp
QScreen *screen = windowHandle() ? windowHandle()->screen() : qApp->primaryScreen();
QRect screenGeom = screen->geometry();
int x = (screenGeom.width() - overlay->width()) / 2;
int y = (screenGeom.height() - overlay->height()) / 2;
```

This works because:
- In fullscreen, the window fills the screen exactly, so screen-relative and window-relative coordinates are equivalent.
- `QScreen::geometry()` returns the physical screen dimensions immediately — no layout processing needed.
- `windowHandle()->screen()` correctly handles multi-monitor setups (overlay centers on whichever screen the window is on).

## Files to Modify

- `shared/nexis/app.cpp` — `showKioskOverlay()` method, lines 455-458 only.
