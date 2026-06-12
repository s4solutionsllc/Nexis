#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QDateTime>
#include "Managers/cleaning_profiles_service.h"

// FW-12: CleaningProfilesService — schema parse/validate, glob expansion against
// a QTemporaryDir, safe-vs-aggressive gating, and exclusions integration.
class TestCleaningProfiles : public QObject
{
    Q_OBJECT

private:
    using Service = CleaningProfilesService;
    using Profile = Service::Profile;
    using ValidationError = Service::ValidationError;

    QString writeFile(const QString &path, const QByteArray &data)
    {
        QFileInfo(path).absoluteDir().mkpath(".");
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(data);
        f.close();
        return path;
    }

    void writeProfile(const QString &dir, const QString &name, const QByteArray &json)
    {
        QDir().mkpath(dir);
        writeFile(dir + "/" + name + ".json", json);
    }

private slots:
    void initTestCase();

    void parseProfile_minimalValidProfile_succeeds();
    void parseProfile_missingId_rejected();
    void parseProfile_missingPaths_rejected();
    void parseProfile_emptyPaths_rejected();
    void parseProfile_invalidSafety_rejected();
    void parseProfile_negativeAge_rejected();
    void parseProfile_invalidPlatform_rejected();
    void parseProfile_malformedJson_rejected();
    void parseProfile_safetyDefaultsToSafe();

    void loadFrom_filtersByPlatform();
    void loadFrom_userProfileOverridesBundledById();
    void loadFrom_userOnlyProfileIsAppended();
    void loadFrom_collectsValidationErrors();

    void expandPaths_simpleGlob_returnsMatchingEntries();
    void expandPaths_homePlaceholder_resolves();
    void expandPaths_unresolvedPlaceholder_silentlyDropsTokenSegment();
    void expandPaths_nestedWildcards_walkAcrossSegments();
    void expandPaths_minAgeDays_filtersRecentFiles();
    void expandPaths_unknownPath_returnsEmpty();

    void scan_aggressiveProfileGatedOff_excluded();
    void scan_aggressiveProfileGatedOn_included();
    void scan_excludedPathsHonored();
    void scan_perProfileBreakdown_isPopulated();
};

void TestCleaningProfiles::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

// ─── parseProfile ────────────────────────────────────────────────────────────

void TestCleaningProfiles::parseProfile_minimalValidProfile_succeeds()
{
    const QByteArray json = R"({
        "id": "demo",
        "name": "Demo App",
        "paths": ["$HOME/.cache/demo"]
    })";
    Profile p;
    ValidationError err;
    QVERIFY(Service::parseProfile(json, p, err));
    QCOMPARE(p.id, QString("demo"));
    QCOMPARE(p.name, QString("Demo App"));
    QCOMPARE(p.app, QString("Demo App"));    // falls back to name
    QCOMPARE(p.safety, Service::Safety::Safe);
    QCOMPARE(p.paths.size(), 1);
    QCOMPARE(p.minAgeDays, 0);
    QVERIFY(p.platforms.isEmpty());          // empty = both
}

void TestCleaningProfiles::parseProfile_missingId_rejected()
{
    const QByteArray json = R"({
        "name": "x",
        "paths": ["/tmp/x"]
    })";
    Profile p;
    ValidationError err;
    QVERIFY(!Service::parseProfile(json, p, err));
    QVERIFY(err.message.contains("id", Qt::CaseInsensitive));
}

void TestCleaningProfiles::parseProfile_missingPaths_rejected()
{
    const QByteArray json = R"({
        "id": "x",
        "name": "X"
    })";
    Profile p;
    ValidationError err;
    QVERIFY(!Service::parseProfile(json, p, err));
    QVERIFY(err.message.contains("paths"));
}

void TestCleaningProfiles::parseProfile_emptyPaths_rejected()
{
    const QByteArray json = R"({
        "id": "x",
        "name": "X",
        "paths": []
    })";
    Profile p;
    ValidationError err;
    QVERIFY(!Service::parseProfile(json, p, err));
    QVERIFY(err.message.contains("paths"));
}

