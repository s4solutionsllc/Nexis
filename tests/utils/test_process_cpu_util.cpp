#include <QTest>
#include "process_cpu_util.h"

class TestProcessCpuUtil : public QObject
{
    Q_OBJECT

private slots:
    void logicalCpuCount_isPositive();
    void toSystemPercent_divisesByCpuCount();
    void toSystemPercent_singleCpuUnchanged();
    void toSystemPercent_clampsToHundred();
    void toSystemPercent_zeroAndNegative();
    void toSystemPercent_invalidCpuCountTreatedAsOne();
    void migrate_roundsUp();
    void migrate_keepsNonZeroEnabled();
    void migrate_disabledStaysDisabled();
    void migrate_clampsToHundred();
    void formatPercent_oneDecimal();
};

void TestProcessCpuUtil::logicalCpuCount_isPositive()
{
    QVERIFY(ProcessCpuUtil::logicalCpuCount() >= 1);
}

void TestProcessCpuUtil::toSystemPercent_divisesByCpuCount()
{
    QCOMPARE(ProcessCpuUtil::toSystemPercent(253.776, 16), 253.776 / 16);
    QCOMPARE(ProcessCpuUtil::toSystemPercent(400.0, 8), 50.0);
}

void TestProcessCpuUtil::toSystemPercent_singleCpuUnchanged()
{
    QCOMPARE(ProcessCpuUtil::toSystemPercent(42.5, 1), 42.5);
}

void TestProcessCpuUtil::toSystemPercent_clampsToHundred()
{
    QCOMPARE(ProcessCpuUtil::toSystemPercent(1700.0, 16), 100.0);
}

void TestProcessCpuUtil::toSystemPercent_zeroAndNegative()
{
    QCOMPARE(ProcessCpuUtil::toSystemPercent(0.0, 16), 0.0);
    QCOMPARE(ProcessCpuUtil::toSystemPercent(-5.0, 16), 0.0);
}

void TestProcessCpuUtil::toSystemPercent_invalidCpuCountTreatedAsOne()
{
    QCOMPARE(ProcessCpuUtil::toSystemPercent(30.0, 0), 30.0);
    QCOMPARE(ProcessCpuUtil::toSystemPercent(30.0, -4), 30.0);
}

void TestProcessCpuUtil::migrate_roundsUp()
{
    QCOMPARE(ProcessCpuUtil::migratePerCoreThreshold(150, 16), 10);
    QCOMPARE(ProcessCpuUtil::migratePerCoreThreshold(160, 16), 10);
    QCOMPARE(ProcessCpuUtil::migratePerCoreThreshold(161, 16), 11);
}

void TestProcessCpuUtil::migrate_keepsNonZeroEnabled()
{
    QCOMPARE(ProcessCpuUtil::migratePerCoreThreshold(1, 16), 1);
}

void TestProcessCpuUtil::migrate_disabledStaysDisabled()
{
    QCOMPARE(ProcessCpuUtil::migratePerCoreThreshold(0, 16), 0);
    QCOMPARE(ProcessCpuUtil::migratePerCoreThreshold(-3, 16), 0);
}

void TestProcessCpuUtil::migrate_clampsToHundred()
{
    QCOMPARE(ProcessCpuUtil::migratePerCoreThreshold(1000, 4), 100);
}

void TestProcessCpuUtil::formatPercent_oneDecimal()
{
    QCOMPARE(ProcessCpuUtil::formatPercent(15.861), QString("15.9"));
    QCOMPARE(ProcessCpuUtil::formatPercent(0.0165964), QString("0.0"));
    QCOMPARE(ProcessCpuUtil::formatPercent(0.0), QString("0.0"));
    QCOMPARE(ProcessCpuUtil::formatPercent(100.0), QString("100.0"));
}

QTEST_MAIN(TestProcessCpuUtil)
#include "test_process_cpu_util.moc"
