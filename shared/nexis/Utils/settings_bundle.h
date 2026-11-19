#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

class SettingManager;

// SSO-27150: versioned settings export/import so a Nexis setup can be backed
// up or moved to another machine. The bundle is a JSON document holding an
// allow-listed subset of settings.ini plus the user's cleaning profiles.
// Anything not on the allow-list (window geometry, sensor ids, usage history,
// trusted-binder prefixes, WoL hosts, schedules, auto-clean targets) is never
// written to a bundle and is reported as unknown when found in one.
struct SettingsBundleReport {
    bool ok = false;                 // false: the bundle was refused outright
    QString error;                   // why, when !ok
    int formatVersion = 0;

    QStringList applied;             // setting keys written (or that would be)
    QStringList unknownKeys;         // not on the allow-list; ignored
    QStringList invalidKeys;         // "<key>: <reason>"; ignored
    QStringList profilesApplied;     // profile ids written (or that would be)
    QStringList profilesRejected;    // "<id or index>: <reason>"; ignored
};

class SettingsBundle
{
public:
    static constexpr int kFormatVersion = 1;

    // Keys a bundle may carry, in export order.
    static QStringList exportableKeys();

    // Serialises the allow-listed settings that are explicitly set in sm, and
    // every valid user profile under profilesDir.
    static QByteArray exportBundle(const SettingManager *sm,
                                   const QString &profilesDir,
                                   const QString &appVersion);

    // Validates data and, unless dryRun, writes it. Valid entries are applied
    // even when others are invalid; the report lists what was skipped. A
    // bundle that is malformed or from a newer format version is refused whole.
    // Cleaner exclusions and cookie keep-domains merge into the existing lists
    // so an import can never remove protection; every other key is replaced.
    static SettingsBundleReport importBundle(const QByteArray &data,
                                             SettingManager *sm,
                                             const QString &profilesDir,
                                             bool dryRun);
};