void TestCleaningProfiles::parseProfile_invalidSafety_rejected()
{
    const QByteArray json = R"({
        "id": "x",
        "name": "X",
        "safety": "nuclear",
        "paths": ["/tmp/x"]
    })";
    Profile p;
    ValidationError err;
    QVERIFY(!Service::parseProfile(json, p, err));
    QVERIFY(err.message.contains("safety", Qt::CaseInsensitive));
}

void TestCleaningProfiles::parseProfile_negativeAge_rejected()
{
    const QByteArray json = R"({
        "id": "x",
        "name": "X",
        "paths": ["/tmp/x"],
        "minAgeDays": -5
    })";
    Profile p;
    ValidationError err;
    QVERIFY(!Service::parseProfile(json, p, err));
    QVERIFY(err.message.contains("minAgeDays"));
}

void TestCleaningProfiles::parseProfile_invalidPlatform_rejected()
{
    const QByteArray json = R"({
        "id": "x",
        "name": "X",
        "paths": ["/tmp/x"],
        "platforms": ["bsd"]
    })";
    Profile p;
    ValidationError err;
    QVERIFY(!Service::parseProfile(json, p, err));
    QVERIFY(err.message.contains("platform", Qt::CaseInsensitive));
}

void TestCleaningProfiles::parseProfile_malformedJson_rejected()
{
    const QByteArray json = "{ this is not json ;;;";
    Profile p;
    ValidationError err;
    QVERIFY(!Service::parseProfile(json, p, err));
    QVERIFY(err.message.contains("JSON", Qt::CaseInsensitive));
}

void TestCleaningProfiles::parseProfile_safetyDefaultsToSafe()
{
    const QByteArray json = R"({
        "id": "x",
        "name": "X",
        "paths": ["/tmp/x"]
    })";
    Profile p;
    ValidationError err;
    QVERIFY(Service::parseProfile(json, p, err));
    QCOMPARE(p.safety, Service::Safety::Safe);
}

// ─── loadFrom (platform filter + user override + error collection) ───────────

void TestCleaningProfiles::loadFrom_filtersByPlatform()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString bundle = tmp.path() + "/bundle";

    writeProfile(bundle, "linux-only", R"({
        "id": "linux-only", "name": "Lin", "paths": ["/tmp/lin"],
        "platforms": ["linux"]
    })");
    writeProfile(bundle, "mac-only", R"({
        "id": "mac-only", "name": "Mac", "paths": ["/tmp/mac"],
        "platforms": ["macos"]
    })");
    writeProfile(bundle, "both", R"({
        "id": "both", "name": "Both", "paths": ["/tmp/both"]
    })");

    Service svc;
    const auto loadedLin = svc.loadFrom({bundle}, {}, "linux");
    QStringList linIds;
    for (const auto &p : loadedLin) linIds << p.id;
    QVERIFY(linIds.contains("linux-only"));
    QVERIFY(linIds.contains("both"));
    QVERIFY(!linIds.contains("mac-only"));

    const auto loadedMac = svc.loadFrom({bundle}, {}, "macos");
    QStringList macIds;
    for (const auto &p : loadedMac) macIds << p.id;
    QVERIFY(macIds.contains("mac-only"));
    QVERIFY(macIds.contains("both"));
    QVERIFY(!macIds.contains("linux-only"));
}

void TestCleaningProfiles::loadFrom_userProfileOverridesBundledById()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString bundle = tmp.path() + "/bundle";
    const QString user   = tmp.path() + "/user";

    writeProfile(bundle, "shared", R"({
        "id": "shared", "name": "Bundled name",
        "paths": ["/tmp/bundled"]
    })");
    writeProfile(user, "shared", R"({
        "id": "shared", "name": "User name",
        "paths": ["/tmp/user"]
    })");

    Service svc;
    const auto loaded = svc.loadFrom({bundle}, {user}, "linux");
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(loaded.first().name, QString("User name"));
    QCOMPARE(loaded.first().paths.first(), QString("/tmp/user"));
}

