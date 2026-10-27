#include <QTest>
#include "Info/update_info_linux.h"

class TestUpdateInfo : public QObject
{
    Q_OBJECT

private slots:
    void apt_normalLine_counted();
    void apt_phasedBracketLine_stillIncluded();
    void apt_phasedAt100BracketLine_stillIncluded();
    void apt_mixedLines_allUpgradableIncluded();
    void apt_headerLine_notCounted();

    void aptCachePolicy_phasedPackage_percentageParsed();
    void aptCachePolicy_nonPhasedPackage_noPercentageReported();
    void aptCachePolicy_multiplePackages_eachKeyedCorrectly();
    void aptCachePolicy_reporterRegressionFixture_allTwelvePackagesDetected();
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

// GH#524: apt list --upgradable's "[phased N%]" annotation is not reliable,
// so parseAptLines no longer string-matches it to exclude entries — phased
// detection moved to apt-cache policy (see aptCachePolicy_* tests below).
void TestUpdateInfo::apt_phasedBracketLine_stillIncluded()
{
    QStringList lines = {
        "apparmor/jammy-updates 3.0.4-2ubuntu2.4 amd64 [phased 40%] [upgradable from: 3.0.4-2ubuntu2.3]"
    };
    UpdateCheckResult result;
    UpdateInfoLinux::parseAptLines(lines, result);
    QCOMPARE(result.entries.size(), 1);
    QCOMPARE(result.entries.first().name, QString("apparmor"));
}

void TestUpdateInfo::apt_phasedAt100BracketLine_stillIncluded()
{
    QStringList lines = {
        "firmware-sof-signed/jammy-updates 2.2.6 amd64 [phased 100%] [upgradable from: 2.2.5]"
    };
    UpdateCheckResult result;
    UpdateInfoLinux::parseAptLines(lines, result);
    QCOMPARE(result.entries.size(), 1);
    QCOMPARE(result.entries.first().name, QString("firmware-sof-signed"));
}

void TestUpdateInfo::apt_mixedLines_allUpgradableIncluded()
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
    QCOMPARE(result.entries.size(), 4);
    QCOMPARE(result.entries.at(0).name, QString("curl"));
    QCOMPARE(result.entries.at(1).name, QString("apparmor"));
    QCOMPARE(result.entries.at(2).name, QString("libapparmor1"));
    QCOMPARE(result.entries.at(3).name, QString("git"));
}

void TestUpdateInfo::apt_headerLine_notCounted()
{
    QStringList lines = { "Listing..." };
    UpdateCheckResult result;
    UpdateInfoLinux::parseAptLines(lines, result);
    QCOMPARE(result.entries.size(), 0);
}

void TestUpdateInfo::aptCachePolicy_phasedPackage_percentageParsed()
{
    QString output =
        "rtkit:\n"
        "  Installed: 0.13-3\n"
        "  Candidate: 0.14-1ubuntu2\n"
        "  Version table:\n"
        "     0.14-1ubuntu2 500 (phased 20%)\n"
        "        500 http://archive.ubuntu.com/ubuntu noble-updates/main amd64 Packages\n"
        " *** 0.13-3 100\n"
        "        100 /var/lib/dpkg/status\n";

    QMap<QString, int> percentages = UpdateInfoLinux::parseAptCachePolicyPhased(output);
    QCOMPARE(percentages.value("rtkit", -1), 20);
}

void TestUpdateInfo::aptCachePolicy_nonPhasedPackage_noPercentageReported()
{
    QString output =
        "curl:\n"
        "  Installed: 7.81.0-1ubuntu1.16\n"
        "  Candidate: 7.81.0-1ubuntu1.18\n"
        "  Version table:\n"
        "     7.81.0-1ubuntu1.18 500\n"
        "        500 http://archive.ubuntu.com/ubuntu jammy-updates/main amd64 Packages\n"
        " *** 7.81.0-1ubuntu1.16 100\n"
        "        100 /var/lib/dpkg/status\n";

    QMap<QString, int> percentages = UpdateInfoLinux::parseAptCachePolicyPhased(output);
    QVERIFY(!percentages.contains("curl"));
}

