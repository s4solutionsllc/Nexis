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

#ifdef Q_OS_MAC
    void isSafe_systemDirectory_isDenied();
    void isSafe_launchDaemons_isDenied();
    void isSafe_appleBundleId_isDenied();
    void isSafe_homebrewCellar_isDenied();
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
