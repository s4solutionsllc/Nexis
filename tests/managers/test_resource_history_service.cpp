// SSO-10930 / GH#220: ResourceHistoryService's sample-aggregation math
// (avg/max CPU% over a live-subscribed window, NULL-tolerant fields, the
// live vs. background-direct source tagging) — exercised as pure functions
// (F2: "injectable ... sampler") so this runs without a timer or a real
// DataRefreshService subscription.

#include <QTest>

#include "resource_history_service.h"

using Source = ResourceHistoryStore::SampleSource;

class TestResourceHistoryService : public QObject
{
    Q_OBJECT

private slots:
    void liveSampleAveragesAndMaxes();
    void liveSampleWithNoCpuTicksIsNullButKeepsOtherFields();
    void liveSampleWithShortLoadListLeavesLoadNull();
    void backgroundSampleHasNoCpuMax();
};

void TestResourceHistoryService::liveSampleAveragesAndMaxes()
{
    MemorySnapshot mem;
    mem.used = 4ull * 1024 * 1024 * 1024;
    mem.total = 16ull * 1024 * 1024 * 1024;
    mem.swapUsed = 0;
    mem.swapTotal = 2ull * 1024 * 1024 * 1024;

    // Three cpuUpdated ticks of 10, 20, 30 during the window: sum=60, max=30.
    const auto sample = ResourceHistoryService::makeLiveSample(
        1000, /*cpuSum=*/60.0, /*cpuMax=*/30.0, /*cpuSampleCount=*/3,
        QList<double>{1.5, 1.2, 0.9}, /*hasMemSnapshot=*/true, mem);

    QCOMPARE(sample.ts, qint64(1000));
    QCOMPARE(static_cast<int>(sample.source), static_cast<int>(Source::LiveAggregated));
    QVERIFY(sample.cpuPct.has_value());
    QCOMPARE(*sample.cpuPct, 20.0); // 60/3
    QVERIFY(sample.cpuMaxPct.has_value());
    QCOMPARE(*sample.cpuMaxPct, 30.0);
    QCOMPARE(*sample.load1, 1.5);
    QCOMPARE(*sample.load5, 1.2);
    QCOMPARE(*sample.load15, 0.9);
    QCOMPARE(*sample.memUsedBytes, mem.used);
    QCOMPARE(*sample.memTotalBytes, mem.total);
    QCOMPARE(*sample.swapUsedBytes, mem.swapUsed);
    QCOMPARE(*sample.swapTotalBytes, mem.swapTotal);
}

void TestResourceHistoryService::liveSampleWithNoCpuTicksIsNullButKeepsOtherFields()
{
    // A 10s window where DataRefreshService's fast tick never fired (e.g.
    // subscription just started) has nothing to average — cpu fields go
    // NULL rather than reporting a bogus 0%.
    MemorySnapshot mem;
    mem.used = 100;
    mem.total = 200;

    const auto sample = ResourceHistoryService::makeLiveSample(
        2000, /*cpuSum=*/0.0, /*cpuMax=*/0.0, /*cpuSampleCount=*/0,
        QList<double>{1.0, 1.0, 1.0}, /*hasMemSnapshot=*/true, mem);

    QVERIFY(!sample.cpuPct.has_value());
    QVERIFY(!sample.cpuMaxPct.has_value());
    QVERIFY(sample.load1.has_value());
    QVERIFY(sample.memUsedBytes.has_value());
}

void TestResourceHistoryService::liveSampleWithShortLoadListLeavesLoadNull()
{
    MemorySnapshot mem;
    const auto sample = ResourceHistoryService::makeLiveSample(
        3000, 10.0, 10.0, 1, QList<double>{}, /*hasMemSnapshot=*/false, mem);

    QVERIFY(!sample.load1.has_value());
    QVERIFY(!sample.load5.has_value());
    QVERIFY(!sample.load15.has_value());
    QVERIFY(!sample.memUsedBytes.has_value());
}

void TestResourceHistoryService::backgroundSampleHasNoCpuMax()
{
    // Decision 1: a background-direct tick samples CpuInfo once — there's
    // no "max over the window" to report, so cpu_max_pct must stay NULL.
    MemorySnapshot mem;
    mem.used = 1;
    mem.total = 2;
    mem.swapUsed = 0;
    mem.swapTotal = 0;

    const auto sample = ResourceHistoryService::makeBackgroundSample(
        4000, 55.0, QList<double>{2.0, 1.0, 0.5}, mem);

    QCOMPARE(static_cast<int>(sample.source), static_cast<int>(Source::BackgroundDirect));
    QVERIFY(sample.cpuPct.has_value());
    QCOMPARE(*sample.cpuPct, 55.0);
    QVERIFY(!sample.cpuMaxPct.has_value());
    QCOMPARE(*sample.load1, 2.0);
    QCOMPARE(*sample.memUsedBytes, quint64(1));
}

QTEST_MAIN(TestResourceHistoryService)
#include "test_resource_history_service.moc"
