#include <QTest>
#include "Info/thermal_info.h"

class TestThermalInfo : public QObject
{
    Q_OBJECT

private slots:
    // parseSysfsTemperature
    void temp_normalValue();
    void temp_zero();
    void temp_negative();
    void temp_emptyInput();
    void temp_withWhitespace();

    // sanitizeTempThreshold
    void threshold_normalMax();
    void threshold_normalCrit();
    void threshold_bogusHigh();
    void threshold_zero();
    void threshold_emptyInput();
    void threshold_customMaxSane();
};

void TestThermalInfo::temp_normalValue()
{
    // 45000 millideg = 45.0 °C
    QCOMPARE(ThermalInfo::parseSysfsTemperature("45000"), 45.0);
}

void TestThermalInfo::temp_zero()
{
    QCOMPARE(ThermalInfo::parseSysfsTemperature("0"), 0.0);
}

void TestThermalInfo::temp_negative()
{
    // Some sensors can report negative (e.g., cold environments)
    QCOMPARE(ThermalInfo::parseSysfsTemperature("-5000"), -5.0);
}

void TestThermalInfo::temp_emptyInput()
{
    QCOMPARE(ThermalInfo::parseSysfsTemperature(""), 0.0);
}

void TestThermalInfo::temp_withWhitespace()
{
    QCOMPARE(ThermalInfo::parseSysfsTemperature("  72500\n"), 72.5);
}

void TestThermalInfo::threshold_normalMax()
{
    // 85000 millideg = 85.0 °C, within 200.0 sane limit
    QCOMPARE(ThermalInfo::sanitizeTempThreshold("85000"), 85.0);
}

void TestThermalInfo::threshold_normalCrit()
{
    QCOMPARE(ThermalInfo::sanitizeTempThreshold("105000"), 105.0);
}

void TestThermalInfo::threshold_bogusHigh()
{
    // 250000 millideg = 250.0 °C, exceeds 200.0 limit → -1.0
    QCOMPARE(ThermalInfo::sanitizeTempThreshold("250000"), -1.0);
}

void TestThermalInfo::threshold_zero()
{
    // 0 millideg = 0.0 °C → not positive → -1.0
    QCOMPARE(ThermalInfo::sanitizeTempThreshold("0"), -1.0);
}

void TestThermalInfo::threshold_emptyInput()
{
    QCOMPARE(ThermalInfo::sanitizeTempThreshold(""), -1.0);
}

void TestThermalInfo::threshold_customMaxSane()
{
    // 95000 millideg = 95.0 °C, custom limit of 90.0 → -1.0
    QCOMPARE(ThermalInfo::sanitizeTempThreshold("95000", 90.0), -1.0);
    // But within default limit of 200.0
    QCOMPARE(ThermalInfo::sanitizeTempThreshold("95000", 200.0), 95.0);
}

QTEST_MAIN(TestThermalInfo)
#include "test_thermal_info.moc"
