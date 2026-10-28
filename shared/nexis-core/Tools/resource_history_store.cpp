#include "resource_history_store.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

namespace {

QString uniqueConnectionName()
{
    return QStringLiteral("resource_history-") + QUuid::createUuid().toString();
}

QVariant toVariant(const std::optional<double> &v)
{
    return v.has_value() ? QVariant(*v) : QVariant(QMetaType(QMetaType::Double));
}

QVariant toVariant(const std::optional<quint64> &v)
{
    return v.has_value() ? QVariant(static_cast<qulonglong>(*v)) : QVariant(QMetaType(QMetaType::ULongLong));
}

std::optional<double> toOptDouble(const QVariant &v)
{
    return v.isNull() ? std::nullopt : std::make_optional(v.toDouble());
}

std::optional<quint64> toOptU64(const QVariant &v)
{
    return v.isNull() ? std::nullopt : std::make_optional(static_cast<quint64>(v.toULongLong()));
}

} // namespace

ResourceHistoryStore::ResourceHistoryStore() = default;

ResourceHistoryStore::~ResourceHistoryStore()
{
    close();
}

bool ResourceHistoryStore::open(const QString &path)
{
    close();

    if (!openFresh(path)) {
        mLastError = QStringLiteral("failed to open %1").arg(path);
        qWarning() << "ResourceHistoryStore::open:" << mLastError;
        return false;
    }

    // Verify the file isn't corrupt before trusting it. A corrupt DB fails
    // either the open above or this check; either way, quarantine it and
    // start clean rather than ever surfacing a crash to the caller.
    {
        QSqlQuery check(QSqlDatabase::database(mConnName));
        const bool okOpen = check.exec(QStringLiteral("PRAGMA integrity_check"));
        const bool okResult = okOpen && check.next() && check.value(0).toString() == QLatin1String("ok");
        if (!okResult) {
            qWarning() << "ResourceHistoryStore::open: integrity check failed for" << path
                       << "- quarantining and starting fresh";
            close();
            const QString quarantined = QStringLiteral("%1.corrupt-%2")
                .arg(path)
                .arg(QDateTime::currentSecsSinceEpoch());
            QFile::remove(quarantined);
            QFile::rename(path, quarantined);
            if (!openFresh(path)) {
                mLastError = QStringLiteral("failed to recreate %1 after quarantine").arg(path);
                qWarning() << "ResourceHistoryStore::open:" << mLastError;
                return false;
            }
        }
    }

    if (!ensureSchema()) {
        mLastError = QStringLiteral("failed to create schema for %1").arg(path);
        qWarning() << "ResourceHistoryStore::open:" << mLastError;
        close();
        return false;
    }

    // Security note (SSO-10930 spec): this is a local timeline of when the
    // machine was busy. Lock it to the owner explicitly rather than relying
    // on umask (same pattern as LifecycleAuditLog::lockDownPermissions).
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);

    mPath = path;
    mOpen = true;
    return true;
}

bool ResourceHistoryStore::openFresh(const QString &path)
{
    mConnName = uniqueConnectionName();
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), mConnName);
    db.setDatabaseName(path);
    if (!db.open()) {
        mLastError = db.lastError().text();
        QSqlDatabase::removeDatabase(mConnName);
        mConnName.clear();
        return false;
    }

    QSqlQuery pragma(db);
    pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    pragma.exec(QStringLiteral("PRAGMA user_version=1"));
    return true;
}

bool ResourceHistoryStore::ensureSchema()
{
    QSqlDatabase db = QSqlDatabase::database(mConnName);
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS samples("
            "ts INTEGER NOT NULL,"
            "source INTEGER NOT NULL,"
            "cpu_pct REAL, cpu_max_pct REAL,"
            "load1 REAL, load5 REAL, load15 REAL,"
            "mem_used_bytes INTEGER, mem_total_bytes INTEGER,"
            "swap_used_bytes INTEGER, swap_total_bytes INTEGER)"))) {
        mLastError = q.lastError().text();
        return false;
    }
    if (!q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS samples_ts ON samples(ts)"))) {
        mLastError = q.lastError().text();
        return false;
    }
    return true;
}

void ResourceHistoryStore::close()
{
    if (mConnName.isEmpty())
        return;
    {
        QSqlDatabase db = QSqlDatabase::database(mConnName, false);
        if (db.isValid())
            db.close();
    }
    QSqlDatabase::removeDatabase(mConnName);
    mConnName.clear();
    mOpen = false;
}

bool ResourceHistoryStore::appendBatch(const QList<Sample> &samples)
{
    if (!mOpen || samples.isEmpty())
        return samples.isEmpty();

    QSqlDatabase db = QSqlDatabase::database(mConnName);
    if (!db.transaction()) {
        mLastError = db.lastError().text();
        qWarning() << "ResourceHistoryStore::appendBatch: begin transaction failed:" << mLastError;
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO samples(ts, source, cpu_pct, cpu_max_pct, load1, load5, load15,"
        " mem_used_bytes, mem_total_bytes, swap_used_bytes, swap_total_bytes)"
        " VALUES (?,?,?,?,?,?,?,?,?,?,?)"));

    for (const Sample &s : samples) {
        q.addBindValue(s.ts);
        q.addBindValue(static_cast<int>(s.source));
        q.addBindValue(toVariant(s.cpuPct));
        q.addBindValue(toVariant(s.cpuMaxPct));
        q.addBindValue(toVariant(s.load1));
        q.addBindValue(toVariant(s.load5));
        q.addBindValue(toVariant(s.load15));
        q.addBindValue(toVariant(s.memUsedBytes));
        q.addBindValue(toVariant(s.memTotalBytes));
        q.addBindValue(toVariant(s.swapUsedBytes));
        q.addBindValue(toVariant(s.swapTotalBytes));
        if (!q.exec()) {
            mLastError = q.lastError().text();
            qWarning() << "ResourceHistoryStore::appendBatch: insert failed:" << mLastError;
            db.rollback();
            return false;
        }
    }

    if (!db.commit()) {
        mLastError = db.lastError().text();
        qWarning() << "ResourceHistoryStore::appendBatch: commit failed:" << mLastError;
        db.rollback();
        return false;
    }
    return true;
}

