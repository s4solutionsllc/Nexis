#include <QTest>
#include "mini_monitor_format_util.h"
#include "Info/disk_info.h"

class TestMiniMonitorFormatUtil : public QObject
{
    Q_OBJECT

private slots:
    void formatMetricRow_typical();
    void formatMetricRow_clampsNegative();
    void formatMetricRow_clampsOver100();

    void aggregateDiskUsedPercent_empty();
    void aggregateDiskUsedPercent_singleDisk();
    void aggregateDiskUsedPercent_weightedByCapacity();
    void aggregateDiskUsedPercent_ignoresZeroSizeDisks();

    void scoreColorToken_success();
    void scoreColorToken_successBoundary();
    void scoreColorToken_warning();
    void scoreColorToken_warningBoundary();
    void scoreColorToken_destructive();
    void scoreColorToken_clampsOutOfRange();
};

void TestMiniMonitorFormatUtil::formatMetricRow_typical()
{
    QCOMPARE(MiniMonitorFormatUtil::formatMetricRow("CPU", 42), QString("CPU 42%"));
}

void TestMiniMonitorFormatUtil::formatMetricRow_clampsNegative()
{
    QCOMPARE(MiniMonitorFormatUtil::formatMetricRow("MEM", -5), QString("MEM 0%"));
}

void TestMiniMonitorFormatUtil::formatMetricRow_clampsOver100()
{
    QCOMPARE(MiniMonitorFormatUtil::formatMetricRow("DSK", 150), QString("DSK 100%"));
}

void TestMiniMonitorFormatUtil::aggregateDiskUsedPercent_empty()
{
    QCOMPARE(MiniMonitorFormatUtil::aggregateDiskUsedPercent({}), 0);
}

void TestMiniMonitorFormatUtil::aggregateDiskUsedPercent_singleDisk()
{
    Disk d;
    d.size = 1000;
    d.used = 500;
    QCOMPARE(MiniMonitorFormatUtil::aggregateDiskUsedPercent({d}), 50);
}

void TestMiniMonitorFormatUtil::aggregateDiskUsedPercent_weightedByCapacity()
{
    // Small disk 100% used, large disk 0% used — weighted toward the larger disk.
    Disk small;
    small.size = 100;
    small.used = 100;
    Disk large;
    large.size = 900;
    large.used = 0;
    QCOMPARE(MiniMonitorFormatUtil::aggregateDiskUsedPercent({small, large}), 10);
}

void TestMiniMonitorFormatUtil::aggregateDiskUsedPercent_ignoresZeroSizeDisks()
{
    Disk valid;
    valid.size = 200;
    valid.used = 100;
    Disk zeroSize;
    zeroSize.size = 0;
    zeroSize.used = 0;
    QCOMPARE(MiniMonitorFormatUtil::aggregateDiskUsedPercent({valid, zeroSize}), 50);
}

void TestMiniMonitorFormatUtil::scoreColorToken_success()
{
    QCOMPARE(MiniMonitorFormatUtil::scoreColorToken(90), QString("@successColor"));
}

void TestMiniMonitorFormatUtil::scoreColorToken_successBoundary()
{
    QCOMPARE(MiniMonitorFormatUtil::scoreColorToken(75), QString("@successColor"));
}

void TestMiniMonitorFormatUtil::scoreColorToken_warning()
{
    QCOMPARE(MiniMonitorFormatUtil::scoreColorToken(60), QString("@warningColor"));
}

void TestMiniMonitorFormatUtil::scoreColorToken_warningBoundary()
{
    QCOMPARE(MiniMonitorFormatUtil::scoreColorToken(40), QString("@warningColor"));
}

void TestMiniMonitorFormatUtil::scoreColorToken_destructive()
{
    QCOMPARE(MiniMonitorFormatUtil::scoreColorToken(39), QString("@destructiveColor"));
}

void TestMiniMonitorFormatUtil::scoreColorToken_clampsOutOfRange()
{
    QCOMPARE(MiniMonitorFormatUtil::scoreColorToken(-10), QString("@destructiveColor"));
    QCOMPARE(MiniMonitorFormatUtil::scoreColorToken(150), QString("@successColor"));
}

QTEST_MAIN(TestMiniMonitorFormatUtil)
#include "test_mini_monitor_format_util.moc"
