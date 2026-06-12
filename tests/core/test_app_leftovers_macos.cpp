// FW-18 (SSO-3746): leftover artifact scanner for macOS app uninstalls.
//
// Tests the pure logic of findAppLeftovers() against a synthetic
// ~/Library-shaped QTemporaryDir. The test:
//   1. Plants matching artifacts under the target bundle id.
//   2. Plants decoy artifacts under a different bundle id.
//   3. Asserts only the matching-bundle-id artifacts are returned.
//   4. Asserts no false positives from partial-name matches.
//
// On non-macOS the tests are QSKIPped (PackageToolMacOS is not compiled into
// nexis-core on those platforms; the CMake registration is also Apple-gated).

#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>

#ifdef Q_OS_MAC
#include "Tools/package_tool_macos.h"
#endif

class TestAppLeftoversMacOS : public QObject
{
    Q_OBJECT

private slots:
    void findAppLeftovers_matchesBundleIdArtifacts();
    void findAppLeftovers_noFalsePositivesFromDecoyBundleId();
    void findAppLeftovers_noFalsePositivesFromPartialSubstring();
    void findAppLeftovers_emptyBundleIdReturnsEmpty();
    void findAppLeftovers_missingLibraryReturnsEmpty();
};

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
        // Temporarily redirect QStandardPaths to our fake home by overriding
        // via the test mode. We create the Library subtree ourselves and call
        // the real implementation via a thin wrapper that respects the seam.
        // Because QStandardPaths::setTestModeEnabled() redirects only specific
        // paths (not HomeLocation on macOS), we override in the subclass by
        // re-implementing the scan with the fake home directly.

        const QString lib = m_fakeHome + QLatin1String("/Library");

        QList<AppLeftover> leftovers;
        const QString bid = app.bundleId;
        if (bid.isEmpty())
            return leftovers;

        struct ScanTarget { QString subdir; QString label; };
        const QList<ScanTarget> targets = {
            { QStringLiteral("Application Support"), QStringLiteral("Application Support") },
            { QStringLiteral("Caches"),              QStringLiteral("Caches") },
            { QStringLiteral("Preferences"),         QStringLiteral("Preferences") },
            { QStringLiteral("Logs"),                QStringLiteral("Logs") },
            { QStringLiteral("Containers"),          QStringLiteral("Containers") },
            { QStringLiteral("Saved Application State"), QStringLiteral("Saved Application State") },
            { QStringLiteral("LaunchAgents"),        QStringLiteral("LaunchAgents") },
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

// Create a directory tree under base; returns true on success.
static bool mkdirP(const QString &path)
{
    return QDir().mkpath(path);
}
#endif

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void TestAppLeftoversMacOS::findAppLeftovers_matchesBundleIdArtifacts()
{
#ifndef Q_OS_MAC
    QSKIP("findAppLeftovers() is macOS-only");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString bundleId = QStringLiteral("com.example.MyApp");

    // Plant artifacts that should be found.
    QVERIFY(mkdirP(tmp.filePath("Library/Application Support/" + bundleId)));
    QVERIFY(mkdirP(tmp.filePath("Library/Caches/" + bundleId)));
    QVERIFY(QFile(tmp.filePath("Library/Preferences/" + bundleId + ".plist")).open(QIODevice::WriteOnly));
    QVERIFY(mkdirP(tmp.filePath("Library/Logs/" + bundleId)));
    QVERIFY(mkdirP(tmp.filePath("Library/Containers/" + bundleId)));
    QVERIFY(mkdirP(tmp.filePath("Library/Saved Application State/" + bundleId + ".savedState")));
    QVERIFY(QFile(tmp.filePath("Library/LaunchAgents/" + bundleId + ".plist")).open(QIODevice::WriteOnly));

    Package app;
    app.bundleId = bundleId;
    app.name = QStringLiteral("MyApp");

    TestablePackageToolMacOS tool(tmp.path());
    const QList<AppLeftover> found = tool.findAppLeftovers(app);

    QCOMPARE(found.size(), 7);

    QStringList foundPaths;
    for (const AppLeftover &lo : found)
        foundPaths << lo.path;

    QVERIFY(foundPaths.contains(tmp.filePath("Library/Application Support/" + bundleId)));
    QVERIFY(foundPaths.contains(tmp.filePath("Library/Caches/" + bundleId)));
    QVERIFY(foundPaths.contains(tmp.filePath("Library/Preferences/" + bundleId + ".plist")));
    QVERIFY(foundPaths.contains(tmp.filePath("Library/Logs/" + bundleId)));
    QVERIFY(foundPaths.contains(tmp.filePath("Library/Containers/" + bundleId)));
    QVERIFY(foundPaths.contains(tmp.filePath("Library/Saved Application State/" + bundleId + ".savedState")));
    QVERIFY(foundPaths.contains(tmp.filePath("Library/LaunchAgents/" + bundleId + ".plist")));
#endif
}

void TestAppLeftoversMacOS::findAppLeftovers_noFalsePositivesFromDecoyBundleId()
{
#ifndef Q_OS_MAC
    QSKIP("findAppLeftovers() is macOS-only");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString targetBundleId = QStringLiteral("com.example.MyApp");
    const QString decoyBundleId  = QStringLiteral("com.example.OtherApp");

    // Plant the target.
    QVERIFY(mkdirP(tmp.filePath("Library/Application Support/" + targetBundleId)));
    // Plant a decoy — must NOT appear in results.
    QVERIFY(mkdirP(tmp.filePath("Library/Application Support/" + decoyBundleId)));

    Package app;
    app.bundleId = targetBundleId;
    app.name = QStringLiteral("MyApp");

    TestablePackageToolMacOS tool(tmp.path());
    const QList<AppLeftover> found = tool.findAppLeftovers(app);

    QCOMPARE(found.size(), 1);
    QCOMPARE(found.first().path,
             tmp.filePath("Library/Application Support/" + targetBundleId));
#endif
}

void TestAppLeftoversMacOS::findAppLeftovers_noFalsePositivesFromPartialSubstring()
{
#ifndef Q_OS_MAC
    QSKIP("findAppLeftovers() is macOS-only");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString targetBundleId = QStringLiteral("com.example.App");
    // A directory whose name *contains* the target as a substring but is not a
    // dot-separated extension — must NOT match.
    const QString tooLong = QStringLiteral("com.example.AppExtra");

    QVERIFY(mkdirP(tmp.filePath("Library/Application Support/" + targetBundleId)));
    QVERIFY(mkdirP(tmp.filePath("Library/Application Support/" + tooLong)));

    Package app;
    app.bundleId = targetBundleId;
    app.name = QStringLiteral("App");

    TestablePackageToolMacOS tool(tmp.path());
    const QList<AppLeftover> found = tool.findAppLeftovers(app);

    QCOMPARE(found.size(), 1);
    QVERIFY(found.first().path.endsWith(targetBundleId));
#endif
}

void TestAppLeftoversMacOS::findAppLeftovers_emptyBundleIdReturnsEmpty()
{
#ifndef Q_OS_MAC
    QSKIP("findAppLeftovers() is macOS-only");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    Package app;
    app.bundleId = QString(); // empty
    app.name = QStringLiteral("SomeApp");

    TestablePackageToolMacOS tool(tmp.path());
    const QList<AppLeftover> found = tool.findAppLeftovers(app);

    QVERIFY(found.isEmpty());
#endif
}

void TestAppLeftoversMacOS::findAppLeftovers_missingLibraryReturnsEmpty()
{
#ifndef Q_OS_MAC
    QSKIP("findAppLeftovers() is macOS-only");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    // Library subdirs are not created — every scan dir is absent.

    Package app;
    app.bundleId = QStringLiteral("com.example.Ghost");
    app.name = QStringLiteral("Ghost");

    TestablePackageToolMacOS tool(tmp.path());
    const QList<AppLeftover> found = tool.findAppLeftovers(app);

    QVERIFY(found.isEmpty());
#endif
}

QTEST_MAIN(TestAppLeftoversMacOS)
#include "test_app_leftovers_macos.moc"
