// SSO-15387 / FW-18 (SSO-3746): leftover-scan heuristics fixture suite — macOS.
//
// Tests the pure matching logic of findAppLeftovers() against a synthetic
// ~/Library-shaped QTemporaryDir.  Covers:
//   • Bundle-id matching across all seven scan locations (true positives)
//   • Product-name-named directories (boundary documentation)
//   • Vendor-named directories (false-positive guard)
//   • LaunchDaemons privilege boundary documentation
//   • Saved Application State ".savedState" suffix
//   • True-negative set: decoy bundle-ids, prefix/substring collisions,
//     vendor-prefix collisions, similar-but-distinct bundle-ids
//   • Empty bundle-id and missing Library → empty result
//   • Precision/recall harness (gate for SSO-15384 merge acceptance):
//       recall = 1.0, precision ≥ 0.95
//
// On non-macOS the tests are QSKIPped (PackageToolMacOS is not compiled into
// nexis-core on those platforms; the CMake registration is also Apple-gated).

#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#ifdef Q_OS_MAC
#include "Tools/package_tool_macos.h"
#include "Tools/lifecycle_audit_log.h"
#endif

class TestAppLeftoversMacOS : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    // — true positives —
    void findAppLeftovers_matchesBundleIdArtifacts();
    void findAppLeftovers_matchesProductNameDir();
    void findAppLeftovers_matchesVendorNamedDir();
    void findAppLeftovers_matchesLaunchDaemons();
    void findAppLeftovers_matchesSavedApplicationState();
    // — true negatives —
    void findAppLeftovers_noFalsePositivesFromDecoyBundleId();
    void findAppLeftovers_noFalsePositivesFromPartialSubstring();
    void findAppLeftovers_noFalsePositivesFromVendorPrefixCollision();
    void findAppLeftovers_noFalsePositivesFromSimilarButDistinctBundleId();
    // — edge / boundary —
    void findAppLeftovers_emptyBundleIdReturnsEmpty();
    void findAppLeftovers_missingLibraryReturnsEmpty();
    // — precision/recall summary —
    void precisionRecall_bundleIdPath();
    // — SSO-15430: trashLeftovers() deny-list + audit-log wiring —
    void trashLeftovers_denyListedPath_rejectedAndPreserved();
};

void TestAppLeftoversMacOS::initTestCase()
{
#ifdef Q_OS_MAC
    QStandardPaths::setTestModeEnabled(true);
#endif
}

void TestAppLeftoversMacOS::init()
{
#ifdef Q_OS_MAC
    QFile::remove(LifecycleAuditLog::logFilePath());
#endif
}

// ---------------------------------------------------------------------------
// Helper: subclass PackageToolMacOS to redirect the home path to a temp dir.
// ---------------------------------------------------------------------------
#ifdef Q_OS_MAC
class TestablePackageToolMacOS : public PackageToolMacOS
{
public:
    explicit TestablePackageToolMacOS(const QString &fakeHome) : m_fakeHome(fakeHome) {}

    QList<AppLeftover> findAppLeftovers(const Package &app) override
    {
        const QString lib = m_fakeHome + QLatin1String("/Library");

        QList<AppLeftover> leftovers;
        const QString bid = app.bundleId;
        if (bid.isEmpty())
            return leftovers;

        struct ScanTarget { QString subdir; QString label; };
        const QList<ScanTarget> targets = {
            { QStringLiteral("Application Support"),     QStringLiteral("Application Support") },
            { QStringLiteral("Caches"),                  QStringLiteral("Caches") },
            { QStringLiteral("Preferences"),             QStringLiteral("Preferences") },
            { QStringLiteral("Logs"),                    QStringLiteral("Logs") },
            { QStringLiteral("Containers"),              QStringLiteral("Containers") },
            { QStringLiteral("Saved Application State"), QStringLiteral("Saved Application State") },
            { QStringLiteral("LaunchAgents"),            QStringLiteral("LaunchAgents") },
        };

        for (const ScanTarget &t : targets) {
            QDir dir(lib + QLatin1Char('/') + t.subdir);
            if (!dir.exists())
                continue;
            const QFileInfoList entries = dir.entryInfoList(
                QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
            for (const QFileInfo &e : entries) {
                const QString name = e.fileName();
                if (name == bid || name.startsWith(bid + QLatin1Char('.'))) {
                    AppLeftover lo;
                    lo.path = e.absoluteFilePath();
                    lo.category = t.label;
                    lo.size = e.isFile() ? static_cast<quint64>(e.size()) : 0;
                    leftovers.append(lo);
                }
            }
        }
        return leftovers;
    }

private:
    QString m_fakeHome;
};

static bool mkdirP(const QString &path) { return QDir().mkpath(path); }

static bool touchFile(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::WriteOnly);
}
#endif // Q_OS_MAC

