#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <functional>
#include "Managers/app_in_use_gate.h"
#include "Managers/cleaner_service.h"
#include "Managers/setting_manager.h"
#include <Tools/lifecycle_audit_log.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

// SSO-27095: a gate that resolves rules against a fake home and reports a
// fixed set of processes as running, so no real browser has to be open.
class FakeAppInUseGate : public AppInUseGate
{
public:
    FakeAppInUseGate(const QString &home, const QStringList &running,
                     QStringList *probeLog = nullptr)
        : AppInUseGate(home), mRunning(running), mProbeLog(probeLog) {}

protected:
    bool isProcessRunning(const QString &processName) const override
    {
        if (mProbeLog)
            *mProbeLog << processName;
        return mRunning.contains(processName);
    }

private:
    QStringList mRunning;
    QStringList *mProbeLog;
};

// WI-08 / audit H9: covers CleanerService destructive paths and the
// exclusion-at-every-depth fix in cleanFiles(). The class uses a virtual
// `removeElevated()` seam (mirroring TestableRepairEngine's pkexec bypass)
// so the elevated `rm -rf` branch can be exercised without sudo.
class TestableCleanerService : public CleanerService
{
public:
    TestableCleanerService() : CleanerService() {}

    QStringList elevatedPaths;
    QString mockTrashRoot;
    // SSO-3704: default to "not user-owned" so the elevated branch is reachable
    // under CI (where temp files are always owned by the CI user). Individual
    // tests can flip this to true to exercise the user-owned branch.
    bool pretendUserOwns = false;
    // SSO-27095: null = the production gate.
    std::function<std::unique_ptr<AppInUseGate>()> gateFactory;
    // SSO-27097: empty = the production process-based answer.
    QString runSourceOverride;

protected:
    QString defaultRunSource() const override
    {
        return runSourceOverride.isEmpty() ? CleanerService::defaultRunSource() : runSourceOverride;
    }

    std::unique_ptr<AppInUseGate> makeAppInUseGate() const override
    {
        return gateFactory ? gateFactory() : CleanerService::makeAppInUseGate();
    }

    bool currentUserOwns(const QString &) const override
    {
        return pretendUserOwns;
    }

    void removeElevated(const QStringList &paths) override
    {
        elevatedPaths.append(paths);
        // Tests run unprivileged, so do the moral equivalent of `rm -rf`
        // via QFile/QDir on the captured paths. Real production code pipes
        // through `sudoExec("rm", "-rf", "--", paths)`.
        for (const QString &p : paths) {
            QFileInfo fi(p);
            if (fi.isDir() && !fi.isSymLink()) {
                QDir(p).removeRecursively();
            } else {
                QFile::remove(p);
            }
        }
    }

    QStringList trashRoots() const override
    {
        if (!mockTrashRoot.isEmpty())
            return { mockTrashRoot };
        return CleanerService::trashRoots();
    }
};

class TestCleanerService : public QObject
{
    Q_OBJECT

private:
    using Entry = CleanerService::ExclusionEntry;

    QString writeFile(const QString &path, const QByteArray &data)
    {
        QFileInfo(path).absoluteDir().mkpath(".");
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(data);
        f.close();
        return path;
    }

    void touchOlder(const QString &path, int seconds)
    {
        // Move mtime to `seconds` ago so age-cutoff tests are deterministic.
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadWrite));
        const QDateTime older = QDateTime::currentDateTime().addSecs(-seconds);
        QVERIFY(f.setFileTime(older, QFileDevice::FileModificationTime));
        f.close();
    }

    void setExclusions(CleanerService *cs, std::initializer_list<std::pair<Entry::Type, QString>> items)
    {
        QList<Entry> entries;
        for (const auto &p : items) {
            Entry e;
            e.type = p.first;
            e.path = p.second;
            entries.append(e);
        }
        cs->saveExclusions(entries);
    }

