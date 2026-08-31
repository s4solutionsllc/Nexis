#ifndef MAC_TWEAKS_CATALOG_H
#define MAC_TWEAKS_CATALOG_H

// SSO-23857: catalog of hidden macOS `defaults` preferences exposed by the
// Tweaks pane (Finder / Dock / Screenshots / Animations / Login Window).
// Pure data + pure/CommandUtil-backed orchestration — no Qt Widgets
// dependency, shared by both the Tweaks pane UI and the command palette.

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVersionNumber>

#include "nexis-core_global.h"
#include "mac_defaults_tool.h"

// One selectable value for a String-typed tweak with a fixed set of choices
// (e.g. screenshot format). Empty options list means free text instead.
struct NEXISCORESHARED_EXPORT MacTweakOption {
    QString  label;
    QVariant value;
};

struct NEXISCORESHARED_EXPORT MacTweakDef {
    QString id;                    // stable id, e.g. "finder.show_hidden_files"
    QString category;              // "Finder" | "Dock" | "Screenshots" | "Animations" | "Login Window"
    QString name;                  // display name
    QString description;           // shown under the name; documents the system default
    QString domain;                // `defaults` domain, e.g. "com.apple.finder" or a plist path
    QString key;                   // `defaults` key within the domain
    MacDefaultsValueType type = MacDefaultsValueType::Bool;
    QVariant defaultValue;         // documented system default (used for display + toggle logic)
    QVariant enabledValue = true;  // Bool tweaks: value written when turned on
    QVariant disabledValue = false;// Bool tweaks: value written when turned off
    QList<MacTweakOption> options; // String tweaks with a fixed set of choices; empty == free text
    QStringList killApps;          // processes to `killall` after a successful write/revert
    bool requiresSudo = false;     // domain lives outside the user's preference domain
    QVersionNumber minOsVersion;   // null/invalid == supported on every version

    bool hasVersionGate() const { return !minOsVersion.isNull(); }
};

class NEXISCORESHARED_EXPORT MacTweaksCatalog
{
public:
    static QList<MacTweakDef> all();
    static QStringList categories();
    static const MacTweakDef *findById(const QString &id);

    // Per-OS-version gating (SSO-23857 acceptance criteria): tweaks whose
    // minOsVersion exceeds osVersion are unsupported on that OS and must be
    // hidden/disabled with an explanation, not silently no-op.
    static bool isSupported(const MacTweakDef &tweak, const QVersionNumber &osVersion);
    static QList<MacTweakDef> supportedFor(const QVersionNumber &osVersion);

    // Orchestration helpers shared by the widget and the command palette.
    static MacDefaultsReadResult readCurrent(const MacTweakDef &tweak);
    // Effective value: the read value if explicitly set, else the documented default.
    static QVariant effectiveValue(const MacTweakDef &tweak, const MacDefaultsReadResult &read);
    // Bool tweaks only — flips enabled/disabled based on the current effective value.
    static MacDefaultsWriteResult toggleBoolTweak(const MacTweakDef &tweak);
    static MacDefaultsWriteResult resetToDefault(const MacTweakDef &tweak);
};

#endif // MAC_TWEAKS_CATALOG_H
