#include <QTest>
#include <QList>

#include "Managers/data_refresh_service.h"

// SSO-3380 / WI-18: cover the predicate that gates the cpuUpdated emit
// when the producer (host_processor_info on macOS, /proc/stat on Linux)
// returns an empty list on a transient read failure. Before this guard
// existed, DashboardPage::onCpuUpdated indexed percents.at(0) and
// ResourcesPage::onCpuUpdated indexed percents.at(j+1) — UB in release.
class TestDataRefreshService : public QObject
{
    Q_OBJECT

private slots:
    void cpuPayload_emptyListIsNotEmittable();
    void cpuPayload_singleOverallEntryIsEmittable();
    void cpuPayload_overallPlusPerCoreIsEmittable();
};

void TestDataRefreshService::cpuPayload_emptyListIsNotEmittable()
{
    QList<int> percents; // producer failure: host_processor_info / /proc/stat
    QVERIFY(!DataRefreshService::isCpuPayloadEmittable(percents));
}

void TestDataRefreshService::cpuPayload_singleOverallEntryIsEmittable()
{
    // Minimum viable payload: overall % only. DashboardPage reads .at(0);
    // ResourcesPage now bounds-checks before reading .at(j+1).
    QList<int> percents = {42};
    QVERIFY(DataRefreshService::isCpuPayloadEmittable(percents));
}

void TestDataRefreshService::cpuPayload_overallPlusPerCoreIsEmittable()
{
    QList<int> percents = {42, 30, 55, 12, 78};
    QVERIFY(DataRefreshService::isCpuPayloadEmittable(percents));
}

QTEST_APPLESS_MAIN(TestDataRefreshService)
#include "test_data_refresh_service.moc"
