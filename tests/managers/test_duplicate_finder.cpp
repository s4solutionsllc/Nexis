// FW-08 / SSO-3736: DuplicateFinderService — duplicate grouping (content-
// verified, not size-only), top-N large-file ranking, empty-folder
// detection, exclusion engine compliance, and the never-delete-last-copy
// invariant. Uses the trash + exclusion seams (mirroring the WI-08
// TestableCleanerService pattern) so destructive paths are exercised
// without touching the user's real trash or settings.

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "Services/duplicate_finder_service.h"
#include "Managers/cleaner_service.h"

namespace {

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

QList<Entry> entries(std::initializer_list<std::pair<Entry::Type, QString>> items)
{
    QList<Entry> out;
    for (const auto &p : items) {
        Entry e;
        e.type = p.first;
        e.path = p.second;
        out.append(e);
    }
    return out;
}

// FW-08 test seam — subclass the production service so we can:
//   - inject an exclusion list without going through SettingManager, and
//   - capture moveToTrash() calls without touching the user's real trash.
// The constructor is protected on the base class for exactly this purpose.
class TestableDuplicateFinderService : public DuplicateFinderService
{
public:
    TestableDuplicateFinderService() : DuplicateFinderService(nullptr) {}

    QList<Entry> injectedExclusions;
    QStringList trashed;
    // Populated by tests that need to force moveToTrash() to fail for a
    // specific path (e.g. simulating a filesystem permission denial).
    QSet<QString> failingPaths;

protected:
    QList<Entry> loadExclusions() const override { return injectedExclusions; }

    // SSO-17858: run the pipeline synchronously so *Finished fires before
    // QSignalSpy::wait() is even called, removing this suite from the
    // cross-thread queued-connection timing this test doesn't need to
    // exercise (these tests assert pipeline correctness, not QtConcurrent
    // scheduling). See duplicate_finder_service.h for the rationale.
    bool runsAsynchronously() const override { return false; }

    bool moveToTrash(const QString &path) override
    {
        if (failingPaths.contains(path))
            return false;
        trashed.append(path);
        // The cleaner's real trash path moves the file off-disk; mimic
        // that so post-trash assertions about disk state match production.
        QFile::remove(path);
        return true;
    }
};

DuplicateGroup makeGroup(quint64 size, const QStringList &paths)
{
    DuplicateGroup g;
    g.fileSize = size;
    for (const QString &p : paths)
        g.files.append(QFileInfo(p));
    return g;
}

} // namespace

class TestDuplicateFinder : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // --- Duplicate grouping ---
    void duplicates_groupsKnownIdenticalFiles();
    void duplicates_sameSizeDifferentContent_notGrouped();
    void duplicates_singletonSize_dropped();

    // --- Large-file ranking ---
    void largest_topN_orderedDescending();
    void largest_topNZero_returnsAll();
    void largest_ranking_stableForEqualSizes();
    void largest_scan_emitsResults();

    // --- Empty folders ---
    void emptyFolders_detectsLeaves();
    void emptyFolders_ignoresNonEmpty();
    void emptyFolders_excludedFolder_notReported();

    // --- Exclusion engine ---
    void exclusions_excludedFile_notFlaggedAsDuplicate();
    void exclusions_excludedFolder_membersHidden();
    void exclusions_largestScan_respected();

    // --- Never-delete-last-copy ---
    void lastCopy_filterKeepsAtLeastOne();
    void lastCopy_filterIsNoopWhenSomeAreKept();
    void lastCopy_filterDeterministic();
    void lastCopy_trashFiles_dropsLastCopyEntry();
    void lastCopy_trashFiles_dropsExcludedEntry();
    void lastCopy_wouldRemoveLastCopy_detectsViolation();
};

void TestDuplicateFinder::initTestCase()
{
    // Match the cleaner test conventions: route QStandardPaths to a
    // sandbox in case any production code path is reached unexpectedly.
    QStandardPaths::setTestModeEnabled(true);
}

// ─── Duplicate grouping ───────────────────────────────────────────────────────

