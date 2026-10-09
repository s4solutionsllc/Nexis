#include <QtTest>
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>

#include "Pages/DiskTools/disk_tools_page.h"
#include "Services/duplicate_finder_service.h"

// SSO-25782: DiskToolsPage's Largest Files and Empty Folders modes, driven
// through a synchronous service whose trash and exclusion seams are stubbed
// so nothing touches the real trash or the user's saved exclusions.

namespace {

class FakeFinderService : public DuplicateFinderService
{
public:
    FakeFinderService() : DuplicateFinderService(nullptr) {}

    QList<CleanerService::ExclusionEntry> injectedExclusions;
    QStringList trashed;

protected:
    QList<CleanerService::ExclusionEntry> loadExclusions() const override { return injectedExclusions; }
    bool runsAsynchronously() const override { return false; }
    bool moveToTrash(const QString &path) override
    {
        trashed.append(path);
        return true;
    }
};

void writeFile(const QString &path, int bytes)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QByteArray(bytes, 'x'));
}

// Answers the page's "Move to Trash?" QMessageBox::question with Yes.
void acceptNextMessageBox()
{
    QTimer::singleShot(0, []() {
        auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        QVERIFY2(box, "expected a confirmation dialog");
        box->button(QMessageBox::Yes)->click();
    });
}

}

class TestDiskToolsPage : public QObject
{
    Q_OBJECT

private:
    // Replaces the default Home/Downloads/Documents roots with `dir` in every
    // mode's list, so a scan never walks the real home directory.
    void useOnlyDirectory(DiskToolsPage &page, const QString &dir)
    {
        const QList<QListWidget *> lists = page.findChildren<QListWidget *>(QStringLiteral("diskToolsDirList"));
        QCOMPARE(lists.size(), 4);
        for (QListWidget *list : lists) {
            list->clear();
            list->addItem(dir);
        }
    }

    QPushButton *scanButton(QTreeWidget *tree)
    {
        // The page-level Scan button sits in the same stacked page as the tree.
        QWidget *modePage = tree->parentWidget()->parentWidget();
        return modePage->findChild<QPushButton *>(QStringLiteral("btnScan"));
    }

    QPushButton *trashButton(QTreeWidget *tree)
    {
        QWidget *modePage = tree->parentWidget()->parentWidget();
        return modePage->findChild<QPushButton *>(QStringLiteral("btnTrash"));
    }

private slots:
    void fourModes_shareOneDirectoryList()
    {
        FakeFinderService service;
        DiskToolsPage page(nullptr, &service);

        auto *stack = page.findChild<QStackedWidget *>(QStringLiteral("stackedModes"));
        QVERIFY(stack);
        QCOMPARE(stack->count(), 4);

        const QList<QListWidget *> lists = page.findChildren<QListWidget *>(QStringLiteral("diskToolsDirList"));
        QCOMPARE(lists.size(), 4);
        for (QListWidget *list : lists)
            QCOMPARE(list->count(), lists.first()->count());
    }