void TestCleaningProfiles::loadFrom_userOnlyProfileIsAppended()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString bundle = tmp.path() + "/bundle";
    const QString user   = tmp.path() + "/user";

    writeProfile(bundle, "bundled", R"({
        "id": "bundled", "name": "B", "paths": ["/tmp/b"]
    })");
    writeProfile(user, "custom", R"({
        "id": "custom", "name": "C", "paths": ["/tmp/c"]
    })");

    Service svc;
    const auto loaded = svc.loadFrom({bundle}, {user}, "linux");
    QStringList ids;
    for (const auto &p : loaded) ids << p.id;
    QVERIFY(ids.contains("bundled"));
    QVERIFY(ids.contains("custom"));
}

void TestCleaningProfiles::loadFrom_collectsValidationErrors()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString bundle = tmp.path() + "/bundle";

    writeProfile(bundle, "good", R"({
        "id": "good", "name": "G", "paths": ["/tmp/g"]
    })");
    writeProfile(bundle, "broken", "not json at all");

    Service svc;
    const auto loaded = svc.loadFrom({bundle}, {}, "linux");
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(loaded.first().id, QString("good"));

    const auto errs = svc.lastErrors();
    QCOMPARE(errs.size(), 1);
    QVERIFY(errs.first().file.endsWith("broken.json"));
}

// ─── expandPaths ────────────────────────────────────────────────────────────

void TestCleaningProfiles::expandPaths_simpleGlob_returnsMatchingEntries()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString root = tmp.path() + "/sandbox";
    QDir().mkpath(root + "/match-A");
    QDir().mkpath(root + "/match-B");
    QDir().mkpath(root + "/skipme");

    Profile p;
    p.id = "demo";
    p.paths = {root + "/match-*"};

    Service svc;
    const QFileInfoList entries = svc.expandPaths(p);
    QStringList names;
    for (const QFileInfo &fi : entries) names << fi.fileName();
    names.sort();
    QCOMPARE(names, (QStringList() << "match-A" << "match-B"));
}

void TestCleaningProfiles::expandPaths_homePlaceholder_resolves()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QDir().mkpath(tmp.path() + "/.cache/demo");
    writeFile(tmp.path() + "/.cache/demo/log.txt", "hi");

    Profile p;
    p.id = "demo";
    p.paths = {"$HOME/.cache/demo"};

    Service svc;
    auto resolver = [&](const QString &name) -> QString {
        return (name == "HOME") ? tmp.path() : QString();
    };
    const QFileInfoList entries = svc.expandPaths(p, resolver);
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().fileName(), QString("demo"));
}

void TestCleaningProfiles::expandPaths_unresolvedPlaceholder_silentlyDropsTokenSegment()
{
    // An unresolved placeholder should not crash and should not silently match
    // every filesystem object — the expansion yields an empty list. A profile
    // referencing $XYZ on a system without XYZ defined just produces zero
    // candidates, which is the desired safe behavior.
    Profile p;
    p.id = "demo";
    p.paths = {"$DOES_NOT_EXIST_ABCDEF/cache"};

    Service svc;
    auto resolver = [](const QString &) -> QString { return QString(); };
    const QFileInfoList entries = svc.expandPaths(p, resolver);
    QVERIFY(entries.isEmpty());
}

void TestCleaningProfiles::expandPaths_nestedWildcards_walkAcrossSegments()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QDir().mkpath(tmp.path() + "/profile-A/Cache");
    QDir().mkpath(tmp.path() + "/profile-B/Cache");
    QDir().mkpath(tmp.path() + "/profile-C/NotCache");

    Profile p;
    p.id = "demo";
    p.paths = {tmp.path() + "/*/Cache"};

    Service svc;
    const QFileInfoList entries = svc.expandPaths(p);
    QCOMPARE(entries.size(), 2);
}

