#include <QTest>
#include "Info/update_info_linux.h"

class TestUpdateInfo : public QObject
{
    Q_OBJECT

private slots:
    // SSO-3741 (FW-13) — Linux parsers
    void apt_normalLine_counted();
    void apt_phasedLine_excluded();
    void apt_phasedAt100_excluded();
    void apt_mixedLines_onlyNormalCounted();
    void apt_headerLine_notCounted();
    void dnf_packageTable_parsed();
    void dnf_blankAndJunkLines_ignored();
    void pacman_qu_rowsParsed();
    void zypper_listUpdates_rowsParsed();
    void snap_refreshList_skipsHeader();
    void snap_emptyMessage_ignored();
    void flatpak_remoteLsUpdates_rowsParsed();
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

// ---------------------------------------------------------------------------
// SSO-3741 (FW-13): dnf check-update output. Each upgradable package shows
// up as "<name>.<arch>  <new-version>  <repo>" — we parse the name (stripping
// arch) and new version, and ignore blank lines / Obsoleting Packages headers.
void TestUpdateInfo::dnf_packageTable_parsed()
{
    QStringList lines = {
        "kernel.x86_64        6.10.1-1.fc40       updates",
        "openssl-libs.x86_64  3.2.2-3.fc40        updates",
    };
    UpdateCheckResult result;
    UpdateInfoLinux::parseDnfCheckUpdateLines(lines, result);
    QCOMPARE(result.entries.size(), 2);
    QCOMPARE(result.entries.at(0).name, QString("kernel"));
    QCOMPARE(result.entries.at(0).version, QString("6.10.1-1.fc40"));
    QCOMPARE(result.entries.at(0).source, QString("dnf"));
    QCOMPARE(result.entries.at(1).name, QString("openssl-libs"));
}

void TestUpdateInfo::dnf_blankAndJunkLines_ignored()
{
    QStringList lines = {
        "",
        "Obsoleting Packages",
        "kernel.x86_64        6.10.1-1.fc40       updates",
        "    quick brown fox",      // < 3 cols after split, dropped
        "noarchhere    abc def",    // first col has no '.', dropped
    };
    UpdateCheckResult result;
    UpdateInfoLinux::parseDnfCheckUpdateLines(lines, result);
    QCOMPARE(result.entries.size(), 1);
    QCOMPARE(result.entries.first().name, QString("kernel"));
}

// pacman -Qu emits "<name> <old> -> <new>" — column 3 is the new version.
void TestUpdateInfo::pacman_qu_rowsParsed()
{
    QStringList lines = {
        "linux 6.10.1.arch1-1 -> 6.10.2.arch1-1",
        "openssl 3.3.1-1 -> 3.3.2-1",
    };
    UpdateCheckResult result;
    UpdateInfoLinux::parsePacmanQuLines(lines, result);
    QCOMPARE(result.entries.size(), 2);
    QCOMPARE(result.entries.at(0).name, QString("linux"));
    QCOMPARE(result.entries.at(0).version, QString("6.10.2.arch1-1"));
    QCOMPARE(result.entries.at(1).name, QString("openssl"));
}

// zypper list-updates emits a "v | repo | name | curver | newver | arch" table
// where the leading "v |" tag marks an actionable update row.
void TestUpdateInfo::zypper_listUpdates_rowsParsed()
{
    QStringList lines = {
        "v | Main Repo | bash | 5.2.15 | 5.2.21 | x86_64",
        "v | Main Repo | openssl | 3.0.10 | 3.0.12 | x86_64",
        "S | Repo | other | 1.0 | 1.0 | x86_64",  // not an update
    };
    UpdateCheckResult result;
    UpdateInfoLinux::parseZypperListUpdatesLines(lines, result);
    QCOMPARE(result.entries.size(), 2);
    QCOMPARE(result.entries.at(0).name, QString("bash"));
    QCOMPARE(result.entries.at(0).version, QString("5.2.21"));
    QCOMPARE(result.entries.at(1).name, QString("openssl"));
}

// `snap refresh --list` emits a header row that we drop, then space-separated
// rows whose first two columns are the snap name and the offered revision.
void TestUpdateInfo::snap_refreshList_skipsHeader()
{
    QStringList lines = {
        "Name     Version  Rev   Size   Publisher  Notes",
        "firefox  128.0    4173  240MB  canonical  -",
        "core22   20240701 1410  77MB   canonical  base",
    };
    UpdateCheckResult result;
    UpdateInfoLinux::parseSnapRefreshLines(lines, result);
    QCOMPARE(result.entries.size(), 2);
    QCOMPARE(result.entries.at(0).name, QString("firefox"));
    QCOMPARE(result.entries.at(0).version, QString("128.0"));
    QCOMPARE(result.entries.at(1).name, QString("core22"));
}

void TestUpdateInfo::snap_emptyMessage_ignored()
{
    // snap emits a single non-table line when nothing is refreshable.
    QStringList lines = { "All snaps up to date." };
    UpdateCheckResult result;
    UpdateInfoLinux::parseSnapRefreshLines(lines, result);
    // The "header skip" consumes the first non-empty line; the parser then
    // ignores anything that doesn't look like a table row.
    QCOMPARE(result.entries.size(), 0);
}

// flatpak --columns=application,version emits tab-separated rows.
void TestUpdateInfo::flatpak_remoteLsUpdates_rowsParsed()
{
    QStringList lines = {
        "org.mozilla.firefox\t128.0",
        "com.github.tchx84.Flatseal\t2.2.0",
    };
    UpdateCheckResult result;
    UpdateInfoLinux::parseFlatpakUpdateLines(lines, result);
    QCOMPARE(result.entries.size(), 2);
    QCOMPARE(result.entries.at(0).name, QString("org.mozilla.firefox"));
    QCOMPARE(result.entries.at(0).version, QString("128.0"));
    QCOMPARE(result.entries.at(1).source, QString("flatpak"));
}

QTEST_MAIN(TestUpdateInfo)
#include "test_update_info.moc"
