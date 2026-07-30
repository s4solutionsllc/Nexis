// SSO-15567: incremental (async) leftover size scan — CrumbsScanner.
//
// CrumbsScanner::scanCrumbs() previously walked all ~/Library/* roots and
// returned one final QList<CrumbCandidate>, so CrumbsReviewDialog blocked
// then populated all at once. It now streams matches via an itemFoundCb as
// they're discovered (per scan-location), mirroring TrustSafetyRunner
// (SSO-15380). These tests exercise the pure, synchronous
// scanCrumbsUnderHome() seam directly — no thread/QApplication needed —
// covering:
//   • itemFoundCb fires once per match, in discovery order, before the
//     final sort.
//   • The final returned list is unaffected by streaming: same matches,
//     same sizes, same sort-by-size-desc order as calling with no callback.
//   • Cancellation (QAtomicInt) stops the walk early and yields a partial,
//     still-consistent result.
//
// On non-macOS the tests are QSKIPped (CrumbsScanner is not compiled into
// nexis-core on those platforms; the CMake registration is also Apple-gated).

#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>

#ifdef Q_OS_MAC
#include "Tools/crumbs_scanner.h"
#endif

class TestCrumbsScannerMacOS : public QObject
{
    Q_OBJECT

private slots:
    void scanCrumbs_streamsEachMatchViaCallback();
    void scanCrumbs_callbackOrderMatchesDiscoveryNotFinalSort();
    void scanCrumbs_finalResultUnaffectedByStreaming();
    void scanCrumbs_emptyBundleIdsReturnsEmptyAndNoCallbacks();
    void scanCrumbs_cancellationStopsEarly();
};

#ifdef Q_OS_MAC
namespace {

bool mkdirP(const QString &path) { return QDir().mkpath(path); }

bool writeFile(const QString &path, qint64 bytes)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    if (bytes > 0)
        f.write(QByteArray(static_cast<int>(bytes), 'x'));
    return true;
}

} // namespace
#endif

void TestCrumbsScannerMacOS::scanCrumbs_streamsEachMatchViaCallback()
{
#ifndef Q_OS_MAC
    QSKIP("CrumbsScanner is macOS-only");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString bid = QStringLiteral("com.example.MyApp");
    QVERIFY(mkdirP(tmp.filePath("Library/Application Support/" + bid)));
    QVERIFY(mkdirP(tmp.filePath("Library/Caches/" + bid)));
    QVERIFY(mkdirP(tmp.filePath("Library/Preferences")));
    QVERIFY(writeFile(tmp.filePath("Library/Preferences/" + bid + ".plist"), 128));

    QList<CrumbsScanner::CrumbCandidate> streamed;
    const auto found = CrumbsScanner::scanCrumbsUnderHome(
        tmp.path(), { bid }, nullptr,
        [&](const CrumbsScanner::CrumbCandidate &c) { streamed.append(c); });

    // Every match reported via the callback, and nothing extra.
    QCOMPARE(streamed.size(), 3);
    QCOMPARE(found.size(), 3);
#endif
}

void TestCrumbsScannerMacOS::scanCrumbs_callbackOrderMatchesDiscoveryNotFinalSort()
{
#ifndef Q_OS_MAC
    QSKIP("CrumbsScanner is macOS-only");
#else
    // Plant matches with sizes in the OPPOSITE order of the final
    // sort-by-size-desc so the test would fail if the callback silently
    // received the already-sorted list instead of discovery order.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString bid = QStringLiteral("com.example.Ordered");
    // Preferences is walked first (searchRoots() order) and gets the
    // SMALLEST file; Caches is walked later and gets the LARGEST.
    QVERIFY(mkdirP(tmp.filePath("Library/Preferences")));
    QVERIFY(writeFile(tmp.filePath("Library/Preferences/" + bid + ".plist"), 16));
    QVERIFY(mkdirP(tmp.filePath("Library/Caches/" + bid)));
    QVERIFY(writeFile(tmp.filePath("Library/Caches/" + bid + "/big.bin"), 4096));

    QStringList streamedPaths;
    const auto found = CrumbsScanner::scanCrumbsUnderHome(
        tmp.path(), { bid }, nullptr,
        [&](const CrumbsScanner::CrumbCandidate &c) { streamedPaths << c.path; });

    QCOMPARE(streamedPaths.size(), 2);
    // Discovery order: Preferences (small) before Caches (large).
    QVERIFY(streamedPaths.first().contains(QLatin1String("Preferences")));
    QVERIFY(streamedPaths.last().contains(QLatin1String("Caches")));

    // Final result order: Caches (large) before Preferences (small).
    QCOMPARE(found.size(), 2);
    QVERIFY(found.first().path.contains(QLatin1String("Caches")));
    QVERIFY(found.last().path.contains(QLatin1String("Preferences")));
#endif
}