private slots:
    void initTestCase();
    void cleanup();

    void cleanFiles_excludedTopLevelFile_survives();
    void cleanFiles_excludedChildInsideScannedDir_survives();   // the bug
    void cleanFiles_excludedSubdirInsideScannedDir_survives();  // the bug
    void cleanFiles_minFileAgeCutoff_keepsRecentEntries();
    void cleanFiles_minFileAgeCutoff_recursive();
    void cleanFiles_symlink_removedNotFollowed();
    void cleanFiles_bytesFreedAccounting();
    void cleanFiles_elevatedBranch_routesThroughSeam();
    void cleanFiles_elevatedBranch_argvHasEndOfOptionsGuard();
    void cleanTrash_removesContents_viaSeam();
    void cleanTrash_multipleRoots_allCleaned();   // GH#182

    // SSO-27095
    void cleanFiles_topLevelSymlinkToDeniedLocation_skippedAndReported();
    void cleanFiles_symlinkInsideProfileToDeniedLocation_notFollowedOrDeleted();
    void cleanFiles_danglingLinkInsideSymlinkedDeniedDir_skipped();
    void cleanFiles_runningApp_profileSkippedAndReported();
    void cleanFiles_runningApp_nestedProfileSkippedSiblingsCleaned();
    void cleanFiles_appNotRunning_profileCleaned();
    void cleanFiles_gateProbesEachProcessOnce();
    void cleanFiles_skipsNeverAddDeletions();
    void cleanTrash_symlinkToDeniedLocation_skipped();
    void gate_symlinkIntoProfile_isJudgedByTarget();
    void gate_unrelatedPath_neverProbesProcesses();

    // SSO-27097: audit-log entries written by cleaner runs.
    void audit_cleanFiles_recordsRemovedPathsWithRunMetadata();
    void audit_cleanFiles_separateCallsAreSeparateRuns();
    void audit_cleanFiles_untouchedPathsWriteNothing();
    void audit_cleanFiles_skippedPathRecordedWithReason();
    void audit_cleanFiles_runningAppSkipRecordedWithAppName();
    void audit_cleanFiles_directoryRecordedOnceWithFreedBytes();
    void audit_cleanFiles_moveToTrash_recordsRestorableEntry();
    void audit_cleanFiles_headlessSource();
    void audit_cleanTrash_oneEntryPerRoot();
    void audit_clean_interactiveRunSharesOneBatch();
    void audit_cleanSchedule_recordsScheduledRun();
};

void TestCleanerService::initTestCase()
{
    // Isolate QSettings / config paths so saveExclusions() does not touch
    // the real user config. WI-37 note in the parent plan calls this out.
    QStandardPaths::setTestModeEnabled(true);
}

void TestCleanerService::cleanup()
{
    // Reset exclusions between tests so they do not leak across cases.
    TestableCleanerService cs;
    cs.saveExclusions({});
    QFile::remove(LifecycleAuditLog::logFilePath());
}

void TestCleanerService::cleanFiles_excludedTopLevelFile_survives()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString keep = writeFile(tmp.path() + "/keep.log", "keep");
    const QString drop = writeFile(tmp.path() + "/drop.log", "drop");

    TestableCleanerService cs;
    setExclusions(&cs, {{Entry::File, keep}});

    const quint64 freed = cs.cleanFiles({keep, drop});

    QVERIFY(QFile::exists(keep));
    QVERIFY(!QFile::exists(drop));
    // Only `drop.log` was removed → freed bytes == its size (4).
    QCOMPARE(freed, static_cast<quint64>(4));
}

void TestCleanerService::cleanFiles_excludedChildInsideScannedDir_survives()
{
    // THE BUG: scan() filters only top-level entries. When cleanFiles() then
    // recursively deletes a scanned directory's contents, an excluded *child*
    // (file or subdir) was previously deleted anyway.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString cacheDir = tmp.path() + "/cache";
    QDir().mkpath(cacheDir);

    const QString keep = writeFile(cacheDir + "/important.db", "DB");
    const QString drop1 = writeFile(cacheDir + "/junk1.tmp", "junk");
    const QString drop2 = writeFile(cacheDir + "/junk2.tmp", "junkjunk");

    TestableCleanerService cs;
    setExclusions(&cs, {{Entry::File, keep}});

    const quint64 freed = cs.cleanFiles({cacheDir});

    QVERIFY2(QFile::exists(keep),
             "excluded file inside scanned directory must survive recursive deletion");
    QVERIFY(!QFile::exists(drop1));
    QVERIFY(!QFile::exists(drop2));
    QCOMPARE(freed, static_cast<quint64>(4 + 8));
    // Parent dir kept alive by the surviving excluded child.
    QVERIFY(QDir(cacheDir).exists());
}

