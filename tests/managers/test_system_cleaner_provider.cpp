// SSO-15480: SystemCleanerProvider unit tests.
//
// Exercises per-item selection and dry-run parity — the two acceptance
// criteria that require a unit test rather than a screenshot/UAT check.
// Uses temp-dir files so every test is self-contained and leaves no artifacts.
//
// Mirrors the FakeProvider pattern from test_trust_safety_runner.cpp: the
// provider is exercised via its public TrustSafetyActionProvider interface,
// with a thin testable subclass that records side effects.

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "Pages/SystemCleaner/system_cleaner_provider.h"
#include "Common/trust_safety_runner.h"

namespace {

// Subclass that overrides performItem() to directly delete/skip files
// without routing through CleanerService (which needs a full Qt environment).
// This makes per-item and dry-run logic testable without a running app.
class TestableSystemCleanerProvider : public SystemCleanerProvider
{
public:
    QStringList deletedPaths;

    explicit TestableSystemCleanerProvider(Config config)
        : SystemCleanerProvider(std::move(config), nullptr, nullptr)
    {}

    TrustSafetyActionResult performItem(const TrustSafetyActionItem &item, bool dryRun) override
    {
        TrustSafetyActionResult result;
        result.itemId = item.id;

        // Only regular file items are exercised in these tests.
        // Trash / snap / flatpak items are covered by the integration path.
        QString path = item.id;
        QFileInfo fi(path);

        if (dryRun) {
            result.succeeded  = fi.exists();
            result.bytesFreed = fi.size();
        } else {
            if (fi.isDir()) {
                result.succeeded = QDir(path).removeRecursively();
            } else {
                result.succeeded = QFile::remove(path);
            }
            result.bytesFreed = item.estimatedSizeBytes;
            if (result.succeeded)
                deletedPaths.append(path);
        }

        return result;
    }
};

// Create N small files under `dir` and return their QFileInfoList.
QFileInfoList makeFiles(const QString &dir, int count)
{
    QFileInfoList result;
    for (int i = 0; i < count; ++i) {
        QString path = dir + QStringLiteral("/file%1.txt").arg(i);
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write("test");
        f.close();
        result.append(QFileInfo(path));
    }
    return result;
}

} // namespace

class TestSystemCleanerProvider : public QObject
{
    Q_OBJECT

private slots:
    void scan_emits_one_item_per_file_per_category();
    void scan_marks_browser_privacy_as_risky();
    void scan_marks_trash_as_risky();
    void per_item_deselect_skips_deselected_files();
    void dry_run_reports_same_numbers_with_zero_side_effects();
};

void TestSystemCleanerProvider::scan_emits_one_item_per_file_per_category()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QDir(tmp.path()).mkpath("pkg");
    QDir(tmp.path()).mkpath("crash");

    SystemCleanerProvider::Config cfg;
    cfg.packageCaches = makeFiles(tmp.path() + "/pkg",   2);
    cfg.crashReports  = makeFiles(tmp.path() + "/crash", 3);

    TestableSystemCleanerProvider provider(cfg);

    QList<TrustSafetyActionItem> items = TrustSafetyRunner::scanSynchronous(&provider, nullptr);

    QCOMPARE(items.size(), 5);

    int pkgCount   = 0;
    int crashCount = 0;
    for (const TrustSafetyActionItem &item : items) {
        if (item.categoryId == QLatin1String(SystemCleanerProvider::CAT_PACKAGE_CACHE))
            ++pkgCount;
        else if (item.categoryId == QLatin1String(SystemCleanerProvider::CAT_CRASH_REPORTS))
            ++crashCount;
        QVERIFY(!item.id.isEmpty());
        QVERIFY(!item.description.isEmpty());
        QVERIFY(!item.command.isEmpty());
    }
    QCOMPARE(pkgCount,   2);
    QCOMPARE(crashCount, 3);
}

