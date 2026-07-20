// SSO-15381: FileShredderService — overwrite-then-unlink correctness (proven
// at the filesystem level via a hard link, not just mocked), recursive
// folder shredding + empty-dir cleanup, symlink safety (unlinked, target
// never touched), per-item failure accounting, and the contained/duplicate
// path dedup used before both preview and shred.

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QtConcurrent>

#include <algorithm>
#include <unistd.h>

#include "Services/file_shredder_service.h"

namespace {

QString writeFile(const QString &path, const QByteArray &data)
{
    QFileInfo(path).absoluteDir().mkpath(".");
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(data);
    f.close();
    return path;
}

// Test seam: exposes the protected single-file/symlink/dir primitives, and
// lets a test force a specific path to fail without touching it — mirrors
// the DuplicateFinderService trash-seam pattern.
class TestableFileShredderService : public FileShredderService
{
public:
    TestableFileShredderService() : FileShredderService(nullptr) {}

    QSet<QString> failingPaths;

    bool callOverwriteAndUnlink(const QString &path, quint64 size)
    {
        return overwriteAndUnlinkFile(path, size);
    }

protected:
    bool overwriteAndUnlinkFile(const QString &path, quint64 fileSize) override
    {
        if (failingPaths.contains(path))
            return false;
        return FileShredderService::overwriteAndUnlinkFile(path, fileSize);
    }
};

} // namespace

class TestFileShredder : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void overwrite_zeroesDataBeforeUnlink();
    void overwrite_missingFile_returnsFalse();

    void shred_singleFile_removesIt();
    void shred_folder_recursiveAndRemovesEmptyDir();
    void shred_symlink_unlinksLinkNotTarget();
    void shred_reportsFailureCount_whenDeleteFails();

    void preview_countsFilesAndBytesRecursively();

    void dedupe_dropsNestedPath();
    void dedupe_dropsExactDuplicate();
};

void TestFileShredder::initTestCase()
{
    // SSO-14443: pre-warm the global QThreadPool so the first QtConcurrent::run()
    // call doesn't pay thread-creation latency inside a spy.wait() timeout.
    QtConcurrent::run([] {}).waitForFinished();
}

// ─── Overwrite primitive ──────────────────────────────────────────────────────

void TestFileShredder::overwrite_zeroesDataBeforeUnlink()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString original = tmp.path() + "/secret.txt";
    const QByteArray payload = QByteArray("S3CR3T-PAYLOAD-").repeated(64);
    writeFile(original, payload);

    // A hard link to the same inode lets us read back the on-disk bytes
    // *after* shredding the original name — proving the overwrite pass
    // touched the actual data, not just the directory entry.
    const QString hardLink = tmp.path() + "/secret_hardlink.txt";
    QVERIFY(::link(original.toLocal8Bit().constData(), hardLink.toLocal8Bit().constData()) == 0);

    TestableFileShredderService svc;
    QVERIFY(svc.callOverwriteAndUnlink(original, static_cast<quint64>(payload.size())));

    QVERIFY(!QFile::exists(original));
    QVERIFY(QFile::exists(hardLink));

    QFile check(hardLink);
    QVERIFY(check.open(QIODevice::ReadOnly));
    const QByteArray remaining = check.readAll();
    check.close();

    QCOMPARE(remaining.size(), payload.size());
    QVERIFY(remaining != payload);
    QVERIFY(std::all_of(remaining.begin(), remaining.end(), [](char c) { return c == '\0'; }));
}

void TestFileShredder::overwrite_missingFile_returnsFalse()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    TestableFileShredderService svc;
    QVERIFY(!svc.callOverwriteAndUnlink(tmp.path() + "/does-not-exist.txt", 0));
}

// ─── shred() ───────────────────────────────────────────────────────────────

void TestFileShredder::shred_singleFile_removesIt()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QByteArray payload(1024, 'X');
    const QString path = writeFile(tmp.path() + "/lonefile.bin", payload);

    TestableFileShredderService svc;
    QSignalSpy spy(&svc, &FileShredderService::shredFinished);
    svc.shred({path});

    QVERIFY(spy.wait(30000));
    QCOMPARE(spy.count(), 1);

    const auto args = spy.takeFirst();
    QCOMPARE(args.at(0).toInt(), 1);   // shredded
    QCOMPARE(args.at(1).toInt(), 0);   // failed
    QCOMPARE(args.at(2).toULongLong(), static_cast<qulonglong>(payload.size()));

    QVERIFY(!QFile::exists(path));
}

