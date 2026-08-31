#include <QTest>
#include "health_score_inputs.h"
#include <Info/memory_info.h>
#include <Info/disk_info.h>

class TestHealthScoreInputs : public QObject
{
    Q_OBJECT

private slots:
    void cpuScore_noLoad();
    void cpuScore_zeroCoreCount();
    void cpuScore_halfLoad();
    void cpuScore_fullyLoaded();
    void cpuScore_overloaded();

    void memoryScore_zeroTotal();
    void memoryScore_empty();
    void memoryScore_half();
    void memoryScore_full();

    void diskScore_empty();
    void diskScore_skipsZeroSizeDisks();
    void diskScore_capacityWeighted();
};

void TestHealthScoreInputs::cpuScore_noLoad()
{
    QCOMPARE(HealthScoreInputs::cpuScore(4, 0.0), 100);
}

void TestHealthScoreInputs::cpuScore_zeroCoreCount()
{
    QCOMPARE(HealthScoreInputs::cpuScore(0, 2.0), 100);
}

void TestHealthScoreInputs::cpuScore_halfLoad()
{
    QCOMPARE(HealthScoreInputs::cpuScore(4, 2.0), 50);
}

void TestHealthScoreInputs::cpuScore_fullyLoaded()
{
    QCOMPARE(HealthScoreInputs::cpuScore(4, 4.0), 0);
}

void TestHealthScoreInputs::cpuScore_overloaded()
{
    // load1m/coreCount > 1 clamps to 0, never negative.
    QCOMPARE(HealthScoreInputs::cpuScore(4, 8.0), 0);
}

void TestHealthScoreInputs::memoryScore_zeroTotal()
{
    MemorySnapshot snap;
    snap.total = 0;
    snap.used = 0;
    QCOMPARE(HealthScoreInputs::memoryScore(snap), 100);
}

void TestHealthScoreInputs::memoryScore_empty()
{
    MemorySnapshot snap;
    snap.total = 1000;
    snap.used = 0;
    QCOMPARE(HealthScoreInputs::memoryScore(snap), 100);
}

void TestHealthScoreInputs::memoryScore_half()
{
    MemorySnapshot snap;
    snap.total = 1000;
    snap.used = 500;
    QCOMPARE(HealthScoreInputs::memoryScore(snap), 50);
}

void TestHealthScoreInputs::memoryScore_full()
{
    MemorySnapshot snap;
    snap.total = 1000;
    snap.used = 1000;
    QCOMPARE(HealthScoreInputs::memoryScore(snap), 0);
}

void TestHealthScoreInputs::diskScore_empty()
{
    QCOMPARE(HealthScoreInputs::diskScore(QList<Disk>()), 100);
}

void TestHealthScoreInputs::diskScore_skipsZeroSizeDisks()
{
    Disk zero;
    zero.size = 0;
    zero.used = 0;
    Disk half;
    half.size = 1000;
    half.used = 500;
    QCOMPARE(HealthScoreInputs::diskScore({zero, half}), 50);
}

void TestHealthScoreInputs::diskScore_capacityWeighted()
{
    // A full 1000-byte disk and an empty 3000-byte disk: (0*1000 + 100*3000)/4000 = 75.
    Disk full;
    full.size = 1000;
    full.used = 1000;
    Disk empty;
    empty.size = 3000;
    empty.used = 0;
    QCOMPARE(HealthScoreInputs::diskScore({full, empty}), 75);
}

QTEST_MAIN(TestHealthScoreInputs)
#include "test_health_score_inputs.moc"
