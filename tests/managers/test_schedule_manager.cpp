#include <QTest>
#include "Managers/schedule_manager.h"

class TestScheduleManager : public QObject
{
    Q_OBJECT

private:
    ScheduleManager::CleaningSchedule makeSchedule(
        ScheduleManager::Frequency freq, int hour = 15, int minute = 0)
    {
        ScheduleManager::CleaningSchedule s;
        s.frequency = freq;
        s.hour = hour;
        s.minute = minute;
        return s;
    }

private slots:
    // ── getNextRunTime tests ─────────────────────────────────────────────────
    void nextRun_dailyFuture();
    void nextRun_dailyPast();
    void nextRun_weeklyFutureDay();
    void nextRun_weeklyPastDay();
    void nextRun_weeklySameDay_futureTime();
    void nextRun_weeklySameDay_pastTime();
    void nextRun_monthlyFuture();
    void nextRun_monthlyPast();
    void nextRun_monthlyDay31InFeb();
    void nextRun_everyNDays_withLastRun();
    void nextRun_everyNDays_noLastRun();

    // ── isEveryNDaysGateBlocked tests (SSO-3369) ────────────────────────────
    void gate_everyNDays_blocksBeforeInterval();
    void gate_everyNDays_runsAtBoundary();
    void gate_everyNDays_runsAfterInterval();
    void gate_everyNDays_runsWithoutLastRun();
    void gate_everyNDays_runsForNonPositiveN();
    void gate_dailyNeverBlocked();
    void gate_weeklyNeverBlocked();

    // ── frequencyDisplayText tests ──────────────────────────────────────────
    void displayText_daily();
    void displayText_everyNDays();
    void displayText_weekly();
    void displayText_monthly();
};

// We test via the singleton since getNextRunTime is a const member.
// The `now` parameter makes all tests deterministic.

void TestScheduleManager::nextRun_dailyFuture()
{
    auto s = makeSchedule(ScheduleManager::Daily, 15, 0);
    QDateTime now(QDate(2025, 6, 15), QTime(10, 0)); // Wed 10:00
    QDateTime result = ScheduleManager::ins()->getNextRunTime(s, now);
    QCOMPARE(result, QDateTime(QDate(2025, 6, 15), QTime(15, 0)));
}

void TestScheduleManager::nextRun_dailyPast()
{
    auto s = makeSchedule(ScheduleManager::Daily, 8, 0);
    QDateTime now(QDate(2025, 6, 15), QTime(10, 0));
    QDateTime result = ScheduleManager::ins()->getNextRunTime(s, now);
    QCOMPARE(result, QDateTime(QDate(2025, 6, 16), QTime(8, 0)));
}

void TestScheduleManager::nextRun_weeklyFutureDay()
{
    auto s = makeSchedule(ScheduleManager::Weekly, 15, 0);
    s.dayOfWeek = 5; // Friday (0=Sun convention)
    // now = Wednesday June 18, 2025
    QDateTime now(QDate(2025, 6, 18), QTime(10, 0));
    QDateTime result = ScheduleManager::ins()->getNextRunTime(s, now);
    // Friday June 20
    QCOMPARE(result, QDateTime(QDate(2025, 6, 20), QTime(15, 0)));
}

void TestScheduleManager::nextRun_weeklyPastDay()
{
    auto s = makeSchedule(ScheduleManager::Weekly, 15, 0);
    s.dayOfWeek = 1; // Monday
    // now = Wednesday June 18, 2025
    QDateTime now(QDate(2025, 6, 18), QTime(10, 0));
    QDateTime result = ScheduleManager::ins()->getNextRunTime(s, now);
    // Next Monday = June 23
    QCOMPARE(result, QDateTime(QDate(2025, 6, 23), QTime(15, 0)));
}

void TestScheduleManager::nextRun_weeklySameDay_futureTime()
{
    auto s = makeSchedule(ScheduleManager::Weekly, 15, 0);
    // June 18 2025 is a Wednesday; Qt dayOfWeek()=3 → 3%7=3 in code's convention
    s.dayOfWeek = 3; // Wednesday
    QDateTime now(QDate(2025, 6, 18), QTime(10, 0));
    QDateTime result = ScheduleManager::ins()->getNextRunTime(s, now);
    QCOMPARE(result, QDateTime(QDate(2025, 6, 18), QTime(15, 0)));
}

