#ifndef GNOME_SETTINGS_CONSTANTS_H
#define GNOME_SETTINGS_CONSTANTS_H

#include <QString>

namespace GnomeSchema {
    const QString INTERFACE  = "org.gnome.desktop.interface";
    const QString WM_PREFS   = "org.gnome.desktop.wm.preferences";
    const QString MUTTER     = "org.gnome.mutter";
    const QString MOUSE      = "org.gnome.desktop.peripherals.mouse";
    const QString TOUCHPAD   = "org.gnome.desktop.peripherals.touchpad";
    const QString BACKGROUND = "org.gnome.desktop.background";
    const QString SOUND      = "org.gnome.desktop.sound";
}

namespace GnomeKey {
    // Interface
    const QString COLOR_SCHEME       = "color-scheme";
    const QString GTK_THEME          = "gtk-theme";
    const QString ICON_THEME         = "icon-theme";
    const QString CURSOR_THEME       = "cursor-theme";
    const QString CURSOR_SIZE        = "cursor-size";
    const QString FONT_NAME          = "font-name";
    const QString DOCUMENT_FONT      = "document-font-name";
    const QString MONOSPACE_FONT     = "monospace-font-name";
    const QString TEXT_SCALING       = "text-scaling-factor";
    const QString ENABLE_ANIMATIONS  = "enable-animations";
    const QString ENABLE_HOT_CORNERS = "enable-hot-corners";
    const QString CLOCK_FORMAT       = "clock-format";
    const QString CLOCK_SECONDS      = "clock-show-seconds";
    const QString CLOCK_WEEKDAY      = "clock-show-weekday";
    const QString SHOW_BATTERY_PCT   = "show-battery-percentage";
    const QString FONT_ANTIALIASING  = "font-antialiasing";
    const QString FONT_HINTING       = "font-hinting";

    // WM Preferences
    const QString BUTTON_LAYOUT       = "button-layout";
    const QString FOCUS_MODE          = "focus-mode";
    const QString TITLEBAR_FONT       = "titlebar-font";
    const QString NUM_WORKSPACES      = "num-workspaces";
    const QString ACTION_DBL_CLICK    = "action-double-click-titlebar";
    const QString ACTION_MID_CLICK    = "action-middle-click-titlebar";
    const QString ACTION_RIGHT_CLICK  = "action-right-click-titlebar";
    const QString AUTO_RAISE          = "auto-raise";
    const QString RAISE_ON_CLICK      = "raise-on-click";

    // Mutter
    const QString DYNAMIC_WORKSPACES     = "dynamic-workspaces";
    const QString EDGE_TILING            = "edge-tiling";
    const QString AUTO_MAXIMIZE          = "auto-maximize";
    const QString CENTER_NEW_WINDOWS     = "center-new-windows";
    const QString WORKSPACES_PRIMARY     = "workspaces-only-on-primary";

    // Mouse
    const QString NATURAL_SCROLL   = "natural-scroll";
    const QString SPEED            = "speed";
    const QString ACCEL_PROFILE    = "accel-profile";
    const QString LEFT_HANDED      = "left-handed";

    // Touchpad
    const QString TAP_TO_CLICK       = "tap-to-click";
    const QString TWO_FINGER_SCROLL  = "two-finger-scrolling-enabled";
    const QString EDGE_SCROLLING     = "edge-scrolling-enabled";
    const QString DISABLE_TYPING     = "disable-while-typing";

    // Background
    const QString PICTURE_URI       = "picture-uri";
    const QString PICTURE_URI_DARK  = "picture-uri-dark";
    const QString PICTURE_OPTIONS   = "picture-options";

    // Sound
    const QString EVENT_SOUNDS      = "event-sounds";
    const QString INPUT_FEEDBACK    = "input-feedback-sounds";
    const QString VOLUME_OVER_100   = "allow-volume-above-100-percent";
}

#endif // GNOME_SETTINGS_CONSTANTS_H
