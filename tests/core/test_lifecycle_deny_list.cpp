// SSO-15386 / SSO-15373 (CISO safety controls, §2): LifecycleDenyList is the
// single centralized "safe to delete" check shared by the paired-uninstaller
// leftover cleanup and the orphan-leftover scanner.
//
// Tests cover:
//   1. Hard-denied platform locations are rejected.
//   2. A path resolves through a symlink into a denied location — the
//      literal-string form must not be sufficient to slip past the check
//      (Complete Mediation: check the resolved path, not the input string).
//   3. Ordinary user-owned paths outside any denied location are allowed.
//   4. Degenerate inputs (empty string, "/") are rejected.
//
// macOS- and Linux-specific deny-list contents are exercised only on their
// respective platforms; the shared/degenerate cases run everywhere.

#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>

#include "Tools/lifecycle_deny_list.h"

class TestLifecycleDenyList : public QObject
{
    Q_OBJECT

private slots:
    void isSafe_emptyPath_isDenied();
    void isSafe_rootPath_isDenied();
    void isSafe_ordinaryUserPath_isSafe();
    void isSafe_symlinkIntoDeniedLocation_isDenied();

    // SSO-27095: the System Cleaner / CleanerML variant.
    void isSafeForCleaning_degenerateInputs_areDenied();
    void isSafeForCleaning_symlinkIntoDeniedLocation_isDenied();
    void isSafeForCleaning_danglingLeafUnderSymlinkedDeniedDir_isDenied();
    void isSafeForCleaning_ordinaryPathOutsideHome_isAllowed();
    void isSafeForCleaning_usrLocalIsNotDeniedByUsr();
    void isSafeForCleaning_deniedRootsAndCredentialStores();

#ifdef Q_OS_MAC
    void isSafe_systemDirectory_isDenied();
    void isSafe_launchDaemons_isDenied();
    void isSafe_appleBundleId_isDenied();
    void isSafe_homebrewCellar_isDenied();
    void isSafe_outsideHome_isDenied();
#endif

#ifdef Q_OS_LINUX
    void isSafe_etcDirectory_isDenied();
    void isSafe_outsideHome_isDenied();
    void isSafe_gnomeKeyring_isDenied();
#endif
};

void TestLifecycleDenyList::isSafe_emptyPath_isDenied()
{
    QVERIFY(!LifecycleDenyList::isSafe(QString()));
}

void TestLifecycleDenyList::isSafe_rootPath_isDenied()
{
    QVERIFY(!LifecycleDenyList::isSafe(QStringLiteral("/")));
}

void TestLifecycleDenyList::isSafe_ordinaryUserPath_isSafe()
{
#if !defined(Q_OS_MAC) && !defined(Q_OS_LINUX)
    QSKIP("no platform-specific deny-list implemented for this OS");
#else
    // Rooted under $HOME (not the default /tmp) because the Linux deny-list
    // rejects anything outside $HOME by default (CISO §2) — a /tmp-based
    // QTemporaryDir would be a false-deny here, not a real assertion.
    QTemporaryDir tmp(QDir::homePath() + QStringLiteral("/.nexis-deny-list-test-XXXXXX"));
    QVERIFY(tmp.isValid());

    const QString leftover = tmp.filePath("com.example.SomeOldApp");
    QVERIFY(QDir().mkpath(leftover));

    QVERIFY(LifecycleDenyList::isSafe(leftover));
#endif
}

void TestLifecycleDenyList::isSafe_symlinkIntoDeniedLocation_isDenied()
{
#ifdef Q_OS_WIN
    QSKIP("symlink creation requires elevated privileges on Windows");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

#if defined(Q_OS_MAC)
    const QString deniedTarget = QStringLiteral("/System");
#elif defined(Q_OS_LINUX)
    const QString deniedTarget = QStringLiteral("/etc");
#else
    QSKIP("no platform-specific deny-list implemented for this OS");
    return;
#endif

    const QString linkPath = tmp.filePath("looks-like-a-leftover");
    QVERIFY(QFile::link(deniedTarget, linkPath));

    // The literal path lives in a harmless temp dir; only resolving the
    // symlink reveals it points into a denied location.
    QVERIFY(!LifecycleDenyList::isSafe(linkPath));
#endif
}

