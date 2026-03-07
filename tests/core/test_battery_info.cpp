#include <QTest>
#include "Info/battery_info.h"

class TestBatteryInfo : public QObject
{
    Q_OBJECT

private slots:
    // deriveCondition
    void condition_good();
    void condition_fair();
    void condition_replace();
    void condition_boundary80();
    void condition_boundary60();

    // deriveHealthPercent
    void health_normalBattery();
    void health_degradedBattery();
    void health_newBattery();
    void health_zeroDesign();
    void health_zeroMax();
    void health_negativeValues();
    void health_overCapacity();
};

void TestBatteryInfo::condition_good()
{
    QCOMPARE(BatteryInfo::deriveCondition(95), QString("Good"));
}

void TestBatteryInfo::condition_fair()
{
    QCOMPARE(BatteryInfo::deriveCondition(70), QString("Fair"));
}

void TestBatteryInfo::condition_replace()
{
    QCOMPARE(BatteryInfo::deriveCondition(50), QString("Replace"));
}

void TestBatteryInfo::condition_boundary80()
{
    // Exactly 80% should be "Good"
    QCOMPARE(BatteryInfo::deriveCondition(80), QString("Good"));
}

void TestBatteryInfo::condition_boundary60()
{
    // Exactly 60% should be "Fair"
    QCOMPARE(BatteryInfo::deriveCondition(60), QString("Fair"));
    // 59% should be "Replace"
    QCOMPARE(BatteryInfo::deriveCondition(59), QString("Replace"));
}

void TestBatteryInfo::health_normalBattery()
{
    // 4000 mAh max / 5000 mAh design = 80%
    QCOMPARE(BatteryInfo::deriveHealthPercent(4000.0, 5000.0), 80);
}

void TestBatteryInfo::health_degradedBattery()
{
    // 2500 / 5000 = 50%
    QCOMPARE(BatteryInfo::deriveHealthPercent(2500.0, 5000.0), 50);
}

void TestBatteryInfo::health_newBattery()
{
    // Full capacity = 100%
    QCOMPARE(BatteryInfo::deriveHealthPercent(5000.0, 5000.0), 100);
}

void TestBatteryInfo::health_zeroDesign()
{
    QCOMPARE(BatteryInfo::deriveHealthPercent(4000.0, 0.0), -1);
}

void TestBatteryInfo::health_zeroMax()
{
    QCOMPARE(BatteryInfo::deriveHealthPercent(0.0, 5000.0), -1);
}

void TestBatteryInfo::health_negativeValues()
{
    QCOMPARE(BatteryInfo::deriveHealthPercent(-1.0, 5000.0), -1);
    QCOMPARE(BatteryInfo::deriveHealthPercent(4000.0, -1.0), -1);
}

void TestBatteryInfo::health_overCapacity()
{
    // Some batteries report max > design (calibration artifact), should clamp to 100
    QCOMPARE(BatteryInfo::deriveHealthPercent(5200.0, 5000.0), 100);
}

QTEST_MAIN(TestBatteryInfo)
#include "test_battery_info.moc"
