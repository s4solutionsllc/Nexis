#include <QTest>
#include "process_tree_util.h"

class TestProcessTreeUtil : public QObject
{
    Q_OBJECT

private slots:
    void emptyInput();
    void linksChildToParent();
    void missingParentBecomesRoot();
    void selfParentBecomesRoot();
    void zeroParentIsRoot();
    void cycleIsBroken();
    void cycleWithTailKeepsTailAttached();
};

void TestProcessTreeUtil::emptyInput()
{
    QVERIFY(ProcessTreeUtil::resolveParents({}).isEmpty());
}

void TestProcessTreeUtil::linksChildToParent()
{
    const auto parents = ProcessTreeUtil::resolveParents({{1, 0}, {10, 1}, {11, 10}});
    QCOMPARE(parents.value(1), pid_t(0));
    QCOMPARE(parents.value(10), pid_t(1));
    QCOMPARE(parents.value(11), pid_t(10));
}

void TestProcessTreeUtil::missingParentBecomesRoot()
{
    const auto parents = ProcessTreeUtil::resolveParents({{20, 999}});
    QCOMPARE(parents.value(20), pid_t(0));
}

void TestProcessTreeUtil::selfParentBecomesRoot()
{
    const auto parents = ProcessTreeUtil::resolveParents({{5, 5}});
    QCOMPARE(parents.value(5), pid_t(0));
}

void TestProcessTreeUtil::zeroParentIsRoot()
{
    const auto parents = ProcessTreeUtil::resolveParents({{2, 0}});
    QCOMPARE(parents.value(2), pid_t(0));
}

void TestProcessTreeUtil::cycleIsBroken()
{
    const auto parents = ProcessTreeUtil::resolveParents({{3, 4}, {4, 5}, {5, 3}});
    int roots = 0;
    for (auto it = parents.constBegin(); it != parents.constEnd(); ++it)
        roots += it.value() == 0;
    QCOMPARE(roots, 1);
    QCOMPARE(parents.value(3), pid_t(0));   // lowest pid in the cycle is detached
}

void TestProcessTreeUtil::cycleWithTailKeepsTailAttached()
{
    const auto parents = ProcessTreeUtil::resolveParents({{3, 4}, {4, 3}, {9, 4}});
    QCOMPARE(parents.value(3), pid_t(0));
    QCOMPARE(parents.value(4), pid_t(3));
    QCOMPARE(parents.value(9), pid_t(4));
}

QTEST_MAIN(TestProcessTreeUtil)
#include "test_process_tree_util.moc"
