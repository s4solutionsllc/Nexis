#include <QTest>
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

QTEST_MAIN(TestFormatUtil)
#include "test_format_util.moc"
