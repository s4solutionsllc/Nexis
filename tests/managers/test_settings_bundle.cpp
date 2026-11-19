// SSO-27150: settings export/import — versioned bundle, validation report,
// round trip, and the "never export secrets / machine state" allow-list.

#include <QTest>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "Managers/setting_manager.h"
#include "Utils/settings_bundle.h"

namespace {

const char kExclusions[] = R"([{"type":"folder","path":"/home/u/keep"},{"type":"file","path":"/home/u/a.txt"}])";

const char kProfile[] = R"({"id":"my-app","name":"My App","paths":["$HOME/.cache/my-app"],"safety":"safe"})";

void writeFile(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QByteArray bundleWith(const QJsonObject &settings, int version = 1, const QJsonArray &profiles = {})
{
    QJsonObject root{{"format", "nexis-settings"}, {"version", version},
                     {"settings", settings}, {"cleaningProfiles", profiles}};
    return QJsonDocument(root).toJson();
}

} // namespace

class TestSettingsBundle : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void export_isVersionedAndAllowListed();
    void export_neverIncludesMachineStateOrSecrets();
    void roundTrip_restoresSettingsExclusionsAndProfiles();
    void import_reportsUnknownAndInvalidKeysAndAppliesTheRest();
    void import_exclusionsAndCookieDomainsMergeNeverShrink();
    void import_dryRunWritesNothing();
    void import_refusesNewerVersion();
    void import_refusesMalformedBundles();
    void import_rejectsBadProfilesWithoutTouchingDisk();
    void import_profileIdCannotEscapeProfilesDir();

private:
    QTemporaryDir mDir;
    QString profilesDir() const { return mDir.path() + "/profiles"; }
};

void TestSettingsBundle::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(mDir.isValid());
}

void TestSettingsBundle::export_isVersionedAndAllowListed()
{
    SettingManager *sm = SettingManager::ins();
    sm->setCpuAlertPercent(77);

    const QJsonObject root = QJsonDocument::fromJson(
        SettingsBundle::exportBundle(sm, profilesDir(), "9.9.9")).object();
    QCOMPARE(root["format"].toString(), QString("nexis-settings"));
    QCOMPARE(root["version"].toInt(), SettingsBundle::kFormatVersion);
    QCOMPARE(root["appVersion"].toString(), QString("9.9.9"));
    QCOMPARE(root["settings"].toObject()["CPUAlertPercent"].toInt(), 77);
    for (const QString &k : root["settings"].toObject().keys())
        QVERIFY2(SettingsBundle::exportableKeys().contains(k), qPrintable(k));
}

void TestSettingsBundle::export_neverIncludesMachineStateOrSecrets()
{
    SettingManager *sm = SettingManager::ins();
    const QStringList machineOnly = {
        SettingKeys::WindowGeometry, SettingKeys::WindowState, SettingKeys::MiniMonitorGeometry,
        SettingKeys::TrustedBinderPrefixes, SettingKeys::WolHostNames, SettingKeys::NetUsageHistory,
        SettingKeys::Schedules, SettingKeys::ReportSchedules, SettingKeys::DownloadsAutoCleanPath,
        SettingKeys::DownloadsAutoCleanEnabled, SettingKeys::TempSensorId, SettingKeys::DiskAnalyzerCustomPath,
        SettingKeys::KioskMonitorName, SettingKeys::BatteryAlertSnoozedUntil,
    };
    for (const QString &k : machineOnly)
        sm->setRawValue(k, QStringLiteral("local-only-marker"));

    const QByteArray out = SettingsBundle::exportBundle(sm, profilesDir(), "1");
    const QJsonObject exported = QJsonDocument::fromJson(out).object()["settings"].toObject();
    for (const QString &k : machineOnly) {
        QVERIFY2(!exported.contains(k), qPrintable(k));
        QVERIFY2(!SettingsBundle::exportableKeys().contains(k), qPrintable(k));
    }
    QVERIFY(!out.contains("local-only-marker"));
}

