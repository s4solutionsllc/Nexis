# BUG-08 Research: Wayland Compatibility

## Bug Summary

App fails to launch with `QT_QPA_PLATFORM=wayland`. Upstream: [oguzhaninan/Stacer#494](https://github.com/oguzhaninan/Stacer/issues/494).

## Root Cause Analysis

### Primary: Unchecked `primaryScreen()` null dereference (CRASH)

**3 call sites, all identical pattern:**

| File | Line | Code |
|---|---|---|
| `shared/nexis/app.cpp` | 34 | `qApp->primaryScreen()->availableGeometry()` |
| `linux/nexis/Pages/StartupApps/startup_app_edit.cpp` | 35 | `qApp->primaryScreen()->availableGeometry()` |
| `macos/nexis/Pages/StartupApps/startup_app_edit.cpp` | 42 | `qApp->primaryScreen()->availableGeometry()` |

On X11, `QGuiApplication::primaryScreen()` returns a valid `QScreen*` immediately because X11 provides screen geometry synchronously during connection setup.

On Wayland, screen information is delivered asynchronously via the `wl_output` protocol. Depending on compositor timing, `primaryScreen()` may return `nullptr` before the first surface is created. Calling `availableGeometry()` on a null pointer causes an immediate segfault.

This is almost certainly the launch crash reported in the upstream issue.

**app.cpp:30-35 — the crash site:**
```cpp
void App::init()
{
    setGeometry(
        QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter,
            size(), qApp->primaryScreen()->availableGeometry())
    );
```

### Secondary: Window activation unreliable on Wayland

**3 call sites:**

| File | Line | Code | Context |
|---|---|---|---|
| `shared/nexis/app.cpp` | 183 | `raise()` | Tray icon activation |
| `shared/nexis/app.cpp` | 184 | `activateWindow()` | Tray icon activation |
| `shared/nexis/app.cpp` | 206 | `activateWindow()` | Single-instance re-focus |

Wayland's security model prevents applications from stealing focus or raising themselves above other windows. `raise()` and `activateWindow()` are silently ignored by most Wayland compositors (GNOME's Mutter, KDE's KWin, wlroots-based). The tray icon click and single-instance re-focus features will not work.

This is a degraded-functionality issue, not a crash. The `xdg-activation` Wayland protocol (supported by Qt 6.3+) provides a proper focus-request mechanism, but the app doesn't use it.

### Tertiary: SlidingStackedWidget uses raise()

`shared/nexis/sliding_stacked_widget.cpp:114`:
```cpp
widget(next)->raise();
```

This is a `raise()` on a child widget within the app's own window — it adjusts stacking order within the QStackedWidget, not across windows. This should work fine on Wayland since it's internal widget management, not cross-window manipulation.

## What's NOT a Problem

### No X11/XCB API usage
Searched entire codebase — zero references to:
- `#include <X11/...>`, `Display*`, `XOpenDisplay`
- `QX11Info`, `XCB_*`
- `_NET_WM`, `XChangeProperty`, `XGetWindowProperty`
- `xdotool`, `xprop`, `xwininfo`, `wmctrl`

### No X11 build dependencies
- `CMakeLists.txt` has no `find_package(X11)` or XCB link targets
- Qt6 modules used are all platform-agnostic: Core, Gui, Widgets, Charts, Svg, Concurrent, Network

### Standard QApplication usage
- `main.cpp:62`: `QApplication app(argc, argv)` — correct, does not force xcb
- No `QT_QPA_PLATFORM` set in code
- No platform-specific `QGuiApplication` subclassing

### Graphics effects
- `Utilities::addDropShadow()` uses `QGraphicsDropShadowEffect` — works on Wayland
- No platform-specific rendering code

### System commands
- All system info gathering uses POSIX tools (`lscpu`, `/proc/*`, `sysctl`, etc.) — no X11 CLI tools

### Clipboard / Drag-and-drop
- `QClipboard` included in `search_page.cpp` but not used in any operations
- No drag-and-drop implementations

## Wayland Compatibility Checklist

| Area | Status | Notes |
|---|---|---|
| Platform plugin selection | PASS | Standard QApplication, no xcb forcing |
| X11/XCB API usage | PASS | None found |
| Build dependencies | PASS | No X11/XCB link targets |
| Screen access | **FAIL** | `primaryScreen()` dereference without null check — crash |
| Window activation | **FAIL** | `raise()` + `activateWindow()` silently fail on Wayland |
| System tray | WARN | `QSystemTrayIcon` works but activation handler can't refocus window |
| System commands | PASS | POSIX tools only |
| Graphics effects | PASS | QGraphicsDropShadowEffect compatible |
| Clipboard | PASS | Not used |
| Drag-and-drop | PASS | Not used |

## Recommended Fixes

### Fix A: Guard primaryScreen() calls (CRITICAL — fixes the crash)

Replace all 3 call sites with a null-safe pattern:

```cpp
QScreen *screen = qApp->primaryScreen();
if (screen) {
    setGeometry(
        QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter,
            size(), screen->availableGeometry())
    );
}
```

If `screen` is null, the widget uses Qt's default positioning. This is acceptable — the window appears at the compositor's default position rather than centered.

### Fix B: Make window activation Wayland-aware (MODERATE)

For the tray icon and single-instance re-focus, use `QWindow::requestActivate()` instead of `activateWindow()`. On Qt 6.3+ with Wayland, this uses the `xdg-activation` protocol which some compositors honor.

```cpp
connect(mTrayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason) {
    setWindowState(windowState() & ~Qt::WindowMinimized);
    show();
    if (windowHandle())
        windowHandle()->requestActivate();
});
```

Note: This is still best-effort on Wayland. Some compositors may still decline the focus request.

### Fix C: No build changes needed

The Qt6 Wayland platform plugin (`libqwayland-generic.so`) is part of the Qt6 Wayland package and is discovered automatically at runtime. No CMake changes are required — the user just needs `qt6-wayland` (or equivalent) installed.

## Affected Files

| File | Change Needed |
|---|---|
| `shared/nexis/app.cpp` | Guard `primaryScreen()` at line 34; improve tray activation at lines 180-185 |
| `linux/nexis/Pages/StartupApps/startup_app_edit.cpp` | Guard `primaryScreen()` at line 35 |
| `macos/nexis/Pages/StartupApps/startup_app_edit.cpp` | Guard `primaryScreen()` at line 42 (macOS doesn't use Wayland, but defensive coding) |

## Fix Complexity

**LOW.** The crash fix is 3 trivial null-checks. The window activation improvement is a minor API change. No architectural changes required. The app is already well-structured for Wayland — the only issues are the null dereference and the inherent Wayland limitation on window activation.
