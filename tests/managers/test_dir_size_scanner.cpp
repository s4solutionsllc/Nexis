// SSO-3737 / FW-09: built-in disk-space visualizer.
//
// Exercises DirSizeScanner::scanSynchronous() against a real QTemporaryDir
// tree. The pure traversal is what feeds the treemap, so we lock down the
// size aggregation, symlink handling, hard-link dedup, and hidden-file
// inclusion here; the rendering layer is UAT.

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextStream>

#include <unistd.h>      // symlink(), link()

#include "Managers/dir_size_scanner.h"

class TestDirSizeScanner : public QObject
{
    Q_OBJECT

private slots:
    void aggregates_nested_directory_sizes();
    void hidden_files_are_counted();
    void symlinks_are_not_followed_or_double_counted();
    void hard_links_counted_once_per_scan();
    void empty_directory_yields_zero_size();
    void async_start_emits_finished_with_tree();
    void cancel_during_async_scan_emits_cancelled();

private:
    static QString writeFile(const QString &dirPath,
                             const QString &name,
                             qint64 size);
    static DirSizeNode *findChild(DirSizeNode *parent, const QString &name);
};

QString TestDirSizeScanner::writeFile(const QString &dirPath,
                                      const QString &name,
                                      qint64 size)
{
    QDir().mkpath(dirPath);
    const QString path = dirPath + "/" + name;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return {};
    QByteArray payload(size, 'x');
    f.write(payload);
    f.close();
    return path;
}

DirSizeNode *TestDirSizeScanner::findChild(DirSizeNode *parent,
                                           const QString &name)
{
    if (!parent)
        return nullptr;
    for (auto &c : parent->children) {
        if (c->name == name)
            return c.get();
    }
    return nullptr;
}

void TestDirSizeScanner::aggregates_nested_directory_sizes()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // tree:
    //   root/
    //     top.bin        (1024)
    //     sub/
    //       a.bin        (2048)
    //       inner/
    //         b.bin      (4096)
    writeFile(tmp.path(),                 "top.bin", 1024);
    writeFile(tmp.path() + "/sub",        "a.bin",   2048);
    writeFile(tmp.path() + "/sub/inner",  "b.bin",   4096);

    DirSizeNodePtr root = DirSizeScanner::scanSynchronous(tmp.path());
    QVERIFY(root);
    QCOMPARE(root->size, qint64(1024 + 2048 + 4096));
    QCOMPARE(root->fileCount, 3);

    DirSizeNode *sub = findChild(root.get(), "sub");
    QVERIFY(sub);
    QVERIFY(sub->isDir);
    QCOMPARE(sub->size, qint64(2048 + 4096));
    QCOMPARE(sub->fileCount, 2);

    DirSizeNode *inner = findChild(sub, "inner");
    QVERIFY(inner);
    QCOMPARE(inner->size, qint64(4096));
    QCOMPARE(inner->fileCount, 1);
}

void TestDirSizeScanner::hidden_files_are_counted()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    writeFile(tmp.path(), ".dotfile",  512);
    writeFile(tmp.path(), "visible",  1024);

    DirSizeNodePtr root = DirSizeScanner::scanSynchronous(tmp.path());
    QVERIFY(root);
    QCOMPARE(root->size, qint64(512 + 1024));
    QCOMPARE(root->fileCount, 2);

    QVERIFY(findChild(root.get(), ".dotfile") != nullptr);
}

void TestDirSizeScanner::symlinks_are_not_followed_or_double_counted()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // target/heavy.bin is the "real" 4 KiB file. We then drop a symlink
    // *inside the scan root* that points back at it. A naive walker that
    // follows symlinks would double-count or loop forever; we want it
    // surfaced as a zero-byte leaf.
    const QString target = writeFile(tmp.path() + "/target", "heavy.bin", 4096);
    QVERIFY(!target.isEmpty());

    const QString linkPath = tmp.path() + "/link_to_target";
    QCOMPARE(::symlink(target.toLocal8Bit().constData(),
                       linkPath.toLocal8Bit().constData()), 0);

    DirSizeNodePtr root = DirSizeScanner::scanSynchronous(tmp.path());
    QVERIFY(root);

    // Only the real file should contribute bytes.
    QCOMPARE(root->size, qint64(4096));
    QCOMPARE(root->fileCount, 1);

    DirSizeNode *sym = findChild(root.get(), "link_to_target");
    QVERIFY(sym);
    QVERIFY(sym->isSymLink);
    QCOMPARE(sym->size, qint64(0));
}

void TestDirSizeScanner::hard_links_counted_once_per_scan()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString original = writeFile(tmp.path(), "original.bin", 8192);
    QVERIFY(!original.isEmpty());

    const QString linkPath = tmp.path() + "/hardlink.bin";
    if (::link(original.toLocal8Bit().constData(),
               linkPath.toLocal8Bit().constData()) != 0) {
        QSKIP("hard links unsupported on this filesystem");
    }

    DirSizeNodePtr root = DirSizeScanner::scanSynchronous(tmp.path());
    QVERIFY(root);

    // The aggregate should count the bytes exactly once even though two
    // dirents reference the same inode.
    QCOMPARE(root->size, qint64(8192));
    // ...but both names should still be visible to the user.
    QCOMPARE(static_cast<int>(root->children.size()), 2);
}

void TestDirSizeScanner::empty_directory_yields_zero_size()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    DirSizeNodePtr root = DirSizeScanner::scanSynchronous(tmp.path());
    QVERIFY(root);
    QVERIFY(root->isDir);
    QCOMPARE(root->size, qint64(0));
    QCOMPARE(root->fileCount, 0);
    QVERIFY(root->children.empty());
}

void TestDirSizeScanner::async_start_emits_finished_with_tree()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    writeFile(tmp.path(), "a.bin", 1024);
    writeFile(tmp.path() + "/d", "b.bin", 2048);

    DirSizeScanner scanner;
    QSignalSpy finishedSpy(&scanner, &DirSizeScanner::finished);
    scanner.start(tmp.path());

    QVERIFY(finishedSpy.wait(10000));
    QCOMPARE(finishedSpy.count(), 1);

    DirSizeNodePtr root = finishedSpy.takeFirst().at(0).value<DirSizeNodePtr>();
    QVERIFY(root);
    QCOMPARE(root->size, qint64(1024 + 2048));
    QCOMPARE(root->fileCount, 2);
}

void TestDirSizeScanner::cancel_during_async_scan_emits_cancelled()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Populate enough entries that there's a realistic window between start()
    // and cancel() — the worker thread polls between dir entries.
    for (int i = 0; i < 64; ++i) {
        const QString sub = tmp.path() + QString("/d%1").arg(i);
        for (int j = 0; j < 16; ++j) {
            writeFile(sub, QString("f%1.bin").arg(j), 4096);
        }
    }

    DirSizeScanner scanner;
    QSignalSpy cancelledSpy(&scanner, &DirSizeScanner::cancelled);
    QSignalSpy finishedSpy(&scanner, &DirSizeScanner::finished);

    scanner.start(tmp.path());
    scanner.cancel();

    // One of the two signals will fire; if cancel landed before the worker
    // even started a tight scan, finished() may win. Either way we assert
    // the scanner exits cleanly with exactly one terminal signal.
    const bool gotTerminal = cancelledSpy.wait(10000) ||
                             finishedSpy.count() > 0 ||
                             cancelledSpy.count() > 0;
    QVERIFY(gotTerminal);
    QCOMPARE(cancelledSpy.count() + finishedSpy.count(), 1);
}

QTEST_MAIN(TestDirSizeScanner)
#include "test_dir_size_scanner.moc"
