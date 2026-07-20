#include <QTest>
#include "Info/rapl_power_info.h"

class TestRaplPowerInfo : public QObject
{
    Q_OBJECT

private slots:
    void delta_normalIncrement();
    void delta_zeroElapsed();
    void delta_wrapAround();
    void delta_wrapWithUnknownRange();
    void delta_exactlyAtMax();
};

void TestRaplPowerInfo::delta_normalIncrement()
{
    // No wrap: counter simply advanced.
    quint64 d = RaplPowerInfo::energyDeltaUj(1000, 1500, 65532610);
    QCOMPARE(d, quint64(500));
}

void TestRaplPowerInfo::delta_zeroElapsed()
{
    quint64 d = RaplPowerInfo::energyDeltaUj(1000, 1000, 65532610);
    QCOMPARE(d, quint64(0));
}

void TestRaplPowerInfo::delta_wrapAround()
{
    // Counter was near the top of its range and wrapped back to a small value.
    const quint64 maxRange = 65532610;
    quint64 d = RaplPowerInfo::energyDeltaUj(maxRange - 100, 50, maxRange);
    QCOMPARE(d, quint64(150));
}

void TestRaplPowerInfo::delta_wrapWithUnknownRange()
{
    // If max_energy_range_uj couldn't be read (0), we can't safely infer the
    // wrap distance — treat as no delta rather than reporting a bogus spike.
    quint64 d = RaplPowerInfo::energyDeltaUj(65532500, 50, 0);
    QCOMPARE(d, quint64(0));
}

void TestRaplPowerInfo::delta_exactlyAtMax()
{
    const quint64 maxRange = 65532610;
    quint64 d = RaplPowerInfo::energyDeltaUj(maxRange, 0, maxRange);
    QCOMPARE(d, quint64(0));
}

QTEST_MAIN(TestRaplPowerInfo)
#include "test_rapl_power_info.moc"
