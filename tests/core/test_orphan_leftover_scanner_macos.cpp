// SSO-15386 T3 (SSO-15373 §5): multi-signal orphan-leftover scanner — macOS.
//
// Tests PackageToolMacOS::findOrphanLeftovers() via a TestablePackageToolMacOS
// subclass that reimplements the scan rooted at a QTemporaryDir instead of
// ~/Library, mirroring the TestablePackageToolMacOS pattern already used in
// test_app_leftovers_macos.cpp. Unlike findAppLeftovers() (matched against a
// known-just-uninstalled bundle id), there is no ground truth here, so a
// result is only reported when >= 3 of 4 independent signals corroborate
// (CISO higher-confidence bar, see package_tool_shared.h OrphanSignal /
// OrphanLeftover doc comments).
//
// Covers:
//   • Multi-signal corroboration (4-of-4 and 3-of-4 corroborating cases)
//   • Single- and two-signal cases that must NOT match (confidence bar)
//   • Deny-list (T1) cross-check — a would-be 4-signal match is still
//     excluded when LifecycleDenyList::isSafe() rejects it
//   • True-negative: an app that is still installed is never flagged,
//     regardless of staleness
//   • Adversarial near-miss from the SSO-15387 fixture corpus (shared
//     vendor/group-container dir) — documented as a known gap via QWARN,
//     same convention as tests/core/test_orphan_scan_fixtures.cpp
//   • Empty ~/Library → empty result
//
// On non-macOS the tests are QSKIPped (PackageToolMacOS is not compiled into
// nexis-core on those platforms; the CMake registration is also Apple-gated).

#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QDateTime>
#include <QStringList>

#ifdef Q_OS_MAC
#include "Tools/package_tool_macos.h"
#include "Tools/lifecycle_deny_list.h"
#include <QRegularExpression>
#include <sys/stat.h>
#include <sys/time.h>
#endif

class TestOrphanLeftoverScannerMacOS : public QObject
{
    Q_OBJECT

private slots:
    void findOrphanLeftovers_fourSignalMatchIncluded();
    void findOrphanLeftovers_threeSignalMatchIncluded();
    void findOrphanLeftovers_singleSignalNotIncluded();
    void findOrphanLeftovers_twoSignalsNotIncluded();
    void findOrphanLeftovers_installedAppNeverFlaggedRegardlessOfAge();
    void findOrphanLeftovers_denyListBlocksAppleLeafDespiteFourSignals();
    void findOrphanLeftovers_sharedVendorDirNearMissDocumented();
    void findOrphanLeftovers_emptyLibraryReturnsEmpty();
};

// ---------------------------------------------------------------------------
// Helper: subclass PackageToolMacOS to redirect the scan root to a temp dir
// and inject a synthetic installed-bundle-id set instead of enumerating
// /Applications. Reimplements the same algorithm as
// macos/nexis-core/Tools/package_tool.cpp so this file has no dependency on
// the anonymous-namespace helpers there (same tradeoff already accepted by
// TestablePackageToolMacOS in test_app_leftovers_macos.cpp).
// ---------------------------------------------------------------------------
#ifdef Q_OS_MAC
class TestablePackageToolMacOS : public PackageToolMacOS
{
public:
    TestablePackageToolMacOS(const QString &fakeHome, const QSet<QString> &installedBundleIds)
        : m_fakeHome(fakeHome), m_installedBundleIds(installedBundleIds) {}

    QList<Package> getInstalledApps() override
    {
        QList<Package> apps;
        for (const QString &bid : m_installedBundleIds) {
            Package p;
            p.bundleId = bid;
            p.name = bid;
            apps.append(p);
        }
        return apps;
    }

