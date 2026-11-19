#include "lifecycle_audit_log.h"

#include <algorithm>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QHash>
#include <QStandardPaths>

namespace LifecycleAuditLog {

namespace {

QString actionToString(Action action)
{
    switch (action) {
    case Action::PermanentlyDeleted:
        return QStringLiteral("permanently_deleted");
    case Action::Skipped:
        return QStringLiteral("skipped");
    case Action::Restored:
        return QStringLiteral("restored");
    case Action::MovedToTrash:
    default:
        return QStringLiteral("moved_to_trash");
    }
}

Action actionFromString(const QString &s)
{
    if (s == QLatin1String("permanently_deleted"))
        return Action::PermanentlyDeleted;
    if (s == QLatin1String("skipped"))
        return Action::Skipped;
    if (s == QLatin1String("restored"))
        return Action::Restored;
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
    obj[QStringLiteral("runSource")] = entry.runSource;
    obj[QStringLiteral("category")] = entry.category;
    obj[QStringLiteral("skipReason")] = entry.skipReason;
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
    out->runSource = obj.value(QStringLiteral("runSource")).toString();
    out->category = obj.value(QStringLiteral("category")).toString();
    out->skipReason = obj.value(QStringLiteral("skipReason")).toString();
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
    return appendAll({entry});
}

bool appendAll(const QList<Entry> &entries)
{
    if (entries.isEmpty())
        return true;

    const QString path = logFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    const bool isNewFile = !QFile::exists(path);

    QFile file(path);
    if (!file.open(QIODevice::Append | QIODevice::Text))
        return false;

    QByteArray lines;
    for (const Entry &entry : entries)
        lines += QJsonDocument(toJson(entry)).toJson(QJsonDocument::Compact) + '\n';
    const qint64 written = file.write(lines);
    file.close();

    if (isNewFile)
        lockDownPermissions(path);

    return written == lines.size();
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
    const int firstKeptByAge = [&]() -> int {
        for (int i = 0; i < all.size(); ++i) {
            if (all.at(i).timestamp.toUTC() >= cutoff)
                return i;
        }
        return static_cast<int>(all.size());
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

bool Run::isRestorable(const Entry &item) const
{
    return item.action == Action::MovedToTrash
        && !item.trashDestination.isEmpty()
        && !restoredDestinations.contains(item.trashDestination);
}

QList<Run> groupIntoRuns(const QList<Entry> &entries)
{
    QList<Run> runs;
    QHash<QString, int> indexByBatch;

    for (const Entry &entry : entries) {
        auto it = indexByBatch.find(entry.batchId);
        if (it == indexByBatch.end()) {
            Run run;
            run.batchId = entry.batchId;
            run.timestamp = entry.timestamp;
            run.runSource = entry.runSource;
            runs.append(run);
            it = indexByBatch.insert(entry.batchId, static_cast<int>(runs.size()) - 1);
        }
        Run &run = runs[it.value()];
        if (entry.timestamp < run.timestamp)
            run.timestamp = entry.timestamp;

        switch (entry.action) {
        case Action::Restored:
            run.restoredDestinations << entry.trashDestination;
            continue;
        case Action::Skipped:
            ++run.skippedCount;
            break;
        case Action::MovedToTrash:
        case Action::PermanentlyDeleted:
            ++run.removedCount;
            run.bytesFreed += entry.sizeBytes;
            break;
        }
        run.items.append(entry);
    }

    std::stable_sort(runs.begin(), runs.end(), [](const Run &a, const Run &b) {
        return a.timestamp > b.timestamp;
    });
    return runs;
}

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("LifecycleAuditLog", text);
}

bool pathExistsNoFollow(const QString &path)
{
    const QFileInfo fi(path);
    return fi.exists() || fi.isSymLink();
}

// Restore must never move an arbitrary path: the log is user-writable, so
// only accept sources that sit under a Trash directory (XDG "Trash" or
// "~/.Trash" / per-volume ".Trash-UID").
bool isInsideTrash(const QString &path)
{
    const QStringList parts = QDir::cleanPath(path).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (int i = 0; i + 1 < parts.size(); ++i) {
        if (parts.at(i) == QLatin1String("Trash") || parts.at(i).startsWith(QLatin1String(".Trash")))
            return true;
    }
    return false;
}

} // namespace

RestoreResult restore(const Entry &entry)
{
    RestoreResult result;

    if (entry.action != Action::MovedToTrash || entry.trashDestination.isEmpty()
        || entry.originalPath.isEmpty() || !isInsideTrash(entry.trashDestination)) {
        result.status = RestoreResult::Status::NotRestorable;
        result.message = tr("This item was not moved to the Trash, so it cannot be restored.");
        return result;
    }

    if (!pathExistsNoFollow(entry.trashDestination)) {
        result.status = RestoreResult::Status::TrashItemMissing;
        result.message = tr("The item is no longer in the Trash (it may have been emptied or restored already).");
        return result;
    }

    if (pathExistsNoFollow(entry.originalPath)) {
        result.status = RestoreResult::Status::DestinationExists;
        result.message = tr("Something already exists at the original location, so nothing was restored: %1")
                             .arg(entry.originalPath);
        return result;
    }

    QDir().mkpath(QFileInfo(entry.originalPath).absolutePath());
    if (!QFile::rename(entry.trashDestination, entry.originalPath)) {
        result.status = RestoreResult::Status::Failed;
        result.message = tr("Could not move the item back to %1.").arg(entry.originalPath);
        return result;
    }

#ifndef Q_OS_MACOS
    // XDG trash spec: <trash>/files/<name> pairs with <trash>/info/<name>.trashinfo.
    const QFileInfo trashed(entry.trashDestination);
    QDir infoDir(trashed.absoluteDir());
    if (infoDir.dirName() == QLatin1String("files") && infoDir.cdUp() && infoDir.cd(QStringLiteral("info")))
        QFile::remove(infoDir.filePath(trashed.fileName() + QStringLiteral(".trashinfo")));
#endif

    Entry restored = entry;
    restored.timestamp = QDateTime::currentDateTimeUtc();
    restored.action = Action::Restored;
    appendAll({restored});

    result.status = RestoreResult::Status::Restored;
    result.message = tr("Restored to %1").arg(entry.originalPath);
    return result;
}

} // namespace LifecycleAuditLog