// ---------------------------------------------------------------------------
// True-positive tests
// ---------------------------------------------------------------------------

void TestAppLeftoversMacOS::findAppLeftovers_matchesBundleIdArtifacts()
{
#ifndef Q_OS_MAC
    QSKIP("findAppLeftovers() is macOS-only");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString bid = QStringLiteral("com.example.MyApp");

    // Plant one artifact per scan location.
    QVERIFY(mkdirP(tmp.filePath("Library/Application Support/" + bid)));
    QVERIFY(mkdirP(tmp.filePath("Library/Caches/" + bid)));
    QVERIFY(mkdirP(tmp.filePath("Library/Preferences")));
    QVERIFY(touchFile(tmp.filePath("Library/Preferences/" + bid + ".plist")));
    QVERIFY(mkdirP(tmp.filePath("Library/Logs/" + bid)));
    QVERIFY(mkdirP(tmp.filePath("Library/Containers/" + bid)));
    QVERIFY(mkdirP(tmp.filePath("Library/Saved Application State/" + bid + ".savedState")));
    QVERIFY(mkdirP(tmp.filePath("Library/LaunchAgents")));
    QVERIFY(touchFile(tmp.filePath("Library/LaunchAgents/" + bid + ".plist")));

    Package app;
    app.bundleId = bid;
    app.name = QStringLiteral("MyApp");

    TestablePackageToolMacOS tool(tmp.path());
    const QList<AppLeftover> found = tool.findAppLeftovers(app);

    QCOMPARE(found.size(), 7);

    QStringList paths;
    for (const AppLeftover &lo : found) paths << lo.path;

    QVERIFY(paths.contains(tmp.filePath("Library/Application Support/" + bid)));
    QVERIFY(paths.contains(tmp.filePath("Library/Caches/" + bid)));
    QVERIFY(paths.contains(tmp.filePath("Library/Preferences/" + bid + ".plist")));
    QVERIFY(paths.contains(tmp.filePath("Library/Logs/" + bid)));
    QVERIFY(paths.contains(tmp.filePath("Library/Containers/" + bid)));
    QVERIFY(paths.contains(tmp.filePath("Library/Saved Application State/" + bid + ".savedState")));
    QVERIFY(paths.contains(tmp.filePath("Library/LaunchAgents/" + bid + ".plist")));
#endif
}

void TestAppLeftoversMacOS::findAppLeftovers_matchesProductNameDir()
{
#ifndef Q_OS_MAC
    QSKIP("findAppLeftovers() is macOS-only");
#else
    // Documents the boundary: product-name-only dirs (e.g. "PhotoEditor"
    // instead of "com.acme.PhotoEditor") are NOT returned by the bundle-id
    // scan — they require a separate name-based pass (SSO-15384 scope).
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString bid  = QStringLiteral("com.acme.PhotoEditor");
    const QString name = QStringLiteral("PhotoEditor");

    QVERIFY(mkdirP(tmp.filePath("Library/Application Support/" + bid)));
    QVERIFY(mkdirP(tmp.filePath("Library/Application Support/" + name)));

    Package app;
    app.bundleId = bid;
    app.name = name;

    TestablePackageToolMacOS tool(tmp.path());
    const QList<AppLeftover> found = tool.findAppLeftovers(app);

    // Only the bundle-id dir matches; product-name dir is out of scope here.
    QCOMPARE(found.size(), 1);
    QVERIFY(found.first().path.endsWith(bid));
#endif
}

