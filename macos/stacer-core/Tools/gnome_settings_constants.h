#ifndef GNOME_SETTINGS_CONSTANTS_H
#define GNOME_SETTINGS_CONSTANTS_H

#include <QString>

// On macOS, map to macOS preference domains and keys
namespace GnomeSchema {
    const QString INTERFACE  = "NSGlobalDomain";
    const QString WM_PREFS   = "com.apple.dock";
    const QString MUTTER     = "com.apple.dock";
    const QString MOUSE      = "NSGlobalDomain";
    const QString TOUCHPAD   = "com.apple.AppleMultitouchTrackpad";
    const QString BACKGROUND = "com.apple.desktop";
    const QString SOUND      = "com.apple.sound.beep";
}

namespace GnomeKey {
    // Interface / Appearance
    const QString COLOR_SCHEME       = "AppleInterfaceStyle";
    const QString GTK_THEME          = "AppleInterfaceStyle";
    const QString ICON_THEME         = "AppleInterfaceStyle";
    const QString CURSOR_THEME       = "AppleInterfaceStyle";
    const QString CURSOR_SIZE        = "AppleInterfaceStyle";
    const QString FONT_NAME          = "AppleInterfaceStyle";
    const QString DOCUMENT_FONT      = "AppleInterfaceStyle";
    const QString MONOSPACE_FONT     = "AppleInterfaceStyle";
    const QString TEXT_SCALING       = "AppleInterfaceStyle";
    const QString ENABLE_ANIMATIONS  = "NSAutomaticWindowAnimationsEnabled";
    const QString ENABLE_HOT_CORNERS = "wvous-tl-corner";
    const QString CLOCK_FORMAT       = "AppleICUForce24HourTime";
    const QString CLOCK_SECONDS      = "AppleICUForce24HourTime";
    const QString CLOCK_WEEKDAY      = "AppleICUForce24HourTime";
    const QString SHOW_BATTERY_PCT   = "ShowPercent";
    const QString FONT_ANTIALIASING  = "AppleFontSmoothing";
    const QString FONT_HINTING       = "AppleFontSmoothing";

    // Dock
    const QString BUTTON_LAYOUT       = "orientation";
    const QString FOCUS_MODE          = "orientation";
    const QString TITLEBAR_FONT       = "orientation";
    const QString NUM_WORKSPACES      = "orientation";
    const QString ACTION_DBL_CLICK    = "orientation";
    const QString ACTION_MID_CLICK    = "orientation";
    const QString ACTION_RIGHT_CLICK  = "orientation";
    const QString AUTO_RAISE          = "autohide";
    const QString RAISE_ON_CLICK      = "autohide";

    // Window Management
    const QString DYNAMIC_WORKSPACES     = "mru-spaces";
    const QString EDGE_TILING            = "mru-spaces";
    const QString AUTO_MAXIMIZE          = "mru-spaces";
    const QString CENTER_NEW_WINDOWS     = "mru-spaces";
    const QString WORKSPACES_PRIMARY     = "mru-spaces";

    // Mouse
    const QString NATURAL_SCROLL   = "com.apple.swipescrolldirection";
    const QString SPEED            = "com.apple.mouse.scaling";
    const QString ACCEL_PROFILE    = "com.apple.mouse.scaling";
    const QString LEFT_HANDED      = "com.apple.mouse.scaling";

    // Trackpad
    const QString TAP_TO_CLICK       = "Clicking";
    const QString TWO_FINGER_SCROLL  = "TrackpadScroll";
    const QString EDGE_SCROLLING     = "TrackpadScroll";
    const QString DISABLE_TYPING     = "TrackpadScroll";

    // Background
    const QString PICTURE_URI       = "Background";
    const QString PICTURE_URI_DARK  = "Background";
    const QString PICTURE_OPTIONS   = "Background";

    // Sound
    const QString EVENT_SOUNDS      = "com.apple.sound.beep.flash";
    const QString INPUT_FEEDBACK    = "com.apple.sound.beep.flash";
    const QString VOLUME_OVER_100   = "com.apple.sound.beep.flash";
}

#endif // GNOME_SETTINGS_CONSTANTS_H
