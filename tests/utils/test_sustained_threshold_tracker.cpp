#include <QTest>
#include "sustained_threshold_tracker.h"

class TestSustainedThresholdTracker : public QObject
{
    Q_OBJECT

private:
    static SustainedThresholdTracker::Config cfg(double threshold, qint64 durationMs,
                                                 qint64 cooldownMs = 0, qint64 maxGapMs = 30000)
    {
        SustainedThresholdTracker::Config c;
        c.threshold = threshold;
        c.durationMs = durationMs;
        c.cooldownMs = cooldownMs;
        c.maxGapMs = maxGapMs;
        return c;
    }

private slots:
    void zeroDuration_firesOnFirstSampleAbove();
    void spikeShorterThanDuration_doesNotFire();
    void sustainedForDuration_firesExactlyOnce();
    void notFiredBeforeDurationElapsed();
    void valueEqualToThreshold_isNotAbove();
    void dropBelowThreshold_restartsTheWindow();
    void secondEpisodeAfterRecovery_firesAgain();
    void cooldown_holdsBackASecondEpisode();
    void cooldown_firesOnceExpiredIfStillAbove();
    void gapLongerThanMaxGap_restartsTheEpisode();
    void clockGoingBackwards_restartsTheEpisode();
    void setConfig_restartsEpisodeButKeepsCooldown();
    void reset_clearsEpisode();
};

void TestSustainedThresholdTracker::zeroDuration_firesOnFirstSampleAbove()
{
    SustainedThresholdTracker t(cfg(90, 0));
    QVERIFY(!t.update(50, 0));
    QVERIFY(t.update(91, 5000));
}

void TestSustainedThresholdTracker::spikeShorterThanDuration_doesNotFire()
{
    SustainedThresholdTracker t(cfg(90, 30000));
    QVERIFY(!t.update(50, 0));
    QVERIFY(!t.update(99, 5000));
    QVERIFY(!t.update(99, 10000));
    QVERIFY(!t.update(40, 15000));
    QVERIFY(!t.update(40, 20000));
    QVERIFY(!t.isAbove());
}

void TestSustainedThresholdTracker::sustainedForDuration_firesExactlyOnce()
{
    SustainedThresholdTracker t(cfg(90, 30000));
    int fired = 0;
    for (qint64 now = 0; now <= 120000; now += 5000)
        fired += t.update(95, now) ? 1 : 0;
    QCOMPARE(fired, 1);
}

void TestSustainedThresholdTracker::notFiredBeforeDurationElapsed()
{
    SustainedThresholdTracker t(cfg(90, 30000));
    QVERIFY(!t.update(95, 0));
    QVERIFY(!t.update(95, 25000));
    QVERIFY(t.update(95, 30000));
}

void TestSustainedThresholdTracker::valueEqualToThreshold_isNotAbove()
{
    SustainedThresholdTracker t(cfg(90, 0));
    QVERIFY(!t.update(90, 0));
    QVERIFY(!t.isAbove());
}

void TestSustainedThresholdTracker::dropBelowThreshold_restartsTheWindow()
{
    SustainedThresholdTracker t(cfg(90, 30000));
    QVERIFY(!t.update(95, 0));
    QVERIFY(!t.update(95, 20000));
    QVERIFY(!t.update(80, 25000));
    // 25 s above again, but only 5 s since the dip ended the first run.
    QVERIFY(!t.update(95, 30000));
    QVERIFY(!t.update(95, 50000));
    QVERIFY(t.update(95, 60000));
}

void TestSustainedThresholdTracker::secondEpisodeAfterRecovery_firesAgain()
{
    SustainedThresholdTracker t(cfg(90, 10000));
    QVERIFY(!t.update(95, 0));
    QVERIFY(t.update(95, 10000));
    QVERIFY(!t.update(50, 15000));
    QVERIFY(!t.update(95, 20000));
    QVERIFY(t.update(95, 30000));
}

void TestSustainedThresholdTracker::cooldown_holdsBackASecondEpisode()
{
    SustainedThresholdTracker t(cfg(90, 10000, 600000));
    QVERIFY(!t.update(95, 0));
    QVERIFY(t.update(95, 10000));
    QVERIFY(!t.update(50, 15000));
    QVERIFY(!t.update(95, 20000));
    QVERIFY(!t.update(95, 30000));
}

void TestSustainedThresholdTracker::cooldown_firesOnceExpiredIfStillAbove()
{
    SustainedThresholdTracker t(cfg(90, 10000, 60000, 120000));
    QVERIFY(!t.update(95, 0));
    QVERIFY(t.update(95, 10000));
    QVERIFY(!t.update(50, 15000));
    QVERIFY(!t.update(95, 20000));
    QVERIFY(!t.update(95, 40000));
    QVERIFY(!t.update(95, 69000));
    QVERIFY(t.update(95, 70000));
    QVERIFY(!t.update(95, 80000));
}

void TestSustainedThresholdTracker::gapLongerThanMaxGap_restartsTheEpisode()
{
    SustainedThresholdTracker t(cfg(90, 30000, 0, 10000));
    QVERIFY(!t.update(95, 0));
    QVERIFY(!t.update(95, 5000));
    // Window hidden / machine asleep: samples resume far later. The time in
    // between is unknown, so it must not count towards "sustained".
    QVERIFY(!t.update(95, 600000));
    for (qint64 now = 605000; now < 630000; now += 5000)
        QVERIFY(!t.update(95, now));
    QVERIFY(t.update(95, 630000));
}

void TestSustainedThresholdTracker::clockGoingBackwards_restartsTheEpisode()
{
    SustainedThresholdTracker t(cfg(90, 30000));
    QVERIFY(!t.update(95, 100000));
    QVERIFY(!t.update(95, 20000));
    QVERIFY(!t.update(95, 40000));
    QVERIFY(t.update(95, 50000));
}

void TestSustainedThresholdTracker::setConfig_restartsEpisodeButKeepsCooldown()
{
    SustainedThresholdTracker t(cfg(90, 0, 600000));
    QVERIFY(t.update(95, 0));
    t.setConfig(cfg(90, 0, 600000));
    QVERIFY(!t.update(95, 5000));
}

void TestSustainedThresholdTracker::reset_clearsEpisode()
{
    SustainedThresholdTracker t(cfg(90, 30000));
    QVERIFY(!t.update(95, 0));
    QVERIFY(t.isAbove());
    t.reset();
    QVERIFY(!t.isAbove());
    QVERIFY(!t.update(95, 20000));
    QVERIFY(t.update(95, 50000));
}

QTEST_MAIN(TestSustainedThresholdTracker)
#include "test_sustained_threshold_tracker.moc"
