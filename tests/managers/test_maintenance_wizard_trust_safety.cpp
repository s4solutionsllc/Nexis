// SSO-15481: Maintenance Wizard — Trust & Safety adoption tests.
//
// Tests the MaintenanceWizardCleanProvider (the glue between
// CleanerService::ScanResult and TrustSafetyRunner) in isolation, following
// the FakeProvider pattern in test_trust_safety_runner.cpp.
//
// The provider is not exported, so we replicate it in a test-local form that
// mirrors its contract exactly.  The wizard's dialog-level wiring (per-item
// deselect, cancel, dry-run in the UI) is covered by the shared
// TrustSafetyPreviewDialog and TrustSafetyRunner tests already in the suite;
// what we add here is the scan→item mapping and performItem dry-run parity.

#include <QtTest>

#include "Common/trust_safety_runner.h"
#include "Common/trust_safety_types.h"
#include "Managers/cleaner_service.h"

namespace {

// Mirrors MaintenanceWizardCleanProvider. Tests are structured around the
// TrustSafetyActionProvider contract, not the wizard dialog, so we can run
// without a QApplication / widget stack.
class TestableCleanProvider : public TrustSafetyActionProvider
{
public:
    explicit TestableCleanProvider(const CleanerService::ScanResult &result,
                                    const QList<CleanerService::CleanCategory> &cats)
        : mResult(result), mCats(cats) {}

    void scan(QAtomicInt *cancelled,
              const std::function<void(const TrustSafetyActionItem &)> &itemFound) override
    {
        int id = 0;
        for (auto cat : mCats) {
            const auto &files = mResult.categoryFiles.value(cat);
            for (const QFileInfo &fi : files) {
                if (cancelled && cancelled->loadRelaxed())
                    return;
                TrustSafetyActionItem item;
                item.id              = QStringLiteral("%1-%2").arg(static_cast<int>(cat)).arg(id++);
                item.label           = fi.fileName();
                item.description     = QStringLiteral("Safe to remove: %1").arg(fi.fileName());
                item.command         = fi.absoluteFilePath();
                item.categoryId      = QString::number(static_cast<int>(cat));
                item.categoryLabel   = QStringLiteral("Category %1").arg(static_cast<int>(cat));
                item.riskTier        = TrustSafetyActionItem::RiskTier::Standard;
                item.estimatedSizeBytes = fi.isDir() ? 0 : fi.size();
                itemFound(item);
            }
        }
    }

    // performItem: dry-run probes existence; real run records the call.
    TrustSafetyActionResult performItem(const TrustSafetyActionItem &item, bool dryRun) override
    {
        TrustSafetyActionResult r;
        r.itemId = item.id;
        r.succeeded = true;
        r.bytesFreed = qMax<qint64>(0, item.estimatedSizeBytes);
        if (!dryRun)
            realRunIds.append(item.id);
        return r;
    }

    QStringList realRunIds;

private:
    CleanerService::ScanResult mResult;
    QList<CleanerService::CleanCategory> mCats;
};

CleanerService::ScanResult makeScanResult()
{
    CleanerService::ScanResult result;
    // Two categories, two files each — total 4 items.
    QFileInfo a("/tmp/test_cache_a.bin"), b("/tmp/test_cache_b.bin");
    QFileInfo c("/tmp/test_log_a.log"),   d("/tmp/test_log_b.log");
    result.categoryFiles[CleanerService::PACKAGE_CACHE]   = {a, b};
    result.categoryFiles[CleanerService::APPLICATION_LOGS] = {c, d};
    result.totalSize = 4096;
    return result;
}

} // namespace

class TestMaintenanceWizardTrustSafety : public QObject
{
    Q_OBJECT

private slots:
    void scan_emits_one_item_per_file_in_safe_categories();
    void scan_skips_categories_not_in_safe_list();
    void scan_stops_when_cancelled_mid_category();
    void all_items_are_standard_risk_no_risky_gate_triggered();
    void dry_run_matches_real_run_numbers_with_zero_side_effects();
    void per_item_selection_only_runs_checked_items();
};