void TestLifecycleDenyList::isSafeForCleaning_degenerateInputs_areDenied()
{
    QVERIFY(!LifecycleDenyList::isSafeForCleaning(QString()));
    QVERIFY(!LifecycleDenyList::isSafeForCleaning(QStringLiteral("/")));
}

void TestLifecycleDenyList::isSafeForCleaning_symlinkIntoDeniedLocation_isDenied()
{
#if !defined(Q_OS_MAC) && !defined(Q_OS_LINUX)
    QSKIP("no platform-specific deny-list implemented for this OS");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString linkPath = tmp.filePath("looks-like-a-cache");
    QVERIFY(QFile::link(QStringLiteral("/usr"), linkPath));

    QVERIFY(!LifecycleDenyList::isSafeForCleaning(linkPath));
#endif
}

void TestLifecycleDenyList::isSafeForCleaning_danglingLeafUnderSymlinkedDeniedDir_isDenied()
{
#if !defined(Q_OS_MAC) && !defined(Q_OS_LINUX)
    QSKIP("no platform-specific deny-list implemented for this OS");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString linkedDir = tmp.filePath("linked");
    QVERIFY(QFile::link(QStringLiteral("/usr"), linkedDir));

    // The leaf does not exist, so it cannot be canonicalised itself; the check
    // must still resolve the directory it would live in.
    QVERIFY(!LifecycleDenyList::isSafeForCleaning(linkedDir + QStringLiteral("/no-such-entry")));
#endif
}

void TestLifecycleDenyList::isSafeForCleaning_ordinaryPathOutsideHome_isAllowed()
{
#if !defined(Q_OS_MAC) && !defined(Q_OS_LINUX)
    QSKIP("no platform-specific deny-list implemented for this OS");
#else
    // isSafe() would deny this (outside $HOME); the cleaner's own targets
    // (/var/cache, /var/log, the temp dir) live outside $HOME by design.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(!tmp.path().startsWith(QDir::homePath()));

    QVERIFY(LifecycleDenyList::isSafeForCleaning(tmp.filePath("some-cache-file")));
    QVERIFY(LifecycleDenyList::isSafeForCleaning(QStringLiteral("/var/cache/apt/archives/pkg.deb")));
    QVERIFY(LifecycleDenyList::isSafeForCleaning(QStringLiteral("/var/log/syslog.1")));
#endif
}

void TestLifecycleDenyList::isSafeForCleaning_usrLocalIsNotDeniedByUsr()
{
#if !defined(Q_OS_MAC) && !defined(Q_OS_LINUX)
    QSKIP("no platform-specific deny-list implemented for this OS");
#else
    // The broken-symlink scan walks /usr/local/bin; that prefix is
    // admin-managed, not OS-owned.
    QVERIFY(LifecycleDenyList::isSafeForCleaning(QStringLiteral("/usr/local/bin/stale-link")));
    QVERIFY(!LifecycleDenyList::isSafeForCleaning(QStringLiteral("/usr/bin/env")));
    QVERIFY(!LifecycleDenyList::isSafeForCleaning(QStringLiteral("/usr/share/doc")));
#endif
}