void TestCleaningProfiles::expandPaths_minAgeDays_filtersRecentFiles()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString recent = writeFile(tmp.path() + "/recent.log", "r");
    const QString old    = writeFile(tmp.path() + "/old.log", "o");

    // Move old.log's mtime 30 days back.
    QFile f(old);
    QVERIFY(f.open(QIODevice::ReadWrite));
    QVERIFY(f.setFileTime(QDateTime::currentDateTime().addDays(-30),
                          QFileDevice::FileModificationTime));
    f.close();

    Profile p;
    p.id = "demo";
    p.paths = {tmp.path() + "/*.log"};
    p.minAgeDays = 7;

    Service svc;
    const QFileInfoList entries = svc.expandPaths(p);
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().absoluteFilePath(), old);
}

void TestCleaningProfiles::expandPaths_unknownPath_returnsEmpty()
{
    Profile p;
    p.id = "demo";
    p.paths = {"/this/path/does/not/exist/anywhere/xyz"};

    Service svc;
    const QFileInfoList entries = svc.expandPaths(p);
    QVERIFY(entries.isEmpty());
}

// ─── scan (safety gating + exclusions + per-profile breakdown) ──────────────

void TestCleaningProfiles::scan_aggressiveProfileGatedOff_excluded()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    writeFile(tmp.path() + "/safe.cache", "S");
    writeFile(tmp.path() + "/agg.cache", "A");

    Profile safe;
    safe.id = "safe";
    safe.paths = {tmp.path() + "/safe.cache"};
    safe.safety = Service::Safety::Safe;

    Profile agg;
    agg.id = "agg";
    agg.paths = {tmp.path() + "/agg.cache"};
    agg.safety = Service::Safety::Aggressive;

    Service svc;
    const QFileInfoList all = svc.scan({safe, agg}, /*allowAggressive=*/false, {});
    QCOMPARE(all.size(), 1);
    QCOMPARE(all.first().fileName(), QString("safe.cache"));
}

void TestCleaningProfiles::scan_aggressiveProfileGatedOn_included()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    writeFile(tmp.path() + "/safe.cache", "S");
    writeFile(tmp.path() + "/agg.cache", "A");

    Profile safe;
    safe.id = "safe";
    safe.paths = {tmp.path() + "/safe.cache"};
    safe.safety = Service::Safety::Safe;

    Profile agg;
    agg.id = "agg";
    agg.paths = {tmp.path() + "/agg.cache"};
    agg.safety = Service::Safety::Aggressive;

    Service svc;
    const QFileInfoList all = svc.scan({safe, agg}, /*allowAggressive=*/true, {});
    QStringList names;
    for (const QFileInfo &fi : all) names << fi.fileName();
    names.sort();
    QCOMPARE(names, (QStringList() << "agg.cache" << "safe.cache"));
}

void TestCleaningProfiles::scan_excludedPathsHonored()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString keep = writeFile(tmp.path() + "/keep.cache", "K");
    const QString drop = writeFile(tmp.path() + "/drop.cache", "D");

    Profile p;
    p.id = "demo";
    p.paths = {tmp.path() + "/*.cache"};

    Service svc;
    auto excluded = [&](const QString &path) { return path == keep; };
    const QFileInfoList all = svc.scan({p}, false, excluded);

    QCOMPARE(all.size(), 1);
    QCOMPARE(all.first().absoluteFilePath(), drop);
}

void TestCleaningProfiles::scan_perProfileBreakdown_isPopulated()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    writeFile(tmp.path() + "/a.cache", "1");
    writeFile(tmp.path() + "/b.cache", "2");

    Profile p1;
    p1.id = "p1";
    p1.paths = {tmp.path() + "/a.cache"};

    Profile p2;
    p2.id = "p2";
    p2.paths = {tmp.path() + "/b.cache"};

    Service svc;
    QHash<QString, QFileInfoList> perProfile;
    svc.scan({p1, p2}, false, {}, &perProfile);

    QCOMPARE(perProfile.size(), 2);
    QCOMPARE(perProfile.value("p1").size(), 1);
    QCOMPARE(perProfile.value("p1").first().fileName(), QString("a.cache"));
    QCOMPARE(perProfile.value("p2").size(), 1);
    QCOMPARE(perProfile.value("p2").first().fileName(), QString("b.cache"));
}

QTEST_MAIN(TestCleaningProfiles)
#include "test_cleaning_profiles.moc"