void TestScheduleManager::nextRun_weeklySameDay_pastTime()
{
    auto s = makeSchedule(ScheduleManager::Weekly, 8, 0);
    s.dayOfWeek = 3; // Wednesday
    QDateTime now(QDate(2025, 6, 18), QTime(10, 0));
    QDateTime result = ScheduleManager::ins()->getNextRunTime(s, now);
    // Next Wednesday = June 25
    QCOMPARE(result, QDateTime(QDate(2025, 6, 25), QTime(8, 0)));
}

void TestScheduleManager::nextRun_monthlyFuture()
{
    auto s = makeSchedule(ScheduleManager::Monthly, 3, 0);
    s.dayOfMonth = 25;
    QDateTime now(QDate(2025, 6, 15), QTime(10, 0));
    QDateTime result = ScheduleManager::ins()->getNextRunTime(s, now);
    QCOMPARE(result, QDateTime(QDate(2025, 6, 25), QTime(3, 0)));
}

void TestScheduleManager::nextRun_monthlyPast()
{
    auto s = makeSchedule(ScheduleManager::Monthly, 3, 0);
    s.dayOfMonth = 5;
    QDateTime now(QDate(2025, 6, 15), QTime(10, 0));
    QDateTime result = ScheduleManager::ins()->getNextRunTime(s, now);
    QCOMPARE(result, QDateTime(QDate(2025, 7, 5), QTime(3, 0)));
}

void TestScheduleManager::nextRun_monthlyDay31InFeb()
{
    auto s = makeSchedule(ScheduleManager::Monthly, 3, 0);
    s.dayOfMonth = 31;
    QDateTime now(QDate(2025, 2, 15), QTime(10, 0));
    QDateTime result = ScheduleManager::ins()->getNextRunTime(s, now);
    // Feb 2025 has 28 days → clamped to 28
    QCOMPARE(result, QDateTime(QDate(2025, 2, 28), QTime(3, 0)));
}

void TestScheduleManager::nextRun_everyNDays_withLastRun()
{
    auto s = makeSchedule(ScheduleManager::EveryNDays, 15, 0);
    s.everyNDays = 3;
    s.lastRun = QDateTime(QDate(2025, 6, 13), QTime(15, 0)); // 2 days ago
    QDateTime now(QDate(2025, 6, 15), QTime(10, 0));
    QDateTime result = ScheduleManager::ins()->getNextRunTime(s, now);
    // lastRun + 3 days = June 16 at 15:00
    QCOMPARE(result, QDateTime(QDate(2025, 6, 16), QTime(15, 0)));
}

void TestScheduleManager::nextRun_everyNDays_noLastRun()
{
    auto s = makeSchedule(ScheduleManager::EveryNDays, 8, 0);
    s.everyNDays = 5;
    QDateTime now(QDate(2025, 6, 15), QTime(10, 0));
    QDateTime result = ScheduleManager::ins()->getNextRunTime(s, now);
    // No lastRun: today at 08:00 is past → add 5 days = June 20 08:00
    QCOMPARE(result, QDateTime(QDate(2025, 6, 20), QTime(8, 0)));
}

// ── EveryNDays gate tests (SSO-3369) ─────────────────────────────────────────
// On Linux, systemd timers fire daily for EveryNDays schedules; the gate keeps
// the interval honest. Boundary: daysTo == everyNDays runs.

void TestScheduleManager::gate_everyNDays_blocksBeforeInterval()
{
    // everyNDays=7, lastRun=3 days ago → blocked
    QDateTime now(QDate(2025, 6, 15), QTime(15, 0));
    QDateTime lastRun(QDate(2025, 6, 12), QTime(15, 0));
    QVERIFY(ScheduleManager::isEveryNDaysGateBlocked(
        ScheduleManager::EveryNDays, 7, lastRun, now));
}

