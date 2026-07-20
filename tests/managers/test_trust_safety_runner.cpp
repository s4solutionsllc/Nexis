// SSO-15380: Trust & Safety shared component — scan/execute orchestration.
//
// Exercises TrustSafetyRunner's synchronous helpers against a fake
// in-memory TrustSafetyActionProvider. The behaviors under test are exactly
// the acceptance criteria from the issue: incremental discovery, a Stop
// that actually halts remaining work (not just a UI dismiss), and a
// dry-run pass that reports the same numbers as a real run while causing
// zero provider-observable side effects.

#include <QtTest>
#include <QSet>
#include <QSignalSpy>

#include <algorithm>

#include "Common/trust_safety_runner.h"

namespace {

TrustSafetyActionItem makeItem(const QString &id, qint64 size,
                               TrustSafetyActionItem::RiskTier risk = TrustSafetyActionItem::RiskTier::Standard)
{
    TrustSafetyActionItem item;
    item.id = id;
    item.label = id;
    item.description = QStringLiteral("Test item %1").arg(id);
    item.command = QStringLiteral("rm -rf /tmp/%1").arg(id);
    item.categoryId = QStringLiteral("cat");
    item.categoryLabel = QStringLiteral("Category");
    item.riskTier = risk;
    item.estimatedSizeBytes = size;
    return item;
}

// In-memory provider: scan() replays a fixed item list (polling the cancel
// flag between items, per the TrustSafetyActionProvider contract);
// performItem() records call order and which ids actually got a "real"
// (non-dry-run) side effect, so tests can assert dry-run touches nothing.
class FakeProvider : public TrustSafetyActionProvider
{
public:
    QList<TrustSafetyActionItem> discoverable;
    QSet<QString> failIds;
    QStringList performOrder;
    QStringList sideEffectedIds;

    void scan(QAtomicInt *cancelled,
              const std::function<void(const TrustSafetyActionItem &)> &itemFound) override
    {
        for (const TrustSafetyActionItem &item : discoverable) {
            if (cancelled && cancelled->loadRelaxed())
                return;
            itemFound(item);
        }
    }

    TrustSafetyActionResult performItem(const TrustSafetyActionItem &item, bool dryRun) override
    {
        performOrder.append(item.id);

        TrustSafetyActionResult result;
        result.itemId = item.id;
        if (failIds.contains(item.id)) {
            result.succeeded = false;
            result.error = QStringLiteral("simulated failure");
            return result;
        }

        result.succeeded = true;
        result.bytesFreed = qMax<qint64>(0, item.estimatedSizeBytes);
        if (!dryRun)
            sideEffectedIds.append(item.id);
        return result;
    }
};

} // namespace

class TestTrustSafetyRunner : public QObject
{
    Q_OBJECT

private slots:
    void scan_reports_items_incrementally_and_returns_full_list();
    void scan_stops_when_cancelled_between_items();
    void execute_aggregates_bytes_freed_across_all_items();
    void execute_cancel_mid_run_stops_remaining_items_and_reports_partial();
    void execute_dry_run_matches_real_run_numbers_with_zero_side_effects();
    void execute_reports_failures_without_aborting_the_batch();
    void async_scan_emits_items_then_finished();
};

void TestTrustSafetyRunner::scan_reports_items_incrementally_and_returns_full_list()
{
    FakeProvider provider;
    provider.discoverable = {makeItem("a", 100), makeItem("b", 200), makeItem("c", 300)};

    QStringList discoveredOrder;
    QList<TrustSafetyActionItem> items = TrustSafetyRunner::scanSynchronous(
        &provider, nullptr,
        [&](const TrustSafetyActionItem &item) { discoveredOrder.append(item.id); });

    QCOMPARE(items.size(), 3);
    QCOMPARE(discoveredOrder, QStringList({"a", "b", "c"}));
}

void TestTrustSafetyRunner::scan_stops_when_cancelled_between_items()
{
    FakeProvider provider;
    provider.discoverable = {makeItem("a", 1), makeItem("b", 1), makeItem("c", 1),
                              makeItem("d", 1), makeItem("e", 1)};

    QAtomicInt cancelled{0};
    int seen = 0;
    QList<TrustSafetyActionItem> items = TrustSafetyRunner::scanSynchronous(
        &provider, &cancelled,
        [&](const TrustSafetyActionItem &) {
            seen++;
            if (seen == 2)
                cancelled.storeRelaxed(1); // simulates the user hitting Stop mid-scan
        });

    // The scan must actually stop discovering — not just report a
    // "cancelled" flag while quietly finishing the rest.
    QCOMPARE(items.size(), 2);
}

