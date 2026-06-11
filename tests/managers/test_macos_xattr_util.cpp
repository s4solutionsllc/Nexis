// SSO-3731 (FW-04, MX1): MacOsXattrUtil::stripQuarantine() must remove the
// com.apple.quarantine xattr from a plist on disk before `launchctl load`
// on macOS 27. The test writes a stand-in plist under a QTemporaryDir,
// applies a synthetic quarantine attribute via setxattr(), runs the strip,
// and asserts the attribute is gone. QSKIP off-macOS because the xattr
// surface and `com.apple.quarantine` are platform-specific.

#include <QTest>
#include <QFile>
#include <QTemporaryDir>

#include "Utils/macos_xattr_util.h"

#ifdef Q_OS_MACOS
#include <sys/xattr.h>
#include <cerrno>
#include <cstring>
#endif

class TestMacOsXattrUtil : public QObject
{
    Q_OBJECT

private slots:
    void stripsQuarantineFromPlist();
    void idempotentWhenQuarantineAbsent();
    void failsForMissingPath();
};

void TestMacOsXattrUtil::stripsQuarantineFromPlist()
{
#ifndef Q_OS_MACOS
    QSKIP("macOS-only: removexattr() and com.apple.quarantine are platform-specific");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString path = tmp.path() + "/com.nexis.clean.test.plist";
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("<?xml version=\"1.0\"?><plist version=\"1.0\"><dict/></plist>\n");
    f.close();

    const QByteArray local = path.toLocal8Bit();
    const char *quarantineValue = "0001;00000000;Nexis;00000000-0000-0000-0000-000000000000";
    QCOMPARE(setxattr(local.constData(),
                      "com.apple.quarantine",
                      quarantineValue,
                      std::strlen(quarantineValue),
                      0,
                      XATTR_NOFOLLOW),
             0);

    char buf[256];
    QVERIFY(getxattr(local.constData(),
                     "com.apple.quarantine",
                     buf,
                     sizeof(buf),
                     0,
                     XATTR_NOFOLLOW) > 0);

    QVERIFY(MacOsXattrUtil::stripQuarantine(path));

    errno = 0;
    const ssize_t after = getxattr(local.constData(),
                                   "com.apple.quarantine",
                                   buf,
                                   sizeof(buf),
                                   0,
                                   XATTR_NOFOLLOW);
    QCOMPARE(after, static_cast<ssize_t>(-1));
    QCOMPARE(errno, ENOATTR);
#endif
}

void TestMacOsXattrUtil::idempotentWhenQuarantineAbsent()
{
#ifndef Q_OS_MACOS
    QSKIP("macOS-only");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + "/clean.plist";
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("<plist/>");
    f.close();

    // No quarantine attribute set — stripQuarantine must still report success
    // (ENOATTR is treated as already-stripped).
    QVERIFY(MacOsXattrUtil::stripQuarantine(path));
#endif
}

void TestMacOsXattrUtil::failsForMissingPath()
{
#ifndef Q_OS_MACOS
    QSKIP("macOS-only");
#else
    // A path that doesn't exist yields ENOENT, not ENOATTR — the helper must
    // surface that as failure so callers can warn.
    QVERIFY(!MacOsXattrUtil::stripQuarantine(
        "/nonexistent/sso-3731/no-such-plist.plist"));
#endif
}

QTEST_MAIN(TestMacOsXattrUtil)
#include "test_macos_xattr_util.moc"