QList<ResourceHistoryStore::Bucket> ResourceHistoryStore::queryBuckets(qint64 fromTs, qint64 toTs, qint64 bucketSecs) const
{
    QList<Bucket> result;
    if (!mOpen || bucketSecs <= 0)
        return result;

    QSqlQuery q(QSqlDatabase::database(mConnName));
    q.prepare(QStringLiteral(
        "SELECT (ts / :bucket) * :bucket AS bucket_ts,"
        " AVG(cpu_pct), MAX(COALESCE(cpu_max_pct, cpu_pct)),"
        " AVG(load1), AVG(load5), AVG(load15),"
        " AVG(mem_used_bytes), MAX(mem_used_bytes), AVG(swap_used_bytes)"
        " FROM samples WHERE ts >= :from AND ts <= :to"
        " GROUP BY bucket_ts ORDER BY bucket_ts"));
    q.bindValue(QStringLiteral(":bucket"), bucketSecs);
    q.bindValue(QStringLiteral(":from"), fromTs);
    q.bindValue(QStringLiteral(":to"), toTs);

    if (!q.exec()) {
        mLastError = q.lastError().text();
        qWarning() << "ResourceHistoryStore::queryBuckets: query failed:" << mLastError;
        return result;
    }

    while (q.next()) {
        Bucket b;
        b.ts = q.value(0).toLongLong();
        b.cpuAvg = toOptDouble(q.value(1));
        b.cpuMax = toOptDouble(q.value(2));
        b.load1Avg = toOptDouble(q.value(3));
        b.load5Avg = toOptDouble(q.value(4));
        b.load15Avg = toOptDouble(q.value(5));
        b.memUsedAvg = toOptDouble(q.value(6));
        b.memUsedMax = toOptDouble(q.value(7));
        b.swapUsedAvg = toOptDouble(q.value(8));
        result.append(b);
    }
    return result;
}

QList<ResourceHistoryStore::Sample> ResourceHistoryStore::queryRaw(qint64 fromTs, qint64 toTs) const
{
    QList<Sample> result;
    if (!mOpen)
        return result;

    QSqlQuery q(QSqlDatabase::database(mConnName));
    q.prepare(QStringLiteral(
        "SELECT ts, source, cpu_pct, cpu_max_pct, load1, load5, load15,"
        " mem_used_bytes, mem_total_bytes, swap_used_bytes, swap_total_bytes"
        " FROM samples WHERE ts >= ? AND ts <= ? ORDER BY ts ASC"));
    q.addBindValue(fromTs);
    q.addBindValue(toTs);

    if (!q.exec()) {
        mLastError = q.lastError().text();
        qWarning() << "ResourceHistoryStore::queryRaw: query failed:" << mLastError;
        return result;
    }

    while (q.next()) {
        Sample s;
        s.ts = q.value(0).toLongLong();
        s.source = static_cast<SampleSource>(q.value(1).toInt());
        s.cpuPct = toOptDouble(q.value(2));
        s.cpuMaxPct = toOptDouble(q.value(3));
        s.load1 = toOptDouble(q.value(4));
        s.load5 = toOptDouble(q.value(5));
        s.load15 = toOptDouble(q.value(6));
        s.memUsedBytes = toOptU64(q.value(7));
        s.memTotalBytes = toOptU64(q.value(8));
        s.swapUsedBytes = toOptU64(q.value(9));
        s.swapTotalBytes = toOptU64(q.value(10));
        result.append(s);
    }
    return result;
}

bool ResourceHistoryStore::pruneOlderThan(qint64 ts)
{
    if (!mOpen)
        return false;

    QSqlQuery q(QSqlDatabase::database(mConnName));
    q.prepare(QStringLiteral("DELETE FROM samples WHERE ts < ?"));
    q.addBindValue(ts);
    if (!q.exec()) {
        mLastError = q.lastError().text();
        qWarning() << "ResourceHistoryStore::pruneOlderThan: delete failed:" << mLastError;
        return false;
    }
    return true;
}

bool ResourceHistoryStore::clearAll()
{
    if (!mOpen)
        return false;

    QSqlDatabase db = QSqlDatabase::database(mConnName);
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("DELETE FROM samples"))) {
        mLastError = q.lastError().text();
        qWarning() << "ResourceHistoryStore::clearAll: delete failed:" << mLastError;
        return false;
    }
    if (!q.exec(QStringLiteral("VACUUM"))) {
        // Rows are already gone; a failed VACUUM just leaves disk space
        // unreclaimed, which isn't a reason to report the clear as failed
        // (same reasoning as BrowserSqliteCleaner::deleteHistory).
        mLastError = q.lastError().text();
        qWarning() << "ResourceHistoryStore::clearAll: VACUUM failed:" << mLastError;
    }
    return true;
}

qint64 ResourceHistoryStore::rowCount() const
{
    if (!mOpen)
        return 0;

    QSqlQuery q(QSqlDatabase::database(mConnName));
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM samples")) || !q.next()) {
        mLastError = q.lastError().text();
        return 0;
    }
    return q.value(0).toLongLong();
}
