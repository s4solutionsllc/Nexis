#include <QTest>
#include <climits>
#include "format_util.h"

class TestFormatUtil : public QObject
{
    Q_OBJECT

private slots:
    void formatBytes_zero();
    void formatBytes_singleByte();
    void formatBytes_bytes();
    void formatBytes_kibibytes();
    void formatBytes_mebibytes();
    void formatBytes_gibibytes();
    void formatBytes_tebibytes();
    void formatBytes_multiTiB();
    void formatBytes_maxUint64();
    void formatBytes_nearBoundary();
};

void TestFormatUtil::formatBytes_zero()
{
    QCOMPARE(FormatUtil::formatBytes(0), QString("0 bytes"));
}

void TestFormatUtil::formatBytes_singleByte()
{
    QCOMPARE(FormatUtil::formatBytes(1), QString("1 byte"));
}

void TestFormatUtil::formatBytes_bytes()
{
    QCOMPARE(FormatUtil::formatBytes(2), QString("2 bytes"));
    QCOMPARE(FormatUtil::formatBytes(1023), QString("1023 bytes"));
}

void TestFormatUtil::formatBytes_kibibytes()
{
    QCOMPARE(FormatUtil::formatBytes(1024), QString("1.0 KiB"));
    QCOMPARE(FormatUtil::formatBytes(1536), QString("1.5 KiB"));
}

void TestFormatUtil::formatBytes_mebibytes()
{
    QCOMPARE(FormatUtil::formatBytes(1048576), QString("1.0 MiB"));
}

void TestFormatUtil::formatBytes_gibibytes()
{
    QCOMPARE(FormatUtil::formatBytes(1073741824), QString("1.0 GiB"));
}

void TestFormatUtil::formatBytes_tebibytes()
{
    QCOMPARE(FormatUtil::formatBytes(1099511627776), QString("1.0 TiB"));
}

void TestFormatUtil::formatBytes_multiTiB()
{
    // 2 TiB = 2199023255552
    QCOMPARE(FormatUtil::formatBytes(2199023255552ULL), QString("2.0 TiB"));
}

void TestFormatUtil::formatBytes_maxUint64()
{
    // UINT64_MAX should produce a valid TiB string without crashing
    QString result = FormatUtil::formatBytes(UINT64_MAX);
    QVERIFY(!result.isEmpty());
    QVERIFY(result.contains("TiB"));
}

void TestFormatUtil::formatBytes_nearBoundary()
{
    // 1023 * 1024 = 1047552 — just below 1 MiB boundary
    QCOMPARE(FormatUtil::formatBytes(1023ULL * 1024), QString("1023.0 KiB"));
}

QTEST_MAIN(TestFormatUtil)
#include "test_format_util.moc"
