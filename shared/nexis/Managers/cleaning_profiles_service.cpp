#include "cleaning_profiles_service.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QDateTime>
#include <QDebug>

CleaningProfilesService *CleaningProfilesService::instance = nullptr;

CleaningProfilesService::CleaningProfilesService() : QObject(nullptr) {}

CleaningProfilesService *CleaningProfilesService::ins()
{
    if (!instance)
        instance = new CleaningProfilesService;
    return instance;
}

QString CleaningProfilesService::currentPlatform()
{
#ifdef Q_OS_MACOS
    return QStringLiteral("macos");
#else
    return QStringLiteral("linux");
#endif
}

QStringList CleaningProfilesService::defaultBundledProfileRoots()
{
    return { QStringLiteral(":/cleaning_profiles/") + currentPlatform() };
}

QString CleaningProfilesService::defaultUserProfilesDir()
{
    // Live under the same config root as exclusions/schedules so a single
    // backup of the Nexis config preserves user profile customizations.
    const QString cfg = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return cfg + QStringLiteral("/cleaning_profiles");
}

CleaningProfilesService::Safety
CleaningProfilesService::safetyFromString(const QString &s, bool *ok)
{
    const QString v = s.trimmed().toLower();
    if (v == "safe") { if (ok) *ok = true; return Safety::Safe; }
    if (v == "aggressive") { if (ok) *ok = true; return Safety::Aggressive; }
    if (ok) *ok = false;
    return Safety::Safe;
}

QString CleaningProfilesService::safetyToString(Safety s)
{
    return (s == Safety::Safe) ? QStringLiteral("safe") : QStringLiteral("aggressive");
}

bool CleaningProfilesService::parseProfile(const QByteArray &json,
                                           Profile &out,
                                           ValidationError &err)
{
    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
        err.message = QStringLiteral("JSON parse error: ") + parseErr.errorString();
        return false;
    }
    if (!doc.isObject()) {
        err.message = QStringLiteral("Profile root must be a JSON object");
        return false;
    }

    const QJsonObject obj = doc.object();

    auto requireString = [&](const QString &key, QString &dst) -> bool {
        if (!obj.contains(key) || !obj.value(key).isString()) {
            err.message = QStringLiteral("Missing or non-string field: ") + key;
            return false;
        }
        dst = obj.value(key).toString().trimmed();
        if (dst.isEmpty()) {
            err.message = QStringLiteral("Empty required field: ") + key;
            return false;
        }
        return true;
    };

    if (!requireString("id", out.id)) return false;
    err.id = out.id;
    if (!requireString("name", out.name)) return false;

    // `app` defaults to `name` if not provided.
    if (obj.contains("app") && obj.value("app").isString())
        out.app = obj.value("app").toString().trimmed();
    if (out.app.isEmpty())
        out.app = out.name;

    if (obj.contains("description") && obj.value("description").isString())
        out.description = obj.value("description").toString();

    // platforms: optional. Empty list = both.
    if (obj.contains("platforms")) {
        if (!obj.value("platforms").isArray()) {
            err.message = QStringLiteral("Field 'platforms' must be an array");
            return false;
        }
        const QJsonArray arr = obj.value("platforms").toArray();
        for (const QJsonValue &v : arr) {
            if (!v.isString()) {
                err.message = QStringLiteral("Field 'platforms' entries must be strings");
                return false;
            }
            const QString p = v.toString().trimmed().toLower();
            if (p != "linux" && p != "macos") {
                err.message = QStringLiteral("Unknown platform value: ") + p;
                return false;
            }
            out.platforms.append(p);
        }
    }

    // safety: optional, defaults to "safe"
    if (obj.contains("safety")) {
        if (!obj.value("safety").isString()) {
            err.message = QStringLiteral("Field 'safety' must be a string");
            return false;
        }
        bool ok = false;
        out.safety = safetyFromString(obj.value("safety").toString(), &ok);
        if (!ok) {
            err.message = QStringLiteral("Invalid safety value (expected 'safe' or 'aggressive')");
            return false;
        }
    }

    // paths: required, non-empty array of strings
    if (!obj.contains("paths") || !obj.value("paths").isArray()) {
        err.message = QStringLiteral("Missing required array field: paths");
        return false;
    }
    const QJsonArray paths = obj.value("paths").toArray();
    if (paths.isEmpty()) {
        err.message = QStringLiteral("Field 'paths' must contain at least one entry");
        return false;
    }
    for (const QJsonValue &v : paths) {
        if (!v.isString()) {
            err.message = QStringLiteral("Field 'paths' entries must be strings");
            return false;
        }
        const QString p = v.toString().trimmed();
        if (p.isEmpty()) {
            err.message = QStringLiteral("Empty path entry");
            return false;
        }
        out.paths.append(p);
    }

    // minAgeDays: optional, non-negative integer
    if (obj.contains("minAgeDays")) {
        const QJsonValue v = obj.value("minAgeDays");
        if (!v.isDouble()) {
            err.message = QStringLiteral("Field 'minAgeDays' must be an integer");
            return false;
        }
        const int age = v.toInt(-1);
        if (age < 0) {
            err.message = QStringLiteral("Field 'minAgeDays' must be >= 0");
            return false;
        }
        out.minAgeDays = age;
    }

    return true;
}