void TestDuplicateFinder::duplicates_groupsKnownIdenticalFiles()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Two pairs of duplicates + one unique file. Group A: same 16-byte
    // payload; Group B: a longer payload shared by two different files.
    const QByteArray payloadA = QByteArray(16, 'A');
    const QByteArray payloadB = QByteArray(256, 'B');
    const QByteArray unique   = QByteArray(16, 'Z');

    writeFile(tmp.path() + "/a1.bin", payloadA);
    writeFile(tmp.path() + "/sub/a2.bin", payloadA);
    writeFile(tmp.path() + "/b1.bin", payloadB);
    writeFile(tmp.path() + "/sub/b2.bin", payloadB);
    writeFile(tmp.path() + "/unique.bin", unique);

    TestableDuplicateFinderService svc;
    QSignalSpy spy(&svc, &DuplicateFinderService::scanFinished);
    svc.scan({tmp.path()}, /*minSize=*/0);

    QVERIFY(spy.wait(30000));
    QCOMPARE(spy.count(), 1);

    const auto results = spy.takeFirst().at(0).value<QList<DuplicateGroup>>();
    QCOMPARE(results.size(), 2);

    // The 256-byte group has more wasted space and should sort first.
    QCOMPARE(results.at(0).fileSize, static_cast<quint64>(256));
    QCOMPARE(results.at(0).files.size(), 2);
    QCOMPARE(results.at(1).fileSize, static_cast<quint64>(16));
    QCOMPARE(results.at(1).files.size(), 2);
}

void TestDuplicateFinder::duplicates_sameSizeDifferentContent_notGrouped()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Two files at the same size but with different content must not
    // make it past the full-hash stage. Use a payload long enough to
    // exceed the partial-hash window so we know the content-verification
    // step is what filtered them, not the size pre-filter alone.
    const int len = 8192;
    QByteArray contentX(len, 'X');
    QByteArray contentY(len, 'X');
    contentY[len - 1] = static_cast<char>('Y');  // differ in the tail

    writeFile(tmp.path() + "/x.bin", contentX);
    writeFile(tmp.path() + "/y.bin", contentY);

    TestableDuplicateFinderService svc;
    QSignalSpy spy(&svc, &DuplicateFinderService::scanFinished);
    svc.scan({tmp.path()}, /*minSize=*/0);

    QVERIFY(spy.wait(30000));
    const auto results = spy.takeFirst().at(0).value<QList<DuplicateGroup>>();
    QVERIFY2(results.isEmpty(),
             "same-size, different-content files must not be grouped");
}

void TestDuplicateFinder::duplicates_singletonSize_dropped()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    writeFile(tmp.path() + "/only.bin", QByteArray("alone"));

    TestableDuplicateFinderService svc;
    QSignalSpy spy(&svc, &DuplicateFinderService::scanFinished);
    svc.scan({tmp.path()}, /*minSize=*/0);

    QVERIFY(spy.wait(30000));
    const auto results = spy.takeFirst().at(0).value<QList<DuplicateGroup>>();
    QVERIFY(results.isEmpty());
}

// ─── Large-file ranking ──────────────────────────────────────────────────────

void TestDuplicateFinder::largest_topN_orderedDescending()
{
    QList<LargeFileEntry> in;
    auto mk = [](const QString &path, quint64 sz) {
        LargeFileEntry e; e.info = QFileInfo(path); e.size = sz; return e;
    };
    in << mk("/a", 100) << mk("/b", 500) << mk("/c", 250) << mk("/d", 800);

    const auto ranked = DuplicateFinderService::rankLargest(in, 2);
    QCOMPARE(ranked.size(), 2);
    QCOMPARE(ranked.at(0).size, quint64(800));
    QCOMPARE(ranked.at(1).size, quint64(500));
}

void TestDuplicateFinder::largest_topNZero_returnsAll()
{
    QList<LargeFileEntry> in;
    auto mk = [](const QString &path, quint64 sz) {
        LargeFileEntry e; e.info = QFileInfo(path); e.size = sz; return e;
    };
    in << mk("/a", 10) << mk("/b", 20) << mk("/c", 30);

    const auto ranked = DuplicateFinderService::rankLargest(in, 0);
    QCOMPARE(ranked.size(), 3);
    QCOMPARE(ranked.at(0).size, quint64(30));
    QCOMPARE(ranked.at(2).size, quint64(10));
}