    void largest_listsTopN_uncheckedAndSortedBySize()
    {
        QTemporaryDir tmp;
        writeFile(tmp.filePath("small.bin"), 10);
        writeFile(tmp.filePath("big.bin"), 3000);
        writeFile(tmp.filePath("mid.bin"), 200);

        FakeFinderService service;
        DiskToolsPage page(nullptr, &service);
        useOnlyDirectory(page, tmp.path());

        auto *tree = page.findChild<QTreeWidget *>(QStringLiteral("treeWidgetLargest"));
        QVERIFY(tree);
        auto *spin = page.findChild<QSpinBox *>(QStringLiteral("spinTopN"));
        QVERIFY(spin);
        spin->setMinimum(1);
        spin->setValue(2);

        scanButton(tree)->click();

        QCOMPARE(tree->topLevelItemCount(), 2);
        QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("big.bin"));
        QCOMPARE(tree->topLevelItem(1)->text(0), QStringLiteral("mid.bin"));
        for (int i = 0; i < tree->topLevelItemCount(); ++i)
            QCOMPARE(tree->topLevelItem(i)->checkState(0), Qt::Unchecked);
        QVERIFY(!trashButton(tree)->isEnabled());
    }

    void largest_trashGoesThroughService_andRemovesRows()
    {
        QTemporaryDir tmp;
        writeFile(tmp.filePath("a.bin"), 500);
        writeFile(tmp.filePath("b.bin"), 100);

        FakeFinderService service;
        DiskToolsPage page(nullptr, &service);
        useOnlyDirectory(page, tmp.path());

        auto *tree = page.findChild<QTreeWidget *>(QStringLiteral("treeWidgetLargest"));
        scanButton(tree)->click();
        QCOMPARE(tree->topLevelItemCount(), 2);

        tree->topLevelItem(0)->setCheckState(0, Qt::Checked);
        QVERIFY(trashButton(tree)->isEnabled());

        acceptNextMessageBox();
        trashButton(tree)->click();

        QCOMPARE(service.trashed, QStringList{QFileInfo(tmp.filePath("a.bin")).absoluteFilePath()});
        QCOMPARE(tree->topLevelItemCount(), 1);
        QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("b.bin"));
        QVERIFY(!trashButton(tree)->isEnabled());
    }

    void largest_excludedPath_neverListed()
    {
        QTemporaryDir tmp;
        QVERIFY(QDir(tmp.path()).mkdir("keep"));
        writeFile(tmp.filePath("keep/protected.bin"), 5000);
        writeFile(tmp.filePath("loose.bin"), 100);

        FakeFinderService service;
        CleanerService::ExclusionEntry entry;
        entry.path = QFileInfo(tmp.filePath("keep")).absoluteFilePath();
        entry.type = CleanerService::ExclusionEntry::Folder;
        service.injectedExclusions = {entry};

        DiskToolsPage page(nullptr, &service);
        useOnlyDirectory(page, tmp.path());

        auto *tree = page.findChild<QTreeWidget *>(QStringLiteral("treeWidgetLargest"));
        scanButton(tree)->click();

        QCOMPARE(tree->topLevelItemCount(), 1);
        QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("loose.bin"));
    }

    void emptyFolders_listedUnchecked_andTrashedThroughService()
    {
        QTemporaryDir tmp;
        QDir root(tmp.path());
        QVERIFY(root.mkpath("empty_one"));
        QVERIFY(root.mkpath("has_file"));
        writeFile(tmp.filePath("has_file/x.txt"), 1);

        FakeFinderService service;
        DiskToolsPage page(nullptr, &service);
        useOnlyDirectory(page, tmp.path());

        auto *tree = page.findChild<QTreeWidget *>(QStringLiteral("treeWidgetEmptyFolders"));
        QVERIFY(tree);
        scanButton(tree)->click();

        QCOMPARE(tree->topLevelItemCount(), 1);
        QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("empty_one"));
        QCOMPARE(tree->topLevelItem(0)->checkState(0), Qt::Unchecked);

        tree->topLevelItem(0)->setCheckState(0, Qt::Checked);
        acceptNextMessageBox();
        trashButton(tree)->click();

        QCOMPARE(service.trashed, QStringList{QFileInfo(tmp.filePath("empty_one")).absoluteFilePath()});
        QCOMPARE(tree->topLevelItemCount(), 0);
    }

    // Large & Old used to call QFile::moveToTrash directly, bypassing the
    // cleaner exclusion engine.
    void largeOld_trashGoesThroughService()
    {
        QTemporaryDir tmp;
        writeFile(tmp.filePath("huge.bin"), 2 * 1024 * 1024);
        writeFile(tmp.filePath("tiny.bin"), 10);

        FakeFinderService service;
        DiskToolsPage page(nullptr, &service);
        useOnlyDirectory(page, tmp.path());

        auto *tree = page.findChild<QTreeWidget *>(QStringLiteral("treeWidgetLargeOld"));
        QVERIFY(tree);
        QWidget *modePage = page.findChild<QWidget *>(QStringLiteral("pageLargeOld"));
        QVERIFY(modePage);
        const QList<QSpinBox *> spins = modePage->findChildren<QSpinBox *>();
        QVERIFY(!spins.isEmpty());
        spins.first()->setValue(1);
        const QList<QComboBox *> combos = modePage->findChildren<QComboBox *>();
        QCOMPARE(combos.size(), 3);
        combos.last()->setCurrentIndex(1);

        modePage->findChild<QPushButton *>(QStringLiteral("btnScan"))->click();
        QTRY_COMPARE(tree->topLevelItemCount(), 1);
        QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("huge.bin"));

        tree->topLevelItem(0)->setCheckState(0, Qt::Checked);
        acceptNextMessageBox();
        modePage->findChild<QPushButton *>(QStringLiteral("btnTrash"))->click();

        QCOMPARE(service.trashed, QStringList{QFileInfo(tmp.filePath("huge.bin")).absoluteFilePath()});
        QCOMPARE(tree->topLevelItemCount(), 0);
    }

    void emptyScan_showsEmptyStateAgain()
    {
        QTemporaryDir tmp;
        writeFile(tmp.filePath("only.txt"), 1);

        FakeFinderService service;
        DiskToolsPage page(nullptr, &service);
        useOnlyDirectory(page, tmp.path());

        auto *tree = page.findChild<QTreeWidget *>(QStringLiteral("treeWidgetEmptyFolders"));
        scanButton(tree)->click();

        QCOMPARE(tree->topLevelItemCount(), 0);
        QVERIFY(tree->isHidden());
    }
};

QTEST_MAIN(TestDiskToolsPage)
#include "test_disk_tools_page.moc"
