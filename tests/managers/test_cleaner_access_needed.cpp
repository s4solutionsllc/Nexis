// SSO-3732 / FW-05: macOS 27 silently denies cross-team app-container access.
// Without this fix the cleaner reported success and freed zero bytes. These
// tests pin two behaviours: (a) the policy-denial outcome from removeFile()
// flows through cleanFiles()/recursive walk into both the
// accessNeededDetected() signal and CleanResult::accessDeniedPaths, and (b)
// bytes for refused paths are NOT credited to totalFreed. macOS-gated because
// the TCC heuristic and the Privacy & Security deep link only apply on
// Darwin; the seam is platform-neutral but the user-facing surface is not.
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include "Managers/cleaner_service.h"
#include "Managers/setting_manager.h"

#ifdef Q_OS_MACOS

class TestableCleanerService : public CleanerService
{
public:
    TestableCleanerService() : CleanerService() {}

    // Paths whose removal should report AccessDeniedByPolicy. Tests pre-load
    // this set so the real TCC layer isn't required.
    QStringList deniedPaths;
    bool containerProbeDenied = false;
    QStringList elevatedPaths;

protected:
    FileRemoval removeFile(const QString &path) override
    {
        if (deniedPaths.contains(path))
            return FileRemoval::AccessDeniedByPolicy;
        return CleanerService::removeFile(path);
    }

    bool macOSContainerAccessProbablyDenied() const override
    {
        return containerProbeDenied;
    }

    void removeElevated(const QStringList &paths) override
    {
        // No-op record for the elevated branch; not exercised by these
        // policy-denial cases but kept here so any incidental routing
        // doesn't hit sudo.
        elevatedPaths.append(paths);
    }

    bool currentUserOwns(const QString &path) const override
    {
        Q_UNUSED(path);
        // Force the user-owned branch so removeFile() is consulted for plain
        // files at the top level (the TCC path on macOS), instead of the
        // elevated rm fallback.
        return true;
    }

    QStringList trashRoots() const override { return {}; }
};

class TestCleanerAccessNeeded : public QObject
{
    Q_OBJECT

private:
    QString writeFile(const QString &path, const QByteArray &data)
    {
        QFileInfo(path).absoluteDir().mkpath(".");
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(data);
        f.close();
        return path;
    }

private slots:
    void initTestCase();
    void cleanup();

    void deepLink_returnsMacOSPrivacyPanel();
    void message_isTranslatable_andNonEmpty();
    void cleanFiles_topLevelFile_accessDenied_signalsAndDoesNotCountBytes();
    void cleanFiles_recursiveChild_accessDenied_signalsAndDoesNotCountBytes();
    void cleanFiles_mixedSuccessAndDenied_countsOnlySuccessfulBytes();
    void cleanFiles_genericFailure_doesNotEmitAccessNeeded();
    void clean_propagatesAccessDeniedCountIntoResult();
    void scan_macOSContainerProbeDenied_emitsAccessNeededOnce();
    void scan_macOSContainerProbeOk_doesNotEmit();
};

void TestCleanerAccessNeeded::initTestCase()
{
    // Redirect QSettings/config so saveExclusions and trend persistence don't
    // touch the real user config — same pattern as test_cleaner_service.cpp.
    QStandardPaths::setTestModeEnabled(true);
}

void TestCleanerAccessNeeded::cleanup()
{
    TestableCleanerService cs;
    cs.saveExclusions({});
}

void TestCleanerAccessNeeded::deepLink_returnsMacOSPrivacyPanel()
{
    const QString link = CleanerService::accessNeededDeepLink();
    QVERIFY(!link.isEmpty());
    QVERIFY2(link.startsWith("x-apple.systempreferences:"),
             "deep link must be a System Settings URI so QDesktopServices "
             "drops the user directly into Privacy & Security");
    QVERIFY(link.contains("PrivacySecurity"));
    QVERIFY(link.contains("Privacy_AllFiles"));
}

void TestCleanerAccessNeeded::message_isTranslatable_andNonEmpty()
{
    const QString msg = CleanerService::accessNeededMessage();
    QVERIFY(!msg.isEmpty());
    QVERIFY2(msg.contains("Full Disk Access"),
             "message must call out the Privacy & Security control the user "
             "needs to toggle, not a generic 'permissions' failure");
    QVERIFY(msg.contains("Privacy"));
}

void TestCleanerAccessNeeded::cleanFiles_topLevelFile_accessDenied_signalsAndDoesNotCountBytes()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString denied = writeFile(tmp.path() + "/denied.cache", "ABCDEFGH");  // 8 bytes
    const QString ok     = writeFile(tmp.path() + "/ok.cache",     "12");        // 2 bytes

    TestableCleanerService cs;
    cs.deniedPaths << denied;

    QSignalSpy spy(&cs, &CleanerService::accessNeededDetected);
    const quint64 freed = cs.cleanFiles({denied, ok});

    // Only `ok.cache` should be credited — the TCC-denied path must NOT add
    // to totalFreed.
    QCOMPARE(freed, static_cast<quint64>(2));
    QVERIFY(QFile::exists(denied));
    QVERIFY(!QFile::exists(ok));
    QCOMPARE(cs.lastAccessDeniedCount(), 1);
    QCOMPARE(spy.count(), 1);
    const QList<QVariant> args = spy.takeFirst();
    QCOMPARE(args.size(), 2);
    QVERIFY(!args.at(0).toString().isEmpty());
    QVERIFY(args.at(1).toString().startsWith("x-apple.systempreferences:"));
}

