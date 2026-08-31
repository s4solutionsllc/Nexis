#include <QTest>

#include "Utils/mac_os_version_util.h"

class TestMacOsVersionUtil : public QObject
{
    Q_OBJECT

private slots:
    void toVersionNumber_convertsMajorMinorMicro();
    void toVersionNumber_handlesUnknownOsType();
    void current_returnsNonNullVersion();
};

void TestMacOsVersionUtil::toVersionNumber_convertsMajorMinorMicro()
{
    const QOperatingSystemVersion osVersion(QOperatingSystemVersion::MacOS, 14, 5, 1);
    const QVersionNumber v = MacOsVersionUtil::toVersionNumber(osVersion);
    QCOMPARE(v, QVersionNumber(14, 5, 1));
    QVERIFY(v >= QVersionNumber(13, 0));
    QVERIFY(v < QVersionNumber(15, 0));
}

void TestMacOsVersionUtil::toVersionNumber_handlesUnknownOsType()
{
    const QOperatingSystemVersion osVersion(QOperatingSystemVersion::Windows, 10, 0, 0);
    const QVersionNumber v = MacOsVersionUtil::toVersionNumber(osVersion);
    QCOMPARE(v, QVersionNumber(10, 0, 0));
}

void TestMacOsVersionUtil::current_returnsNonNullVersion()
{
    // Whatever platform the test runs on, current() should report *something*.
    QVERIFY(!MacOsVersionUtil::current().isNull());
}

QTEST_MAIN(TestMacOsVersionUtil)
#include "test_mac_os_version_util.moc"