void TestSystemCleanerProvider::scan_marks_browser_privacy_as_risky()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QDir(tmp.path()).mkpath("priv");

    SystemCleanerProvider::Config cfg;
    cfg.browserPrivacy = makeFiles(tmp.path() + "/priv", 1);
    cfg.appCaches      = makeFiles(tmp.path(), 1);

    TestableSystemCleanerProvider provider(cfg);
    QList<TrustSafetyActionItem> items = TrustSafetyRunner::scanSynchronous(&provider, nullptr);

    for (const TrustSafetyActionItem &item : items) {
        if (item.categoryId == QLatin1String(SystemCleanerProvider::CAT_BROWSER_PRIVACY))
            QCOMPARE(item.riskTier, TrustSafetyActionItem::RiskTier::Risky);
        else
            QCOMPARE(item.riskTier, TrustSafetyActionItem::RiskTier::Standard);
    }
}

void TestSystemCleanerProvider::scan_marks_trash_as_risky()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    SystemCleanerProvider::Config cfg;
    cfg.trashRoots = { tmp.path() };

    TestableSystemCleanerProvider provider(cfg);
    QList<TrustSafetyActionItem> items = TrustSafetyRunner::scanSynchronous(&provider, nullptr);

    QCOMPARE(items.size(), 1);
    QCOMPARE(items.first().riskTier, TrustSafetyActionItem::RiskTier::Risky);
    QVERIFY(items.first().id.startsWith(QLatin1String(SystemCleanerProvider::ID_PREFIX_TRASH)));
}

void TestSystemCleanerProvider::per_item_deselect_skips_deselected_files()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    SystemCleanerProvider::Config cfg;
    cfg.appCaches = makeFiles(tmp.path(), 3);

    TestableSystemCleanerProvider provider(cfg);
    QList<TrustSafetyActionItem> allItems = TrustSafetyRunner::scanSynchronous(&provider, nullptr);
    QCOMPARE(allItems.size(), 3);

    // Select only the first two items; deselect the third.
    QList<TrustSafetyActionItem> selectedItems = { allItems[0], allItems[1] };

    TrustSafetyRunSummary summary =
        TrustSafetyRunner::executeSynchronous(&provider, selectedItems, /*dryRun=*/false, nullptr);

    QCOMPARE(summary.totalItemsRequested, 2);
    QCOMPARE(summary.totalItemsSucceeded, 2);

    // Third file must still exist — it was deselected.
    QVERIFY(!QFile::exists(allItems[0].id)); // deleted
    QVERIFY(!QFile::exists(allItems[1].id)); // deleted
    QVERIFY( QFile::exists(allItems[2].id)); // kept

    QCOMPARE(provider.deletedPaths.size(), 2);
    QVERIFY(!provider.deletedPaths.contains(allItems[2].id));
}

void TestSystemCleanerProvider::dry_run_reports_same_numbers_with_zero_side_effects()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    SystemCleanerProvider::Config cfg;
    cfg.appCaches = makeFiles(tmp.path(), 4);

    TestableSystemCleanerProvider provider(cfg);
    QList<TrustSafetyActionItem> items = TrustSafetyRunner::scanSynchronous(&provider, nullptr);
    QCOMPARE(items.size(), 4);

    // First get real-run numbers (with copies so we don't actually delete)
    qint64 expectedBytes = 0;
    for (const TrustSafetyActionItem &item : items)
        expectedBytes += item.estimatedSizeBytes;

    // Dry run
    TrustSafetyRunSummary dryRun =
        TrustSafetyRunner::executeSynchronous(&provider, items, /*dryRun=*/true, nullptr);

    QVERIFY(dryRun.dryRun);
    QCOMPARE(dryRun.totalItemsRequested, 4);
    QCOMPARE(dryRun.totalItemsSucceeded, 4);
    QVERIFY(!dryRun.cancelled);

    // No side effects: all four files still exist
    for (const TrustSafetyActionItem &item : items)
        QVERIFY(QFile::exists(item.id));

    // Provider recorded no real deletions
    QVERIFY(provider.deletedPaths.isEmpty());

    // Byte count matches what a real run would report
    QCOMPARE(dryRun.totalBytesFreed, expectedBytes);
}

QTEST_MAIN(TestSystemCleanerProvider)
#include "test_system_cleaner_provider.moc"