void TestScheduleManager::gate_everyNDays_runsAtBoundary()
{
    // everyNDays=7, lastRun exactly 7 days ago → NOT blocked (boundary runs)
    QDateTime now(QDate(2025, 6, 15), QTime(15, 0));
    QDateTime lastRun(QDate(2025, 6, 8), QTime(15, 0));
    QVERIFY(!ScheduleManager::isEveryNDaysGateBlocked(
        ScheduleManager::EveryNDays, 7, lastRun, now));
}

void TestScheduleManager::gate_everyNDays_runsAfterInterval()
{
    // everyNDays=7, lastRun=8 days ago → not blocked
    QDateTime now(QDate(2025, 6, 15), QTime(15, 0));
    QDateTime lastRun(QDate(2025, 6, 7), QTime(15, 0));
    QVERIFY(!ScheduleManager::isEveryNDaysGateBlocked(
        ScheduleManager::EveryNDays, 7, lastRun, now));
}

void TestScheduleManager::gate_everyNDays_runsWithoutLastRun()
{
    // First run after schedule creation: lastRun invalid → not blocked
    QDateTime now(QDate(2025, 6, 15), QTime(15, 0));
    QVERIFY(!ScheduleManager::isEveryNDaysGateBlocked(
        ScheduleManager::EveryNDays, 7, QDateTime(), now));
}

void TestScheduleManager::gate_everyNDays_runsForNonPositiveN()
{
    // Malformed N (0 or negative) treated as "no interval" → not blocked,
    // so the clean still runs and we don't accidentally permanently block it.
    QDateTime now(QDate(2025, 6, 15), QTime(15, 0));
    QDateTime lastRun(QDate(2025, 6, 14), QTime(15, 0));
    QVERIFY(!ScheduleManager::isEveryNDaysGateBlocked(
        ScheduleManager::EveryNDays, 0, lastRun, now));
    QVERIFY(!ScheduleManager::isEveryNDaysGateBlocked(
        ScheduleManager::EveryNDays, -1, lastRun, now));
}

void TestScheduleManager::gate_dailyNeverBlocked()
{
    QDateTime now(QDate(2025, 6, 15), QTime(15, 0));
    QDateTime lastRun(QDate(2025, 6, 15), QTime(3, 0)); // earlier today
    QVERIFY(!ScheduleManager::isEveryNDaysGateBlocked(
        ScheduleManager::Daily, 7, lastRun, now));
}

void TestScheduleManager::gate_weeklyNeverBlocked()
{
    QDateTime now(QDate(2025, 6, 15), QTime(15, 0));
    QDateTime lastRun(QDate(2025, 6, 14), QTime(15, 0));
    QVERIFY(!ScheduleManager::isEveryNDaysGateBlocked(
        ScheduleManager::Weekly, 7, lastRun, now));
}

// ── Display Text Tests ───────────────────────────────────────────────────────

void TestScheduleManager::displayText_daily()
{
    auto s = makeSchedule(ScheduleManager::Daily, 3, 0);
    QCOMPARE(ScheduleManager::frequencyDisplayText(s), QString("Daily at 03:00"));
}

void TestScheduleManager::displayText_everyNDays()
{
    auto s = makeSchedule(ScheduleManager::EveryNDays, 14, 30);
    s.everyNDays = 5;
    QCOMPARE(ScheduleManager::frequencyDisplayText(s), QString("Every 5 days at 14:30"));
}

void TestScheduleManager::displayText_weekly()
{
    auto s = makeSchedule(ScheduleManager::Weekly, 9, 0);
    s.dayOfWeek = 1; // Monday
    QCOMPARE(ScheduleManager::frequencyDisplayText(s), QString("Weekly on Monday at 09:00"));
}

void TestScheduleManager::displayText_monthly()
{
    auto s = makeSchedule(ScheduleManager::Monthly, 22, 0);
    s.dayOfMonth = 15;
    QCOMPARE(ScheduleManager::frequencyDisplayText(s), QString("Monthly on day 15 at 22:00"));
}

QTEST_MAIN(TestScheduleManager)
#include "test_schedule_manager.moc"
