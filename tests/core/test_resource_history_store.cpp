// SSO-10930 / GH#220: ResourceHistoryStore — batch insert, bucketed range
// queries (avg/max math), prune, clearAll, NULL tolerance, and corrupt-file
// recovery.

#include <QTest>
#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "Tools/resource_history_store.h"

using Sample = ResourceHistoryStore::Sample;
using Source = ResourceHistoryStore::SampleSource;

namespace {

Sample makeSample(qint64 ts, Source source, double cpuPct, std::optional<double> cpuMax,
                   quint64 memUsed, quint64 memTotal)
{
    Sample s;
    s.ts = ts;
    s.source = source;
    s.cpuPct = cpuPct;
    s.cpuMaxPct = cpuMax;
    s.load1 = 1.0;
    s.load5 = 1.5;
    s.load15 = 2.0;
    s.memUsedBytes = memUsed;
    s.memTotalBytes = memTotal;
    s.swapUsedBytes = 0;
    s.swapTotalBytes = 1024;
    return s;
}

} // namespace

class TestResourceHistoryStore : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void openCreatesSchema();
    void appendAndRowCount();
    void bucketMathAvgAndMax();
    void nullMetricsTolerated();
    void pruneOlderThanRemovesOldRows();
    void clearAllEmptiesTable();
    void corruptFileIsQuarantinedAndRecovered();

private:
    QTemporaryDir mDir;
    QString mDbPath;
};

void TestResourceHistoryStore::init()
{
    QVERIFY(mDir.isValid());
    mDbPath = mDir.filePath(QStringLiteral("resource_history.db"));
}

void TestResourceHistoryStore::cleanup()
{
    QFile::remove(mDbPath);
}

void TestResourceHistoryStore::openCreatesSchema()
{
    ResourceHistoryStore store;
    QVERIFY(store.open(mDbPath));
    QVERIFY(store.isOpen());
    QCOMPARE(store.rowCount(), qint64(0));
    QVERIFY(QFile::exists(mDbPath));
}

void TestResourceHistoryStore::appendAndRowCount()
{
    ResourceHistoryStore store;
    QVERIFY(store.open(mDbPath));

    QList<Sample> batch;
    batch << makeSample(1000, Source::LiveAggregated, 10.0, 20.0, 100, 1000);
    batch << makeSample(1010, Source::LiveAggregated, 15.0, 25.0, 110, 1000);
    QVERIFY(store.appendBatch(batch));

    QCOMPARE(store.rowCount(), qint64(2));

    const auto raw = store.queryRaw(0, 2000);
    QCOMPARE(raw.size(), 2);
    QCOMPARE(raw.at(0).ts, qint64(1000));
    QVERIFY(raw.at(0).cpuPct.has_value());
    QCOMPARE(*raw.at(0).cpuPct, 10.0);
    QCOMPARE(static_cast<int>(raw.at(0).source), static_cast<int>(Source::LiveAggregated));
}

void TestResourceHistoryStore::bucketMathAvgAndMax()
{
    ResourceHistoryStore store;
    QVERIFY(store.open(mDbPath));

    // Two samples land in bucket 0 (ts 0, 5), one in bucket 1 (ts 10) for a
    // 10-second bucket width.
    QList<Sample> batch;
    batch << makeSample(0, Source::LiveAggregated, 10.0, 20.0, 100, 1000);
    batch << makeSample(5, Source::LiveAggregated, 30.0, 40.0, 200, 1000);
    batch << makeSample(10, Source::LiveAggregated, 50.0, 60.0, 300, 1000);
    QVERIFY(store.appendBatch(batch));

    const auto buckets = store.queryBuckets(0, 10, 10);
    QCOMPARE(buckets.size(), 2);

    QCOMPARE(buckets.at(0).ts, qint64(0));
    QVERIFY(buckets.at(0).cpuAvg.has_value());
    QCOMPARE(*buckets.at(0).cpuAvg, 20.0);   // avg(10, 30)
    QVERIFY(buckets.at(0).cpuMax.has_value());
    QCOMPARE(*buckets.at(0).cpuMax, 40.0);   // max(20, 40)
    QVERIFY(buckets.at(0).memUsedMax.has_value());
    QCOMPARE(*buckets.at(0).memUsedMax, 200.0);

    QCOMPARE(buckets.at(1).ts, qint64(10));
    QCOMPARE(*buckets.at(1).cpuAvg, 50.0);
    QCOMPARE(*buckets.at(1).cpuMax, 60.0);
}

