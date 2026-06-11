// SSO-3367 / audit A1: CommandUtil now has a single error-handling contract.
// No path throws QString — exec(), sudoExec(), and execWithStatus() all
// surface failures through ExecResult (or, for the legacy QString-returning
// wrappers, an empty string plus a qCritical log).
//
// These tests assert that contract: the previously-throwing `exec()` is now
// non-throwing, execWithStatus captures exit code and stderr, and
// sudoExecWithStatus honours the NEXIS_SUDO_BYPASS testing seam.

#include <QTest>
#include "command_util.h"

class TestCommandUtil : public QObject
{
    Q_OBJECT

private slots:
    void exec_echoReturnsOutput();
    void exec_invalidCommandDoesNotThrow();
    void exec_invalidCommandReturnsEmpty();
    void exec_trimmedOutput();

    void execWithStatus_successExitCode();
    void execWithStatus_failureExitCode();
    void execWithStatus_invalidCommand();
    void execWithStatus_capturesStderr();
    void execWithStatus_acceptsStdin();

    void sudoExec_bypass_returnsCommandOutput();
    void sudoExec_bypass_failureReturnsEmptyDoesNotThrow();
    void sudoExecWithStatus_bypass_capturesExitCode();
    void sudoExecWithStatus_bypass_capturesStderr();

    void isExecutable_knownCommand();
    void isExecutable_unknownCommand();
    void isExecutable_emptyString();
};

void TestCommandUtil::exec_echoReturnsOutput()
{
    QString output = CommandUtil::exec("echo", {"hello"});
    QCOMPARE(output.trimmed(), QString("hello"));
}

void TestCommandUtil::exec_invalidCommandDoesNotThrow()
{
    bool threw = false;
    try {
        (void)CommandUtil::exec("nonexistent_binary_xyz_12345");
    } catch (...) {
        threw = true;
    }
    QVERIFY2(!threw, "CommandUtil::exec must not throw on QProcess failure (SSO-3367)");
}

void TestCommandUtil::exec_invalidCommandReturnsEmpty()
{
    QString output = CommandUtil::exec("nonexistent_binary_xyz_12345");
    QVERIFY(output.isEmpty());
}

void TestCommandUtil::exec_trimmedOutput()
{
    QString output = CommandUtil::exec("echo", {"  spaced  "});
    QVERIFY(!output.isEmpty());
}

void TestCommandUtil::execWithStatus_successExitCode()
{
    ExecResult result = CommandUtil::execWithStatus("echo", {"test"});
    QCOMPARE(result.exitCode, 0);
    QVERIFY(result.ok());
    QVERIFY(result.output.trimmed() == "test");
}

void TestCommandUtil::execWithStatus_failureExitCode()
{
    ExecResult result = CommandUtil::execWithStatus("false");
    QVERIFY(result.exitCode != 0);
    QVERIFY(!result.ok());
}

void TestCommandUtil::execWithStatus_invalidCommand()
{
    ExecResult result = CommandUtil::execWithStatus("nonexistent_binary_xyz_12345");
    QVERIFY(result.exitCode != 0);
    QVERIFY(!result.ok());
    QVERIFY(!result.error.isEmpty());
}

void TestCommandUtil::execWithStatus_capturesStderr()
{
    // `sh -c "echo boom 1>&2; exit 3"` writes "boom" to stderr and exits non-zero.
    ExecResult result = CommandUtil::execWithStatus(
        "sh", {"-c", "echo boom 1>&2; exit 3"});
    QCOMPARE(result.exitCode, 3);
    QVERIFY(!result.ok());
    QVERIFY(result.error.contains("boom"));
}

void TestCommandUtil::execWithStatus_acceptsStdin()
{
    // `cat` echoes stdin to stdout — confirms the stdin overload pipes data.
    ExecResult result = CommandUtil::execWithStatus("cat", {}, QByteArray("piped"));
    QCOMPARE(result.exitCode, 0);
    QVERIFY(result.ok());
    QCOMPARE(result.output.trimmed(), QString("piped"));
}

void TestCommandUtil::sudoExec_bypass_returnsCommandOutput()
{
    // NEXIS_SUDO_BYPASS routes sudoExec through execWithStatus directly so the
    // test exercises the error-reporting path without pkexec/osascript.
    qputenv("NEXIS_SUDO_BYPASS", "1");
    QString output = CommandUtil::sudoExec("echo", {"sudo-hello"});
    qunsetenv("NEXIS_SUDO_BYPASS");
    QCOMPARE(output.trimmed(), QString("sudo-hello"));
}

void TestCommandUtil::sudoExec_bypass_failureReturnsEmptyDoesNotThrow()
{
    qputenv("NEXIS_SUDO_BYPASS", "1");
    bool threw = false;
    QString output;
    try {
        output = CommandUtil::sudoExec("nonexistent_binary_xyz_12345");
    } catch (...) {
        threw = true;
    }
    qunsetenv("NEXIS_SUDO_BYPASS");
    QVERIFY2(!threw, "CommandUtil::sudoExec must not throw on failure (SSO-3367)");
    QVERIFY(output.isEmpty());
}

void TestCommandUtil::sudoExecWithStatus_bypass_capturesExitCode()
{
    qputenv("NEXIS_SUDO_BYPASS", "1");
    ExecResult result = CommandUtil::sudoExecWithStatus("false");
    qunsetenv("NEXIS_SUDO_BYPASS");
    QVERIFY(!result.ok());
    QVERIFY(result.exitCode != 0);
}

void TestCommandUtil::sudoExecWithStatus_bypass_capturesStderr()
{
    qputenv("NEXIS_SUDO_BYPASS", "1");
    ExecResult result = CommandUtil::sudoExecWithStatus(
        "sh", {"-c", "echo nope 1>&2; exit 7"});
    qunsetenv("NEXIS_SUDO_BYPASS");
    QCOMPARE(result.exitCode, 7);
    QVERIFY(result.error.contains("nope"));
}

void TestCommandUtil::isExecutable_knownCommand()
{
    QVERIFY(CommandUtil::isExecutable("ls"));
}

void TestCommandUtil::isExecutable_unknownCommand()
{
    QVERIFY(!CommandUtil::isExecutable("nonexistent_binary_xyz_12345"));
}

void TestCommandUtil::isExecutable_emptyString()
{
    QVERIFY(!CommandUtil::isExecutable(""));
}

QTEST_MAIN(TestCommandUtil)
#include "test_command_util.moc"
