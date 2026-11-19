#include <QTest>
#include <QCheckBox>
#include <QTreeView>

#include "Managers/data_refresh_service.h"
#include "Pages/Processes/processes_page.h"
#include "nexis_roles.h"

namespace {

Process makeProc(pid_t pid, pid_t ppid, const QString &cmd, bool isApp = false)
{
    Process p;
    p.setPid(pid);
    p.setPpid(ppid);
    p.setUname(QStringLiteral("tester"));
    p.setCmd(cmd);
    p.setName(cmd);
    p.setIsAppProcess(isApp);
    return p;
}

}

class TestProcessesPageTree : public QObject
{
    Q_OBJECT

    ProcessesPage *mPage = nullptr;
    QTreeView *mView = nullptr;
    QCheckBox *mTreeCheck = nullptr;

    void feed(const QList<Process> &procs)
    {
        emit DataRefreshService::ins()->processesUpdated(procs, QStringLiteral("tester"));
    }

    QModelIndex indexForPid(pid_t pid, const QModelIndex &parent = QModelIndex()) const
    {
        QAbstractItemModel *m = mView->model();
        for (int r = 0; r < m->rowCount(parent); ++r) {
            const QModelIndex idx = m->index(r, 0, parent);
            if (!idx.data(GroupHeaderRole).toBool() && idx.data(SortRole).toLongLong() == pid)
                return idx;
            const QModelIndex nested = indexForPid(pid, idx);
            if (nested.isValid())
                return nested;
        }
        return {};
    }

private slots:
    void init()
    {
        mPage = new ProcessesPage();
        mView = mPage->findChild<QTreeView *>("tableProcess");
        mTreeCheck = mPage->findChild<QCheckBox *>("checkProcessTree");
        QVERIFY(mView && mTreeCheck);
    }

    void cleanup()
    {
        delete mPage;
    }

    void groupModeKeepsRowsUnderSectionHeaders()
    {
        feed({makeProc(1, 0, "init"), makeProc(10, 1, "child", true)});
        QCOMPARE(indexForPid(10).parent().data(GroupHeaderRole).toBool(), true);
        QCOMPARE(indexForPid(1).parent().data(GroupHeaderRole).toBool(), true);
    }

    void treeModeNestsChildUnderParent()
    {
        feed({makeProc(1, 0, "init"), makeProc(10, 1, "child"), makeProc(11, 10, "grandchild")});
        mTreeCheck->setChecked(true);

        const QModelIndex init = indexForPid(1);
        QVERIFY(init.isValid());
        QVERIFY(!init.parent().isValid());
        QCOMPARE(indexForPid(10).parent(), init);
        QCOMPARE(indexForPid(11).parent(), indexForPid(10));
        QVERIFY(mView->isExpanded(init));
    }

    void treeModeHidesSectionHeaders()
    {
        feed({makeProc(1, 0, "init")});
        mTreeCheck->setChecked(true);
        QAbstractItemModel *m = mView->model();
        for (int r = 0; r < m->rowCount(); ++r)
            QVERIFY(!m->index(r, 0).data(GroupHeaderRole).toBool());
    }

    void childArrivingBeforeItsParentIsStillNested()
    {
        mTreeCheck->setChecked(true);
        feed({makeProc(10, 1, "child"), makeProc(1, 0, "init")});
        QCOMPARE(indexForPid(10).parent(), indexForPid(1));
    }

    void orphanOfExitedParentMovesToRoot()
    {
        mTreeCheck->setChecked(true);
        feed({makeProc(1, 0, "init"), makeProc(10, 1, "shell"), makeProc(11, 10, "job")});
        feed({makeProc(1, 0, "init"), makeProc(11, 1, "job")});
        QVERIFY(!indexForPid(10).isValid());
        QCOMPARE(indexForPid(11).parent(), indexForPid(1));
    }

    void exitedSubtreeIsRemovedWithoutDangling()
    {
        mTreeCheck->setChecked(true);
        feed({makeProc(1, 0, "init"), makeProc(10, 1, "a"), makeProc(11, 10, "b"), makeProc(12, 11, "c")});
        feed({makeProc(1, 0, "init")});
        QVERIFY(indexForPid(1).isValid());
        QVERIFY(!indexForPid(10).isValid());
        QVERIFY(!indexForPid(12).isValid());
        feed({makeProc(1, 0, "init"), makeProc(12, 1, "c")});
        QCOMPARE(indexForPid(12).parent(), indexForPid(1));
    }

    void swappedParentChildDoesNotFormCycle()
    {
        mTreeCheck->setChecked(true);
        feed({makeProc(1, 0, "init"), makeProc(10, 1, "x"), makeProc(11, 10, "y")});
        feed({makeProc(1, 0, "init"), makeProc(10, 11, "x"), makeProc(11, 1, "y")});
        QCOMPARE(indexForPid(11).parent(), indexForPid(1));
        QCOMPARE(indexForPid(10).parent(), indexForPid(11));
    }

    void collapsedBranchStaysCollapsedWhenChildAppears()
    {
        mTreeCheck->setChecked(true);
        feed({makeProc(1, 0, "init"), makeProc(10, 1, "a")});
        mView->collapse(indexForPid(1));
        feed({makeProc(1, 0, "init"), makeProc(10, 1, "a"), makeProc(11, 1, "b")});
        QVERIFY(!mView->isExpanded(indexForPid(1)));
    }

    void togglingBackRestoresGroupsAndSpans()
    {
        feed({makeProc(1, 0, "init"), makeProc(10, 1, "child", true)});
        mTreeCheck->setChecked(true);
        mTreeCheck->setChecked(false);

        const QModelIndex row = indexForPid(10);
        QVERIFY(row.isValid());
        QVERIFY(row.parent().data(GroupHeaderRole).toBool());
        QVERIFY(mView->isExpanded(row.parent()));
        QVERIFY(mView->isFirstColumnSpanned(row.parent().row(), row.parent().parent()));
    }
};

QTEST_MAIN(TestProcessesPageTree)
#include "test_processes_page_tree.moc"