QString CleaningProfilesService::defaultResolverImpl(const QString &placeholder)
{
    const QString key = placeholder.toUpper();
    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    if (key == "HOME")
        return home;
    if (key == "XDG_CACHE_HOME")
        return env.contains("XDG_CACHE_HOME") ? env.value("XDG_CACHE_HOME") : home + "/.cache";
    if (key == "XDG_CONFIG_HOME")
        return env.contains("XDG_CONFIG_HOME") ? env.value("XDG_CONFIG_HOME") : home + "/.config";
    if (key == "XDG_DATA_HOME")
        return env.contains("XDG_DATA_HOME") ? env.value("XDG_DATA_HOME") : home + "/.local/share";
    if (key == "TMPDIR")
        return env.contains("TMPDIR") ? env.value("TMPDIR") : QDir::tempPath();
    if (key == "LIBRARY")
        return home + "/Library";

    // Unknown placeholder — fall back to the live environment so profile
    // authors can opt into any env var without code changes.
    if (env.contains(key))
        return env.value(key);
    return QString();
}

QString CleaningProfilesService::resolvePlaceholders(const QString &input,
                                                     const PathResolver &resolver) const
{
    QString s = input;
    if (s.startsWith("~/") || s == "~")
        s.replace(0, 1, QStringLiteral("$HOME"));

    // Resolve $VAR / ${VAR} tokens. Loop until no more substitutions to handle
    // nested expansions; cap iterations to avoid pathological cycles.
    static const QRegularExpression varRe(QStringLiteral("\\$\\{?([A-Za-z_][A-Za-z0-9_]*)\\}?"));
    for (int i = 0; i < 8; ++i) {
        QRegularExpressionMatch m = varRe.match(s);
        if (!m.hasMatch())
            break;
        const QString name = m.captured(1);
        const QString val = resolver ? resolver(name) : defaultResolverImpl(name);
        // Empty resolution → leave the token in place but stop expanding it,
        // otherwise globExpand will produce a literal "$VAR" filename which
        // will simply not match anything (the right behavior — the profile
        // silently expands to zero entries on that token).
        if (val.isEmpty()) {
            s.replace(m.capturedStart(), m.capturedLength(), QStringLiteral("<<UNRESOLVED>>"));
            continue;
        }
        s.replace(m.capturedStart(), m.capturedLength(), val);
    }
    return s;
}