void TestUpdateInfo::aptCachePolicy_multiplePackages_eachKeyedCorrectly()
{
    QString output =
        "curl:\n"
        "  Installed: 7.81.0-1ubuntu1.16\n"
        "  Candidate: 7.81.0-1ubuntu1.18\n"
        "  Version table:\n"
        "     7.81.0-1ubuntu1.18 500\n"
        "        500 http://archive.ubuntu.com/ubuntu jammy-updates/main amd64 Packages\n"
        " *** 7.81.0-1ubuntu1.16 100\n"
        "        100 /var/lib/dpkg/status\n"
        "rtkit:\n"
        "  Installed: 0.13-3\n"
        "  Candidate: 0.14-1ubuntu2\n"
        "  Version table:\n"
        "     0.14-1ubuntu2 500 (phased 20%)\n"
        "        500 http://archive.ubuntu.com/ubuntu noble-updates/main amd64 Packages\n"
        " *** 0.13-3 100\n"
        "        100 /var/lib/dpkg/status\n"
        "ubuntu-pro-client-l10n:\n"
        "  Installed: 36~24.04\n"
        "  Candidate: 37~24.04\n"
        "  Version table:\n"
        "     37~24.04 500 (phased 100%)\n"
        "        500 http://archive.ubuntu.com/ubuntu noble-updates/main amd64 Packages\n"
        " *** 36~24.04 100\n"
        "        100 /var/lib/dpkg/status\n";

    QMap<QString, int> percentages = UpdateInfoLinux::parseAptCachePolicyPhased(output);
    QVERIFY(!percentages.contains("curl"));
    QCOMPARE(percentages.value("rtkit", -1), 20);
    // Present at 100% — checkApt() treats this as no longer phased (reverts
    // to a normal, actionable entry), but the raw parse still reports it.
    QCOMPARE(percentages.value("ubuntu-pro-client-l10n", -1), 100);
}

// GH#524 regression fixture: the reporter's 12-package list, none of which
// reliably carried "[phased N%]" in their apt list --upgradable output, but
// all of which apt-cache policy correctly reports as phased below 100%
// (except one, included here to also cover the 100%-reverts-to-normal case).
void TestUpdateInfo::aptCachePolicy_reporterRegressionFixture_allTwelvePackagesDetected()
{
    struct Fixture { QString name; QString installed; QString candidate; int percent; };
    const QList<Fixture> fixtures = {
        {"containerd",                "1.7.19-0ubuntu1",   "1.7.20-0ubuntu1",   40},
        {"gir1.2-adw-1",               "1.5.1-1",           "1.5.2-1",           20},
        {"gir1.2-mutter-18",           "46.0-1ubuntu1",     "46.1-1ubuntu1",     20},
        {"libadwaita-1-0",             "1.5.1-1",           "1.5.2-1",           20},
        {"libinsights0",               "0.9-1",             "0.10-1",            60},
        {"libmutter-18-0",             "46.0-1ubuntu1",     "46.1-1ubuntu1",     20},
        {"mutter-common",              "46.0-1ubuntu1",     "46.1-1ubuntu1",     20},
        {"mutter-common-bin",          "46.0-1ubuntu1",     "46.1-1ubuntu1",     20},
        {"rtkit",                      "0.13-3",            "0.14-1ubuntu2",     20},
        {"ubuntu-insights",            "0.9-1",             "0.10-1",            60},
        {"ubuntu-pro-client",          "36~24.04",          "37~24.04",          80},
        {"ubuntu-pro-client-l10n",     "36~24.04",          "37~24.04",          100},
    };

    QString output;
    for (const Fixture &f : fixtures) {
        output += f.name + ":\n";
        output += "  Installed: " + f.installed + "\n";
        output += "  Candidate: " + f.candidate + "\n";
        output += "  Version table:\n";
        output += "     " + f.candidate + " 500 (phased " + QString::number(f.percent) + "%)\n";
        output += "        500 http://archive.ubuntu.com/ubuntu noble-updates/main amd64 Packages\n";
        output += " *** " + f.installed + " 100\n";
        output += "        100 /var/lib/dpkg/status\n";
    }

    QMap<QString, int> percentages = UpdateInfoLinux::parseAptCachePolicyPhased(output);
    QCOMPARE(percentages.size(), fixtures.size());
    for (const Fixture &f : fixtures)
        QCOMPARE(percentages.value(f.name, -1), f.percent);

    // Mirror checkApt()'s < 100 cutoff: 11 stay phased/non-actionable, the
    // 100% package reverts to a normal entry.
    int phasedCount = 0;
    for (const Fixture &f : fixtures) {
        if (percentages.value(f.name, -1) < 100)
            ++phasedCount;
    }
    QCOMPARE(phasedCount, 11);
}

QTEST_MAIN(TestUpdateInfo)
#include "test_update_info.moc"
