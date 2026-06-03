#include <QTest>
#include "Info/update_info_linux.h"

class TestUpdateInfo : public QObject
{
    Q_OBJECT

private slots:
    void apt_normalLine_counted();
    void apt_phasedLine_excluded();
    void apt_phasedAt100_excluded();
    void apt_mixedLines_onlyNormalCounted();
    void apt_headerLine_notCounted();
};

void TestUpdateInfo::apt_normalLine_counted()
{
    QStringList lines = {
        "curl/jammy-updates 7.81.0-1ubuntu1.18 amd64 [upgradable from: 7.81.0-1ubuntu1.16]"
    };
    UpdateCheckResult result;
    UpdateInfoLinux::parseAptLines(lines, result);
    QCOMPARE(result.entries.size(), 1);
    QCOMPARE(result.entries.first().name, QString("curl"));
    QCOMPARE(result.entries.first().source, QString("apt"));
}

void TestUpdateInfo::apt_phasedLine_excluded()
{
    QStringList lines = {
        "apparmor/jammy-updates 3.0.4-2ubuntu2.4 amd64 [phased 40%] [upgradable from: 3.0.4-2ubuntu2.3]"
    };
    UpdateCheckResult result;
    UpdateInfoLinux::parseAptLines(lines, result);
    QCOMPARE(result.entries.size(), 0);
}

void TestUpdateInfo::apt_phasedAt100_excluded()
{
    QStringList lines = {
        "firmware-sof-signed/jammy-updates 2.2.6 amd64 [phased 100%] [upgradable from: 2.2.5]"
    };
    UpdateCheckResult result;
    UpdateInfoLinux::parseAptLines(lines, result);
    QCOMPARE(result.entries.size(), 0);
}

void TestUpdateInfo::apt_mixedLines_onlyNormalCounted()
{
    QStringList lines = {
        "Listing...",
        "curl/jammy-updates 7.81.0 amd64 [upgradable from: 7.80.0]",
        "apparmor/jammy-updates 3.0.4 amd64 [phased 40%] [upgradable from: 3.0.3]",
        "libapparmor1/jammy-updates 3.0.4 amd64 [phased 40%] [upgradable from: 3.0.3]",
        "git/jammy-updates 1:2.34.1-1ubuntu1.12 amd64 [upgradable from: 1:2.34.1-1ubuntu1.11]",
    };
    UpdateCheckResult result;
    UpdateInfoLinux::parseAptLines(lines, result);
    QCOMPARE(result.entries.size(), 2);
    QCOMPARE(result.entries.at(0).name, QString("curl"));
    QCOMPARE(result.entries.at(1).name, QString("git"));
}

void TestUpdateInfo::apt_headerLine_notCounted()
{
    QStringList lines = { "Listing..." };
    UpdateCheckResult result;
    UpdateInfoLinux::parseAptLines(lines, result);
    QCOMPARE(result.entries.size(), 0);
}

QTEST_MAIN(TestUpdateInfo)
#include "test_update_info.moc"