// Expand a single pattern. The pattern may contain `*`, `?` wildcards in any
// segment. We walk the path segment-by-segment using QDir's name filters so
// nested wildcards (`$HOME/.config/*/Cache`) work.
QFileInfoList CleaningProfilesService::globExpand(const QString &pattern,
                                                  int minAgeDays) const
{
    if (pattern.contains(QStringLiteral("<<UNRESOLVED>>")))
        return {};

    QStringList segments = pattern.split('/', Qt::SkipEmptyParts);
    if (segments.isEmpty())
        return {};

    // Track absolute candidates. Seeded with "/" for absolute patterns.
    QStringList current;
    if (pattern.startsWith('/'))
        current << QStringLiteral("/");
    else
        current << QDir::currentPath();

    for (int i = 0; i < segments.size(); ++i) {
        const QString seg = segments.at(i);
        const bool isLast = (i == segments.size() - 1);
        QStringList next;
        const bool hasWildcard = seg.contains('*') || seg.contains('?') || seg.contains('[');

        for (const QString &base : std::as_const(current)) {
            if (!hasWildcard) {
                const QString joined = (base == "/") ? "/" + seg : base + "/" + seg;
                // Don't filter out non-existent intermediate dirs — a non-
                // existent terminal path is a no-op for the cleaner.
                if (QFileInfo::exists(joined) || isLast)
                    next << joined;
                continue;
            }

            QDir dir(base);
            if (!dir.exists())
                continue;
            const QFileInfoList kids = dir.entryInfoList(
                {seg},
                QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
            for (const QFileInfo &k : kids)
                next << k.absoluteFilePath();
        }

        current = next;
        if (current.isEmpty())
            return {};
    }

    QDateTime cutoff;
    if (minAgeDays > 0)
        cutoff = QDateTime::currentDateTime().addDays(-minAgeDays);

    QFileInfoList result;
    for (const QString &path : std::as_const(current)) {
        QFileInfo fi(path);
        if (!fi.exists() && !fi.isSymLink())
            continue;
        if (minAgeDays > 0) {
            const QDateTime mt = fi.lastModified();
            if (mt.isValid() && mt > cutoff)
                continue;
        }
        result.append(fi);
    }
    return result;
}

QFileInfoList CleaningProfilesService::expandPaths(const Profile &profile,
                                                   const PathResolver &resolver) const
{
    QFileInfoList result;
    for (const QString &raw : profile.paths) {
        const QString resolved = resolvePlaceholders(raw, resolver);
        result.append(globExpand(resolved, profile.minAgeDays));
    }
    return result;
}

QFileInfoList CleaningProfilesService::scan(const QList<Profile> &profiles,
                                            bool allowAggressive,
                                            const std::function<bool(const QString &)> &isExcluded,
                                            QHash<QString, QFileInfoList> *perProfile,
                                            const PathResolver &resolver) const
{
    QFileInfoList all;
    for (const Profile &p : profiles) {
        // Skip aggressive profiles unless explicitly enabled.
        if (p.safety == Safety::Aggressive && !allowAggressive)
            continue;

        const QFileInfoList raw = expandPaths(p, resolver);
        QFileInfoList filtered;
        for (const QFileInfo &fi : raw) {
            const QString abs = fi.absoluteFilePath();
            if (isExcluded && isExcluded(abs))
                continue;
            filtered.append(fi);
        }
        if (perProfile)
            (*perProfile)[p.id] = filtered;
        all.append(filtered);
    }
    return all;
}

QList<CleaningProfilesService::Profile>
CleaningProfilesService::readProfilesFromDir(const QString &dir,
                                             const QString &platform)
{
    QList<Profile> out;
    QDir d(dir);
    if (!d.exists())
        return out;

    QStringList files;
    if (dir.startsWith(':')) {
        // Qt resource path: QDirIterator handles ":/..." prefixes.
        QDirIterator it(dir, QStringList() << "*.json", QDir::Files);
        while (it.hasNext())
            files << it.next();
    } else {
        const QFileInfoList entries = d.entryInfoList(
            QStringList() << "*.json", QDir::Files);
        for (const QFileInfo &fi : entries)
            files << fi.absoluteFilePath();
    }

    for (const QString &path : files) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            ValidationError err;
            err.file = path;
            err.message = QStringLiteral("Cannot open profile: ") + f.errorString();
            mLastErrors.append(err);
            continue;
        }
        const QByteArray blob = f.readAll();
        f.close();

        Profile p;
        ValidationError err;
        err.file = path;
        if (!parseProfile(blob, p, err)) {
            mLastErrors.append(err);
            continue;
        }
        // Filter by platform. An empty `platforms` list means "both".
        if (!p.platforms.isEmpty() && !p.platforms.contains(platform))
            continue;
        p.source = path;
        out.append(p);
    }
    return out;
}

QList<CleaningProfilesService::Profile> CleaningProfilesService::loadFrom(
    const QStringList &bundledRoots,
    const QStringList &userRoots,
    const QString &platform)
{
    mLastErrors.clear();
    QList<Profile> bundled;
    for (const QString &root : bundledRoots)
        bundled.append(readProfilesFromDir(root, platform));

    QList<Profile> user;
    for (const QString &root : userRoots)
        user.append(readProfilesFromDir(root, platform));

    // User profiles override bundled by id; preserve bundled order otherwise.
    QHash<QString, Profile> userById;
    for (const Profile &p : user)
        userById.insert(p.id, p);

    QList<Profile> result;
    QSet<QString> seenIds;
    for (const Profile &p : bundled) {
        if (userById.contains(p.id)) {
            result.append(userById.value(p.id));
        } else {
            result.append(p);
        }
        seenIds.insert(p.id);
    }
    // Append remaining user-only profiles.
    for (const Profile &p : user) {
        if (!seenIds.contains(p.id)) {
            result.append(p);
            seenIds.insert(p.id);
        }
    }
    return result;
}

QList<CleaningProfilesService::Profile> CleaningProfilesService::loadAll()
{
    return loadFrom(defaultBundledProfileRoots(),
                    { defaultUserProfilesDir() },
                    currentPlatform());
}
