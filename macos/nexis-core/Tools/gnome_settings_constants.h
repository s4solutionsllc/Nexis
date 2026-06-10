#ifndef GNOME_SETTINGS_CONSTANTS_H
#define GNOME_SETTINGS_CONSTANTS_H

#include <QString>

// SSO-3391 / WI-29: GNOME Settings has no valid mapping on macOS.
//
// `shared/nexis-core/Tools/gnome_settings_tool.h` unconditionally pulls in
// this header, so it must compile on macOS. But the GNOME Settings page is
// hidden on macOS (`app.cpp` gates the sidebar button under
// `#ifdef Q_OS_MAC`), the `ToolManager::checkGnomeSettings()` check is
// hard-coded to return false on macOS, and `GnomeSettingsToolMacOS` is a
// stub whose setters all refuse. Nothing in the macOS build reaches these
// constants at runtime — they exist only so the shared headers compile.
//
// Earlier revisions populated these with real macOS preference domains and
// keys (e.g. `NSGlobalDomain` / `AppleInterfaceStyle` / dock `orientation`),
// which would have corrupted `NSGlobalDomain` if `isAvailable()` had ever
// flipped back to true. The values are now empty strings so that, in the
// event a guard ever regresses, a downstream `defaults write` would fail
// with an obviously-invalid domain/key rather than silently mangling a real
// Apple preference.

namespace GnomeSchema {
    const QString INTERFACE  = "";
    const QString WM_PREFS   = "";
    const QString MUTTER     = "";
    const QString MOUSE      = "";
    const QString TOUCHPAD   = "";
    const QString BACKGROUND = "";
    const QString SOUND      = "";
}

namespace GnomeKey {
    // Interface / Appearance
    const QString COLOR_SCHEME       = "";
    const QString GTK_THEME          = "";
    const QString ICON_THEME         = "";
    const QString CURSOR_THEME       = "";
    const QString CURSOR_SIZE        = "";
    const QString FONT_NAME          = "";
    const QString DOCUMENT_FONT      = "";
    const QString MONOSPACE_FONT     = "";
    const QString TEXT_SCALING       = "";
    const QString ENABLE_ANIMATIONS  = "";
    const QString ENABLE_HOT_CORNERS = "";
    const QString CLOCK_FORMAT       = "";
    const QString CLOCK_SECONDS      = "";
    const QString CLOCK_WEEKDAY      = "";
    const QString SHOW_BATTERY_PCT   = "";
    const QString FONT_ANTIALIASING  = "";
    const QString FONT_HINTING       = "";

    // Window Management
    const QString BUTTON_LAYOUT       = "";
    const QString FOCUS_MODE          = "";
    const QString TITLEBAR_FONT       = "";
    const QString NUM_WORKSPACES      = "";
    const QString ACTION_DBL_CLICK    = "";
    const QString ACTION_MID_CLICK    = "";
    const QString ACTION_RIGHT_CLICK  = "";
    const QString AUTO_RAISE          = "";
    const QString RAISE_ON_CLICK      = "";

    const QString DYNAMIC_WORKSPACES     = "";
    const QString EDGE_TILING            = "";
    const QString AUTO_MAXIMIZE          = "";
    const QString CENTER_NEW_WINDOWS     = "";
    const QString WORKSPACES_PRIMARY     = "";

    // Mouse / Trackpad
    const QString NATURAL_SCROLL   = "";
    const QString SPEED            = "";
    const QString ACCEL_PROFILE    = "";
    const QString LEFT_HANDED      = "";

    const QString TAP_TO_CLICK       = "";
    const QString TWO_FINGER_SCROLL  = "";
    const QString EDGE_SCROLLING     = "";
    const QString DISABLE_TYPING     = "";

    // Background
    const QString PICTURE_URI       = "";
    const QString PICTURE_URI_DARK  = "";
    const QString PICTURE_OPTIONS   = "";

    // Sound
    const QString EVENT_SOUNDS      = "";
    const QString INPUT_FEEDBACK    = "";
    const QString VOLUME_OVER_100   = "";
}

#endif // GNOME_SETTINGS_CONSTANTS_H