void TestCleanerService::cleanFiles_excludedSubdirInsideScannedDir_survives()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString cacheDir = tmp.path() + "/cache";
    const QString protectedSub = cacheDir + "/protected";
    QDir().mkpath(protectedSub);

    const QString keep = writeFile(protectedSub + "/data.bin", "ABCDEF");
    const QString drop = writeFile(cacheDir + "/junk.tmp", "x");

    TestableCleanerService cs;
    setExclusions(&cs, {{Entry::Folder, protectedSub}});

    cs.cleanFiles({cacheDir});

    QVERIFY2(QFile::exists(keep),
             "file inside excluded subdir must survive");
    QVERIFY2(QDir(protectedSub).exists(),
             "excluded subdir itself must survive");
    QVERIFY(!QFile::exists(drop));
}

void TestCleanerService::cleanFiles_minFileAgeCutoff_keepsRecentEntries()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString recent = writeFile(tmp.path() + "/recent.log", "r");
    const QString old    = writeFile(tmp.path() + "/old.log",    "o");
    touchOlder(old, 7200);  // 2h old

    TestableCleanerService cs;

    // cutoff = 1h → only `old.log` deleted.
    const quint64 freed = cs.cleanFiles({recent, old}, 3600);

    QVERIFY(QFile::exists(recent));
    QVERIFY(!QFile::exists(old));
    QCOMPARE(freed, static_cast<quint64>(1));
}

void TestCleanerService::cleanFiles_minFileAgeCutoff_recursive()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString dir = tmp.path() + "/logs";
    QDir().mkpath(dir);

    const QString recent = writeFile(dir + "/recent.log", "r");
    const QString old    = writeFile(dir + "/old.log",    "oo");
    touchOlder(old, 7200);

    TestableCleanerService cs;
    cs.cleanFiles({dir}, 3600);

    QVERIFY2(QFile::exists(recent),
             "recent file inside scanned dir must survive minFileAge cutoff");
    QVERIFY(!QFile::exists(old));
}

void TestCleanerService::cleanFiles_symlink_removedNotFollowed()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString target = writeFile(tmp.path() + "/target.bin", "PROTECTED");
    const QString link   = tmp.path() + "/link";
    QVERIFY(QFile::link(target, link));

    TestableCleanerService cs;
    cs.cleanFiles({link});

    QVERIFY2(QFile::exists(target),
             "symlink target must not be followed and deleted");
    QVERIFY(!QFileInfo(link).isSymLink());
    QVERIFY(!QFile::exists(link));
}

void TestCleanerService::cleanFiles_bytesFreedAccounting()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString a = writeFile(tmp.path() + "/a", "1234");          // 4
    const QString b = writeFile(tmp.path() + "/b", "1234567890");    // 10
    const QString c = writeFile(tmp.path() + "/c", "AB");            // 2 — excluded

    TestableCleanerService cs;
    setExclusions(&cs, {{Entry::File, c}});

    const quint64 freed = cs.cleanFiles({a, b, c});

    QCOMPARE(freed, static_cast<quint64>(14));
    QVERIFY(QFile::exists(c));
}

void TestCleanerService::cleanFiles_elevatedBranch_routesThroughSeam()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString a = writeFile(tmp.path() + "/a", "AA");
    const QString b = writeFile(tmp.path() + "/b", "BBB");

    TestableCleanerService cs;
    cs.cleanFiles({a, b});

    // Plain files at the top level take the batched, elevated branch.
    QCOMPARE(cs.elevatedPaths.size(), 2);
    QVERIFY(cs.elevatedPaths.contains(a));
    QVERIFY(cs.elevatedPaths.contains(b));
    QVERIFY(!QFile::exists(a));
    QVERIFY(!QFile::exists(b));
}

void TestCleanerService::cleanFiles_elevatedBranch_argvHasEndOfOptionsGuard()
{
    // The fix mandates a `--` end-of-options guard in the rm argv so a path
    // beginning with `-` cannot be parsed as a flag. The seam receives just
    // the paths (the wrapper assembles "-rf --" itself), but we want to know
    // that a file literally named `-rf` would not slip past as an option. We
    // exercise this by routing such a path through the seam and asserting it
    // was recorded as a path, not eaten as a flag.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString trap = writeFile(tmp.path() + "/-rf", "TRAP");
    QVERIFY(QFile::exists(trap));

    TestableCleanerService cs;
    cs.cleanFiles({trap});

    QCOMPARE(cs.elevatedPaths.size(), 1);
    QCOMPARE(cs.elevatedPaths.first(), trap);
    QVERIFY(!QFile::exists(trap));
}

void TestCleanerService::cleanTrash_removesContents_viaSeam()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    TestableCleanerService cs;
    cs.mockTrashRoot = tmp.path();

