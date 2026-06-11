// SSO-3363 / audit H2: LogProvider::cancel() must not double-delete or
// dereference a null QProcess pointer when the connected finished() slot
// fires synchronously while waitForFinished() is processing events.
//
// We exercise the base-class cancel() via a small test subclass that:
//  * spawns a real long-running child (`sh -c "sleep 5"`) so kill() will
//    cause QProcess::CrashExit and drive the dangerous error path, and
//  * connects a slot to finished() that mimics the historical cleanup
//    pattern (null mProcess + deleteLater()). With the broken cancel(),
//    waitForFinished() would deliver finished() synchronously and the
//    subsequent mProcess->deleteLater() in cancel() would deref nullptr.

#include <QSignalSpy>
#include <QtTest/QtTest>

#include "Pages/SystemLogs/log_provider.h"

class TestableLogProvider : public LogProvider
{
    Q_OBJECT
public:
    explicit TestableLogProvider(QObject *parent = nullptr)
        : LogProvider(parent) {}

    void fetchLogs(int /*maxEntries*/ = 0, int /*maxSeverity*/ = 0) override
    {
        if (mBusy)
            return;
        mBusy = true;
        mProcess = new QProcess(this);

        // The historical "broken" cleanup pattern: the slot nulls the member
        // and deletes the QProcess. cancel() must defend against this firing
        // during waitForFinished().
        connect(mProcess, &QProcess::finished, this,
                [this](int, QProcess::ExitStatus) {
                    if (mProcess) {
                        mProcess->deleteLater();
                        mProcess = nullptr;
                    }
                    mBusy = false;
                });

        mProcess->start(QStringLiteral("sh"),
                        {QStringLiteral("-c"), QStringLiteral("sleep 5")});
        mProcess->waitForStarted(3000);
    }

    // Start a child that exits immediately, so the connected slot performs
    // the cleanup naturally before cancel() ever runs.
    void fetchInstantlyFinishing()
    {
        if (mBusy)
            return;
        mBusy = true;
        mProcess = new QProcess(this);
        connect(mProcess, &QProcess::finished, this,
                [this](int, QProcess::ExitStatus) {
                    if (mProcess) {
                        mProcess->deleteLater();
                        mProcess = nullptr;
                    }
                    mBusy = false;
                });
        mProcess->start(QStringLiteral("sh"),
                        {QStringLiteral("-c"), QStringLiteral("true")});
    }

    QProcess *currentProcess() const { return mProcess; }
};

class TestLogProvider : public QObject
{
    Q_OBJECT

private slots:
    void cancel_noOpWhenNoProcess();
    void cancel_safeWhileRunning();
    void cancel_safeWhenCalledTwice();
    void cancel_safeAfterNaturalFinish();
};

void TestLogProvider::cancel_noOpWhenNoProcess()
{
    TestableLogProvider provider;
    QVERIFY(!provider.isBusy());
    QVERIFY(provider.currentProcess() == nullptr);

    // Must not crash, must not change state.
    provider.cancel();

    QVERIFY(!provider.isBusy());
    QVERIFY(provider.currentProcess() == nullptr);
}

void TestLogProvider::cancel_safeWhileRunning()
{
    TestableLogProvider provider;
    provider.fetchLogs();

    QVERIFY(provider.isBusy());
    QProcess *p = provider.currentProcess();
    QVERIFY(p != nullptr);
    QCOMPARE(p->state(), QProcess::Running);

    QSignalSpy destroyedSpy(p, &QObject::destroyed);

    // The historical bug: waitForFinished() would synchronously deliver
    // finished() (from kill()'s CrashExit), the slot would null mProcess
    // and call deleteLater(), and the following mProcess->deleteLater()
    // in cancel() would dereference null.
    provider.cancel();

    QVERIFY(!provider.isBusy());
    QVERIFY(provider.currentProcess() == nullptr);

    // The QProcess must be destroyed exactly once, via the deleteLater()
    // queued by cancel() itself (slot was disconnected first).
    QTRY_COMPARE_WITH_TIMEOUT(destroyedSpy.count(), 1, 5000);
}

void TestLogProvider::cancel_safeWhenCalledTwice()
{
    TestableLogProvider provider;
    provider.fetchLogs();
    QVERIFY(provider.currentProcess() != nullptr);

    provider.cancel();
    // Second cancel() must be a no-op on the now-null member.
    provider.cancel();

    QVERIFY(!provider.isBusy());
    QVERIFY(provider.currentProcess() == nullptr);
}

void TestLogProvider::cancel_safeAfterNaturalFinish()
{
    TestableLogProvider provider;
    provider.fetchInstantlyFinishing();

    // Spin the event loop until the slot has cleaned up.
    QTRY_VERIFY_WITH_TIMEOUT(provider.currentProcess() == nullptr, 5000);
    QVERIFY(!provider.isBusy());

    // Now cancel() runs with a null member — must early-return safely.
    provider.cancel();
}

QTEST_MAIN(TestLogProvider)
#include "test_log_provider.moc"