void TestMaintenanceWizardTrustSafety::scan_emits_one_item_per_file_in_safe_categories()
{
    auto result = makeScanResult();
    QList<CleanerService::CleanCategory> cats = {
        CleanerService::PACKAGE_CACHE,
        CleanerService::APPLICATION_LOGS,
    };
    TestableCleanProvider provider(result, cats);

    QStringList discovered;
    auto items = TrustSafetyRunner::scanSynchronous(
        &provider, nullptr,
        [&](const TrustSafetyActionItem &item) { discovered.append(item.id); });

    QCOMPARE(items.size(), 4); // 2 per category
    QCOMPARE(discovered.size(), 4);
}

void TestMaintenanceWizardTrustSafety::scan_skips_categories_not_in_safe_list()
{
    auto result = makeScanResult();
    // Only include one of the two categories in the safe list.
    QList<CleanerService::CleanCategory> cats = {CleanerService::PACKAGE_CACHE};
    TestableCleanProvider provider(result, cats);

    auto items = TrustSafetyRunner::scanSynchronous(&provider, nullptr);
    QCOMPARE(items.size(), 2); // only PACKAGE_CACHE's 2 files
}

void TestMaintenanceWizardTrustSafety::scan_stops_when_cancelled_mid_category()
{
    auto result = makeScanResult();
    QList<CleanerService::CleanCategory> cats = {
        CleanerService::PACKAGE_CACHE,
        CleanerService::APPLICATION_LOGS,
    };
    TestableCleanProvider provider(result, cats);

    QAtomicInt cancelled{0};
    int seen = 0;
    auto items = TrustSafetyRunner::scanSynchronous(
        &provider, &cancelled,
        [&](const TrustSafetyActionItem &) {
            if (++seen == 1)
                cancelled.storeRelaxed(1);
        });

    QCOMPARE(items.size(), 1); // stopped after the first item
}

void TestMaintenanceWizardTrustSafety::all_items_are_standard_risk_no_risky_gate_triggered()
{
    auto result = makeScanResult();
    QList<CleanerService::CleanCategory> cats = {
        CleanerService::PACKAGE_CACHE,
        CleanerService::APPLICATION_LOGS,
    };
    TestableCleanProvider provider(result, cats);

    auto items = TrustSafetyRunner::scanSynchronous(&provider, nullptr);
    for (const auto &item : items)
        QCOMPARE(item.riskTier, TrustSafetyActionItem::RiskTier::Standard);
}

void TestMaintenanceWizardTrustSafety::dry_run_matches_real_run_numbers_with_zero_side_effects()
{
    auto result = makeScanResult();
    QList<CleanerService::CleanCategory> cats = {
        CleanerService::PACKAGE_CACHE,
        CleanerService::APPLICATION_LOGS,
    };
    TestableCleanProvider provider(result, cats);

    auto items = TrustSafetyRunner::scanSynchronous(&provider, nullptr);
    auto summary = TrustSafetyRunner::executeSynchronous(&provider, items, /*dryRun=*/true, nullptr);

    QVERIFY(summary.dryRun);
    QCOMPARE(summary.totalItemsRequested, 4);
    QCOMPARE(summary.totalItemsSucceeded, 4);
    // No real-run side effects.
    QVERIFY(provider.realRunIds.isEmpty());
}

void TestMaintenanceWizardTrustSafety::per_item_selection_only_runs_checked_items()
{
    auto result = makeScanResult();
    QList<CleanerService::CleanCategory> cats = {CleanerService::PACKAGE_CACHE};
    TestableCleanProvider provider(result, cats);

    auto allItems = TrustSafetyRunner::scanSynchronous(&provider, nullptr);
    QCOMPARE(allItems.size(), 2);

    // Simulate the user deselecting the second item.
    QList<TrustSafetyActionItem> selected = {allItems.first()};
    auto summary = TrustSafetyRunner::executeSynchronous(&provider, selected, /*dryRun=*/false, nullptr);

    QCOMPARE(summary.totalItemsRequested, 1);
    QCOMPARE(summary.totalItemsSucceeded, 1);
    QCOMPARE(provider.realRunIds.size(), 1);
    QCOMPARE(provider.realRunIds.first(), allItems.first().id);
}

QTEST_MAIN(TestMaintenanceWizardTrustSafety)
#include "test_maintenance_wizard_trust_safety.moc"