    QList<OrphanLeftover> findOrphanLeftovers() override
    {
        const QString lib = m_fakeHome + QLatin1String("/Library");

        struct ScanTarget { QString path; QString label; };
        const QList<ScanTarget> targets = {
            { lib + QLatin1String("/Application Support"),     QStringLiteral("Application Support") },
            { lib + QLatin1String("/Caches"),                  QStringLiteral("Caches") },
            { lib + QLatin1String("/Preferences"),              QStringLiteral("Preferences") },
            { lib + QLatin1String("/Containers"),               QStringLiteral("Containers") },
            { lib + QLatin1String("/Saved Application State"), QStringLiteral("Saved Application State") },
            { lib + QLatin1String("/Logs"),                     QStringLiteral("Logs") },
            { lib + QLatin1String("/LaunchAgents"),             QStringLiteral("LaunchAgents") },
        };

        QList<OrphanLeftover> result;
        for (const ScanTarget &target : targets) {
            QDir dir(target.path);
            if (!dir.exists())
                continue;
            const QFileInfoList entries = dir.entryInfoList(
                QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
            for (const QFileInfo &entry : entries)
                evaluate(result, entry);
        }
        return result;
    }

private:
    QString m_fakeHome;
    QSet<QString> m_installedBundleIds;

    static QString stripKnownSuffix(const QString &name)
    {
        if (name.endsWith(QLatin1String(".plist")))
            return name.chopped(6);
        if (name.endsWith(QLatin1String(".savedState")))
            return name.chopped(11);
        return name;
    }

    static bool looksLikeBundleId(const QString &name)
    {
        static const QRegularExpression re(
            QStringLiteral("^[A-Za-z][A-Za-z0-9-]*(\\.[A-Za-z][A-Za-z0-9-]*){2,}$"));
        return re.match(name).hasMatch();
    }

    void evaluate(QList<OrphanLeftover> &out, const QFileInfo &entry) const
    {
        const QString baseName = stripKnownSuffix(entry.fileName());
        const bool hasInstalledApp = m_installedBundleIds.contains(baseName);

        QList<OrphanSignal> detectedSignals;
        if (!hasInstalledApp)
            detectedSignals.append({QStringLiteral("no_installed_app"), QString()});
        if (looksLikeBundleId(baseName) && !hasInstalledApp)
            detectedSignals.append({QStringLiteral("naming_convention"), QString()});

        const QDateTime modified = entry.lastModified();
        const QDateTime accessed = entry.lastRead();
        const QDateTime now = QDateTime::currentDateTime();
        if (modified.isValid() && modified.daysTo(now) >= 30)
            detectedSignals.append({QStringLiteral("age_threshold"), QString()});
        if (accessed.isValid() && accessed.daysTo(now) >= 7)
            detectedSignals.append({QStringLiteral("not_recently_accessed"), QString()});

        if (detectedSignals.size() < 3)
            return;

        const QString canonical = entry.canonicalFilePath();
        const QString resolvedCanonical = canonical.isEmpty() ? entry.absoluteFilePath() : canonical;
        if (!LifecycleDenyList::isSafe(resolvedCanonical, m_fakeHome))
            return;

        OrphanLeftover leftover;
        leftover.path = entry.absoluteFilePath();
        leftover.canonicalPath = resolvedCanonical;
        leftover.category = QString();
        leftover.size = 0;
        leftover.matchedSignals = detectedSignals;
        leftover.confidenceScore = detectedSignals.size();
        leftover.lastModified = modified;
        leftover.lastAccessed = accessed;
        out.append(leftover);
    }
};

static bool mkdirP(const QString &path) { return QDir().mkpath(path); }

// QFile::setFileTime() requires an open file handle and does not work on
// directories. Use POSIX utimes() so backdate() works on both files and dirs.
static bool backdate(const QString &path, const QDateTime &dt, QFileDevice::FileTime which)
{
    const QByteArray nativePath = path.toLocal8Bit();
    struct stat st;
    if (::stat(nativePath.constData(), &st) != 0)
        return false;
    struct timeval times[2];
    times[0].tv_sec = st.st_atime;  times[0].tv_usec = 0;
    times[1].tv_sec = st.st_mtime;  times[1].tv_usec = 0;
    const time_t ts = static_cast<time_t>(dt.toSecsSinceEpoch());
    if (which == QFileDevice::FileAccessTime)
        times[0].tv_sec = ts;
    else
        times[1].tv_sec = ts;
    return ::utimes(nativePath.constData(), times) == 0;
}

static const OrphanLeftover *findByPathSuffix(const QList<OrphanLeftover> &list, const QString &suffix)
{
    for (const OrphanLeftover &o : list) {
        if (o.path.endsWith(suffix))
            return &o;
    }
    return nullptr;
}

static bool hasSignal(const OrphanLeftover &o, const QString &ruleId)
{
    for (const OrphanSignal &s : o.matchedSignals) {
        if (s.ruleId == ruleId)
            return true;
    }
    return false;
}
#endif // Q_OS_MAC

// ---------------------------------------------------------------------------

void TestOrphanLeftoverScannerMacOS::findOrphanLeftovers_fourSignalMatchIncluded()
{
#ifndef Q_OS_MAC
    QSKIP("findOrphanLeftovers() is macOS-only");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString dirPath = tmp.filePath("Library/Application Support/com.old.RemovedApp");
    QVERIFY(mkdirP(dirPath));

    const QDateTime old = QDateTime::currentDateTime().addDays(-45);
    QVERIFY(backdate(dirPath, old, QFileDevice::FileModificationTime));
    QVERIFY(backdate(dirPath, old, QFileDevice::FileAccessTime));

    TestablePackageToolMacOS tool(tmp.path(), { QStringLiteral("com.current.LiveApp") });
    const QList<OrphanLeftover> result = tool.findOrphanLeftovers();

    const OrphanLeftover *match = findByPathSuffix(result, QStringLiteral("com.old.RemovedApp"));
    QVERIFY(match);
    QCOMPARE(match->confidenceScore, 4);
    QVERIFY(hasSignal(*match, QStringLiteral("no_installed_app")));
    QVERIFY(hasSignal(*match, QStringLiteral("naming_convention")));
    QVERIFY(hasSignal(*match, QStringLiteral("age_threshold")));
    QVERIFY(hasSignal(*match, QStringLiteral("not_recently_accessed")));
#endif
}

void TestOrphanLeftoverScannerMacOS::findOrphanLeftovers_threeSignalMatchIncluded()
{
#ifndef Q_OS_MAC
    QSKIP("findOrphanLeftovers() is macOS-only");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Old mtime, but recently accessed — 3 of 4 signals (no age-of-access
    // signal) still clears the confidence bar.
    const QString dirPath = tmp.filePath("Library/Caches/com.stale.CacheApp");
    QVERIFY(mkdirP(dirPath));
    QVERIFY(backdate(dirPath, QDateTime::currentDateTime().addDays(-45), QFileDevice::FileModificationTime));

    TestablePackageToolMacOS tool(tmp.path(), {});
    const QList<OrphanLeftover> result = tool.findOrphanLeftovers();

    const OrphanLeftover *match = findByPathSuffix(result, QStringLiteral("com.stale.CacheApp"));
    QVERIFY(match);
    QCOMPARE(match->confidenceScore, 3);
    QVERIFY(hasSignal(*match, QStringLiteral("age_threshold")));
    QVERIFY(!hasSignal(*match, QStringLiteral("not_recently_accessed")));
#endif
}

void TestOrphanLeftoverScannerMacOS::findOrphanLeftovers_singleSignalNotIncluded()
{
#ifndef Q_OS_MAC
    QSKIP("findOrphanLeftovers() is macOS-only");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Generic, non-bundle-id-shaped name: only "no_installed_app" can fire.
    const QString dirPath = tmp.filePath("Library/Application Support/OldBackupFolder");
    QVERIFY(mkdirP(dirPath));

    TestablePackageToolMacOS tool(tmp.path(), {});
    const QList<OrphanLeftover> result = tool.findOrphanLeftovers();

    QVERIFY(!findByPathSuffix(result, QStringLiteral("OldBackupFolder")));
#endif
}

void TestOrphanLeftoverScannerMacOS::findOrphanLeftovers_twoSignalsNotIncluded()
{
#ifndef Q_OS_MAC
    QSKIP("findOrphanLeftovers() is macOS-only");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Bundle-id-shaped, not installed, but freshly touched (no temporal
    // signals) — a recently-uninstalled app's leftover shouldn't be flagged
    // yet; that's exactly what the temporal signals are for.
    const QString dirPath = tmp.filePath("Library/Application Support/com.justremoved.FreshApp");
    QVERIFY(mkdirP(dirPath));

    TestablePackageToolMacOS tool(tmp.path(), {});
    const QList<OrphanLeftover> result = tool.findOrphanLeftovers();

    QVERIFY(!findByPathSuffix(result, QStringLiteral("com.justremoved.FreshApp")));
#endif
}

void TestOrphanLeftoverScannerMacOS::findOrphanLeftovers_installedAppNeverFlaggedRegardlessOfAge()
{
#ifndef Q_OS_MAC
    QSKIP("findOrphanLeftovers() is macOS-only");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString dirPath = tmp.filePath("Library/Application Support/com.current.LiveApp");
    QVERIFY(mkdirP(dirPath));
    const QDateTime old = QDateTime::currentDateTime().addDays(-90);
    QVERIFY(backdate(dirPath, old, QFileDevice::FileModificationTime));
    QVERIFY(backdate(dirPath, old, QFileDevice::FileAccessTime));

    TestablePackageToolMacOS tool(tmp.path(), { QStringLiteral("com.current.LiveApp") });
    const QList<OrphanLeftover> result = tool.findOrphanLeftovers();

    // Exact bundle-id match to an installed app caps confidence at 2
    // (age + access only) — never reaches the 3-signal bar.
    QVERIFY(!findByPathSuffix(result, QStringLiteral("com.current.LiveApp")));
#endif
}

void TestOrphanLeftoverScannerMacOS::findOrphanLeftovers_denyListBlocksAppleLeafDespiteFourSignals()
{
#ifndef Q_OS_MAC
    QSKIP("findOrphanLeftovers() is macOS-only");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Would-be 4-signal match, but LifecycleDenyList::isSafe() unconditionally
    // rejects any "com.apple.*" leaf name (SSO-15373 §2) — the T1 cross-check
    // must win regardless of signal strength.
    const QString dirPath = tmp.filePath("Library/Application Support/com.apple.SomeOldHelper");
    QVERIFY(mkdirP(dirPath));
    const QDateTime old = QDateTime::currentDateTime().addDays(-60);
    QVERIFY(backdate(dirPath, old, QFileDevice::FileModificationTime));
    QVERIFY(backdate(dirPath, old, QFileDevice::FileAccessTime));

    TestablePackageToolMacOS tool(tmp.path(), {});
    const QList<OrphanLeftover> result = tool.findOrphanLeftovers();

    QVERIFY(!findByPathSuffix(result, QStringLiteral("com.apple.SomeOldHelper")));
#endif
}

void TestOrphanLeftoverScannerMacOS::findOrphanLeftovers_sharedVendorDirNearMissDocumented()
{
#ifndef Q_OS_MAC
    QSKIP("findOrphanLeftovers() is macOS-only");
#else
    // SSO-15387 near-miss fixture: "com.adobe.shared" is a group container
    // still used by an installed app ("com.adobe.Photoshop2025"), but the
    // exact-bundle-id-match algorithm has no way to know that — this is the
    // same documented gap as
    // TestOrphanScanFixtures::macos_sharedVendorDirNotReturnedWhenStillInstalled
    // in test_orphan_scan_fixtures.cpp. SSO-15386 needs a post-MVP
    // vendor-group suppression allowlist to close it; until then we document
    // rather than hide the gap.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString photoshopDir = tmp.filePath("Library/Application Support/com.adobe.Photoshop2025");
    const QString sharedDir = tmp.filePath("Library/Application Support/com.adobe.shared");
    QVERIFY(mkdirP(photoshopDir));
    QVERIFY(mkdirP(sharedDir));

    const QDateTime old = QDateTime::currentDateTime().addDays(-45);
    QVERIFY(backdate(sharedDir, old, QFileDevice::FileModificationTime));
    QVERIFY(backdate(sharedDir, old, QFileDevice::FileAccessTime));

    TestablePackageToolMacOS tool(tmp.path(), { QStringLiteral("com.adobe.Photoshop2025") });
    const QList<OrphanLeftover> result = tool.findOrphanLeftovers();

    // The still-installed app itself must never be flagged.
    QVERIFY(!findByPathSuffix(result, QStringLiteral("com.adobe.Photoshop2025")));

    if (findByPathSuffix(result, QStringLiteral("com.adobe.shared"))) {
        QWARN("Near-miss: com.adobe.shared flagged as orphan despite Photoshop2025 "
              "still installed — SSO-15386 needs vendor-group suppression");
    }
#endif
}

void TestOrphanLeftoverScannerMacOS::findOrphanLeftovers_emptyLibraryReturnsEmpty()
{
#ifndef Q_OS_MAC
    QSKIP("findOrphanLeftovers() is macOS-only");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    TestablePackageToolMacOS tool(tmp.path(), {});
    const QList<OrphanLeftover> result = tool.findOrphanLeftovers();

    QVERIFY(result.isEmpty());
#endif
}

QTEST_MAIN(TestOrphanLeftoverScannerMacOS)
#include "test_orphan_leftover_scanner_macos.moc"
