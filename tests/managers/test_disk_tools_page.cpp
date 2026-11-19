#include <QtTest>
#include <atomic>
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
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

// Same stubs on a real worker thread, for the pause/resume wiring.
class AsyncFakeFinderService : public FakeFinderService
{
protected:
    bool runsAsynchronously() const override { return true; }
};

void writeFile(const QString &path, int bytes, char fill = 'x')
{
    QFileInfo(path).absoluteDir().mkpath(QStringLiteral("."));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QByteArray(bytes, fill));
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


// Clicks the confirm button of the "Replace Duplicates with Links" preview.
void confirmNextLinkDialog()
{
    QTimer::singleShot(0, []() {
        QWidget *dlg = QApplication::activeModalWidget();
        QVERIFY2(dlg && dlg->objectName() == "LinkReplaceDialog", "expected the link preview");
        auto *confirm = dlg->findChild<QPushButton *>(QStringLiteral("btnLinkReplaceConfirm"));
        QVERIFY(confirm && confirm->isEnabled());
        confirm->click();
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

    // The Duplicate Finder page's Match combo, and its min-size unit combo.
    QComboBox *comboWithItem(DiskToolsPage &page, const QString &item)
    {
        for (QComboBox *c : page.findChildren<QComboBox *>()) {
            if (c->findText(item) >= 0)
                return c;
        }
        return nullptr;
    }

    // Duplicate mode defaults to "files of at least 1 MB"; the test files are small.
    void allowSmallDuplicates(DiskToolsPage &page)
    {
        QComboBox *unit = comboWithItem(page, QStringLiteral("KB"));
        QVERIFY(unit);
        unit->setCurrentIndex(0);
    }

    QPushButton *trashButton(QTreeWidget *tree)
    {
        QWidget *modePage = tree->parentWidget()->parentWidget();
        return modePage->findChild<QPushButton *>(QStringLiteral("btnTrash"));
    }

private slots:
    void initTestCase()
    {
        // Large & Old reads exclusions through the CleanerService singleton;
        // keep that off the developer's real settings file.
        QStandardPaths::setTestModeEnabled(true);
    }

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

    // SSO-27146: the Match combo gains "Duplicate folders"; folder groups
    // are trashed through the service and never offered link replacement.
    void dupFolders_listsIdenticalFolders_trashGoesThroughService()
    {
        QTemporaryDir tmp;
        writeFile(tmp.filePath("a/f.bin"), 2000, 'q');
        writeFile(tmp.filePath("b/f.bin"), 2000, 'q');

        FakeFinderService service;
        DiskToolsPage page(nullptr, &service);
        useOnlyDirectory(page, tmp.path());
        allowSmallDuplicates(page);
        QComboBox *match = comboWithItem(page, QStringLiteral("Duplicate folders"));
        QVERIFY(match);
        match->setCurrentIndex(match->findText(QStringLiteral("Duplicate folders")));

        auto *tree = page.findChild<QTreeWidget *>(QStringLiteral("treeWidgetDuplicates"));
        QVERIFY(tree);
        scanButton(tree)->click();

        QCOMPARE(tree->topLevelItemCount(), 1);
        QTreeWidgetItem *group = tree->topLevelItem(0);
        QVERIFY(group->text(0).startsWith(QStringLiteral("Identical folders")));
        QCOMPARE(group->childCount(), 2);
        QCOMPARE(group->child(0)->checkState(0), Qt::Unchecked);
        QCOMPARE(group->child(1)->checkState(0), Qt::Checked);
        QCOMPARE(group->child(1)->data(2, Qt::UserRole).toULongLong(), quint64(2000));
        QVERIFY(!page.findChild<QPushButton *>(QStringLiteral("btnDupLink"))->isEnabled());

        acceptNextMessageBox();
        trashButton(tree)->click();
        QCOMPARE(service.trashed, QStringList{QFileInfo(tmp.filePath("b")).absoluteFilePath()});
    }

    // SSO-27146: preview dialog -> hard links -> rows leave the tree.
    void dupLinks_replaceCheckedCopyWithHardLink()
    {
        QTemporaryDir tmp;
        const QString a = tmp.filePath("a.bin");
        const QString b = tmp.filePath("b.bin");
        writeFile(a, 2000, 'z');
        writeFile(b, 2000, 'z');

        FakeFinderService service;
        DiskToolsPage page(nullptr, &service);
        useOnlyDirectory(page, tmp.path());
        allowSmallDuplicates(page);

        auto *tree = page.findChild<QTreeWidget *>(QStringLiteral("treeWidgetDuplicates"));
        scanButton(tree)->click();
        QCOMPARE(tree->topLevelItemCount(), 1);

        auto *linkButton = page.findChild<QPushButton *>(QStringLiteral("btnDupLink"));
        QVERIFY(linkButton);
        QVERIFY2(linkButton->isEnabled(), "the pre-checked duplicate is linkable");

        confirmNextLinkDialog();
        linkButton->click();

        QVERIFY(DuplicateFinderService::isSameFile(a, b));
        QCOMPARE(tree->topLevelItemCount(), 0); // no longer two separate copies
        QVERIFY(!linkButton->isEnabled());
    }

    // SSO-27146: Pause on the page parks the service; the same button resumes.
    void dupScan_pauseButton_parksAndResumes()
    {
        QTemporaryDir tmp;
        for (int i = 0; i < 8; ++i) {
            writeFile(tmp.filePath(QStringLiteral("x/%1.bin").arg(i)), 2000, char('a' + i));
            writeFile(tmp.filePath(QStringLiteral("y/%1.bin").arg(i)), 2000, char('a' + i));
        }

        AsyncFakeFinderService service;
        std::atomic<bool> pausedOnce{false};
        QObject::connect(&service, &DuplicateFinderService::progressUpdated, &service,
                         [&](int, int, int, const QString &) {
            if (!pausedOnce.exchange(true))
                service.pause();
        }, Qt::DirectConnection);

        DiskToolsPage page(nullptr, &service);
        useOnlyDirectory(page, tmp.path());
        allowSmallDuplicates(page);

        auto *tree = page.findChild<QTreeWidget *>(QStringLiteral("treeWidgetDuplicates"));
        auto *pauseButton = page.findChild<QPushButton *>(QStringLiteral("btnDupPause"));
        QVERIFY(pauseButton);
        QVERIFY(pauseButton->isHidden());

        scanButton(tree)->click();
        QTRY_VERIFY(service.isPaused());
        QTRY_COMPARE(pauseButton->text(), QStringLiteral("Resume"));
        QVERIFY(!pauseButton->isHidden());
        QCOMPARE(tree->topLevelItemCount(), 0);

        pauseButton->click();
        QVERIFY(!service.isPaused());
        QTRY_COMPARE_WITH_TIMEOUT(tree->topLevelItemCount(), 8, 15000);
        QTRY_VERIFY(pauseButton->isHidden());
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

    // SSO-26117: at the app's 700px minimum window width (page width here is
    // narrower still, since the sidebar and margins also eat into it), the
    // four mode buttons used to shrink below sizeHint() and clip their
    // labels on both sides. Drive a real show()/expose cycle (same headless
    // offscreen backend as TestBtmRow::shownRow_realPostLayoutGeometryNeverClipsBadges)
    // so this asserts on actual post-layout geometry, not just sizeHint() math.
    void modeBar_narrowWidth_reflowsToGridWithNoClipping()
    {
        FakeFinderService service;
        DiskToolsPage page(nullptr, &service);

        QWidget *modeBarRow = page.findChild<QWidget *>(QStringLiteral("modeBarRow"));
        QVERIFY(modeBarRow);
        const QList<QPushButton *> modeButtons =
            modeBarRow->findChildren<QPushButton *>(QString(), Qt::FindDirectChildrenOnly);
        QCOMPARE(modeButtons.size(), 4);

        page.resize(440, 480);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));

        for (QPushButton *button : modeButtons)
            QVERIFY(button->width() >= button->sizeHint().width());
    }

    // Companion to the narrow-width case above: once all four fit, they stay
    // in a single row rather than the 2x2 grid the narrow case reflows into.
    void modeBar_wideWidth_staysSingleRow()
    {
        FakeFinderService service;
        DiskToolsPage page(nullptr, &service);

        QWidget *modeBarRow = page.findChild<QWidget *>(QStringLiteral("modeBarRow"));
        QVERIFY(modeBarRow);
        const QList<QPushButton *> modeButtons =
            modeBarRow->findChildren<QPushButton *>(QString(), Qt::FindDirectChildrenOnly);
        QCOMPARE(modeButtons.size(), 4);

        page.resize(1000, 480);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));

        const int firstY = modeButtons.first()->y();
        for (QPushButton *button : modeButtons)
            QCOMPARE(button->y(), firstY);
    }
};

QTEST_MAIN(TestDiskToolsPage)
#include "test_disk_tools_page.moc"
