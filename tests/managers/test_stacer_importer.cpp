// SSO-15396: Stacer settings importer — parsing and mapping tests.
// Uses a real Stacer INI fixture (tests/fixtures/stacer/settings.ini).

#include <QTest>
#include <QStandardPaths>
#include <QFile>

#include "Managers/setting_manager.h"
#include "Utils/stacer_importer.h"

class TestStacerImporter : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void missingFile_returnsFileNotFound();
    void validFixture_fileFound();
    void validFixture_noEquivalentContainsThemeName();
    void validFixture_noEquivalentContainsFontSizeOffset();
    void validFixture_alertPercentsInWillChangeOrAlreadyMatch();
    void validFixture_startPageMigratedToDashboard();
    void validFixture_totalCountsMatchExpected();
    void apply_writesSettingsToManager();
};

static QString fixturePath()
{
    return QString(PROJECT_SOURCE_DIR) + "/tests/fixtures/stacer/settings.ini";
}

void TestStacerImporter::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    const QString cfg = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QFile::remove(cfg + "/settings.ini");
}

void TestStacerImporter::missingFile_returnsFileNotFound()
{
    StacerImportResult r = StacerImporter::parse("/nonexistent/path/settings.ini",
                                                  SettingManager::ins());
    QVERIFY(!r.fileFound);
    QVERIFY(!r.errorMessage.isEmpty());
}

void TestStacerImporter::validFixture_fileFound()
{
    StacerImportResult r = StacerImporter::parse(fixturePath(), SettingManager::ins());
    QVERIFY(r.fileFound);
    QVERIFY(r.errorMessage.isEmpty());
}

void TestStacerImporter::validFixture_noEquivalentContainsThemeName()
{
    StacerImportResult r = StacerImporter::parse(fixturePath(), SettingManager::ins());
    QVERIFY(r.noEquivalent.contains("ThemeName"));
}

void TestStacerImporter::validFixture_noEquivalentContainsFontSizeOffset()
{
    StacerImportResult r = StacerImporter::parse(fixturePath(), SettingManager::ins());
    QVERIFY(r.noEquivalent.contains("FontSizeOffset"));
}

void TestStacerImporter::validFixture_alertPercentsInWillChangeOrAlreadyMatch()
{
    StacerImportResult r = StacerImporter::parse(fixturePath(), SettingManager::ins());

    // CPUAlertPercent=85 in the fixture. It either differs from the Nexis default
    // (0) and appears in willChange, or it already matches and appears in alreadyMatch.
    bool foundCpu = false;
    for (const StacerMappedEntry &e : r.willChange) {
        if (e.key == "CPUAlertPercent") { foundCpu = true; break; }
    }
    if (!foundCpu) {
        // Already matches — must be in alreadyMatch list.
        QVERIFY(r.alreadyMatch.contains("CPU alert threshold"));
    } else {
        QVERIFY(foundCpu);
    }
}

void TestStacerImporter::validFixture_startPageMigratedToDashboard()
{
    // Fixture has StartPage=Dashboard (legacy English title).
    // migrateStartPageId should normalise this to the stable id "dashboard".
    StacerImportResult r = StacerImporter::parse(fixturePath(), SettingManager::ins());
    for (const StacerMappedEntry &e : r.willChange) {
        if (e.key == "StartPage") {
            QCOMPARE(e.toValue, QString("dashboard"));
            return;
        }
    }
    // Also acceptable: already matched "dashboard" — confirm via alreadyMatch.
    QVERIFY(r.alreadyMatch.contains("Start page"));
}

void TestStacerImporter::validFixture_totalCountsMatchExpected()
{
    StacerImportResult r = StacerImporter::parse(fixturePath(), SettingManager::ins());

    // 8 mappable keys + 2 no-equivalent keys = 10 total Stacer keys in fixture.
    const int mappedTotal = r.willChange.size() + r.alreadyMatch.size();
    QCOMPARE(mappedTotal, 8);
    QCOMPARE(r.noEquivalent.size(), 2);
}

void TestStacerImporter::apply_writesSettingsToManager()
{
    SettingManager *sm = SettingManager::ins();
    // Reset to known state.
    sm->setCpuAlertPercent(0);
    sm->setMemoryAlertPercent(0);
    sm->setDiskAlertPercent(0);

    StacerImportResult r = StacerImporter::parse(fixturePath(), sm);
    StacerImporter::apply(r, sm);

    // Fixture sets CPU=85, Memory=90, Disk=80 — verify they were written.
    // (They might already match if a prior test applied them; the value must
    // match the fixture regardless.)
    QCOMPARE(sm->getCpuAlertPercent(), 85);
    QCOMPARE(sm->getMemoryAlertPercent(), 90);
    QCOMPARE(sm->getDiskAlertPercent(), 80);
}

QTEST_MAIN(TestStacerImporter)
#include "test_stacer_importer.moc"
