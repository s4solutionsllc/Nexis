#include "lifecycle_audit_log.h"

#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace LifecycleAuditLog {

namespace {

QString actionToString(Action action)
{
    switch (action) {
    case Action::PermanentlyDeleted:
        return QStringLiteral("permanently_deleted");
    case Action::MovedToTrash:
    default:
        return QStringLiteral("moved_to_trash");
    }
}

Action actionFromString(const QString &s)
{
    if (s == QLatin1String("permanently_deleted"))
        return Action::PermanentlyDeleted;
    return Action::MovedToTrash;
}

QJsonObject toJson(const Entry &entry)
{
    QJsonObject obj;
    obj[QStringLiteral("timestamp")] = entry.timestamp.toUTC().toString(Qt::ISODateWithMs);
    obj[QStringLiteral("batchId")] = entry.batchId;
    obj[QStringLiteral("originalPath")] = entry.originalPath;
    obj[QStringLiteral("canonicalPath")] = entry.canonicalPath;
    obj[QStringLiteral("action")] = actionToString(entry.action);
    obj[QStringLiteral("trashDestination")] = entry.trashDestination;
    obj[QStringLiteral("matchingRuleIds")] = QJsonArray::fromStringList(entry.matchingRuleIds);
    obj[QStringLiteral("confidenceScore")] = entry.confidenceScore;
    obj[QStringLiteral("sizeBytes")] = static_cast<double>(entry.sizeBytes);
    obj[QStringLiteral("nexisVersion")] = entry.nexisVersion;
    obj[QStringLiteral("processStopAction")] = entry.processStopAction;
    return obj;
}

bool fromJson(const QJsonObject &obj, Entry *out)
{
    if (obj.isEmpty())
        return false;

    out->timestamp = QDateTime::fromString(obj.value(QStringLiteral("timestamp")).toString(), Qt::ISODateWithMs);
    if (!out->timestamp.isValid())
        return false;

    out->batchId = obj.value(QStringLiteral("batchId")).toString();
    out->originalPath = obj.value(QStringLiteral("originalPath")).toString();
    out->canonicalPath = obj.value(QStringLiteral("canonicalPath")).toString();
    out->action = actionFromString(obj.value(QStringLiteral("action")).toString());
    out->trashDestination = obj.value(QStringLiteral("trashDestination")).toString();

    out->matchingRuleIds.clear();
    for (const QJsonValue &v : obj.value(QStringLiteral("matchingRuleIds")).toArray())
        out->matchingRuleIds << v.toString();

    out->confidenceScore = obj.value(QStringLiteral("confidenceScore")).toInt(-1);
    out->sizeBytes = static_cast<quint64>(obj.value(QStringLiteral("sizeBytes")).toDouble(0));
    out->nexisVersion = obj.value(QStringLiteral("nexisVersion")).toString();
    out->processStopAction = obj.value(QStringLiteral("processStopAction")).toString();
    return true;
}

void lockDownPermissions(const QString &path)
{
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

} // namespace

QString logFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dir + QStringLiteral("/lifecycle_audit.jsonl");
}

bool append(const Entry &entry)
{
    const QString path = logFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    const bool isNewFile = !QFile::exists(path);

    QFile file(path);
    if (!file.open(QIODevice::Append | QIODevice::Text))
        return false;

    const QByteArray line = QJsonDocument(toJson(entry)).toJson(QJsonDocument::Compact) + '\n';
    const qint64 written = file.write(line);
    file.close();

    if (isNewFile)
        lockDownPermissions(path);

    return written == line.size();
}

QList<Entry> readAll()
{
    QList<Entry> entries;
    QFile file(logFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return entries;

    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        if (line.isEmpty())
            continue;

        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject())
            continue; // skip malformed lines rather than aborting the read

        Entry entry;
        if (fromJson(doc.object(), &entry))
            entries.append(entry);
    }
    return entries;
}

void prune(int minRetainedCount)
{
    const QList<Entry> all = readAll();
    if (all.isEmpty())
        return;

    const QDateTime cutoff = QDateTime::currentDateTimeUtc().addDays(-90);
    const int firstKeptByAge = [&]() {
        for (int i = 0; i < all.size(); ++i) {
            if (all.at(i).timestamp.toUTC() >= cutoff)
                return i;
        }
        return all.size();
    }();

    // CISO §3: retain at minimum the last N operations OR 90 days,
    // whichever is longer — so the kept set is the union of "recent by
    // age" and "most recent N", not their intersection.
    const int firstKeptByCount = qMax(0, all.size() - minRetainedCount);
    const int firstKept = qMin(firstKeptByAge, firstKeptByCount);

    if (firstKept == 0)
        return; // nothing to prune

    QSaveFile out(logFilePath());
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    for (int i = firstKept; i < all.size(); ++i) {
        const QByteArray line = QJsonDocument(toJson(all.at(i))).toJson(QJsonDocument::Compact) + '\n';
        out.write(line);
    }
    out.commit();

    lockDownPermissions(logFilePath());
}

} // namespace LifecycleAuditLog