void TestFileShredder::shred_folder_recursiveAndRemovesEmptyDir()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString root = tmp.path() + "/staged";
    writeFile(root + "/a.txt", QByteArray(100, 'A'));
    writeFile(root + "/sub/b.txt", QByteArray(200, 'B'));
    writeFile(root + "/sub/deeper/c.txt", QByteArray(50, 'C'));

    TestableFileShredderService svc;
    QSignalSpy spy(&svc, &FileShredderService::shredFinished);
    svc.shred({root});

    QVERIFY(spy.wait(30000));
    const auto args = spy.takeFirst();
    QCOMPARE(args.at(0).toInt(), 3);                 // 3 files shredded
    QCOMPARE(args.at(1).toInt(), 0);                 // none failed
    QCOMPARE(args.at(2).toULongLong(), 350ULL);       // 100+200+50

    QVERIFY(!QDir(root).exists());
}

void TestFileShredder::shred_symlink_unlinksLinkNotTarget()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QByteArray payload("do not touch me");
    const QString target = writeFile(tmp.path() + "/target.txt", payload);
    const QString link = tmp.path() + "/link-to-target.txt";
    QVERIFY(QFile::link(target, link));

    TestableFileShredderService svc;
    QSignalSpy spy(&svc, &FileShredderService::shredFinished);
    svc.shred({link});

    QVERIFY(spy.wait(30000));
    const auto args = spy.takeFirst();
    QCOMPARE(args.at(0).toInt(), 1); // the symlink itself
    QCOMPARE(args.at(1).toInt(), 0);

    QVERIFY(!QFile::exists(link));

    QFile targetFile(target);
    QVERIFY(targetFile.open(QIODevice::ReadOnly));
    QCOMPARE(targetFile.readAll(), payload);
}

void TestFileShredder::shred_reportsFailureCount_whenDeleteFails()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString good = writeFile(tmp.path() + "/good.txt", QByteArray(10, 'G'));
    const QString bad = writeFile(tmp.path() + "/bad.txt", QByteArray(10, 'B'));

    TestableFileShredderService svc;
    svc.failingPaths.insert(bad);

    QSignalSpy finishedSpy(&svc, &FileShredderService::shredFinished);
    QSignalSpy failedSpy(&svc, &FileShredderService::itemFailed);
    svc.shred({good, bad});

    QVERIFY(finishedSpy.wait(30000));
    const auto args = finishedSpy.takeFirst();
    QCOMPARE(args.at(0).toInt(), 1); // shredded
    QCOMPARE(args.at(1).toInt(), 1); // failed

    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(failedSpy.takeFirst().at(0).toString(), bad);

    QVERIFY(!QFile::exists(good));
    QVERIFY(QFile::exists(bad)); // the forced failure never touched it
}

// ─── computePreview() ──────────────────────────────────────────────────────

void TestFileShredder::preview_countsFilesAndBytesRecursively()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString root = tmp.path() + "/preview-me";
    writeFile(root + "/one.txt", QByteArray(30, '1'));
    writeFile(root + "/nested/two.txt", QByteArray(70, '2'));

    TestableFileShredderService svc;
    QSignalSpy spy(&svc, &FileShredderService::previewReady);
    svc.computePreview({root});

    QVERIFY(spy.wait(30000));
    const ShredPlan plan = spy.takeFirst().at(0).value<ShredPlan>();

    QCOMPARE(plan.items.size(), 1);
    QCOMPARE(plan.items.first().isDir, true);
    QCOMPARE(plan.items.first().fileCount, 2);
    QCOMPARE(plan.totalFileCount, 2);
    QCOMPARE(plan.totalBytes, 100ULL);

    // Nothing on disk should have been touched by a preview.
    QVERIFY(QDir(root).exists());
    QVERIFY(QFile::exists(root + "/one.txt"));
}

// ─── dedupeContainedPaths() ────────────────────────────────────────────────

void TestFileShredder::dedupe_dropsNestedPath()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString dir = tmp.path() + "/parent";
    const QString nested = writeFile(dir + "/child.txt", QByteArray(5, 'x'));

    const QStringList kept = FileShredderService::dedupeContainedPaths({dir, nested});
    QCOMPARE(kept, QStringList{dir});
}

void TestFileShredder::dedupe_dropsExactDuplicate()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString file = writeFile(tmp.path() + "/once.txt", QByteArray(5, 'y'));

    const QStringList kept = FileShredderService::dedupeContainedPaths({file, file});
    QCOMPARE(kept, QStringList{file});
}

QTEST_MAIN(TestFileShredder)
#include "test_file_shredder_service.moc"
