#include "settings_bundle.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QVariant>

#include "Managers/cleaning_profiles_service.h"
#include "Managers/setting_manager.h"

namespace {

const QString kFormatName = QStringLiteral("nexis-settings");
constexpr int kMaxTextLen = 256;
constexpr int kMaxJsonLen = 256 * 1024;
constexpr int kMaxListEntries = 1000;
constexpr int kMaxProfiles = 500;

enum class Kind { Bool, Int, Identifier, Text, Json, StringList };

struct Spec {
    QString key;
    Kind kind;
    int min = 0;
    int max = 0;
};

const QList<Spec> &specs()
{
    using namespace SettingKeys;
    static const QList<Spec> kSpecs = {
        // General / appearance
        {Language, Kind::Identifier}, {ThemeName, Kind::Identifier},
        {ColorScheme, Kind::Identifier}, {AppFont, Kind::Text},
        {TrayIconStyle, Kind::Identifier}, {StartPage, Kind::Identifier},
        {DashboardLayout, Kind::Json}, {DashboardFooterVisible, Kind::Bool},
        {SidebarCollapsed, Kind::Bool}, {SidebarSectionsCollapsed, Kind::Json},
        {MinimizeToTray, Kind::Bool}, {StartMinimizedToTray, Kind::Bool},
        {AppQuitDialogDontAsk, Kind::Bool}, {AppQuitDialogChoice, Kind::Identifier},
        {DiskAnalyzerTool, Kind::Identifier}, {KioskMode, Kind::Bool},
        {LaunchInKioskMode, Kind::Bool}, {MiniMonitorVisible, Kind::Bool},
        {TrayHealthScoreEnabled, Kind::Bool},
        // Alerts
        {CPUAlertPercent, Kind::Int, 0, 100}, {MemoryAlertPercent, Kind::Int, 0, 100},
        {DiskAlertPercent, Kind::Int, 0, 100}, {BatteryAlertPercent, Kind::Int, 0, 100},
        {CPUAlertSeconds, Kind::Int, 0, 86400}, {MemoryAlertSeconds, Kind::Int, 0, 86400},
        {TempAlertCelsius, Kind::Int, 0, 200}, {TempAlertSeconds, Kind::Int, 0, 86400},
        {DiskHealthAlertEnabled, Kind::Bool}, {ThresholdAlertEnabled, Kind::Bool},
        {ThresholdGB, Kind::Int, 0, 100000}, {UpdateAlertEnabled, Kind::Bool},
        {UpdateCheckIntervalMinutes, Kind::Int, 1, 525600},
        {NetCapGB, Kind::Int, 0, 1000000}, {NetCapAlertEnabled, Kind::Bool},
        {NetCapResetDay, Kind::Int, 1, 31}, {CleaningNotificationsEnabled, Kind::Bool},
        // Cleaner
        {CleanerExclusions, Kind::Json}, {CleanerCookieKeepDomains, Kind::StringList},
        {PreCleanSnapshotEnabled, Kind::Bool}, {CleanerAggressiveProfilesEnabled, Kind::Bool},
        // Processes / history
        {ProcessPinnedNames, Kind::Json}, {ProcessThresholds, Kind::Json},
        {ResourceHistoryEnabled, Kind::Bool}, {ResourceHistoryRetentionDays, Kind::Int, 1, 365},
    };
    return kSpecs;
}

const Spec *findSpec(const QString &key)
{
    for (const Spec &s : specs())
        if (s.key == key)
            return &s;
    return nullptr;
}

bool hasControlChars(const QString &s)
{
    for (const QChar c : s)
        if (c.category() == QChar::Other_Control)
            return true;
    return false;
}

bool validText(const QString &s, int maxLen)
{
    return !s.isEmpty() && s.size() <= maxLen && !hasControlChars(s);
}

QString identifierProblem(const QString &s)
{
    static const QRegularExpression re(QStringLiteral("^[\\w .@+-]{1,64}$"));
    return re.match(s).hasMatch() ? QString() : QStringLiteral("not a valid identifier");
}

QJsonDocument parseJson(const QString &text, QString *why)
{
    if (text.size() > kMaxJsonLen) {
        *why = QStringLiteral("value too large");
        return {};
    }
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &pe);
    if (pe.error != QJsonParseError::NoError || doc.isNull()) {
        *why = QStringLiteral("not valid JSON");
        return {};
    }
    return doc;
}

bool validExclusions(const QJsonArray &arr, QString *why)
{
    if (arr.size() > kMaxListEntries) {
        *why = QStringLiteral("too many entries");
        return false;
    }
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        const QString type = o.value("type").toString();
        if ((type != "file" && type != "folder") || !validText(o.value("path").toString(), 4096)) {
            *why = QStringLiteral("entries must be {type: file|folder, path: text}");
            return false;
        }
    }
    return true;
}