void TestCleanerAccessNeeded::cleanFiles_recursiveChild_accessDenied_signalsAndDoesNotCountBytes()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString dir = tmp.path() + "/cache";
    QDir().mkpath(dir);
    const QString denied = writeFile(dir + "/locked.bin", "01234567");  // 8 bytes
    const QString ok     = writeFile(dir + "/free.bin",   "abc");       // 3 bytes

    TestableCleanerService cs;
    cs.deniedPaths << denied;

    QSignalSpy spy(&cs, &CleanerService::accessNeededDetected);
    const quint64 freed = cs.cleanFiles({dir});

    QCOMPARE(freed, static_cast<quint64>(3));
    QVERIFY(QFile::exists(denied));
    QVERIFY(!QFile::exists(ok));
    QCOMPARE(cs.lastAccessDeniedCount(), 1);
    QCOMPARE(spy.count(), 1);
}

void TestCleanerAccessNeeded::cleanFiles_mixedSuccessAndDenied_countsOnlySuccessfulBytes()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString a = writeFile(tmp.path() + "/a", "1234");   // 4
    const QString b = writeFile(tmp.path() + "/b", "12345");  // 5
    const QString c = writeFile(tmp.path() + "/c", "1234567");// 7

    TestableCleanerService cs;
    cs.deniedPaths << b;

    QSignalSpy spy(&cs, &CleanerService::accessNeededDetected);
    const quint64 freed = cs.cleanFiles({a, b, c});

    // a + c = 11 freed; b's 5 bytes must not be credited even though it was
    // queued before the policy check.
    QCOMPARE(freed, static_cast<quint64>(11));
    QVERIFY(QFile::exists(b));
    QVERIFY(!QFile::exists(a));
    QVERIFY(!QFile::exists(c));
    QCOMPARE(cs.lastAccessDeniedCount(), 1);
    QCOMPARE(spy.count(), 1);
}

void TestCleanerAccessNeeded::cleanFiles_genericFailure_doesNotEmitAccessNeeded()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Pre-delete the file so QFile::remove fails with ENOENT — that should be
    // tallied as a generic NotRemoved outcome, not a policy denial.
    const QString gone = tmp.path() + "/already-gone";
    QFile placeholder(gone);
    placeholder.open(QIODevice::WriteOnly);
    placeholder.write("x");
    placeholder.close();
    QFile::remove(gone);
    QVERIFY(!QFile::exists(gone));

    TestableCleanerService cs;

    QSignalSpy spy(&cs, &CleanerService::accessNeededDetected);
    const quint64 freed = cs.cleanFiles({gone});

    QCOMPARE(freed, static_cast<quint64>(0));
    QCOMPARE(cs.lastAccessDeniedCount(), 0);
    QCOMPARE(spy.count(), 0);
}

void TestCleanerAccessNeeded::clean_propagatesAccessDeniedCountIntoResult()
{
    // Direct exercise of the count contract used by clean() to populate
    // CleanResult.accessDeniedPaths. We call cleanFiles() ourselves rather
    // than the full clean() pipeline so the test stays self-contained — the
    // contract is just "the count cleanFiles() exposes via
    // lastAccessDeniedCount() is what clean() rolls up".
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString d1 = writeFile(tmp.path() + "/d1", "XX");
    const QString d2 = writeFile(tmp.path() + "/d2", "YY");
    const QString ok = writeFile(tmp.path() + "/ok", "Z");

    TestableCleanerService cs;
    cs.deniedPaths << d1 << d2;

    cs.cleanFiles({d1, d2, ok});

    QCOMPARE(cs.lastAccessDeniedCount(), 2);
}

void TestCleanerAccessNeeded::scan_macOSContainerProbeDenied_emitsAccessNeededOnce()
{
    TestableCleanerService cs;
    cs.containerProbeDenied = true;

    QSignalSpy spy(&cs, &CleanerService::accessNeededDetected);

    // Scanning APPLICATION_CACHES is the trigger point for the container
    // access probe. We don't care about the actual scan results here, just
    // that the signal fired exactly once for the macOS probe branch.
    (void)cs.scan({CleanerService::APPLICATION_CACHES});

    QCOMPARE(spy.count(), 1);
    const QList<QVariant> args = spy.first();
    QVERIFY(!args.at(0).toString().isEmpty());
    QVERIFY(args.at(1).toString().startsWith("x-apple.systempreferences:"));
}

void TestCleanerAccessNeeded::scan_macOSContainerProbeOk_doesNotEmit()
{
    TestableCleanerService cs;
    cs.containerProbeDenied = false;

    QSignalSpy spy(&cs, &CleanerService::accessNeededDetected);
    (void)cs.scan({CleanerService::APPLICATION_CACHES});
    QCOMPARE(spy.count(), 0);
}

QTEST_MAIN(TestCleanerAccessNeeded)
#include "test_cleaner_access_needed.moc"

#else  // !Q_OS_MACOS

// Non-macOS builds compile and link a placeholder so the test target keeps
// the same CMake registration on every platform. The behaviour under test is
// macOS-only — there's no TCC equivalent on Linux that this signal targets.
class TestCleanerAccessNeeded : public QObject
{
    Q_OBJECT
private slots:
    void macOS_only_placeholder() { QSKIP("CleanerService access-needed surface is macOS-only"); }
};

QTEST_MAIN(TestCleanerAccessNeeded)
#include "test_cleaner_access_needed.moc"

#endif  // Q_OS_MACOS