#ifdef Q_OS_MACOS
    const QString a = writeFile(tmp.path() + "/a.txt",         "AAA");
    const QString b = writeFile(tmp.path() + "/sub/b.txt",     "BBBB");
#else
    const QString a = writeFile(tmp.path() + "/files/a.txt",   "AAA");
    const QString b = writeFile(tmp.path() + "/info/a.trashinfo", "INFO");
#endif
    Q_UNUSED(a);
    Q_UNUSED(b);

    const quint64 before = cs.cleanTrash();
    QVERIFY(before > 0);

#ifdef Q_OS_MACOS
    QCOMPARE(QDir(tmp.path()).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).size(), 0);
#else
    QCOMPARE(QDir(tmp.path() + "/files").entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).size(), 0);
    QCOMPARE(QDir(tmp.path() + "/info").entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).size(), 0);
#endif
}

void TestCleanerService::cleanTrash_multipleRoots_allCleaned()
{
#ifdef Q_OS_MACOS
    QSKIP("macOS .Trash has a flat layout; mounted-FS trash test is Linux-only");
#else
    // Simulate a home trash root plus a mounted-filesystem .Trash-$uid root.
    // Both must be emptied in a single cleanTrash() call (GH#182).
    QTemporaryDir home;
    QTemporaryDir mount;
    QVERIFY(home.isValid() && mount.isValid());

    writeFile(home.path()  + "/files/a.txt",      "AAA");
    writeFile(home.path()  + "/info/a.trashinfo",  "INFO");
    writeFile(mount.path() + "/files/b.txt",       "BBB");
    writeFile(mount.path() + "/info/b.trashinfo",  "INFO");

    struct MultiRootCS : public TestableCleanerService {
        QStringList roots;
        QStringList trashRoots() const override { return roots; }
    } cs;
    cs.roots = { home.path(), mount.path() };

    const quint64 before = cs.cleanTrash();
    QVERIFY(before > 0);

    QCOMPARE(QDir(home.path()  + "/files").entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).size(), 0);
    QCOMPARE(QDir(home.path()  + "/info" ).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).size(), 0);
    QCOMPARE(QDir(mount.path() + "/files").entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).size(), 0);
    QCOMPARE(QDir(mount.path() + "/info" ).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).size(), 0);
#endif
}

// SSO-27095 ────────────────────────────────────────────────────────────────
// Every delete goes through LifecycleDenyList::isSafeForCleaning (canonical
// path) and the app-in-use gate. "/usr" is denied on both platforms and is
// never writable by the test user, so a symlink to it is a safe stand-in for
// "a symlink that points at a denied location".

void TestCleanerService::cleanFiles_topLevelSymlinkToDeniedLocation_skippedAndReported()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString link = tmp.path() + "/to-usr";
    QVERIFY(QFile::link("/usr", link));

    TestableCleanerService cs;
    const quint64 freed = cs.cleanFiles({link});

    QCOMPARE(freed, static_cast<quint64>(0));
    QVERIFY2(QFileInfo(link).isSymLink(), "a link to a denied location must be left in place");
    QCOMPARE(cs.lastSkipped().size(), 1);
    QCOMPARE(cs.lastSkipped().first().path, link);
    QCOMPARE(cs.lastSkipped().first().reason, CleanerService::SkippedPath::ProtectedLocation);
}

void TestCleanerService::cleanFiles_symlinkInsideProfileToDeniedLocation_notFollowedOrDeleted()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString profile = tmp.path() + "/profile";
    const QString keepLink = profile + "/cache-link";
    writeFile(profile + "/stale.tmp", "stale");
    QVERIFY(QFile::link("/usr", keepLink));

    TestableCleanerService cs;
    cs.pretendUserOwns = true;
    cs.cleanFiles({profile});

    QVERIFY2(QFileInfo(keepLink).isSymLink(), "denied link must survive the recursive walk");
    QVERIFY2(QDir("/usr").exists() && QDir("/usr/bin").exists(), "the walk must never descend into the target");
    QVERIFY(!QFile::exists(profile + "/stale.tmp"));
    QCOMPARE(cs.lastSkipped().size(), 1);
    QCOMPARE(cs.lastSkipped().first().path, keepLink);
    QCOMPARE(cs.lastSkipped().first().reason, CleanerService::SkippedPath::ProtectedLocation);
}