void TestResourceHistoryStore::nullMetricsTolerated()
{
    ResourceHistoryStore store;
    QVERIFY(store.open(mDbPath));

    // Background-direct sample: no cpuMaxPct (NULL per spec).
    Sample s;
    s.ts = 5000;
    s.source = Source::BackgroundDirect;
    s.cpuPct = 42.0;
    s.cpuMaxPct = std::nullopt;
    s.load1 = 0.5;
    // Memory left unset entirely.
    QVERIFY(store.appendBatch({s}));

    const auto raw = store.queryRaw(0, 10000);
    QCOMPARE(raw.size(), 1);
    QVERIFY(raw.at(0).cpuPct.has_value());
    QVERIFY(!raw.at(0).cpuMaxPct.has_value());
    QVERIFY(!raw.at(0).memUsedBytes.has_value());

    // COALESCE(cpu_max_pct, cpu_pct) means a bucket with only NULL-max rows
    // still reports a usable max (falls back to cpu_pct) instead of NULL.
    const auto buckets = store.queryBuckets(0, 10000, 60);
    QCOMPARE(buckets.size(), 1);
    QVERIFY(buckets.at(0).cpuMax.has_value());
    QCOMPARE(*buckets.at(0).cpuMax, 42.0);
}

void TestResourceHistoryStore::pruneOlderThanRemovesOldRows()
{
    ResourceHistoryStore store;
    QVERIFY(store.open(mDbPath));

    QList<Sample> batch;
    batch << makeSample(100, Source::LiveAggregated, 1.0, 1.0, 1, 10);
    batch << makeSample(200, Source::LiveAggregated, 2.0, 2.0, 2, 10);
    batch << makeSample(300, Source::LiveAggregated, 3.0, 3.0, 3, 10);
    QVERIFY(store.appendBatch(batch));

    QVERIFY(store.pruneOlderThan(200));
    QCOMPARE(store.rowCount(), qint64(2));

    const auto raw = store.queryRaw(0, 1000);
    QCOMPARE(raw.size(), 2);
    QCOMPARE(raw.at(0).ts, qint64(200));
}

void TestResourceHistoryStore::clearAllEmptiesTable()
{
    ResourceHistoryStore store;
    QVERIFY(store.open(mDbPath));

    QList<Sample> batch;
    batch << makeSample(1, Source::LiveAggregated, 1.0, 1.0, 1, 10);
    batch << makeSample(2, Source::LiveAggregated, 2.0, 2.0, 2, 10);
    QVERIFY(store.appendBatch(batch));
    QCOMPARE(store.rowCount(), qint64(2));

    QVERIFY(store.clearAll());
    QCOMPARE(store.rowCount(), qint64(0));
}

void TestResourceHistoryStore::corruptFileIsQuarantinedAndRecovered()
{
    // Write garbage bytes where a SQLite file should be.
    {
        QFile f(mDbPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("this is not a sqlite database, just noise to fail integrity_check");
        f.close();
    }

    ResourceHistoryStore store;
    QVERIFY(store.open(mDbPath));
    QVERIFY(store.isOpen());
    QCOMPARE(store.rowCount(), qint64(0));

    // The corrupt original should have been renamed aside, not left in
    // place or silently deleted.
    QDir dir(mDir.path());
    const QStringList quarantined = dir.entryList(QStringList() << "resource_history.db.corrupt-*");
    QCOMPARE(quarantined.size(), 1);

    // The store is fully usable after recovery.
    QVERIFY(store.appendBatch({makeSample(1, Source::LiveAggregated, 1.0, 1.0, 1, 10)}));
    QCOMPARE(store.rowCount(), qint64(1));
}

QTEST_MAIN(TestResourceHistoryStore)
#include "test_resource_history_store.moc"