void TestSettingsBundle::roundTrip_restoresSettingsExclusionsAndProfiles()
{
    SettingManager *sm = SettingManager::ins();
    sm->setCpuAlertPercent(81);
    sm->setThemeName("midnight");
    sm->setMinimizeToTray(true);
    sm->setCleanerExclusions(kExclusions);
    sm->setCleanerCookieKeepDomains({"example.org", "example.com"});
    sm->setDashboardLayout(R"({"tiles":["cpu","ram"]})");
    writeFile(profilesDir() + "/my-app.json", kProfile);

    const QByteArray bundle = SettingsBundle::exportBundle(sm, profilesDir(), "1.0.0");

    sm->setCpuAlertPercent(5);
    sm->setThemeName("default");
    sm->setMinimizeToTray(false);
    sm->setCleanerExclusions("[]");
    sm->setCleanerCookieKeepDomains({});
    sm->clearDashboardLayout();
    QVERIFY(QFile::remove(profilesDir() + "/my-app.json"));

    const SettingsBundleReport r = SettingsBundle::importBundle(bundle, sm, profilesDir(), false);
    QVERIFY2(r.ok, qPrintable(r.error));
    QVERIFY(r.unknownKeys.isEmpty());
    QVERIFY2(r.invalidKeys.isEmpty(), qPrintable(r.invalidKeys.join(";")));
    QVERIFY(r.profilesRejected.isEmpty());

    QCOMPARE(sm->getCpuAlertPercent(), 81);
    QCOMPARE(sm->getThemeName(), QString("midnight"));
    QVERIFY(sm->getMinimizeToTray());
    QCOMPARE(QJsonDocument::fromJson(sm->getCleanerExclusions().toUtf8()),
             QJsonDocument::fromJson(kExclusions));
    QCOMPARE(sm->getCleanerCookieKeepDomains(), (QStringList{"example.org", "example.com"}));
    QCOMPARE(QJsonDocument::fromJson(sm->getDashboardLayout().toUtf8()).object()["tiles"].toArray().size(), 2);
    QCOMPARE(r.profilesApplied, QStringList{"my-app"});
    QVERIFY(QFile::exists(profilesDir() + "/my-app.json"));

    // Second pass: exporting what was restored yields the same settings.
    const QJsonObject again = QJsonDocument::fromJson(SettingsBundle::exportBundle(sm, profilesDir(), "1.0.0")).object();
    QCOMPARE(again["settings"].toObject(), QJsonDocument::fromJson(bundle).object()["settings"].toObject());
    QCOMPARE(again["cleaningProfiles"].toArray(), QJsonDocument::fromJson(bundle).object()["cleaningProfiles"].toArray());
}

void TestSettingsBundle::import_reportsUnknownAndInvalidKeysAndAppliesTheRest()
{
    SettingManager *sm = SettingManager::ins();
    sm->setMemoryAlertPercent(10);
    sm->setCpuAlertPercent(10);

    const SettingsBundleReport r = SettingsBundle::importBundle(bundleWith({
        {"MemoryAlertPercent", 64},
        {"CPUAlertPercent", 250},           // out of range
        {"MinimizeToTray", "yes"},          // wrong type
        {"ThemeName", "bad\nname"},         // fails identifier check
        {"WolHostNames", "[]"},             // real key, deliberately not exportable
        {"NoSuchKey", 1},
    }), sm, profilesDir(), false);

    QVERIFY(r.ok);
    QCOMPARE(r.applied, QStringList{"MemoryAlertPercent"});
    QVERIFY(r.unknownKeys.contains("WolHostNames"));
    QVERIFY(r.unknownKeys.contains("NoSuchKey"));
    QCOMPARE(r.invalidKeys.size(), 3);
    QVERIFY(r.invalidKeys.join("|").contains("CPUAlertPercent"));
    QCOMPARE(sm->getMemoryAlertPercent(), 64);
    QCOMPARE(sm->getCpuAlertPercent(), 10);
}

void TestSettingsBundle::import_exclusionsAndCookieDomainsMergeNeverShrink()
{
    SettingManager *sm = SettingManager::ins();
    sm->setCleanerExclusions(R"([{"type":"folder","path":"/local/only"}])");
    sm->setCleanerCookieKeepDomains({"local.example"});

    const SettingsBundleReport r = SettingsBundle::importBundle(bundleWith({
        {"CleanerExclusions", QJsonDocument::fromJson(kExclusions).array()},
        {"CleanerCookieKeepDomains", QJsonArray{"local.example", "new.example"}},
    }), sm, profilesDir(), false);
    QVERIFY(r.ok);

    const QJsonArray ex = QJsonDocument::fromJson(sm->getCleanerExclusions().toUtf8()).array();
    QCOMPARE(ex.size(), 3);
    QCOMPARE(ex.first().toObject()["path"].toString(), QString("/local/only"));
    QCOMPARE(sm->getCleanerCookieKeepDomains(), (QStringList{"local.example", "new.example"}));

    // Importing the same bundle again adds nothing.
    SettingsBundle::importBundle(bundleWith({{"CleanerExclusions", QJsonDocument::fromJson(kExclusions).array()}}),
                                 sm, profilesDir(), false);
    QCOMPARE(QJsonDocument::fromJson(sm->getCleanerExclusions().toUtf8()).array().size(), 3);
}