// Checks one bundle value against its spec. On success *out holds the value
// in the form settings.ini stores it.
bool checkValue(const Spec &spec, const QJsonValue &v, QVariant *out, QString *why)
{
    switch (spec.kind) {
    case Kind::Bool:
        if (!v.isBool()) { *why = QStringLiteral("expected true/false"); return false; }
        *out = v.toBool();
        return true;
    case Kind::Int: {
        const double d = v.toDouble(-1e18);
        if (!v.isDouble() || d != static_cast<int>(d) || d < spec.min || d > spec.max) {
            *why = QStringLiteral("expected an integer in %1..%2").arg(spec.min).arg(spec.max);
            return false;
        }
        *out = static_cast<int>(d);
        return true;
    }
    case Kind::Identifier:
    case Kind::Text: {
        if (!v.isString()) { *why = QStringLiteral("expected text"); return false; }
        QString s = v.toString();
        if (spec.kind == Kind::Identifier) {
            *why = identifierProblem(s);
            if (!why->isEmpty()) return false;
            if (spec.key == SettingKeys::StartPage)
                s = SettingManager::migrateStartPageId(s);
        } else if (!validText(s, kMaxTextLen)) {
            *why = QStringLiteral("invalid text");
            return false;
        }
        *out = s;
        return true;
    }
    case Kind::StringList: {
        if (!v.isArray() || v.toArray().size() > kMaxListEntries) {
            *why = QStringLiteral("expected a list of text");
            return false;
        }
        QStringList list;
        for (const QJsonValue &e : v.toArray()) {
            if (!e.isString() || !validText(e.toString(), kMaxTextLen)) {
                *why = QStringLiteral("expected a list of text");
                return false;
            }
            list << e.toString();
        }
        *out = list;
        return true;
    }
    case Kind::Json: {
        // Bundles carry JSON settings as real JSON, not as an escaped string.
        const QJsonDocument doc = v.isArray() ? QJsonDocument(v.toArray())
                                  : v.isObject() ? QJsonDocument(v.toObject()) : QJsonDocument();
        if (doc.isNull()) { *why = QStringLiteral("expected a JSON array or object"); return false; }
        if (spec.key == SettingKeys::CleanerExclusions
            && (!v.isArray() || !validExclusions(v.toArray(), why)))
            return false;
        const QString text = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
        if (text.size() > kMaxJsonLen) { *why = QStringLiteral("value too large"); return false; }
        *out = text;
        return true;
    }
    }
    return false;
}

QJsonValue exportValue(const Spec &spec, const QVariant &raw)
{
    switch (spec.kind) {
    case Kind::Bool: return raw.toBool();
    case Kind::Int: return raw.toInt();
    case Kind::StringList: return QJsonArray::fromStringList(raw.toStringList());
    case Kind::Json: {
        QString why;
        const QJsonDocument doc = parseJson(raw.toString(), &why);
        return doc.isArray() ? QJsonValue(doc.array())
               : doc.isObject() ? QJsonValue(doc.object()) : QJsonValue();
    }
    default: return raw.toString();
    }
}

QString profileFileName(const QString &id)
{
    static const QRegularExpression bad(QStringLiteral("[^A-Za-z0-9._-]"));
    QString name = id;
    name.replace(bad, QStringLiteral("_"));
    while (name.startsWith('.'))
        name.remove(0, 1);
    return name.isEmpty() ? QString() : name.left(100) + QStringLiteral(".json");
}

QStringList unionOf(const QStringList &existing, const QStringList &added)
{
    QStringList out = existing;
    for (const QString &s : added)
        if (!out.contains(s))
            out << s;
    return out;
}

QString mergeExclusions(const QString &existingJson, const QString &importedJson)
{
    QJsonArray merged = QJsonDocument::fromJson(existingJson.toUtf8()).array();
    QSet<QString> seen;
    for (const QJsonValue &v : merged)
        seen.insert(v.toObject().value("path").toString());
    for (const QJsonValue &v : QJsonDocument::fromJson(importedJson.toUtf8()).array()) {
        const QString path = v.toObject().value("path").toString();
        if (!seen.contains(path)) {
            seen.insert(path);
            merged.append(v);
        }
    }
    return QString::fromUtf8(QJsonDocument(merged).toJson(QJsonDocument::Compact));
}

} // namespace

QStringList SettingsBundle::exportableKeys()
{
    QStringList keys;
    for (const Spec &s : specs())
        keys << s.key;
    return keys;
}