void TestLifecycleDenyList::isSafeForCleaning_deniedRootsAndCredentialStores()
{
#if defined(Q_OS_MAC)
    QVERIFY(!LifecycleDenyList::isSafeForCleaning(QStringLiteral("/System/Library/CoreServices")));
    QVERIFY(!LifecycleDenyList::isSafeForCleaning(QStringLiteral("/Library/LaunchDaemons/x.plist")));
    QVERIFY(!LifecycleDenyList::isSafeForCleaning(QStringLiteral("/usr/local/Cellar/wget/1.21")));
    QVERIFY(!LifecycleDenyList::isSafeForCleaning(QStringLiteral("/opt/homebrew/bin/brew")));
    QVERIFY(!LifecycleDenyList::isSafeForCleaning(QDir::homePath() + QStringLiteral("/Library/Keychains/login.keychain-db")));
    // Unlike the uninstaller, a com.apple.* leaf is an ordinary cache dir here.
    QVERIFY(LifecycleDenyList::isSafeForCleaning(QDir::homePath() + QStringLiteral("/Library/Caches/com.apple.Safari")));
#elif defined(Q_OS_LINUX)
    QVERIFY(!LifecycleDenyList::isSafeForCleaning(QStringLiteral("/etc/systemd/system")));
    QVERIFY(!LifecycleDenyList::isSafeForCleaning(QStringLiteral("/boot/vmlinuz")));
    QVERIFY(!LifecycleDenyList::isSafeForCleaning(QStringLiteral("/lib/modules")));
    QVERIFY(!LifecycleDenyList::isSafeForCleaning(QDir::homePath() + QStringLiteral("/.local/share/keyrings/login.keyring")));
#else
    QSKIP("no platform-specific deny-list implemented for this OS");
#endif
}

#ifdef Q_OS_MAC

void TestLifecycleDenyList::isSafe_systemDirectory_isDenied()
{
    QVERIFY(!LifecycleDenyList::isSafe(QStringLiteral("/System/Library/CoreServices")));
}

void TestLifecycleDenyList::isSafe_launchDaemons_isDenied()
{
    QVERIFY(!LifecycleDenyList::isSafe(QStringLiteral("/Library/LaunchDaemons/com.example.daemon.plist")));
}

void TestLifecycleDenyList::isSafe_appleBundleId_isDenied()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString applePrefPane = tmp.filePath("com.apple.something.plist");
    QFile f(applePrefPane);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.close();

    QVERIFY(!LifecycleDenyList::isSafe(applePrefPane));
}

void TestLifecycleDenyList::isSafe_homebrewCellar_isDenied()
{
    QVERIFY(!LifecycleDenyList::isSafe(QStringLiteral("/usr/local/Cellar/wget/1.21")));
}

void TestLifecycleDenyList::isSafe_outsideHome_isDenied()
{
    // Regression (SSO-15774 / CISO follow-up SSO-15771): isSafeMacOS() must
    // reject any path outside $HOME even when nothing in kDeniedRoots matches.
    // All current callers source candidate paths from ~/Library, but this test
    // encodes that boundary as a code-level guarantee rather than caller
    // convention alone (Defense in Depth).
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    // QTemporaryDir defaults to /var/folders/… or /tmp — both outside $HOME.
    QVERIFY(!tmp.path().startsWith(QDir::homePath()));

    const QString outsidePath = tmp.filePath("com.example.LeftoverSimulated");
    QVERIFY(QDir().mkpath(outsidePath));

    QVERIFY(!LifecycleDenyList::isSafe(outsidePath));
}

#endif // Q_OS_MAC

#ifdef Q_OS_LINUX

void TestLifecycleDenyList::isSafe_etcDirectory_isDenied()
{
    QVERIFY(!LifecycleDenyList::isSafe(QStringLiteral("/etc/systemd/system")));
}

void TestLifecycleDenyList::isSafe_outsideHome_isDenied()
{
    // A path that is neither under $HOME nor package-owned nor under any
    // explicit deny root still must not be treated as safe — the function
    // only ever narrows what's allowed, defaulting closed outside $HOME.
    QVERIFY(!LifecycleDenyList::isSafe(QStringLiteral("/srv/some-shared-data")));
}

void TestLifecycleDenyList::isSafe_gnomeKeyring_isDenied()
{
    const QString home = QDir::homePath();
    QVERIFY(!LifecycleDenyList::isSafe(home + QStringLiteral("/.local/share/keyrings/login.keyring")));
}

#endif // Q_OS_LINUX

QTEST_MAIN(TestLifecycleDenyList)
#include "test_lifecycle_deny_list.moc"