void TestSettingsBundle::import_dryRunWritesNothing()
{
    SettingManager *sm = SettingManager::ins();
    sm->setCpuAlertPercent(11);
    QDir(profilesDir()).removeRecursively();

    const SettingsBundleReport r = SettingsBundle::importBundle(
        bundleWith({{"CPUAlertPercent", 90}}, 1, {QJsonDocument::fromJson(kProfile).object()}),
        sm, profilesDir(), true);
    QVERIFY(r.ok);
    QCOMPARE(r.applied, QStringList{"CPUAlertPercent"});
    QCOMPARE(r.profilesApplied, QStringList{"my-app"});
    QCOMPARE(sm->getCpuAlertPercent(), 11);
    QVERIFY(!QDir(profilesDir()).exists());
}

void TestSettingsBundle::import_refusesNewerVersion()
{
    SettingManager *sm = SettingManager::ins();
    sm->setCpuAlertPercent(12);
    const SettingsBundleReport r = SettingsBundle::importBundle(
        bundleWith({{"CPUAlertPercent", 99}}, SettingsBundle::kFormatVersion + 1), sm, profilesDir(), false);
    QVERIFY(!r.ok);
    QVERIFY(r.error.contains("version"));
    QCOMPARE(sm->getCpuAlertPercent(), 12);
}

void TestSettingsBundle::import_refusesMalformedBundles()
{
    SettingManager *sm = SettingManager::ins();
    const QList<QByteArray> bad = {
        "not json",
        "[]",
        R"({"version":1,"settings":{}})",                                   // no format
        R"({"format":"something-else","version":1})",
        R"({"format":"nexis-settings"})",                                   // no version
        R"({"format":"nexis-settings","version":0})",
        R"({"format":"nexis-settings","version":"1"})",
        R"({"format":"nexis-settings","version":1,"settings":[]})",
        R"({"format":"nexis-settings","version":1,"cleaningProfiles":{}})",
    };
    for (const QByteArray &b : bad) {
        const SettingsBundleReport r = SettingsBundle::importBundle(b, sm, profilesDir(), true);
        QVERIFY2(!r.ok && !r.error.isEmpty(), b.constData());
    }
}

void TestSettingsBundle::import_rejectsBadProfilesWithoutTouchingDisk()
{
    QDir(profilesDir()).removeRecursively();
    const QJsonArray profiles{
        QJsonObject{{"id", "no-paths"}, {"name", "x"}},
        QJsonObject{{"id", "bad-safety"}, {"name", "x"}, {"paths", QJsonArray{"$HOME/x"}}, {"safety", "yolo"}},
        QJsonValue("not-an-object"),
        QJsonDocument::fromJson(kProfile).object(),
    };
    const SettingsBundleReport r = SettingsBundle::importBundle(
        bundleWith({}, 1, profiles), SettingManager::ins(), profilesDir(), false);
    QVERIFY(r.ok);
    QCOMPARE(r.profilesApplied, QStringList{"my-app"});
    QCOMPARE(r.profilesRejected.size(), 3);
    QCOMPARE(QDir(profilesDir()).entryList({"*.json"}).size(), 1);
}

void TestSettingsBundle::import_profileIdCannotEscapeProfilesDir()
{
    QDir(profilesDir()).removeRecursively();
    const QJsonObject evil{{"id", "../../escaped"}, {"name", "x"}, {"paths", QJsonArray{"$HOME/x"}}};
    const SettingsBundleReport r = SettingsBundle::importBundle(
        bundleWith({}, 1, {evil}), SettingManager::ins(), profilesDir(), false);
    QVERIFY(r.ok);
    QVERIFY(!QFile::exists(mDir.path() + "/escaped.json"));
    const QStringList written = QDir(profilesDir()).entryList({"*.json"});
    QCOMPARE(written.size(), 1);
    QVERIFY(!written.first().contains('/'));
    QVERIFY(!written.first().startsWith('.'));
}

QTEST_MAIN(TestSettingsBundle)
#include "test_settings_bundle.moc"
