// SSO-23861: verify delete-from-treemap uses the same confirm/preview
// discipline as the rest of the app (QMessageBox::question, matching the
// severity of a reversible move-to-trash op — see docs/ARCHITECTURE_REVIEW.md)
// and that a real FileSearchService::moveToTrash() run refreshes the map.
//
// No interactive desktop session is available in CI/sandbox environments, so
// this drives the actual compiled widgets under QT_QPA_PLATFORM=offscreen
// (the same headless backend already relied on by the screenshot regression
// suite) instead of a human clicking through a real display: DiskTreemapDialog
// is invoked exactly as production code invokes it (TreemapView's context menu
// emits trashRequested(), which Qt's meta-object system dispatches to the
// private onTrashRequested() slot — QMetaObject::invokeMethod() reaches that
// same slot here), the confirm dialog is a real QMessageBox driven via its own
// buttons, and moveToTrash() runs its real worker-thread `mv` + trash-metadata
// write against a QTemporaryDir.

#include <QtTest>
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

#include "Pages/Resources/disk_treemap_dialog.h"
#include "Pages/Resources/treemap_view.h"
#include "Managers/dir_size_scanner.h"
#include "Services/file_search_service.h"

namespace {

DirSizeNode *findByPath(DirSizeNode *node, const QString &path)
{
    if (!node)
        return nullptr;
    if (node->path == path)
        return node;
    for (const auto &child : node->children) {
        if (DirSizeNode *found = findByPath(child.get(), path))
            return found;
    }
    return nullptr;
}

void writeFile(const QString &path, const QString &contents)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream(&f) << contents;
}

// Runs a real scan through the dialog's own UI path (sets the folder combo,
// invokes the private onScanClicked() slot exactly like clicking "Scan"
// would) and waits for the treemap to populate.
void scanAndWait(DiskTreemapDialog &dialog, const QString &rootPath)
{
    auto *combo = dialog.findChild<QComboBox *>();
    QVERIFY(combo);
    combo->setCurrentText(rootPath);
    QVERIFY(QMetaObject::invokeMethod(&dialog, "onScanClicked"));

    auto *view = dialog.findChild<TreemapView *>();
    QVERIFY(view);
    QVERIFY2(QTest::qWaitFor([&]() { return view->focus() != nullptr; }, 10000),
             "directory scan did not complete in time");
}

// Arranges the confirm QMessageBox's button click for the *next* modal
// dialog raised on the main event loop (QMessageBox::question() blocks via
// its own nested loop, so this has to be queued ahead of time).
void clickNextConfirmButton(QMessageBox::StandardButton which)
{
    QTimer::singleShot(0, [which]() {
        auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        QVERIFY(box);
        QAbstractButton *btn = box->button(which);
        QVERIFY(btn);
        btn->click();
    });
}

} // namespace

class TestDiskTreemapDeleteConfirm : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cancel_leavesFileInPlaceAndTriggersNoOperation();
    void confirm_movesFileToTrashAndRefreshesMap();
};

// A real desktop session has usually already created ~/.local/share/Trash
// (nautilus/gio/etc. do it on first use); a bare CI/sandbox HOME hasn't, and
// FileSearchToolLinux::buildMoveToTrashArgs() (linux/nexis-core, untouched by
// this change) relies on the destination existing rather than mkpath-ing it
// itself. Recreate that pre-existing-desktop precondition here rather than
// changing production trash-bootstrap behavior, which is out of scope for
// SSO-23861.
void TestDiskTreemapDeleteConfirm::initTestCase()
{
    const QString trash = FileSearchService::ins()->trashPath();
    QVERIFY(QDir().mkpath(trash + "/files"));
    QVERIFY(QDir().mkpath(trash + "/info"));
}

void TestDiskTreemapDeleteConfirm::cancel_leavesFileInPlaceAndTriggersNoOperation()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString victimPath = tmp.filePath("victim-cancel.txt");
    writeFile(victimPath, "do not delete me");

    DiskTreemapDialog dialog;
    scanAndWait(dialog, tmp.path());

    DirSizeNode *victim = findByPath(dialog.findChild<TreemapView *>()->focus(), victimPath);
    QVERIFY2(victim, "scan did not surface the test file");

    QSignalSpy opSpy(FileSearchService::ins(), &FileSearchService::fileOperationFinished);

    clickNextConfirmButton(QMessageBox::No);
    QVERIFY(QMetaObject::invokeMethod(&dialog, "onTrashRequested",
                                      Q_ARG(DirSizeNode *, victim)));

    QVERIFY2(QFile::exists(victimPath), "Cancel must leave the file in place");
    QTest::qWait(200);
    QCOMPARE(opSpy.count(), 0); // Cancel must never touch FileSearchService
}

void TestDiskTreemapDeleteConfirm::confirm_movesFileToTrashAndRefreshesMap()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString victimPath = tmp.filePath("victim-confirm.txt");
    writeFile(victimPath, "delete me for real");
    writeFile(tmp.filePath("bystander.txt"), "leave me alone");

    DiskTreemapDialog dialog;
    scanAndWait(dialog, tmp.path());

    auto *view = dialog.findChild<TreemapView *>();
    const int fileCountBeforeDelete = view->focus()->fileCount;

    DirSizeNode *victim = findByPath(view->focus(), victimPath);
    QVERIFY2(victim, "scan did not surface the test file");

    QSignalSpy opSpy(FileSearchService::ins(), &FileSearchService::fileOperationFinished);

    clickNextConfirmButton(QMessageBox::Yes);
    QVERIFY(QMetaObject::invokeMethod(&dialog, "onTrashRequested",
                                      Q_ARG(DirSizeNode *, victim)));

    QVERIFY2(opSpy.wait(10000), "FileSearchService::fileOperationFinished never fired");
    QCOMPARE(opSpy.count(), 1);
    const auto args = opSpy.takeFirst();
    QCOMPARE(args.at(0).value<FileSearchService::FileOperation>(),
             FileSearchService::FileOperation::MoveToTrash);
    QCOMPARE(args.at(1).toString(), victimPath);
    QVERIFY2(!args.at(2).toBool(),
             qPrintable(QStringLiteral("moveToTrash reported an error: %1").arg(args.at(3).toString())));

    QVERIFY2(!QFile::exists(victimPath), "Confirm must actually remove the file from its original path");

    // Map refresh: the dialog re-scans mLastScannedPath on
    // fileOperationFinished, so the tree's file count should drop by
    // exactly one (the bystander file must still be present, only the
    // deleted entry is gone).
    QVERIFY2(QTest::qWaitFor([&]() {
        DirSizeNode *root = view->focus();
        return root && root->fileCount == fileCountBeforeDelete - 1;
    }, 10000), "treemap did not refresh after the trash operation");

    QVERIFY(findByPath(view->focus(), tmp.filePath("bystander.txt")));
    QVERIFY(!findByPath(view->focus(), victimPath));

    // Clean up the copy this test really did move into the shared sandbox's
    // trash directory, rather than leaving it for the next run.
    const QString trash = FileSearchService::ins()->trashPath();
    const QString fileName = QFileInfo(victimPath).fileName();
    QFile::remove(trash + "/files/" + fileName);
    QFile::remove(trash + "/info/" + fileName + ".trashinfo");
}

QTEST_MAIN(TestDiskTreemapDeleteConfirm)
#include "test_disk_treemap_delete_confirm.moc"