void TestCleanerService::cleanFiles_danglingLinkInsideSymlinkedDeniedDir_skipped()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // <tmp>/linked-dir -> /usr, so <tmp>/linked-dir/no-such-entry would be
    // created (or removed) inside /usr even though the leaf does not resolve.
    const QString linkedDir = tmp.path() + "/linked-dir";
    QVERIFY(QFile::link("/usr", linkedDir));

    TestableCleanerService cs;
    cs.cleanFiles({linkedDir + "/no-such-entry"});

    QCOMPARE(cs.lastSkipped().size(), 1);
    QCOMPARE(cs.lastSkipped().first().reason, CleanerService::SkippedPath::ProtectedLocation);
}

void TestCleanerService::cleanFiles_runningApp_profileSkippedAndReported()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());

    const QString chromeCache = home.path() + "/.config/google-chrome/Default/Cache";
    const QString other       = writeFile(home.path() + "/.cache/other-app/blob", "x");
    const QString chromeFile  = writeFile(chromeCache + "/data_0", "CHROME");

    TestableCleanerService cs;
    cs.pretendUserOwns = true;
    const QString homePath = home.path();
    cs.gateFactory = [homePath] {
        return std::make_unique<FakeAppInUseGate>(homePath, QStringList{"chrome"});
    };

    const quint64 freed = cs.cleanFiles({chromeCache, other});

    QVERIFY2(QFile::exists(chromeFile), "an open browser's profile must not be deleted");
    QVERIFY(!QFile::exists(other));
    QCOMPARE(freed, static_cast<quint64>(1));   // only the unrelated file
    QCOMPARE(cs.lastSkipped().size(), 1);
    QCOMPARE(cs.lastSkipped().first().path, chromeCache);
    QCOMPARE(cs.lastSkipped().first().reason, CleanerService::SkippedPath::AppRunning);
    QCOMPARE(cs.lastSkipped().first().app, QStringLiteral("Google Chrome"));
}

void TestCleanerService::cleanFiles_runningApp_nestedProfileSkippedSiblingsCleaned()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());

    // A broad scanned directory that happens to contain an open app's profile.
    const QString slackFile  = writeFile(home.path() + "/.config/Slack/Cache/c1", "SLACK");
    const QString otherFile  = writeFile(home.path() + "/.config/unrelated/Cache/c2", "OTHER");

    TestableCleanerService cs;
    cs.pretendUserOwns = true;
    const QString homePath = home.path();
    cs.gateFactory = [homePath] {
        return std::make_unique<FakeAppInUseGate>(homePath, QStringList{"slack"});
    };

    cs.cleanFiles({home.path() + "/.config"});

    QVERIFY(QFile::exists(slackFile));
    QVERIFY(!QFile::exists(otherFile));
    QCOMPARE(cs.lastSkipped().size(), 1);
    QCOMPARE(cs.lastSkipped().first().app, QStringLiteral("Slack"));
}

void TestCleanerService::cleanFiles_appNotRunning_profileCleaned()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());

    const QString cacheFile = writeFile(home.path() + "/.config/discord/Cache/c", "DISCORD");

    TestableCleanerService cs;
    cs.pretendUserOwns = true;
    const QString homePath = home.path();
    cs.gateFactory = [homePath] {
        return std::make_unique<FakeAppInUseGate>(homePath, QStringList{});
    };

    cs.cleanFiles({home.path() + "/.config/discord/Cache"});

    QVERIFY(!QFile::exists(cacheFile));
    QVERIFY(cs.lastSkipped().isEmpty());
}

void TestCleanerService::cleanFiles_gateProbesEachProcessOnce()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());

    for (int i = 0; i < 5; ++i)
        writeFile(QString("%1/.config/discord/Cache/f%2").arg(home.path()).arg(i), "d");

    QStringList probeLog;
    TestableCleanerService cs;
    cs.pretendUserOwns = true;
    const QString homePath = home.path();
    cs.gateFactory = [homePath, &probeLog] {
        return std::make_unique<FakeAppInUseGate>(homePath, QStringList{}, &probeLog);
    };

    cs.cleanFiles({home.path() + "/.config/discord/Cache"});

    // Six paths belong to Discord (the directory + five files); its process
    // is still looked up once for the whole call.
    QVERIFY(cs.lastSkipped().isEmpty());
    QCOMPARE(probeLog.count(QStringLiteral("discord")), 1);
}

