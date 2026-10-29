#include <QTest>
#include "Utils/rate_counter_util.h"

#include <limits>

class TestRateCounterUtil : public QObject
{
    Q_OBJECT

private slots:
    void delta_normalIncrement();
    void delta_zeroInterval();
    void delta_counterReset();
    void delta_wrapAround();
};

void TestRateCounterUtil::delta_normalIncrement()
{
    QCOMPARE(RateCounterUtil::guardedDelta(1000, 1500), quint64(500));
}

void TestRateCounterUtil::delta_zeroInterval()
{
    // Same reading twice in a row (e.g. a sample that fails and reports the
    // last known total again) must yield 0, never a value derived from
    // wrapping.
    QCOMPARE(RateCounterUtil::guardedDelta(42, 42), quint64(0));
}

void TestRateCounterUtil::delta_counterReset()
{
    // Device re-enumeration or a failed sample reporting 0 steps the counter
    // backwards. Plain unsigned subtraction would wrap to ~16 EiB.
    QCOMPARE(RateCounterUtil::guardedDelta(5000, 100), quint64(0));
}

void TestRateCounterUtil::delta_wrapAround()
{
    // A counter that goes from near quint64 max back down to a small value
    // is the same "backwards step" case — must clamp to 0, not wrap.
    const quint64 nearMax = std::numeric_limits<quint64>::max() - 100;
    QCOMPARE(RateCounterUtil::guardedDelta(nearMax, 50), quint64(0));
}

QTEST_MAIN(TestRateCounterUtil)
#include "test_rate_counter_util.moc"