QByteArray SettingsBundle::exportBundle(const SettingManager *sm,
                                        const QString &profilesDir,
                                        const QString &appVersion)
{
    QJsonObject settings;
    for (const Spec &s : specs()) {
        if (!sm->hasValue(s.key))
            continue;
        const QJsonValue v = exportValue(s, sm->rawValue(s.key));
        if (!v.isNull())
            settings.insert(s.key, v);
    }

    QJsonArray profiles;
    const QFileInfoList files = QDir(profilesDir).entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QFileInfo &fi : files) {
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QByteArray blob = f.readAll();
        CleaningProfilesService::Profile p;
        CleaningProfilesService::ValidationError err;
        if (CleaningProfilesService::parseProfile(blob, p, err))
            profiles.append(QJsonDocument::fromJson(blob).object());
    }

    QJsonObject root;
    root["format"] = kFormatName;
    root["version"] = kFormatVersion;
    root["appVersion"] = appVersion;
    root["exportedAt"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    root["settings"] = settings;
    root["cleaningProfiles"] = profiles;
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

SettingsBundleReport SettingsBundle::importBundle(const QByteArray &data,
                                                  SettingManager *sm,
                                                  const QString &profilesDir,
                                                  bool dryRun)
{
    SettingsBundleReport r;

    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        r.error = QStringLiteral("Not a Nexis settings file (invalid JSON).");
        return r;
    }
    const QJsonObject root = doc.object();
    if (root.value("format").toString() != kFormatName) {
        r.error = QStringLiteral("Not a Nexis settings file.");
        return r;
    }
    const QJsonValue ver = root.value("version");
    if (!ver.isDouble() || ver.toDouble() < 1 || ver.toDouble() != static_cast<int>(ver.toDouble())) {
        r.error = QStringLiteral("Settings file has no valid format version.");
        return r;
    }
    r.formatVersion = ver.toInt();
    if (r.formatVersion > kFormatVersion) {
        r.error = QStringLiteral("Settings file is format version %1; this Nexis reads up to version %2. "
                                 "Update Nexis to import it.").arg(r.formatVersion).arg(kFormatVersion);
        return r;
    }
    if (root.contains("settings") && !root.value("settings").isObject()) {
        r.error = QStringLiteral("Settings file is malformed ('settings' must be an object).");
        return r;
    }
    if (root.contains("cleaningProfiles") && !root.value("cleaningProfiles").isArray()) {
        r.error = QStringLiteral("Settings file is malformed ('cleaningProfiles' must be a list).");
        return r;
    }

    const QJsonObject settings = root.value("settings").toObject();
    QList<QPair<QString, QVariant>> toWrite;
    for (auto it = settings.constBegin(); it != settings.constEnd(); ++it) {
        const Spec *spec = findSpec(it.key());
        if (!spec) {
            r.unknownKeys << it.key();
            continue;
        }
        QVariant value;
        QString why;
        if (!checkValue(*spec, it.value(), &value, &why)) {
            r.invalidKeys << QStringLiteral("%1: %2").arg(it.key(), why);
            continue;
        }
        toWrite.append({it.key(), value});
        r.applied << it.key();
    }

    struct PendingProfile { QString id; QString fileName; QByteArray json; };
    QList<PendingProfile> profilesToWrite;
    const QJsonArray profiles = root.value("cleaningProfiles").toArray();
    QSet<QString> seenFiles;
    for (int i = 0; i < profiles.size(); ++i) {
        const QString label = QStringLiteral("#%1").arg(i + 1);
        if (i >= kMaxProfiles) {
            r.profilesRejected << QStringLiteral("%1: too many profiles").arg(label);
            continue;
        }
        const QByteArray blob = QJsonDocument(profiles.at(i).toObject()).toJson(QJsonDocument::Indented);
        CleaningProfilesService::Profile p;
        CleaningProfilesService::ValidationError err;
        if (!profiles.at(i).isObject() || !CleaningProfilesService::parseProfile(blob, p, err)) {
            r.profilesRejected << QStringLiteral("%1: %2").arg(err.id.isEmpty() ? label : err.id,
                                                             err.message.isEmpty() ? QStringLiteral("not a profile object") : err.message);
            continue;
        }
        const QString fileName = profileFileName(p.id);
        if (fileName.isEmpty() || seenFiles.contains(fileName)) {
            r.profilesRejected << QStringLiteral("%1: unusable or duplicate id").arg(p.id);
            continue;
        }
        seenFiles.insert(fileName);
        profilesToWrite.append({p.id, fileName, blob});
        r.profilesApplied << p.id;
    }

    r.ok = true;
    if (dryRun)
        return r;

    for (const auto &kv : toWrite) {
        QVariant value = kv.second;
        if (kv.first == SettingKeys::CleanerExclusions)
            value = mergeExclusions(sm->rawValue(kv.first).toString(), value.toString());
        else if (kv.first == SettingKeys::CleanerCookieKeepDomains)
            value = unionOf(sm->rawValue(kv.first).toStringList(), value.toStringList());
        sm->setRawValue(kv.first, value);
    }

    if (!profilesToWrite.isEmpty()) {
        QDir().mkpath(profilesDir);
        for (const PendingProfile &pp : profilesToWrite) {
            QSaveFile f(QDir(profilesDir).filePath(pp.fileName));
            if (!f.open(QIODevice::WriteOnly) || f.write(pp.json) != pp.json.size() || !f.commit()) {
                r.profilesRejected << QStringLiteral("%1: could not write file").arg(pp.id);
                r.profilesApplied.removeAll(pp.id);
            }
        }
    }
    return r;
}
