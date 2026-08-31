#include "mac_tweaks_catalog.h"

#include <QSet>

namespace {

const QString kFinder      = QStringLiteral("Finder");
const QString kDock        = QStringLiteral("Dock");
const QString kScreenshots = QStringLiteral("Screenshots");
const QString kAnimations  = QStringLiteral("Animations");
const QString kLoginWindow = QStringLiteral("Login Window");

// System-wide login window preferences live in a plist path domain rather
// than a bundle-id domain; `defaults` accepts either form identically.
const QString kLoginWindowDomain = QStringLiteral("/Library/Preferences/com.apple.loginwindow");

QList<MacTweakDef> buildCatalog()
{
    QList<MacTweakDef> t;

    // ---- Finder ------------------------------------------------------
    t.append({QStringLiteral("finder.show_hidden_files"), kFinder,
              QObject::tr("Show Hidden Files"),
              QObject::tr("Reveals dotfiles and other hidden items in Finder windows. Default: off."),
              QStringLiteral("com.apple.finder"), QStringLiteral("AppleShowAllFiles"),
              MacDefaultsValueType::Bool, false, true, false, {},
              {QStringLiteral("Finder")}, false, QVersionNumber()});

    t.append({QStringLiteral("finder.show_path_bar"), kFinder,
              QObject::tr("Show Path Bar"),
              QObject::tr("Shows the folder-path breadcrumb at the bottom of Finder windows. Default: off."),
              QStringLiteral("com.apple.finder"), QStringLiteral("ShowPathbar"),
              MacDefaultsValueType::Bool, false, true, false, {},
              {QStringLiteral("Finder")}, false, QVersionNumber()});

    t.append({QStringLiteral("finder.show_status_bar"), kFinder,
              QObject::tr("Show Status Bar"),
              QObject::tr("Shows item count and free-space in Finder windows. Default: off."),
              QStringLiteral("com.apple.finder"), QStringLiteral("ShowStatusBar"),
              MacDefaultsValueType::Bool, false, true, false, {},
              {QStringLiteral("Finder")}, false, QVersionNumber()});

    t.append({QStringLiteral("finder.default_search_scope"), kFinder,
              QObject::tr("Default Search Scope"),
              QObject::tr("Where a new Finder search looks by default. Default: This Mac."),
              QStringLiteral("com.apple.finder"), QStringLiteral("FXDefaultSearchScope"),
              MacDefaultsValueType::String, QStringLiteral("SCev"), {}, {},
              {{QObject::tr("Current Folder"), QStringLiteral("SCcf")},
               {QObject::tr("This Mac"), QStringLiteral("SCev")}},
              {QStringLiteral("Finder")}, false, QVersionNumber()});

    // ---- Dock ----------------------------------------------------------
    t.append({QStringLiteral("dock.autohide"), kDock,
              QObject::tr("Auto-hide the Dock"),
              QObject::tr("Hides the Dock until the pointer touches the screen edge. Default: off."),
              QStringLiteral("com.apple.dock"), QStringLiteral("autohide"),
              MacDefaultsValueType::Bool, false, true, false, {},
              {QStringLiteral("Dock")}, false, QVersionNumber()});

    t.append({QStringLiteral("dock.magnification"), kDock,
              QObject::tr("Dock Magnification"),
              QObject::tr("Enlarges icons under the pointer when hovering the Dock. Default: off."),
              QStringLiteral("com.apple.dock"), QStringLiteral("magnification"),
              MacDefaultsValueType::Bool, false, true, false, {},
              {QStringLiteral("Dock")}, false, QVersionNumber()});

    t.append({QStringLiteral("dock.minimize_to_app_icon"), kDock,
              QObject::tr("Minimize Windows into App Icon"),
              QObject::tr("Minimized windows collapse into their app's Dock icon instead of getting their own. Default: off."),
              QStringLiteral("com.apple.dock"), QStringLiteral("minimize-to-application"),
              MacDefaultsValueType::Bool, false, true, false, {},
              {QStringLiteral("Dock")}, false, QVersionNumber()});

    t.append({QStringLiteral("dock.tile_size"), kDock,
              QObject::tr("Dock Icon Size"),
              QObject::tr("Icon size in pixels, 16-128. Default: 48."),
              QStringLiteral("com.apple.dock"), QStringLiteral("tilesize"),
              MacDefaultsValueType::Int, 48, {}, {}, {},
              {QStringLiteral("Dock")}, false, QVersionNumber()});

    // ---- Screenshots -----------------------------------------------------
    t.append({QStringLiteral("screenshot.disable_shadow"), kScreenshots,
              QObject::tr("Disable Window Shadow"),
              QObject::tr("Omits the drop shadow when screenshotting a window. Default: off (shadow included)."),
              QStringLiteral("com.apple.screencapture"), QStringLiteral("disable-shadow"),
              MacDefaultsValueType::Bool, false, true, false, {},
              {QStringLiteral("SystemUIServer")}, false, QVersionNumber()});

    t.append({QStringLiteral("screenshot.format"), kScreenshots,
              QObject::tr("Screenshot Format"),
              QObject::tr("Image format written by Cmd+Shift+3/4/5. Default: png."),
              QStringLiteral("com.apple.screencapture"), QStringLiteral("type"),
              MacDefaultsValueType::String, QStringLiteral("png"), {}, {},
              {{QStringLiteral("PNG"), QStringLiteral("png")},
               {QStringLiteral("JPEG"), QStringLiteral("jpg")},
               {QStringLiteral("TIFF"), QStringLiteral("tiff")},
               {QStringLiteral("PDF"), QStringLiteral("pdf")}},
              {QStringLiteral("SystemUIServer")}, false, QVersionNumber()});

    t.append({QStringLiteral("screenshot.location"), kScreenshots,
              QObject::tr("Screenshot Save Location"),
              QObject::tr("Folder screenshots are saved to. Default: ~/Desktop."),
              QStringLiteral("com.apple.screencapture"), QStringLiteral("location"),
              MacDefaultsValueType::String, QStringLiteral("~/Desktop"), {}, {}, {},
              {QStringLiteral("SystemUIServer")}, false, QVersionNumber()});

    // ---- Animations ------------------------------------------------------
    t.append({QStringLiteral("animations.disable_window_animations"), kAnimations,
              QObject::tr("Disable Window Open/Close Animations"),
              QObject::tr("Skips the genie/scale animation when opening or closing windows. Default: on (animated)."),
              QStringLiteral("NSGlobalDomain"), QStringLiteral("NSAutomaticWindowAnimationsEnabled"),
              MacDefaultsValueType::Bool, true, false, true, {},
              {}, false, QVersionNumber()});

    t.append({QStringLiteral("animations.reduce_motion"), kAnimations,
              QObject::tr("Reduce Motion"),
              QObject::tr("Reduces UI motion effects system-wide (Accessibility setting). Default: off."),
              QStringLiteral("com.apple.universalaccess"), QStringLiteral("reduceMotion"),
              MacDefaultsValueType::Bool, false, true, false, {},
              {}, false, QVersionNumber(10, 12)});

    // ---- Login Window ------------------------------------------------
    t.append({QStringLiteral("loginwindow.disable_guest_account"), kLoginWindow,
              QObject::tr("Disable Guest Account"),
              QObject::tr("Removes the Guest login option from the login window. Default: off (guest allowed)."),
              kLoginWindowDomain, QStringLiteral("GuestEnabled"),
              MacDefaultsValueType::Bool, true, false, true, {},
              {}, true, QVersionNumber()});

    t.append({QStringLiteral("loginwindow.disable_power_buttons"), kLoginWindow,
              QObject::tr("Disable Power Buttons at Login"),
              QObject::tr("Hides Restart/Sleep/Shut Down from the login window. Default: off (buttons shown)."),
              kLoginWindowDomain, QStringLiteral("PowerButtonDisabled"),
              MacDefaultsValueType::Bool, false, true, false, {},
              {}, true, QVersionNumber(10, 13)});

    return t;
}

} // namespace

