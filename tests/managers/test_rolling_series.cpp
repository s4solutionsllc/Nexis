#include <QTest>
#include <QThread>
#include "Pages/Resources/rolling_series.h"

// SSO-26718: RollingSeries replaces the Resources page's per-chart
// shift-and-insert loops. windowSeconds-based aging is driven off a real
// QElapsedTimer (B11 — honest elapsed time, not a sample count), so the
// window-shrink test below uses short real sleeps rather than mocking time.
class TestRollingSeries : public QObject
{
    Q_OBJECT

private slots:
    void capacityCapsBufferedPoints();
    void windowYMaxShrinksAfterSpikeAges();
    void resetClearsBufferedState();
};

void TestRollingSeries::capacityCapsBufferedPoints()
{
    QSplineSeries series;
    // Large window so only the capacity cap (not time-based eviction) is in
    // play here — pushes happen back-to-back with no real sleep between them.
    RollingSeries rolling(QVector<QXYSeries *>{ &series }, /*windowSeconds=*/600.0, /*capacity=*/3);

    for (int i = 0; i < 2; ++i) {
        rolling.push({ double(i) });
        QCOMPARE(series.count(), i + 1);
    }

    // Capacity is 3 — further pushes must not grow the buffer past it.
    for (int i = 2; i < 8; ++i)
        rolling.push({ double(i) });

    QCOMPARE(series.count(), 3);
}

void TestRollingSeries::windowYMaxShrinksAfterSpikeAges()
{
    QSplineSeries series;
    // Short window so the test doesn't need to wait a real 60s for a point
    // to age out.
    const double windowSeconds = 0.08;
    RollingSeries rolling(QVector<QXYSeries *>{ &series }, windowSeconds, /*capacity=*/64);

    const double floor = 1.0;

    rolling.push({ 100.0 }); // the spike
    QCOMPARE(rolling.windowYMax(floor), 100.0);

    // Push a few low-value samples, with real sleeps between them, until the
    // spike's point has aged past windowSeconds and dropped out of the
    // window entirely.
    bool shrank = false;
    for (int i = 0; i < 20 && !shrank; ++i) {
        QThread::msleep(20);
        rolling.push({ 2.0 });
        if (rolling.windowYMax(floor) < 100.0)
            shrank = true;
    }

    QVERIFY2(shrank, "windowYMax() never shrank after the spike aged out of the window");
    QCOMPARE(rolling.windowYMax(floor), 2.0);
}

void TestRollingSeries::resetClearsBufferedState()
{
    QSplineSeries series;
    RollingSeries rolling(QVector<QXYSeries *>{ &series }, 600.0, 64);

    rolling.push({ 10.0 });
    rolling.push({ 20.0 });
    QCOMPARE(series.count(), 2);
    QCOMPARE(rolling.windowYMax(0.0), 20.0);

    rolling.reset();
    QCOMPARE(series.count(), 0);

    // A reset also restarts the elapsed-time clock — the next push() is
    // treated as the first sample again (no stale points to age).
    rolling.push({ 5.0 });
    QCOMPARE(series.count(), 1);
    QCOMPARE(rolling.windowYMax(0.0), 5.0);
}

QTEST_MAIN(TestRollingSeries)
#include "test_rolling_series.moc"