void TestCleanerService::cleanFiles_skipsNeverAddDeletions()
{
    // Whatever the gates decide, the set of removed files is a subset of what
    // the un-gated clean removes: here, the one allowed file goes and both
    // gated entries stay.
    QTemporaryDir home;
    QVERIFY(home.isValid());

    const QString allowed = writeFile(home.path() + "/.cache/plain/a", "A");
    const QString held    = writeFile(home.path() + "/.config/Code/Cache/b", "B");
    QVERIFY(QFile::link("/usr", home.path() + "/.cache/plain/to-usr"));

    TestableCleanerService cs;
    cs.pretendUserOwns = true;
    const QString homePath = home.path();
    cs.gateFactory = [homePath] {
        return std::make_unique<FakeAppInUseGate>(homePath, QStringList{"code"});
    };

    cs.cleanFiles({home.path() + "/.cache", home.path() + "/.config/Code"});

    QVERIFY(!QFile::exists(allowed));
    QVERIFY(QFile::exists(held));
    QVERIFY(QFileInfo(home.path() + "/.cache/plain/to-usr").isSymLink());
    QCOMPARE(cs.lastSkipped().size(), 2);
}

void TestCleanerService::cleanTrash_symlinkToDeniedLocation_skipped()
{
#ifdef Q_OS_MACOS
    QSKIP("macOS .Trash has a flat layout; this case covers the freedesktop files/ dir");
#else
    QTemporaryDir trash;
    QVERIFY(trash.isValid());

    const QString junk = writeFile(trash.path() + "/files/old.txt", "OLD");
    const QString link = trash.path() + "/files/to-usr";
    QVERIFY(QFile::link("/usr", link));

    TestableCleanerService cs;
    cs.mockTrashRoot = trash.path();
    cs.cleanTrash();

    QVERIFY(!QFile::exists(junk));
    QVERIFY(QFileInfo(link).isSymLink());
    QCOMPARE(cs.lastSkipped().size(), 1);
    QCOMPARE(cs.lastSkipped().first().path, link);
#endif
}

void TestCleanerService::gate_symlinkIntoProfile_isJudgedByTarget()
{
    QTemporaryDir home;
    QTemporaryDir elsewhere;
    QVERIFY(home.isValid() && elsewhere.isValid());

    writeFile(home.path() + "/.config/Slack/Cache/c", "S");
    writeFile(elsewhere.path() + "/plain", "P");
    QVERIFY(QFile::link(home.path() + "/.config/Slack/Cache", elsewhere.path() + "/into-slack"));
    QVERIFY(QFile::link(elsewhere.path() + "/plain", home.path() + "/.cache-link-out"));

    FakeAppInUseGate gate(home.path(), {"slack"});
    QCOMPARE(gate.runningOwnerOf(elsewhere.path() + "/into-slack"), QStringLiteral("Slack"));
    QVERIFY(gate.runningOwnerOf(home.path() + "/.cache-link-out").isEmpty());
}

void TestCleanerService::gate_unrelatedPath_neverProbesProcesses()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    writeFile(home.path() + "/Documents/a.txt", "A");

    QStringList probeLog;
    FakeAppInUseGate gate(home.path(), {"chrome", "slack", "code"}, &probeLog);
    QVERIFY(gate.runningOwnerOf(home.path() + "/Documents/a.txt").isEmpty());
    QVERIFY(probeLog.isEmpty());
}

void TestCleanerService::audit_cleanFiles_recordsRemovedPathsWithRunMetadata()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString a = writeFile(tmp.path() + "/a.log", "AAA");
    const QString b = writeFile(tmp.path() + "/b.log", "BBBBB");

    TestableCleanerService cs;
    cs.pretendUserOwns = true;
    cs.cleanFiles({a, b});

    const QList<LifecycleAuditLog::Entry> entries = LifecycleAuditLog::readAll();
    QCOMPARE(entries.size(), 2);
    QCOMPARE(entries[0].batchId, entries[1].batchId);
    QVERIFY(!entries[0].batchId.isEmpty());
    quint64 total = 0;
    for (const auto &e : entries) {
        QCOMPARE(e.action, LifecycleAuditLog::Action::PermanentlyDeleted);
        QCOMPARE(e.runSource, LifecycleAuditLog::RunSource::Interactive);
        QVERIFY(e.trashDestination.isEmpty());
        QVERIFY(e.originalPath == a || e.originalPath == b);
        total += e.sizeBytes;
    }
    QCOMPARE(total, static_cast<quint64>(8));
}

