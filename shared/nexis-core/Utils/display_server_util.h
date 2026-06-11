#ifndef DISPLAY_SERVER_UTIL_H
#define DISPLAY_SERVER_UTIL_H

#include "nexis-core_global.h"

class QString;

// SSO-3729 / FW-02: GNOME 50 (Ubuntu 26.04) removes the X11 session entirely;
// stock 26.04 GNOME is Wayland-only with XWayland kept around for legacy app
// compatibility. Nexis itself uses only Qt6 abstractions for windowing, so no
// runtime code path is currently X11-only — but providing a single canonical
// detector keeps any future XWayland-gated feature from re-deriving the same
// QGuiApplication::platformName() / WAYLAND_DISPLAY heuristic in five places.
//
// nexis-core links Qt6::Core only (not Qt6::Gui), so the env-var-driven
// detect() lives here; the GUI library forwards `QGuiApplication::platformName()`
// into classify() when it wants the strongest signal.

class NEXISCORESHARED_EXPORT DisplayServerUtil
{
public:
    enum class Kind {
        // Native Wayland client (Qt platform plugin "wayland"/"wayland-egl").
        Wayland,
        // Native X11 client. Under a Wayland-only compositor this means the
        // app is running through XWayland — callers that care about the
        // difference must check isXWayland() too.
        X11,
        // Offscreen QPA (headless/--clean/--check-threshold path; SSO-3368).
        Offscreen,
        // Some other Qt platform plugin (eglfs, vnc, minimal, cocoa, …) or
        // detection ran before QGuiApplication was constructed.
        Other,
    };

    // Env-only detection. Safe to call before QGuiApplication is constructed
    // (cron/headless paths). Uses XDG_SESSION_TYPE, WAYLAND_DISPLAY, DISPLAY.
    // GUI callers that want the strongest signal should pass platformName via
    // classify() instead — Qt's plugin selection is authoritative once Qt is up.
    static Kind detect();

    // True when the given Qt platform is "xcb" (native X11 client) *and*
    // the running session is Wayland — i.e. the X server is XWayland, not
    // a real X.Org. Pass `QGuiApplication::platformName()` here from GUI
    // code; pass an empty string to fall back to env-only inspection
    // (which can only detect XWayland when the user forced QT_QPA_PLATFORM=xcb
    // and a WAYLAND_DISPLAY socket is also present).
    static bool isXWayland(const QString &platformName);

    // Lowercase short name suitable for logs / About dialog:
    // "wayland" / "x11" / "offscreen" / "other". Pure mapping — does not
    // distinguish XWayland from real X.Org (use describeCurrent() for that).
    static QString name(Kind kind);

    // Live description of the running process: same as name(detect()) plus
    // the "xwayland" distinction when we're an xcb client under a Wayland
    // compositor. Pass `QGuiApplication::platformName()` from GUI code so
    // the XWayland branch can be reached; empty falls back to env-only.
    static QString describeCurrent(const QString &platformName);

    // Pure helper — the Qt platformName() string maps onto Kind. Exposed so
    // unit tests can drive every branch without spinning up a QApplication,
    // and so GUI callers can forward QGuiApplication::platformName() into a
    // single source of truth.
    //
    // Empty platformName + non-empty WAYLAND_DISPLAY  → Wayland
    // Empty platformName + non-empty DISPLAY          → X11
    // Empty platformName + "wayland" XDG_SESSION_TYPE → Wayland
    // Empty platformName + "x11"     XDG_SESSION_TYPE → X11
    // Otherwise (nothing set, no Qt app) → Other.
    static Kind classify(const QString &platformName,
                         const QString &xdgSessionType,
                         const QString &waylandDisplay,
                         const QString &display);
};

#endif // DISPLAY_SERVER_UTIL_H