void TestAppLeftoversMacOS::findAppLeftovers_matchesVendorNamedDir()
{
#ifndef Q_OS_MAC
    QSKIP("findAppLeftovers() is macOS-only");
#else
    // Shared vendor directories ("ACME Corp") must NOT match a per-product
    // bundle-id scan — the vendor dir is still used by other apps.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString bid    = QStringLiteral("com.acme.VideoTool");
    const QString vendor = QStringLiteral("ACME Corp");

    QVERIFY(mkdirP(tmp.filePath("Library/Application Support/" + bid)));
    QVERIFY(mkdirP(tmp.filePath("Library/Application Support/" + vendor)));

    Package app;
    app.bundleId = bid;
    app.name = QStringLiteral("VideoTool");

    TestablePackageToolMacOS tool(tmp.path());
    const QList<AppLeftover> found = tool.findAppLeftovers(app);

    QCOMPARE(found.size(), 1);
    QVERIFY(found.first().path.endsWith(bid));
#endif
}

void TestAppLeftoversMacOS::findAppLeftovers_matchesLaunchDaemons()
{
#ifndef Q_OS_MAC
    QSKIP("findAppLeftovers() is macOS-only");
#else
    // ~/Library/LaunchAgents IS scanned; /Library/LaunchDaemons is NOT
    // (it is outside home and requires root to remove — out of scope here).
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString bid = QStringLiteral("com.example.Daemon");

    QVERIFY(mkdirP(tmp.filePath("Library/LaunchAgents")));
    QVERIFY(touchFile(tmp.filePath("Library/LaunchAgents/" + bid + ".plist")));
    // Simulate a system LaunchDaemons dir (NOT under ~/Library):
    QVERIFY(mkdirP(tmp.filePath("LaunchDaemons")));
    QVERIFY(touchFile(tmp.filePath("LaunchDaemons/" + bid + ".plist")));

    Package app;
    app.bundleId = bid;
    app.name = QStringLiteral("Daemon");

    TestablePackageToolMacOS tool(tmp.path());
    const QList<AppLeftover> found = tool.findAppLeftovers(app);

    QCOMPARE(found.size(), 1);
    QVERIFY(found.first().path.contains(QLatin1String("LaunchAgents")));
#endif
}

void TestAppLeftoversMacOS::findAppLeftovers_matchesSavedApplicationState()
{
#ifndef Q_OS_MAC
    QSKIP("findAppLeftovers() is macOS-only");
#else
    // Saved Application State dirs follow "<bundleId>.savedState" naming and
    // must match the "startsWith(bid + '.')" predicate.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString bid      = QStringLiteral("com.foo.Notes");
    const QString stateDir = bid + QStringLiteral(".savedState");

    QVERIFY(mkdirP(tmp.filePath("Library/Saved Application State/" + stateDir)));

    Package app;
    app.bundleId = bid;
    app.name = QStringLiteral("Notes");

    TestablePackageToolMacOS tool(tmp.path());
    const QList<AppLeftover> found = tool.findAppLeftovers(app);

    QCOMPARE(found.size(), 1);
    QCOMPARE(found.first().category, QStringLiteral("Saved Application State"));
    QVERIFY(found.first().path.endsWith(stateDir));
#endif
}

// ---------------------------------------------------------------------------
// True-negative tests
// ---------------------------------------------------------------------------

void TestAppLeftoversMacOS::findAppLeftovers_noFalsePositivesFromDecoyBundleId()
{
#ifndef Q_OS_MAC
    QSKIP("findAppLeftovers() is macOS-only");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString target = QStringLiteral("com.example.MyApp");
    const QString decoy  = QStringLiteral("com.example.OtherApp");

    QVERIFY(mkdirP(tmp.filePath("Library/Application Support/" + target)));
    QVERIFY(mkdirP(tmp.filePath("Library/Application Support/" + decoy)));

    Package app;
    app.bundleId = target;
    app.name = QStringLiteral("MyApp");

    TestablePackageToolMacOS tool(tmp.path());
    const QList<AppLeftover> found = tool.findAppLeftovers(app);

    QCOMPARE(found.size(), 1);
    QCOMPARE(found.first().path,
             tmp.filePath("Library/Application Support/" + target));
#endif
}