QList<MacTweakDef> MacTweaksCatalog::all()
{
    static const QList<MacTweakDef> kCatalog = buildCatalog();
    return kCatalog;
}

QStringList MacTweaksCatalog::categories()
{
    QStringList cats;
    QSet<QString> seen;
    for (const MacTweakDef &t : all()) {
        if (seen.contains(t.category))
            continue;
        seen.insert(t.category);
        cats << t.category;
    }
    return cats;
}

const MacTweakDef *MacTweaksCatalog::findById(const QString &id)
{
    static const QList<MacTweakDef> kCatalog = all();
    for (const MacTweakDef &t : kCatalog) {
        if (t.id == id)
            return &t;
    }
    return nullptr;
}

bool MacTweaksCatalog::isSupported(const MacTweakDef &tweak, const QVersionNumber &osVersion)
{
    if (!tweak.hasVersionGate())
        return true;
    if (osVersion.isNull())
        return true; // unknown OS version — fail open rather than hide unexpectedly
    return osVersion >= tweak.minOsVersion;
}

QList<MacTweakDef> MacTweaksCatalog::supportedFor(const QVersionNumber &osVersion)
{
    QList<MacTweakDef> out;
    for (const MacTweakDef &t : all()) {
        if (isSupported(t, osVersion))
            out << t;
    }
    return out;
}

MacDefaultsReadResult MacTweaksCatalog::readCurrent(const MacTweakDef &tweak)
{
    return MacDefaultsTool::readValue(tweak.domain, tweak.key, tweak.type);
}

QVariant MacTweaksCatalog::effectiveValue(const MacTweakDef &tweak, const MacDefaultsReadResult &read)
{
    return read.found ? read.value : tweak.defaultValue;
}

MacDefaultsWriteResult MacTweaksCatalog::toggleBoolTweak(const MacTweakDef &tweak)
{
    if (tweak.type != MacDefaultsValueType::Bool) {
        MacDefaultsWriteResult r;
        r.ok = false;
        r.errorMsg = QObject::tr("\"%1\" is not a toggleable preference.").arg(tweak.name);
        return r;
    }

    const MacDefaultsReadResult read = readCurrent(tweak);
    const QVariant effective = effectiveValue(tweak, read);
    const QVariant next = (effective == tweak.enabledValue) ? tweak.disabledValue : tweak.enabledValue;
    return MacDefaultsTool::writeValue(tweak.domain, tweak.key, tweak.type, next,
                                        tweak.requiresSudo, tweak.killApps);
}

MacDefaultsWriteResult MacTweaksCatalog::resetToDefault(const MacTweakDef &tweak)
{
    return MacDefaultsTool::revertToDefault(tweak.domain, tweak.key, tweak.requiresSudo, tweak.killApps);
}