void TestDuplicateFinder::largest_ranking_stableForEqualSizes()
{
    // Two files share size 100. Tie-break by path so the ordering is
    // reproducible regardless of QDirIterator's traversal order.
    QList<LargeFileEntry> in;
    auto mk = [](const QString &path, quint64 sz) {
        LargeFileEntry e; e.info = QFileInfo(path); e.size = sz; return e;
    };
    in << mk("/zz", 100) << mk("/aa", 100) << mk("/mm", 200);

    const auto ranked = DuplicateFinderService::rankLargest(in, 0);
    QCOMPARE(ranked.size(), 3);
    QCOMPARE(ranked.at(0).info.absoluteFilePath(), QStringLiteral("/mm"));
    QCOMPARE(ranked.at(1).info.absoluteFilePath(), QStringLiteral("/aa"));
    QCOMPARE(ranked.at(2).info.absoluteFilePath(), QStringLiteral("/zz"));
}

void TestDuplicateFinder::largest_scan_emitsResults()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    writeFile(tmp.path() + "/small.bin", QByteArray(8, 's'));
    writeFile(tmp.path() + "/medium.bin", QByteArray(64, 'm'));
    writeFile(tmp.path() + "/huge.bin", QByteArray(512, 'H'));

    TestableDuplicateFinderService svc;
    QSignalSpy spy(&svc, &DuplicateFinderService::largestScanFinished);
    svc.scanLargest({tmp.path()}, /*topN=*/2);

    QVERIFY(spy.wait(30000));
    const auto results = spy.takeFirst().at(0).value<QList<LargeFileEntry>>();
    QCOMPARE(results.size(), 2);
    QCOMPARE(results.at(0).size, quint64(512));
    QCOMPARE(results.at(1).size, quint64(64));
}

// ─── Empty folders ───────────────────────────────────────────────────────────

void TestDuplicateFinder::emptyFolders_detectsLeaves()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    QDir().mkpath(tmp.path() + "/empty_one");
    QDir().mkpath(tmp.path() + "/empty_two");
    writeFile(tmp.path() + "/non_empty/file.txt", "hi");

    TestableDuplicateFinderService svc;
    QSignalSpy spy(&svc, &DuplicateFinderService::emptyFoldersScanFinished);
    svc.scanEmptyFolders({tmp.path()});

    QVERIFY(spy.wait(30000));
    const auto results = spy.takeFirst().at(0).toStringList();
    QCOMPARE(results.size(), 2);
    QVERIFY(results.contains(tmp.path() + "/empty_one"));
    QVERIFY(results.contains(tmp.path() + "/empty_two"));
}

void TestDuplicateFinder::emptyFolders_ignoresNonEmpty()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // A directory containing a hidden file is NOT empty.
    QDir().mkpath(tmp.path() + "/has_hidden");
    writeFile(tmp.path() + "/has_hidden/.dotfile", "h");

    TestableDuplicateFinderService svc;
    QSignalSpy spy(&svc, &DuplicateFinderService::emptyFoldersScanFinished);
    svc.scanEmptyFolders({tmp.path()});

    QVERIFY(spy.wait(30000));
    const auto results = spy.takeFirst().at(0).toStringList();
    QVERIFY2(!results.contains(tmp.path() + "/has_hidden"),
             "folder with a hidden file must not be reported as empty");
}

void TestDuplicateFinder::emptyFolders_excludedFolder_notReported()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    QDir().mkpath(tmp.path() + "/empty_visible");
    QDir().mkpath(tmp.path() + "/empty_protected");

    TestableDuplicateFinderService svc;
    svc.injectedExclusions = entries({
        {Entry::Folder, tmp.path() + "/empty_protected"}
    });

    QSignalSpy spy(&svc, &DuplicateFinderService::emptyFoldersScanFinished);
    svc.scanEmptyFolders({tmp.path()});
    QVERIFY(spy.wait(30000));
    const auto results = spy.takeFirst().at(0).toStringList();
    QVERIFY(results.contains(tmp.path() + "/empty_visible"));
    QVERIFY(!results.contains(tmp.path() + "/empty_protected"));
}

