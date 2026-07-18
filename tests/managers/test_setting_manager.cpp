// SSO-3388 / audit Q3: start-page is now persisted as a stable, untranslated
// id ("dashboard", "uninstaller", …) rather than the localized combo text.
// The launch path used to match the saved text against translated sidebar
// titles, so changing the UI language silently reset the start page. These
// tests exercise the migration helper and the round-trip via SettingManager.

#include <QTest>
#include <QStandardPaths>
#include <QFile>
#include <QDir>

#include "Managers/setting_manager.h"

class TestSettingManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // Static migrator: covers all legacy-mapping branches without touching disk.
    void migrate_knownIdRoundTrips();
    void migrate_legacyEnglishTitle();
    void migrate_legacyMacApplicationsVariant();
    void migrate_unknownDefaultsToDashboard();
    void migrate_emptyDefaultsToDashboard();

    // Through-the-singleton round-trip: writes go through setStartPage, reads
    // resolve via migrateStartPageId. Slot order is deliberate — the
    // "defaults to dashboard" check runs first against the freshly-redirected
    // test-mode config dir before any setStartPage call dirties it.
    void getStartPage_defaultsToDashboard();
    void getStartPage_roundTripsId();
    void getStartPage_migratesLegacyValue();
    void getStartPage_unknownLegacyDefaultsToDashboard();

    // GH#207 / SSO-8351 — kiosk-at-startup + monitor targeting settings.
    void kioskLaunch_defaultsToFalse();
    void kioskLaunch_roundTrips();
    void kioskMonitorName_defaultsToEmpty();
    void kioskMonitorName_roundTrips();
};

void TestSettingManager::initTestCase()
{
    // Redirect QStandardPaths::writableLocation(AppConfigLocation) into a
    // temp dir so the SettingManager singleton writes to ~/.qttest rather
    // than the developer's real config. Must be called before
    // SettingManager::ins() is first invoked.
    QStandardPaths::setTestModeEnabled(true);

    // The test-mode config dir can persist across runs on the same machine
    // (e.g. when re-running the same binary). Wipe any pre-existing
    // settings.ini so the "defaults" test starts from a truly fresh state.
    const QString cfg = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QFile::remove(cfg + "/settings.ini");
}

void TestSettingManager::migrate_knownIdRoundTrips()
{
    QCOMPARE(SettingManager::migrateStartPageId("dashboard"),     QString("dashboard"));
    QCOMPARE(SettingManager::migrateStartPageId("uninstaller"),   QString("uninstaller"));
    QCOMPARE(SettingManager::migrateStartPageId("systemCleaner"), QString("systemCleaner"));
    QCOMPARE(SettingManager::migrateStartPageId("settings"),      QString("settings"));
}

void TestSettingManager::migrate_legacyEnglishTitle()
{
    // The previous build saved the English combo text directly.
    QCOMPARE(SettingManager::migrateStartPageId("Dashboard"),      QString("dashboard"));
    QCOMPARE(SettingManager::migrateStartPageId("Startup Apps"),   QString("startupApps"));
    QCOMPARE(SettingManager::migrateStartPageId("System Cleaner"), QString("systemCleaner"));
    QCOMPARE(SettingManager::migrateStartPageId("Search"),         QString("search"));
    QCOMPARE(SettingManager::migrateStartPageId("Services"),       QString("services"));
    QCOMPARE(SettingManager::migrateStartPageId("Processes"),      QString("processes"));
    QCOMPARE(SettingManager::migrateStartPageId("Helpers"),        QString("helpers"));
    QCOMPARE(SettingManager::migrateStartPageId("Uninstaller"),    QString("uninstaller"));
    QCOMPARE(SettingManager::migrateStartPageId("Resources"),      QString("resources"));
}

void TestSettingManager::migrate_legacyMacApplicationsVariant()
{
    // On macOS the sidebar tooltip for the uninstaller page is "Applications"
    // rather than "Uninstaller"; that variant must also map back to the
    // uninstaller id so cross-platform start-page is consistent.
    QCOMPARE(SettingManager::migrateStartPageId("Applications"), QString("uninstaller"));
}

void TestSettingManager::migrate_unknownDefaultsToDashboard()
{
    QCOMPARE(SettingManager::migrateStartPageId("Tableau de bord"), QString("dashboard"));
    QCOMPARE(SettingManager::migrateStartPageId("not-a-real-page"), QString("dashboard"));
    QCOMPARE(SettingManager::migrateStartPageId("Дашборд"),         QString("dashboard"));
}

void TestSettingManager::migrate_emptyDefaultsToDashboard()
{
    QCOMPARE(SettingManager::migrateStartPageId(""), QString("dashboard"));
}

void TestSettingManager::getStartPage_defaultsToDashboard()
{
    // Fresh install (no key set): default to the dashboard id, not the
    // previous-build default of QObject::tr("Dashboard").
    QCOMPARE(SettingManager::ins()->getStartPage(), QString("dashboard"));
}

void TestSettingManager::getStartPage_roundTripsId()
{
    SettingManager *sm = SettingManager::ins();
    sm->setStartPage("uninstaller");
    QCOMPARE(sm->getStartPage(), QString("uninstaller"));
    sm->setStartPage("processes");
    QCOMPARE(sm->getStartPage(), QString("processes"));
}

void TestSettingManager::getStartPage_migratesLegacyValue()
{
    SettingManager *sm = SettingManager::ins();
    // Simulate an existing settings.ini saved by the previous build that
    // persisted the localized combo text directly. The legacy macOS sidebar
    // tooltip "Applications" must read back as the canonical id "uninstaller".
    sm->setStartPage("Applications");
    QCOMPARE(sm->getStartPage(), QString("uninstaller"));

    sm->setStartPage("System Cleaner");
    QCOMPARE(sm->getStartPage(), QString("systemCleaner"));
}

void TestSettingManager::getStartPage_unknownLegacyDefaultsToDashboard()
{
    SettingManager *sm = SettingManager::ins();
    // A user who saved their start page while a non-English UI language was
    // active will have an unrecognized string in settings.ini. That should
    // resolve to the dashboard rather than the launch path falling back
    // through a no-match nullptr.
    sm->setStartPage("Some Unknown Translated String");
    QCOMPARE(sm->getStartPage(), QString("dashboard"));
}

void TestSettingManager::kioskLaunch_defaultsToFalse()
{
    // Fresh install: kiosk-at-startup is off unless the user opts in.
    QCOMPARE(SettingManager::ins()->getLaunchInKioskMode(), false);
}

void TestSettingManager::kioskLaunch_roundTrips()
{
    SettingManager *sm = SettingManager::ins();
    sm->setLaunchInKioskMode(true);
    QCOMPARE(sm->getLaunchInKioskMode(), true);
    sm->setLaunchInKioskMode(false);
    QCOMPARE(sm->getLaunchInKioskMode(), false);
}

void TestSettingManager::kioskMonitorName_defaultsToEmpty()
{
    // Empty means "same screen as last time" — no forced monitor.
    QCOMPARE(SettingManager::ins()->getKioskMonitorName(), QString(""));
}

void TestSettingManager::kioskMonitorName_roundTrips()
{
    SettingManager *sm = SettingManager::ins();
    sm->setKioskMonitorName("DP-2");
    QCOMPARE(sm->getKioskMonitorName(), QString("DP-2"));
    sm->setKioskMonitorName("");
    QCOMPARE(sm->getKioskMonitorName(), QString(""));
}

QTEST_MAIN(TestSettingManager)
#include "test_setting_manager.moc"
