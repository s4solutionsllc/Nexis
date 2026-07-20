#include <QTest>
#include <QSignalSpy>
#include "Pages/Helpers/cpu_core_list_model.h"

// SSO-15378: the compact per-core detail view is backed by this model so a
// QListView only ever paints the rows the viewport can show. These tests
// stand in for the "verify against a synthetic 32+-core dataset" acceptance
// criterion — no real high-core-count hardware is available in this
// environment, so the model's handling of a synthetic 64-core payload is
// what's actually exercised here (flagged as such in the PR).
class TestCpuCoreListModel : public QObject
{
    Q_OBJECT

private slots:
    void emptyModel();
    void singleCore();
    void synthetic64Cores();
    void reshapeOnCoreCountChange();
    void shorterClocksListDoesNotCrash();
};

void TestCpuCoreListModel::emptyModel()
{
    CpuCoreListModel model;
    QCOMPARE(model.rowCount(), 0);
}

void TestCpuCoreListModel::singleCore()
{
    CpuCoreListModel model;
    model.updateData({42, 42}, {3200.0});

    QCOMPARE(model.rowCount(), 1);
    QModelIndex idx = model.index(0);
    QCOMPARE(model.data(idx, CpuCoreListModel::UtilizationRole).toInt(), 42);
    QCOMPARE(model.data(idx, CpuCoreListModel::FrequencyMhzRole).toDouble(), 3200.0);
    QCOMPARE(model.data(idx, Qt::DisplayRole).toString(), QStringLiteral("CPU 0"));
}

void TestCpuCoreListModel::synthetic64Cores()
{
    const int coreCount = 64;
    QList<int> percents;
    percents << 50; // aggregate
    QList<double> clocks;
    for (int i = 0; i < coreCount; ++i) {
        percents << (i % 101);
        clocks << 1800.0 + i;
    }

    CpuCoreListModel model;
    model.updateData(percents, clocks);

    QCOMPARE(model.rowCount(), coreCount);
    // Spot-check first, middle, last rows rather than a widget per core —
    // exactly the property that keeps this responsive at high core counts.
    QCOMPARE(model.data(model.index(0), CpuCoreListModel::UtilizationRole).toInt(), 0);
    QCOMPARE(model.data(model.index(32), CpuCoreListModel::UtilizationRole).toInt(), 32);
    QCOMPARE(model.data(model.index(63), CpuCoreListModel::UtilizationRole).toInt(), 63);
    QCOMPARE(model.data(model.index(63), CpuCoreListModel::FrequencyMhzRole).toDouble(), 1863.0);
    QCOMPARE(model.data(model.index(63), Qt::DisplayRole).toString(), QStringLiteral("CPU 63"));
}

void TestCpuCoreListModel::reshapeOnCoreCountChange()
{
    CpuCoreListModel model;
    model.updateData({10, 5, 5}, {1000.0, 1000.0});
    QCOMPARE(model.rowCount(), 2);

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    model.updateData({10, 1, 2, 3, 4}, {1000.0, 1000.0, 1000.0, 1000.0});
    QCOMPARE(model.rowCount(), 4);
    QCOMPARE(resetSpy.count(), 1);
}

void TestCpuCoreListModel::shorterClocksListDoesNotCrash()
{
    // Frequency source can be unavailable/short on some hosts (e.g. no
    // scaling_cur_freq exposure) — rows without a clock sample just report 0.
    CpuCoreListModel model;
    model.updateData({10, 1, 2, 3}, {1000.0});

    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.data(model.index(0), CpuCoreListModel::FrequencyMhzRole).toDouble(), 1000.0);
    QCOMPARE(model.data(model.index(1), CpuCoreListModel::FrequencyMhzRole).toDouble(), 0.0);
    QCOMPARE(model.data(model.index(2), CpuCoreListModel::FrequencyMhzRole).toDouble(), 0.0);
}

QTEST_MAIN(TestCpuCoreListModel)
#include "test_cpu_core_list_model.moc"