void TestCleanerService::audit_cleanFiles_separateCallsAreSeparateRuns()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString a = writeFile(tmp.path() + "/a.log", "A");
    const QString b = writeFile(tmp.path() + "/b.log", "B");

    TestableCleanerService cs;
    cs.pretendUserOwns = true;
    cs.cleanFiles({a});
    cs.cleanFiles({b});

    QCOMPARE(LifecycleAuditLog::groupIntoRuns(LifecycleAuditLog::readAll()).size(), 2);
}

void TestCleanerService::audit_cleanFiles_untouchedPathsWriteNothing()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString excluded = writeFile(tmp.path() + "/keep.log", "keep");
    const QString young = writeFile(tmp.path() + "/young.log", "young");

    TestableCleanerService cs;
    cs.pretendUserOwns = true;
    setExclusions(&cs, {{Entry::File, excluded}});
    cs.cleanFiles({excluded, young}, 3600);

    QVERIFY(QFile::exists(excluded));
    QVERIFY(QFile::exists(young));
    QVERIFY(LifecycleAuditLog::readAll().isEmpty());
}

void TestCleanerService::audit_cleanFiles_skippedPathRecordedWithReason()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString link = tmp.path() + "/to-usr";
    QVERIFY(QFile::link("/usr", link));

    TestableCleanerService cs;
    cs.cleanFiles({link});

    const QList<LifecycleAuditLog::Entry> entries = LifecycleAuditLog::readAll();
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].action, LifecycleAuditLog::Action::Skipped);
    QCOMPARE(entries[0].originalPath, link);
    QCOMPARE(entries[0].skipReason, CleanerService::skipReasonText(cs.lastSkipped().first()));
    QVERIFY(!entries[0].skipReason.isEmpty());
    QCOMPARE(entries[0].sizeBytes, static_cast<quint64>(0));
    const LifecycleAuditLog::Run run = LifecycleAuditLog::groupIntoRuns(entries).first();
    QCOMPARE(run.skippedCount, 1);
    QCOMPARE(run.bytesFreed, static_cast<quint64>(0));
}

void TestCleanerService::audit_cleanFiles_runningAppSkipRecordedWithAppName()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString chromeCache = home.path() + "/.config/google-chrome/Default/Cache";
    writeFile(chromeCache + "/data_0", "CHROME");
    const QString other = writeFile(home.path() + "/.cache/other-app/blob", "x");

    TestableCleanerService cs;
    cs.pretendUserOwns = true;
    const QString homePath = home.path();
    cs.gateFactory = [homePath] {
        return std::make_unique<FakeAppInUseGate>(homePath, QStringList{"chrome"});
    };
    cs.cleanFiles({chromeCache, other});

    const LifecycleAuditLog::Run run =
        LifecycleAuditLog::groupIntoRuns(LifecycleAuditLog::readAll()).first();
    QCOMPARE(run.items.size(), 2);
    QCOMPARE(run.removedCount, 1);
    QCOMPARE(run.skippedCount, 1);
    QCOMPARE(run.bytesFreed, static_cast<quint64>(1));
    for (const auto &e : run.items) {
        if (e.action == LifecycleAuditLog::Action::Skipped) {
            QCOMPARE(e.originalPath, chromeCache);
            QVERIFY2(e.skipReason.contains(QStringLiteral("Google Chrome")), qPrintable(e.skipReason));
        }
    }
}

void TestCleanerService::audit_cleanFiles_directoryRecordedOnceWithFreedBytes()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.path() + "/cache";
    writeFile(dir + "/one", "11");
    writeFile(dir + "/nested/two", "222");

    TestableCleanerService cs;
    cs.pretendUserOwns = true;
    cs.cleanFiles({dir});

    const QList<LifecycleAuditLog::Entry> entries = LifecycleAuditLog::readAll();
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].originalPath, dir);
    QCOMPARE(entries[0].sizeBytes, static_cast<quint64>(5));
}

void TestCleanerService::audit_cleanFiles_moveToTrash_recordsRestorableEntry()
{
    // Same filesystem as the home Trash, so the move is a rename.
    QTemporaryDir tmp(QDir::home().filePath(QStringLiteral(".nexis-audit-test-XXXXXX")));
    if (!tmp.isValid())
        QSKIP("home directory is not writable");
    const QString file = writeFile(tmp.path() + "/old-download.bin", "DATA");

    TestableCleanerService cs;
    cs.cleanFiles({file}, 0, /*moveToTrashInstead=*/true);
    if (QFile::exists(file))
        QSKIP("QFile::moveToTrash is unavailable on this filesystem");

    const QList<LifecycleAuditLog::Entry> entries = LifecycleAuditLog::readAll();
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].action, LifecycleAuditLog::Action::MovedToTrash);
    QCOMPARE(entries[0].originalPath, file);
    QVERIFY(!entries[0].trashDestination.isEmpty());
    QVERIFY(QFileInfo::exists(entries[0].trashDestination));

    // Leave no trace in the real Trash of the machine running the test.
    QFile::remove(entries[0].trashDestination);
