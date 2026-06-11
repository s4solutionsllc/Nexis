// SSO-3391 / WI-29: GNOME Settings has no valid mapping on macOS.
//
// Earlier revisions translated `org.gnome.desktop.interface` keys to
// `NSGlobalDomain` / `com.apple.dock` and wrote them with `defaults write`.
// That mapping table collapsed 9 unrelated GNOME keys onto
// `AppleInterfaceStyle` and several others onto dock `orientation`, so any
// caller would corrupt `NSGlobalDomain` rather than configure GNOME.
//
// The GNOME Settings page is hidden on macOS (`app.cpp` gates the sidebar
// button under `#ifdef Q_OS_MAC`), so this class should never be reached at
// runtime — it survives only to keep the shared `ToolManager` link.
// Every method is a hard no-op: `isAvailable()` reports false so the
// platform-neutral availability check short-circuits, and every setter
// returns false without invoking `defaults`. There is no code path here
// that can write into `NSGlobalDomain` or any other Apple preference
// domain.

#include "gnome_settings_tool_macos.h"

bool GnomeSettingsToolMacOS::isAvailable()
{
    return false;
}

bool GnomeSettingsToolMacOS::schemaExists(const QString &)
{
    return false;
}

QSet<QString> GnomeSettingsToolMacOS::cachedSchemas()
{
    return {};
}

QString GnomeSettingsToolMacOS::getS(const QString &, const QString &)
{
    return QString();
}

bool GnomeSettingsToolMacOS::getB(const QString &, const QString &)
{
    return false;
}

int GnomeSettingsToolMacOS::getI(const QString &, const QString &)
{
    return 0;
}

double GnomeSettingsToolMacOS::getD(const QString &, const QString &)
{
    return 0.0;
}

bool GnomeSettingsToolMacOS::setS(const QString &, const QString &, const QString &)
{
    return false;
}

bool GnomeSettingsToolMacOS::setB(const QString &, const QString &, bool)
{
    return false;
}

bool GnomeSettingsToolMacOS::setI(const QString &, const QString &, int)
{
    return false;
}

bool GnomeSettingsToolMacOS::setD(const QString &, const QString &, double)
{
    return false;
}