void TestCrumbsScannerMacOS::scanCrumbs_finalResultUnaffectedByStreaming()
{
#ifndef Q_OS_MAC
    QSKIP("CrumbsScanner is macOS-only");
#else
    // AC: "No change to final result correctness (same matches as the
    // synchronous version)". Compare the callback-driven result against a
    // call with no callback at all.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString bid = QStringLiteral("com.example.Compare");
    QVERIFY(mkdirP(tmp.filePath("Library/Application Support/" + bid)));
    QVERIFY(writeFile(tmp.filePath("Library/Application Support/" + bid + "/a"), 10));
    QVERIFY(mkdirP(tmp.filePath("Library/Caches/" + bid)));
    QVERIFY(writeFile(tmp.filePath("Library/Caches/" + bid + "/b"), 999));
    QVERIFY(mkdirP(tmp.filePath("Library/Logs/" + bid)));

    const auto withoutCallback =
        CrumbsScanner::scanCrumbsUnderHome(tmp.path(), { bid });
    const auto withCallback =
        CrumbsScanner::scanCrumbsUnderHome(tmp.path(), { bid }, nullptr,
            [](const CrumbsScanner::CrumbCandidate &) {});

    QCOMPARE(withCallback.size(), withoutCallback.size());
    for (int i = 0; i < withoutCallback.size(); ++i) {
        QCOMPARE(withCallback.at(i).path, withoutCallback.at(i).path);
        QCOMPARE(withCallback.at(i).sizeBytes, withoutCallback.at(i).sizeBytes);
    }
    // Sort-by-size-desc preserved.
    for (int i = 1; i < withCallback.size(); ++i)
        QVERIFY(withCallback.at(i - 1).sizeBytes >= withCallback.at(i).sizeBytes);
#endif
}

void TestCrumbsScannerMacOS::scanCrumbs_emptyBundleIdsReturnsEmptyAndNoCallbacks()
{
#ifndef Q_OS_MAC
    QSKIP("CrumbsScanner is macOS-only");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    int callbackCount = 0;
    const auto found = CrumbsScanner::scanCrumbsUnderHome(
        tmp.path(), {}, nullptr,
        [&](const CrumbsScanner::CrumbCandidate &) { ++callbackCount; });

    QVERIFY(found.isEmpty());
    QCOMPARE(callbackCount, 0);
#endif
}

void TestCrumbsScannerMacOS::scanCrumbs_cancellationStopsEarly()
{
#ifndef Q_OS_MAC
    QSKIP("CrumbsScanner is macOS-only");
#else
    // Plant matches across multiple roots, then cancel from inside the
    // callback after the first hit — the walk must stop before visiting
    // later roots, proving async callers can actually abort a long scan
    // instead of just dismissing the UI.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString bid = QStringLiteral("com.example.Cancel");
    QVERIFY(mkdirP(tmp.filePath("Library/Preferences")));
    QVERIFY(writeFile(tmp.filePath("Library/Preferences/" + bid + ".plist"), 1));
    QVERIFY(mkdirP(tmp.filePath("Library/Application Support/" + bid)));
    QVERIFY(mkdirP(tmp.filePath("Library/Caches/" + bid)));

    QAtomicInt cancelled{0};
    int callbackCount = 0;
    const auto found = CrumbsScanner::scanCrumbsUnderHome(
        tmp.path(), { bid }, &cancelled,
        [&](const CrumbsScanner::CrumbCandidate &) {
            ++callbackCount;
            cancelled.storeRelaxed(1);
        });

    QCOMPARE(callbackCount, 1);
    QCOMPARE(found.size(), 1);
#endif
}

QTEST_MAIN(TestCrumbsScannerMacOS)
#include "test_crumbs_scanner_macos.moc"
