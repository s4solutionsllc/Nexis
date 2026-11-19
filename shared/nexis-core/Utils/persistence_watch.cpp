#include "persistence_watch.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>

namespace {

constexpr int kBaselineVersion = 1;

QHash<QString, QString> fingerprintsByKey(const QList<PersistenceEntry> &entries)
{
    QHash<QString, QString> map;
    for (const PersistenceEntry &e : entries)
        map.insert(e.key, e.fingerprint);
    return map;
}

} // namespace

QSet<QString> PersistenceDiff::flagged() const
{
    QSet<QString> set;
    for (const QString &k : added)
        set.insert(k);
    for (const QString &k : changed)
        set.insert(k);
    return set;
}

PersistenceWatch::PersistenceWatch(const QString &baselinePath)
    : mPath(baselinePath)
{
}

PersistenceDiff PersistenceWatch::diff(const QList<PersistenceEntry> &baseline,
                                       const QList<PersistenceEntry> &current)
{
    const QHash<QString, QString> before = fingerprintsByKey(baseline);
    const QHash<QString, QString> now = fingerprintsByKey(current);

    PersistenceDiff d;
    for (const PersistenceEntry &e : current) {
        const auto it = before.constFind(e.key);
        if (it == before.constEnd()) {
            if (!d.added.contains(e.key))
                d.added << e.key;
        } else if (it.value() != e.fingerprint && !d.changed.contains(e.key)) {
            d.changed << e.key;
        }
    }
    for (const PersistenceEntry &e : baseline) {
        if (!now.contains(e.key) && !d.removed.contains(e.key))
            d.removed << e.key;
    }
    return d;
}

PersistenceWatch::Evaluation PersistenceWatch::evaluate(const QList<PersistenceEntry> &current)
{
    Evaluation result;

    QList<PersistenceEntry> baseline;
    if (!load(&baseline)) {
        save(current);
        result.seeded = true;
        return result;
    }

    result.diff = diff(baseline, current);

    if (!result.diff.removed.isEmpty()) {
        const QSet<QString> gone(result.diff.removed.begin(), result.diff.removed.end());
        QList<PersistenceEntry> kept;
        for (const PersistenceEntry &e : baseline) {
            if (!gone.contains(e.key))
                kept << e;
        }
        save(kept);
    }
    return result;
}

bool PersistenceWatch::acknowledge(const QList<PersistenceEntry> &current,
                                   const QSet<QString> &keys)
{
    if (keys.isEmpty())
        return save(current);

    QList<PersistenceEntry> baseline;
    load(&baseline);

    QHash<QString, PersistenceEntry> merged;
    QStringList order;
    for (const PersistenceEntry &e : baseline) {
        if (!merged.contains(e.key))
            order << e.key;
        merged.insert(e.key, e);
    }
    for (const PersistenceEntry &e : current) {
        if (!keys.contains(e.key))
            continue;
        if (!merged.contains(e.key))
            order << e.key;
        merged.insert(e.key, e);
    }

    QList<PersistenceEntry> out;
    for (const QString &k : order)
        out << merged.value(k);
    return save(out);
}

QByteArray PersistenceWatch::serialize(const QList<PersistenceEntry> &entries)
{
    QJsonArray arr;
    for (const PersistenceEntry &e : entries) {
        arr.append(QJsonObject{
            {QStringLiteral("key"), e.key},
            {QStringLiteral("label"), e.label},
            {QStringLiteral("fingerprint"), e.fingerprint},
        });
    }
    const QJsonObject root{
        {QStringLiteral("version"), kBaselineVersion},
        {QStringLiteral("entries"), arr},
    };
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool PersistenceWatch::deserialize(const QByteArray &data, QList<PersistenceEntry> *out)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;

    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("version")).toInt() != kBaselineVersion
        || !root.value(QStringLiteral("entries")).isArray())
        return false;

    QList<PersistenceEntry> entries;
    for (const QJsonValue &v : root.value(QStringLiteral("entries")).toArray()) {
        const QJsonObject o = v.toObject();
        PersistenceEntry e;
        e.key = o.value(QStringLiteral("key")).toString();
        if (e.key.isEmpty())
            return false;
        e.label = o.value(QStringLiteral("label")).toString();
        e.fingerprint = o.value(QStringLiteral("fingerprint")).toString();
        entries << e;
    }
    if (out)
        *out = entries;
    return true;
}

QList<CronEntry> PersistenceWatch::parseCrontab(const QString &text)
{
    static const QRegularExpression envLine(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*\\s*="));
    static const QRegularExpression timeFields(
        QStringLiteral("^(\\S+\\s+\\S+\\s+\\S+\\s+\\S+\\s+\\S+)\\s+(.+)$"));
    static const QRegularExpression special(QStringLiteral("^(@\\S+)\\s+(.+)$"));

    QList<CronEntry> jobs;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')) || envLine.match(line).hasMatch())
            continue;

        QRegularExpressionMatch m = special.match(line);
        if (!m.hasMatch())
            m = timeFields.match(line);
        if (!m.hasMatch())
            continue;

        CronEntry job;
        job.schedule = m.captured(1);
        job.command = m.captured(2).trimmed();
        job.raw = line;
        jobs << job;
    }
    return jobs;
}

QString PersistenceWatch::keyFor(const StartupAppData &item)
{
    if (item.category == LoginItemCategory::CronJob)
        return QStringLiteral("cron:") + item.identifier;
    return QStringLiteral("file:") + item.filePath;
}

PersistenceEntry PersistenceWatch::entryFor(const StartupAppData &item)
{
    PersistenceEntry e;
    e.key = keyFor(item);
    e.label = item.name;
    if (item.category != LoginItemCategory::CronJob)
        e.fingerprint = fingerprintFile(item.filePath);
    return e;
}

StartupAppData PersistenceWatch::itemForCron(const CronEntry &cron)
{
    StartupAppData d;
    d.name = cron.command;
    d.filePath = cron.schedule;
    d.identifier = cron.raw;
    d.readOnly = true;
    d.category = LoginItemCategory::CronJob;
    return d;
}

QString PersistenceWatch::fingerprintContent(const QByteArray &content)
{
    static const QRegularExpression enableKey(
        QStringLiteral("^(Hidden|X-GNOME-Autostart-enabled)\\s*="),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression plistBool(QStringLiteral("^<(true|false)\\s*/>$"));

    const QStringList lines = QString::fromUtf8(content).split(QLatin1Char('\n'));
    QByteArray normalized;
    bool skipPlistValue = false;
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (skipPlistValue) {
            skipPlistValue = false;
            if (plistBool.match(line).hasMatch())
                continue;
        }
        if (line.isEmpty() || enableKey.match(line).hasMatch())
            continue;
        if (line == QLatin1String("<key>Disabled</key>")) {
            skipPlistValue = true;
            continue;
        }
        normalized += line.toUtf8();
        normalized += '\n';
    }
    return QString::fromLatin1(
        QCryptographicHash::hash(normalized, QCryptographicHash::Sha256).toHex());
}

QString PersistenceWatch::fingerprintFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    // Binary plists are hashed as-is: the enable-state stripping only
    // applies to text formats, which is what Nexis' own toggle writes.
    return fingerprintContent(f.readAll());
}

bool PersistenceWatch::load(QList<PersistenceEntry> *out) const
{
    QFile f(mPath);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    return deserialize(f.readAll(), out);
}

bool PersistenceWatch::save(const QList<PersistenceEntry> &entries) const
{
    QDir().mkpath(QFileInfo(mPath).absolutePath());
    QSaveFile f(mPath);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(serialize(entries));
    return f.commit();
}