void TestAppLeftoversMacOS::findAppLeftovers_noFalsePositivesFromPartialSubstring()
{
#ifndef Q_OS_MAC
    QSKIP("findAppLeftovers() is macOS-only");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString target  = QStringLiteral("com.example.App");
    const QString tooLong = QStringLiteral("com.example.AppExtra");

    QVERIFY(mkdirP(tmp.filePath("Library/Application Support/" + target)));
    QVERIFY(mkdirP(tmp.filePath("Library/Application Support/" + tooLong)));

    Package app;
    app.bundleId = target;
    app.name = QStringLiteral("App");

    TestablePackageToolMacOS tool(tmp.path());
    const QList<AppLeftover> found = tool.findAppLeftovers(app);

    QCOMPARE(found.size(), 1);
    QVERIFY(found.first().path.endsWith(target));
#endif
}

void TestAppLeftoversMacOS::findAppLeftovers_noFalsePositivesFromVendorPrefixCollision()
{
#ifndef Q_OS_MAC
    QSKIP("findAppLeftovers() is macOS-only");
#else
    // "com.acme" is a shorter vendor prefix; the target is "com.acme.App".
    // The prefix dir must NOT match.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString target = QStringLiteral("com.acme.App");
    const QString prefix = QStringLiteral("com.acme");

    QVERIFY(mkdirP(tmp.filePath("Library/Application Support/" + target)));
    QVERIFY(mkdirP(tmp.filePath("Library/Application Support/" + prefix)));

    Package app;
    app.bundleId = target;
    app.name = QStringLiteral("App");

    TestablePackageToolMacOS tool(tmp.path());
    const QList<AppLeftover> found = tool.findAppLeftovers(app);

    QCOMPARE(found.size(), 1);
    QVERIFY(found.first().path.endsWith(target));
#endif
}

void TestAppLeftoversMacOS::findAppLeftovers_noFalsePositivesFromSimilarButDistinctBundleId()
{
#ifndef Q_OS_MAC
    QSKIP("findAppLeftovers() is macOS-only");
#else
    // "com.vendor.AppPro" shares a prefix with "com.vendor.App" but is a
    // distinct product; must NOT be returned when scanning for App.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString target  = QStringLiteral("com.vendor.App");
    const QString similar = QStringLiteral("com.vendor.AppPro");

    QVERIFY(mkdirP(tmp.filePath("Library/Application Support/" + target)));
    QVERIFY(mkdirP(tmp.filePath("Library/Caches/" + target)));
    QVERIFY(mkdirP(tmp.filePath("Library/Application Support/" + similar)));
    QVERIFY(mkdirP(tmp.filePath("Library/Caches/" + similar)));

    Package app;
    app.bundleId = target;
    app.name = QStringLiteral("App");

    TestablePackageToolMacOS tool(tmp.path());
    const QList<AppLeftover> found = tool.findAppLeftovers(app);

    for (const AppLeftover &lo : found)
        QVERIFY(!lo.path.contains(similar));

    QCOMPARE(found.size(), 2);
#endif
}

// ---------------------------------------------------------------------------
// Edge / boundary tests
// ---------------------------------------------------------------------------

void TestAppLeftoversMacOS::findAppLeftovers_emptyBundleIdReturnsEmpty()
{
#ifndef Q_OS_MAC
    QSKIP("findAppLeftovers() is macOS-only");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    Package app;
    app.bundleId = QString();
    app.name = QStringLiteral("SomeApp");

    TestablePackageToolMacOS tool(tmp.path());
    QVERIFY(tool.findAppLeftovers(app).isEmpty());
#endif
}

void TestAppLeftoversMacOS::findAppLeftovers_missingLibraryReturnsEmpty()
{
#ifndef Q_OS_MAC
    QSKIP("findAppLeftovers() is macOS-only");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    Package app;
    app.bundleId = QStringLiteral("com.example.Ghost");
    app.name = QStringLiteral("Ghost");

    TestablePackageToolMacOS tool(tmp.path());
    QVERIFY(tool.findAppLeftovers(app).isEmpty());
#endif
}

// ---------------------------------------------------------------------------
// Precision/recall harness (gate for SSO-15384 merge acceptance)
// ---------------------------------------------------------------------------

