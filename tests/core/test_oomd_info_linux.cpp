#include <QTest>
#include <QAtomicPointer>
#include <QFuture>
#include <QThread>
#include <QtConcurrent>

#include "Info/oomd_info_linux.h"

// SSO-26717 (B3): `systemctl show` and the journalctl walk fork real
// processes and are noticeably heavier than a /proc read — update() must
// never run synchronously on the GUI thread. The CommandRunner seam (already
// built for canned-output tests) doubles here to record which thread
// actually invoked the commands, locking down the off-thread contract that
// DataRefreshService::onMediumTick() relies on (mirrors
// TestProcessInfo::offThread_collectRunsOnWorkerThread for WI-21).
class ThreadRecordingRunner : public OomdInfoLinux::CommandRunner
{
public:
    QAtomicPointer<QThread> lastRunThread;

    QByteArray run(const QString &cmd, const QStringList &args, int timeoutMs) override
    {
        Q_UNUSED(cmd);
        Q_UNUSED(args);
        Q_UNUSED(timeoutMs);
        lastRunThread.storeRelaxed(QThread::currentThread());
        return {};
    }

    bool exists(const QString &cmd) override
    {
        Q_UNUSED(cmd);
        return true; // force both systemctl/journalctl branches to run()
    }
};

class TestOomdInfoLinux : public QObject
{
    Q_OBJECT

private slots:
    void update_runsCommandsOffCallingThread();
};

void TestOomdInfoLinux::update_runsCommandsOffCallingThread()
{
    ThreadRecordingRunner runner;
    OomdInfoLinux info(&runner);
    info.setCgroupRootForTesting(QStringLiteral("/nonexistent-for-test"));

    QThread *caller = QThread::currentThread();

    QFuture<void> future = QtConcurrent::run([&info]() {
        info.update();
    });
    future.waitForFinished();

    QThread *worker = runner.lastRunThread.loadRelaxed();
    QVERIFY2(worker != nullptr, "update() never invoked the CommandRunner");
    QVERIFY2(worker != caller,
             "the oomd systemctl/journalctl query must run on a worker thread, not the caller");
}

QTEST_MAIN(TestOomdInfoLinux)
#include "test_oomd_info_linux.moc"
