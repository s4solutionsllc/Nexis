// SSO-3366 — guard PackageToolMacOS::trashApps against AppleScript injection.
//
// The pre-fix implementation interpolated each bundle path into a
// `tell application "Finder" to delete POSIX file "<path>"` AppleScript source
// string and ran it through `osascript -e`. A bundle whose name contained a
// double quote (legal on macOS — any downloaded archive can ship one) would
// terminate the string literal and execute attacker-controlled AppleScript,
// including `do shell script`. The fix routes the path through
// `QFile::moveToTrash` (NSFileManager::trashItemAtURL: on macOS), which
// takes an NSURL and has no shell/AppleScript parsing surface.
//
// This test creates a real `.app` directory whose name contains the same
// metacharacters the old code was vulnerable to (`"`, `;`, `\n`), calls
// `trashApps` on it, and asserts:
//   1. The original bundle was actually moved to trash (no longer exists).
//   2. No side-effect shell script ran — the marker file the AppleScript
//      payload would have created is absent.
//
// On non-macOS the test is QSKIPped (the CMake registration is also gated
// on APPLE, so this is a belt-and-braces guard for IDE/editor builds).

#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#ifdef Q_OS_MAC
#include "Tools/package_tool_macos.h"
#endif

class TestPackageToolMacOS : public QObject
{
    Q_OBJECT

private slots:
    void trashApps_pathWithAppleScriptMetacharacters_noInjection();
};

void TestPackageToolMacOS::trashApps_pathWithAppleScriptMetacharacters_noInjection()
{
#ifndef Q_OS_MAC
    QSKIP("trashApps() is the macOS uninstaller path — covered by QFile::moveToTrash on Darwin only");
#else
    QTemporaryDir workDir;
    QVERIFY(workDir.isValid());

    // Marker the AppleScript injection payload would create if the path were
    // ever fed back into an AppleScript/sh source string. The test fails if
    // this file exists after trashApps returns.
    const QString markerPath = workDir.filePath("sso-3366-pwned");

    // Bundle name carrying the exact metacharacters the audit (S1) called
    // out: `"` would terminate the AppleScript string literal, `;` is a
    // shell statement separator, and a newline closes the `-e` arg.
    // `do shell script "touch <marker>"` is what the old osascript-based
    // implementation would actually have executed.
    const QString appName = QStringLiteral("evil\";\ndo shell script \"touch '")
                                + markerPath
                                + QStringLiteral("'\";--.app");
    const QString appPath = workDir.filePath(appName);

    QVERIFY2(QDir(workDir.path()).mkdir(appName),
             qPrintable(QStringLiteral("could not create bundle at ") + appPath));
    QVERIFY(QFileInfo::exists(appPath));
    QVERIFY(!QFileInfo::exists(markerPath));

    PackageToolMacOS tool;
    const bool ok = tool.trashApps({appPath});

    // Acceptance: trashing this bundle moves exactly that bundle and
    // executes nothing else.
    QVERIFY2(ok, "trashApps must succeed for a literal bundle path even when "
                  "the bundle name contains AppleScript/shell metacharacters");
    QVERIFY2(!QFileInfo::exists(appPath),
             "the bundle should have been moved to trash");
    QVERIFY2(!QFileInfo::exists(markerPath),
             "no injected shell/AppleScript payload may run — marker file "
             "would only exist if `do shell script` was reached");
#endif
}

QTEST_MAIN(TestPackageToolMacOS)
#include "test_package_tool_macos.moc"