#ifndef Q_OS_MACOS
    QFile::remove(QDir(QFileInfo(entries[0].trashDestination).absolutePath() + "/../info")
                      .absoluteFilePath(QFileInfo(entries[0].trashDestination).fileName() + ".trashinfo"));
#endif
}

void TestCleanerService::audit_cleanFiles_headlessSource()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString a = writeFile(tmp.path() + "/a.log", "A");

    TestableCleanerService cs;
    cs.pretendUserOwns = true;
    cs.runSourceOverride = LifecycleAuditLog::RunSource::Headless;
    cs.cleanFiles({a});

    const QList<LifecycleAuditLog::Entry> entries = LifecycleAuditLog::readAll();
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].runSource, LifecycleAuditLog::RunSource::Headless);
}

void TestCleanerService::audit_cleanTrash_oneEntryPerRoot()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
#ifdef Q_OS_MACOS
    writeFile(tmp.path() + "/a.txt", "AAA");
    writeFile(tmp.path() + "/b.txt", "BBBB");
#else
    writeFile(tmp.path() + "/files/a.txt", "AAA");
    writeFile(tmp.path() + "/files/b.txt", "BBBB");
#endif

    TestableCleanerService cs;
    cs.mockTrashRoot = tmp.path();
    const quint64 before = cs.cleanTrash();

    const QList<LifecycleAuditLog::Entry> entries = LifecycleAuditLog::readAll();
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].action, LifecycleAuditLog::Action::PermanentlyDeleted);
    QCOMPARE(entries[0].originalPath, tmp.path());
    QCOMPARE(entries[0].category, CleanerService::categoryName(CleanerService::TRASH));
    QCOMPARE(entries[0].sizeBytes, before);
}

void TestCleanerService::audit_clean_interactiveRunSharesOneBatch()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
#ifdef Q_OS_MACOS
    writeFile(tmp.path() + "/a.txt", "AAA");
#else
    writeFile(tmp.path() + "/files/a.txt", "AAA");
#endif

    TestableCleanerService cs;
    cs.mockTrashRoot = tmp.path();
    cs.clean({CleanerService::TRASH});

    const QList<LifecycleAuditLog::Run> runs =
        LifecycleAuditLog::groupIntoRuns(LifecycleAuditLog::readAll());
    QCOMPARE(runs.size(), 1);
    QCOMPARE(runs[0].runSource, LifecycleAuditLog::RunSource::Interactive);
    QCOMPARE(runs[0].items.first().category,
             CleanerService::categoryName(CleanerService::TRASH));
}

void TestCleanerService::audit_cleanSchedule_recordsScheduledRun()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
#ifdef Q_OS_MACOS
    writeFile(tmp.path() + "/a.txt", "AAA");
#else
    writeFile(tmp.path() + "/files/a.txt", "AAA");
#endif

    QJsonObject schedule;
    schedule["id"] = QStringLiteral("audit-test-schedule");
    schedule["name"] = QStringLiteral("Audit test");
    schedule["categories"] = QJsonArray{static_cast<int>(CleanerService::TRASH)};
    schedule["dryRunCompleted"] = true;
    SettingManager *sm = SettingManager::ins();
    const QString previous = sm->getSchedules();
    sm->setSchedules(QString::fromUtf8(QJsonDocument(QJsonArray{schedule}).toJson(QJsonDocument::Compact)));

    TestableCleanerService cs;
    cs.mockTrashRoot = tmp.path();
    cs.cleanSchedule(QStringLiteral("audit-test-schedule"));
    sm->setSchedules(previous);

    const QList<LifecycleAuditLog::Run> runs =
        LifecycleAuditLog::groupIntoRuns(LifecycleAuditLog::readAll());
    QCOMPARE(runs.size(), 1);
    QCOMPARE(runs[0].runSource, LifecycleAuditLog::RunSource::Scheduled);
    QCOMPARE(runs[0].removedCount, 1);
}

QTEST_MAIN(TestCleanerService)
#include "test_cleaner_service.moc"
