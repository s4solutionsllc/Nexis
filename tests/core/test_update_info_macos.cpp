#include <QTest>
#include "Info/update_info_macos.h"

// SSO-3741 (FW-13): brew JSON / softwareupdate label parsers. Both are pure
// string-only and therefore compile cleanly on Linux too — running them in CI
// catches regressions even when no macOS runner is available.
class TestUpdateInfoMacOS : public QObject
{
    Q_OBJECT

private slots:
    void brew_outdatedJson_formulaeAndCasksParsed();
    void brew_outdatedJson_emptyObject_noEntries();
    void brew_outdatedJson_malformed_returnsNoEntries();
    void softwareupdate_listOutput_labelsExtracted();
    void softwareupdate_noStarLines_ignored();
};

void TestUpdateInfoMacOS::brew_outdatedJson_formulaeAndCasksParsed()
{
    QByteArray json = R"({
        "formulae": [
            { "name": "wget", "current_version": "1.21.4" },
            { "name": "node", "current_version": "20.15.1" }
        ],
        "casks": [
            { "name": "firefox", "current_version": "128.0" }
        ]
    })";
    UpdateCheckResult result;
    UpdateInfoMacOS::parseBrewOutdatedJson(json, result);
    QCOMPARE(result.entries.size(), 3);
    QCOMPARE(result.entries.at(0).source, QString("brew"));
    QCOMPARE(result.entries.at(0).name, QString("wget"));
    QCOMPARE(result.entries.at(0).version, QString("1.21.4"));
    QCOMPARE(result.entries.at(2).name, QString("firefox"));
}

void TestUpdateInfoMacOS::brew_outdatedJson_emptyObject_noEntries()
{
    QByteArray json = R"({"formulae": [], "casks": []})";
    UpdateCheckResult result;
    UpdateInfoMacOS::parseBrewOutdatedJson(json, result);
    QCOMPARE(result.entries.size(), 0);
}

void TestUpdateInfoMacOS::brew_outdatedJson_malformed_returnsNoEntries()
{
    QByteArray json = "not json at all";
    UpdateCheckResult result;
    UpdateInfoMacOS::parseBrewOutdatedJson(json, result);
    QCOMPARE(result.entries.size(), 0);
}

void TestUpdateInfoMacOS::softwareupdate_listOutput_labelsExtracted()
{
    // `softwareupdate -l` prints "* Label: <name>" rows interspersed with
    // titles/descriptions. We pick the lines starting with '*'.
    QStringList lines = {
        "Software Update Tool",
        "",
        "Finding available software",
        "Software Update found the following new or updated software:",
        "* Label: macOS Sonoma 14.5-23F79",
        "\tTitle: macOS Sonoma 14.5, Version: 14.5, Size: 6.4 GiB",
        "* Label: Safari17.5MontereyAuto-17.5",
        "\tTitle: Safari, Version: 17.5",
    };
    UpdateCheckResult result;
    UpdateInfoMacOS::parseSoftwareUpdateLines(lines, result);
    QCOMPARE(result.entries.size(), 2);
    QCOMPARE(result.entries.at(0).source, QString("system"));
    QCOMPARE(result.entries.at(0).name, QString("macOS Sonoma 14.5-23F79"));
    QCOMPARE(result.entries.at(1).name, QString("Safari17.5MontereyAuto-17.5"));
}

void TestUpdateInfoMacOS::softwareupdate_noStarLines_ignored()
{
    QStringList lines = {
        "Software Update Tool",
        "Finding available software",
        "No new software available.",
    };
    UpdateCheckResult result;
    UpdateInfoMacOS::parseSoftwareUpdateLines(lines, result);
    QCOMPARE(result.entries.size(), 0);
}

QTEST_MAIN(TestUpdateInfoMacOS)
#include "test_update_info_macos.moc"
