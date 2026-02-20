#include <QTest>
#include "command_util.h"

class TestCommandUtil : public QObject
{
    Q_OBJECT

private slots:
    void exec_echoReturnsOutput();
    void exec_invalidCommandThrows();
    void exec_trimmedOutput();
    void execWithStatus_successExitCode();
    void execWithStatus_failureExitCode();
    void execWithStatus_invalidCommand();
    void isExecutable_knownCommand();
    void isExecutable_unknownCommand();
    void isExecutable_emptyString();
};

void TestCommandUtil::exec_echoReturnsOutput()
{
    QString output = CommandUtil::exec("echo", {"hello"});
    QCOMPARE(output.trimmed(), QString("hello"));
}

void TestCommandUtil::exec_invalidCommandThrows()
{
    bool threw = false;
    try {
        CommandUtil::exec("nonexistent_binary_xyz_12345");
    } catch (const QString &) {
        threw = true;
    }
    QVERIFY(threw);
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
    QVERIFY(result.output.trimmed() == "test");
}

void TestCommandUtil::execWithStatus_failureExitCode()
{
    ExecResult result = CommandUtil::execWithStatus("false");
    QVERIFY(result.exitCode != 0);
}

void TestCommandUtil::execWithStatus_invalidCommand()
{
    ExecResult result = CommandUtil::execWithStatus("nonexistent_binary_xyz_12345");
    QVERIFY(result.exitCode != 0);
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