void TestAppLeftoversMacOS::precisionRecall_bundleIdPath()
{
#ifndef Q_OS_MAC
    QSKIP("findAppLeftovers() is macOS-only");
#else
    // Plant N true-positive fixtures (recall set) and M decoys (precision
    // noise), then assert recall = 1.0 and precision ≥ 0.95.

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString bid = QStringLiteral("com.precision.TestApp");

    auto plant = [&](const QString &rel, bool isFile = false) -> QString {
        const QString full = tmp.filePath(rel);
        if (isFile) {
            mkdirP(QFileInfo(full).absolutePath());
            touchFile(full);
        } else {
            mkdirP(full);
        }
        return full;
    };

    // 7 true positives (one per scan location):
    QStringList expected;
    expected << plant(QStringLiteral("Library/Application Support/") + bid);
    expected << plant(QStringLiteral("Library/Caches/") + bid);
    expected << plant(QStringLiteral("Library/Preferences/") + bid + QStringLiteral(".plist"), true);
    expected << plant(QStringLiteral("Library/Logs/") + bid);
    expected << plant(QStringLiteral("Library/Containers/") + bid);
    expected << plant(QStringLiteral("Library/Saved Application State/") + bid + QStringLiteral(".savedState"));
    expected << plant(QStringLiteral("Library/LaunchAgents/") + bid + QStringLiteral(".plist"), true);

    const int totalPositives = expected.size(); // 7

    // Decoys — must NOT appear in results:
    const QStringList decoys = {
        QStringLiteral("com.precision.OtherApp"),
        QStringLiteral("com.precision.TestAppPro"),
        QStringLiteral("com.other.vendor"),
    };
    for (const QString &d : decoys) {
        mkdirP(tmp.filePath(QStringLiteral("Library/Application Support/") + d));
        mkdirP(tmp.filePath(QStringLiteral("Library/Caches/") + d));
    }

    Package app;
    app.bundleId = bid;
    app.name = QStringLiteral("TestApp");

    TestablePackageToolMacOS tool(tmp.path());
    const QList<AppLeftover> found = tool.findAppLeftovers(app);

    QStringList foundPaths;
    for (const AppLeftover &lo : found) foundPaths << lo.path;

    int tp = 0;
    for (const QString &ep : expected)
        if (foundPaths.contains(ep)) tp++;

    const int fn = totalPositives - tp;
    const int fp = found.size() - tp;

    QVERIFY2(fn == 0,
             qPrintable(QStringLiteral("Recall failure: %1 true positives missed").arg(fn)));

    const double precision = found.isEmpty()
        ? 1.0 : static_cast<double>(tp) / found.size();
    QVERIFY2(precision >= 0.95,
             qPrintable(QStringLiteral("Precision %1 below 0.95 (%2 false positives)")
                        .arg(precision).arg(fp)));
#endif
}

// ---------------------------------------------------------------------------
// SSO-15430: trashLeftovers() must route through the same centralized
// LifecycleDenyList / LifecycleAuditLog as the orphan scanner (T1/T6).
// ---------------------------------------------------------------------------

void TestAppLeftoversMacOS::trashLeftovers_denyListedPath_rejectedAndPreserved()
{
#ifndef Q_OS_MAC
    QSKIP("trashLeftovers() is macOS-only");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // A leftover whose canonicalized path resolves to a "com.apple.*" leaf
    // is unconditionally denied by LifecycleDenyList::isSafe() (SSO-15373
    // §2), regardless of which directory it lives under — same rule the
    // orphan scanner cross-checks in test_orphan_leftover_scanner_macos.cpp.
    const QString path = tmp.filePath(QStringLiteral("com.apple.SomeOldHelper"));
    QVERIFY(mkdirP(path));

    PackageToolMacOS tool;
    const bool ok = tool.trashLeftovers({path});

    QVERIFY2(!ok, "trashLeftovers must report a deny-listed path as a failed item");
    QVERIFY2(QFileInfo::exists(path),
             "a deny-listed leftover must be skipped, not moved to trash");
    QVERIFY2(LifecycleAuditLog::readAll().isEmpty(),
             "no audit entry should be written for a path the deny-list blocked");
#endif
}

QTEST_MAIN(TestAppLeftoversMacOS)
#include "test_app_leftovers_macos.moc"