void TestTrustSafetyRunner::execute_aggregates_bytes_freed_across_all_items()
{
    FakeProvider provider;
    QList<TrustSafetyActionItem> items = {makeItem("a", 100), makeItem("b", 200), makeItem("c", 300)};

    TrustSafetyRunSummary summary =
        TrustSafetyRunner::executeSynchronous(&provider, items, /*dryRun=*/false, nullptr);

    QCOMPARE(summary.totalItemsRequested, 3);
    QCOMPARE(summary.totalItemsSucceeded, 3);
    QCOMPARE(summary.totalBytesFreed, qint64(600));
    QVERIFY(!summary.cancelled);
    QCOMPARE(provider.sideEffectedIds.size(), 3);
}

void TestTrustSafetyRunner::execute_cancel_mid_run_stops_remaining_items_and_reports_partial()
{
    FakeProvider provider;
    QList<TrustSafetyActionItem> items = {makeItem("a", 1), makeItem("b", 1), makeItem("c", 1),
                                          makeItem("d", 1), makeItem("e", 1)};

    QAtomicInt cancelled{0};
    TrustSafetyRunSummary summary = TrustSafetyRunner::executeSynchronous(
        &provider, items, /*dryRun=*/false, &cancelled,
        [&](const TrustSafetyActionResult &, int done, int, qint64) {
            if (done == 2)
                cancelled.storeRelaxed(1); // Stop hit right after the 2nd item completes
        });

    // Acceptance criterion: Cancel actually stops the underlying operation
    // mid-run — the remaining 3 items must never have reached performItem().
    QCOMPARE(provider.performOrder.size(), 2);
    QVERIFY(summary.cancelled);
    QCOMPARE(summary.results.size(), 2);
    QCOMPARE(summary.totalItemsRequested, 5);
}

void TestTrustSafetyRunner::execute_dry_run_matches_real_run_numbers_with_zero_side_effects()
{
    FakeProvider provider;
    QList<TrustSafetyActionItem> items = {makeItem("a", 100), makeItem("b", 200)};

    TrustSafetyRunSummary summary =
        TrustSafetyRunner::executeSynchronous(&provider, items, /*dryRun=*/true, nullptr);

    // Same itemized preview numbers a real run would report...
    QCOMPARE(summary.totalItemsSucceeded, 2);
    QCOMPARE(summary.totalBytesFreed, qint64(300));
    QVERIFY(summary.dryRun);
    // ...but zero side effects on the provider.
    QVERIFY(provider.sideEffectedIds.isEmpty());
}

void TestTrustSafetyRunner::execute_reports_failures_without_aborting_the_batch()
{
    FakeProvider provider;
    provider.failIds = {"b"};
    QList<TrustSafetyActionItem> items = {makeItem("a", 100), makeItem("b", 200), makeItem("c", 300)};

    TrustSafetyRunSummary summary =
        TrustSafetyRunner::executeSynchronous(&provider, items, /*dryRun=*/false, nullptr);

    QCOMPARE(provider.performOrder.size(), 3); // one failure doesn't stop the rest of the batch
    QCOMPARE(summary.totalItemsSucceeded, 2);
    QCOMPARE(summary.totalBytesFreed, qint64(400)); // 100 + 300, not the failed 200
    QCOMPARE(summary.results.size(), 3);

    const auto failed = std::find_if(summary.results.begin(), summary.results.end(),
        [](const TrustSafetyActionResult &r) { return r.itemId == "b"; });
    QVERIFY(failed != summary.results.end());
    QVERIFY(!failed->succeeded);
    QVERIFY(!failed->error.isEmpty());
}

void TestTrustSafetyRunner::async_scan_emits_items_then_finished()
{
    FakeProvider provider;
    provider.discoverable = {makeItem("a", 1), makeItem("b", 2)};

    TrustSafetyRunner runner(&provider);
    QSignalSpy discoveredSpy(&runner, &TrustSafetyRunner::itemDiscovered);
    QSignalSpy finishedSpy(&runner, &TrustSafetyRunner::scanFinished);

    runner.startScan();

    QVERIFY(finishedSpy.wait(10000));
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(discoveredSpy.count(), 2);

    QList<TrustSafetyActionItem> items = finishedSpy.takeFirst().at(0).value<QList<TrustSafetyActionItem>>();
    QCOMPARE(items.size(), 2);
}

QTEST_MAIN(TestTrustSafetyRunner)
#include "test_trust_safety_runner.moc"