// ─── Exclusion engine ────────────────────────────────────────────────────────

void TestDuplicateFinder::exclusions_excludedFile_notFlaggedAsDuplicate()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Three identical files. Excluding the middle one should leave the
    // remaining pair grouped (still a duplicate), but the excluded file
    // must not appear in any group.
    const QByteArray payload(64, 'D');
    const QString a = writeFile(tmp.path() + "/a.bin", payload);
    const QString b = writeFile(tmp.path() + "/b.bin", payload);
    const QString c = writeFile(tmp.path() + "/c.bin", payload);

    TestableDuplicateFinderService svc;
    svc.injectedExclusions = entries({{Entry::File, b}});

    QSignalSpy spy(&svc, &DuplicateFinderService::scanFinished);
    svc.scan({tmp.path()}, /*minSize=*/0);
    QVERIFY(spy.wait(30000));
    const auto results = spy.takeFirst().at(0).value<QList<DuplicateGroup>>();
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.at(0).files.size(), 2);

    QStringList paths;
    for (const QFileInfo &fi : results.at(0).files)
        paths << fi.absoluteFilePath();
    QVERIFY(paths.contains(a));
    QVERIFY(paths.contains(c));
    QVERIFY2(!paths.contains(b),
             "excluded file must not appear in any duplicate group");
}

void TestDuplicateFinder::exclusions_excludedFolder_membersHidden()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QByteArray payload(64, 'F');
    writeFile(tmp.path() + "/keep/a.bin", payload);
    writeFile(tmp.path() + "/keep/b.bin", payload);
    writeFile(tmp.path() + "/protected/c.bin", payload);

    TestableDuplicateFinderService svc;
    svc.injectedExclusions = entries({
        {Entry::Folder, tmp.path() + "/protected"}
    });

    QSignalSpy spy(&svc, &DuplicateFinderService::scanFinished);
    svc.scan({tmp.path()}, /*minSize=*/0);
    QVERIFY(spy.wait(30000));
    const auto results = spy.takeFirst().at(0).value<QList<DuplicateGroup>>();
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.at(0).files.size(), 2);
    for (const QFileInfo &fi : results.at(0).files) {
        QVERIFY2(!fi.absoluteFilePath().startsWith(tmp.path() + "/protected"),
                 "no member of an excluded folder may appear in results");
    }
}

void TestDuplicateFinder::exclusions_largestScan_respected()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    writeFile(tmp.path() + "/keep/big.bin",      QByteArray(1024, 'K'));
    writeFile(tmp.path() + "/protected/huge.bin", QByteArray(8192, 'P'));

    TestableDuplicateFinderService svc;
    svc.injectedExclusions = entries({
        {Entry::Folder, tmp.path() + "/protected"}
    });

    QSignalSpy spy(&svc, &DuplicateFinderService::largestScanFinished);
    svc.scanLargest({tmp.path()}, /*topN=*/10);
    QVERIFY(spy.wait(30000));
    const auto results = spy.takeFirst().at(0).value<QList<LargeFileEntry>>();
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.at(0).info.absoluteFilePath(), tmp.path() + "/keep/big.bin");
}

// ─── Never-delete-last-copy ──────────────────────────────────────────────────

void TestDuplicateFinder::lastCopy_filterKeepsAtLeastOne()
{
    auto group = makeGroup(100, {"/dup/a", "/dup/b", "/dup/c"});

    // Asking to remove all three must keep one alive — deterministically
    // the smallest path, "/dup/a", so the user-visible Trash action stays
    // reproducible across runs.
    const QStringList safe = DuplicateFinderService::filterSafeTrashCandidates(
        {"/dup/a", "/dup/b", "/dup/c"}, {group});

    QCOMPARE(safe.size(), 2);
    QVERIFY2(!safe.contains(QStringLiteral("/dup/a")),
             "smallest path is retained when the whole group is requested");
    QVERIFY(safe.contains(QStringLiteral("/dup/b")));
    QVERIFY(safe.contains(QStringLiteral("/dup/c")));
}

