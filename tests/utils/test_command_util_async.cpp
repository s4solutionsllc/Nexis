#include <QCoreApplication>
#include <QFuture>
#include <QFutureWatcher>
#include <QSignalSpy>
#include <QTest>
#include <QThread>

#include "command_util.h"

class TestCommandUtilAsync : public QObject
{
    Q_OBJECT

private slots:
    void execAsync_echoReturnsOutput();
    void execAsync_runsOffCallerThread();
    void execAsync_failureExitCode();
    void execAsync_invalidCommand();
};

void TestCommandUtilAsync::execAsync_echoReturnsOutput()
{
    QFuture<ExecResult> future = CommandUtil::execAsync("echo", {"async-hello"});
    QFutureWatcher<ExecResult> watcher;
    QSignalSpy spy(&watcher, &QFutureWatcher<ExecResult>::finished);
    watcher.setFuture(future);
    QVERIFY(spy.wait(5000));

    ExecResult result = future.result();
    QCOMPARE(result.exitCode, 0);
    QCOMPARE(result.output.trimmed(), QString("async-hello"));
}

void TestCommandUtilAsync::execAsync_runsOffCallerThread()
{
    QThread *callerThread = QThread::currentThread();

    QFuture<ExecResult> future = CommandUtil::execAsync("sh", {"-c", "sleep 0.05; echo off-thread"});

    QFutureWatcher<ExecResult> watcher;
    QSignalSpy spy(&watcher, &QFutureWatcher<ExecResult>::finished);
    watcher.setFuture(future);

    // Caller thread is free to keep running while the subprocess blocks.
    QVERIFY(QThread::currentThread() == callerThread);

    QVERIFY(spy.wait(5000));
    QCOMPARE(future.result().output.trimmed(), QString("off-thread"));
}

void TestCommandUtilAsync::execAsync_failureExitCode()
{
    QFuture<ExecResult> future = CommandUtil::execAsync("false");
    QFutureWatcher<ExecResult> watcher;
    QSignalSpy spy(&watcher, &QFutureWatcher<ExecResult>::finished);
    watcher.setFuture(future);
    QVERIFY(spy.wait(5000));
    QVERIFY(future.result().exitCode != 0);
}

void TestCommandUtilAsync::execAsync_invalidCommand()
{
    QFuture<ExecResult> future = CommandUtil::execAsync("nonexistent_binary_xyz_12345");
    QFutureWatcher<ExecResult> watcher;
    QSignalSpy spy(&watcher, &QFutureWatcher<ExecResult>::finished);
    watcher.setFuture(future);
    QVERIFY(spy.wait(5000));
    QVERIFY(future.result().exitCode != 0);
}

QTEST_MAIN(TestCommandUtilAsync)
#include "test_command_util_async.moc"
