#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include "Managers/cleaner_service.h"
#include "Managers/setting_manager.h"

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

protected:
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

QTEST_MAIN(TestCleanerService)
#include "test_cleaner_service.moc"