void TestDuplicateFinder::lastCopy_filterIsNoopWhenSomeAreKept()
{
    auto group = makeGroup(100, {"/dup/a", "/dup/b", "/dup/c"});

    // The user wants to remove only b and c → a survives → no filtering.
    const QStringList in  = {"/dup/b", "/dup/c"};
    const QStringList safe = DuplicateFinderService::filterSafeTrashCandidates(
        in, {group});
    QCOMPARE(safe, in);
}

void TestDuplicateFinder::lastCopy_filterDeterministic()
{
    auto group = makeGroup(100, {"/dup/c", "/dup/b", "/dup/a"});

    // Even though group.files is stored c, b, a, the retained file is
    // the lexicographically smallest of the requested set so the
    // protection rule does not depend on insertion order.
    const QStringList safe1 = DuplicateFinderService::filterSafeTrashCandidates(
        {"/dup/a", "/dup/b", "/dup/c"}, {group});
    const QStringList safe2 = DuplicateFinderService::filterSafeTrashCandidates(
        {"/dup/c", "/dup/b", "/dup/a"}, {group});

    QStringList sorted1 = safe1; std::sort(sorted1.begin(), sorted1.end());
    QStringList sorted2 = safe2; std::sort(sorted2.begin(), sorted2.end());
    QCOMPARE(sorted1, sorted2);
    QCOMPARE(sorted1, QStringList({"/dup/b", "/dup/c"}));
}

void TestDuplicateFinder::lastCopy_trashFiles_dropsLastCopyEntry()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString a = writeFile(tmp.path() + "/a.bin", QByteArray(64, 'D'));
    const QString b = writeFile(tmp.path() + "/b.bin", QByteArray(64, 'D'));
    auto group = makeGroup(64, {a, b});

    TestableDuplicateFinderService svc;
    // No exclusions — we want to prove the last-copy guard, not the
    // exclusion guard.
    svc.injectedExclusions = {};

    const QStringList trashed = svc.trashFiles({a, b}, {group});
    QCOMPARE(trashed.size(), 1);
    // The deterministic last-copy guard retains lexicographically
    // smallest path → `a` survives.
    QCOMPARE(trashed.first(), b);
    QVERIFY2(QFile::exists(a),
             "the last copy of a duplicate group must survive trashFiles()");
    QVERIFY(!QFile::exists(b));
}

void TestDuplicateFinder::lastCopy_trashFiles_dropsExcludedEntry()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString a = writeFile(tmp.path() + "/a.bin", QByteArray(64, 'D'));
    const QString b = writeFile(tmp.path() + "/b.bin", QByteArray(64, 'D'));
    const QString c = writeFile(tmp.path() + "/c.bin", QByteArray(64, 'D'));
    auto group = makeGroup(64, {a, b, c});

    TestableDuplicateFinderService svc;
    svc.injectedExclusions = entries({{Entry::File, b}});

    const QStringList trashed = svc.trashFiles({a, b, c}, {group});

    QVERIFY2(!trashed.contains(b),
             "excluded path must never be trashed even when present in input");
    QVERIFY2(QFile::exists(b),
             "excluded path must remain on disk and serves as the surviving "
             "duplicate-group member");
    // a and c were both safe to remove because b (the excluded member) is
    // still on disk to keep the group non-empty.
    QVERIFY(!QFile::exists(a));
    QVERIFY(!QFile::exists(c));
}

void TestDuplicateFinder::lastCopy_wouldRemoveLastCopy_detectsViolation()
{
    auto g = makeGroup(100, {"/g/a", "/g/b"});
    QVERIFY(DuplicateFinderService::wouldRemoveLastCopy({"/g/a", "/g/b"}, {g}));
    QVERIFY(!DuplicateFinderService::wouldRemoveLastCopy({"/g/a"}, {g}));
    QVERIFY(!DuplicateFinderService::wouldRemoveLastCopy({}, {g}));
}

QTEST_MAIN(TestDuplicateFinder)
#include "test_duplicate_finder.moc"
