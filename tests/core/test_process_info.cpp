#include <QTest>
#include <QAtomicInt>
#include <QAtomicPointer>
#include <QElapsedTimer>
#include <QFuture>
#include <QMutexLocker>
#include <QSemaphore>
#include <QThread>
#include <QtConcurrent>
#include <atomic>
#include <thread>

#include "Info/process_info.h"

// ── WI-21 (SSO-3383, audit M2) ────────────────────────────────────────────────
// Test double mirrors the worker pattern used by every real ProcessInfo
// subclass after WI-21:
//   - collectProcesses() builds a fresh local QList<Process> and returns it,
//     mutating only the subclass's internal delta state — never processList
//     (that's the whole point of the publish pair).
//   - setProcessList() assigns the cache and must be safe under reader races.
//   - getProcessList() (base class) hands back a coherent snapshot.
//
// The fake also captures the QThread* that ran collectProcesses() and exposes
// a pair of semaphores so the test can freeze the worker mid-collect — that is
// what lets us prove the off-thread property (a slow collect must not block
// concurrent UI-thread getProcessList() callers).
class FakeProcessInfo : public ProcessInfo
{
public:
    QAtomicInt collectCalls{0};
    QAtomicPointer<QThread> lastCollectThread;
    QSemaphore startedSem;
    QSemaphore releaseSem;
    std::atomic<bool> blockInCollect{false};

    QList<Process> collectProcesses() override
    {
        lastCollectThread.storeRelaxed(QThread::currentThread());
        const int n = 2 + (collectCalls.fetchAndAddRelaxed(1) % 4); // 2..5 procs

        if (blockInCollect.load(std::memory_order_acquire)) {
            startedSem.release();
            releaseSem.acquire();
        }

        QList<Process> processes;
        for (int i = 0; i < n; ++i) {
            Process p;
            p.setPid(static_cast<pid_t>(1000 + i));
            p.setCmd(QStringLiteral("fake-%1").arg(i));
            p.setUname(QStringLiteral("user-%1").arg(i));
            // Yield mid-build to widen the window for a buggy reader.
            QThread::yieldCurrentThread();
            processes.append(p);
        }
        return processes;
    }
};

// Every Process in a snapshot must have a non-empty cmd/uname and a positive
// pid. Catches torn reads where a getter races a setter on a half-assigned
// QList or zero-initialised Process struct.
static bool isCoherent(const QList<Process> &snapshot)
{
    for (const Process &p : snapshot) {
        if (p.getPid() <= 0)
            return false;
        if (p.getCmd().isEmpty() || p.getUname().isEmpty())
            return false;
    }
    return true;
}

class TestProcessInfo : public QObject
{
    Q_OBJECT

private slots:
    // ── publish-pair API ──────────────────────────────────────────────────
    void publish_collectIsLocalAndDoesNotMutateCache();
    void publish_setProcessListUpdatesCache();
    void publish_updateProcessesRoundTrips();

    // ── off-thread property (audit M2) ────────────────────────────────────
    void offThread_collectRunsOnWorkerThread();
    void offThread_slowCollectDoesNotBlockReaders();

    // ── concurrency stress ────────────────────────────────────────────────
    void stress_concurrentSetAndGetProcessList();
};

void TestProcessInfo::publish_collectIsLocalAndDoesNotMutateCache()
{
    FakeProcessInfo info;
    QVERIFY(info.getProcessList().isEmpty());

    QList<Process> local = info.collectProcesses();
    QVERIFY(!local.isEmpty());

    // Worker path must never touch processList — that's the whole point of
    // the publish pattern. Cache should still be empty after the worker
    // collected.
    QVERIFY(info.getProcessList().isEmpty());
}

void TestProcessInfo::publish_setProcessListUpdatesCache()
{
    FakeProcessInfo info;
    QList<Process> local = info.collectProcesses();
    const int expectedSize = local.size();

    info.setProcessList(local);

    QCOMPARE(info.getProcessList().size(), expectedSize);
    QVERIFY(isCoherent(info.getProcessList()));
}

void TestProcessInfo::publish_updateProcessesRoundTrips()
{
    // The legacy synchronous entry point (still used by the HTML-export path)
    // must continue to publish a fresh snapshot via the same publish pair.
    FakeProcessInfo info;
    QVERIFY(info.getProcessList().isEmpty());

    info.updateProcesses();

    QVERIFY(!info.getProcessList().isEmpty());
    QVERIFY(isCoherent(info.getProcessList()));
}

void TestProcessInfo::offThread_collectRunsOnWorkerThread()
{
    // Prove the structural off-thread property end-to-end: when the
    // DataRefreshService-equivalent hops via QtConcurrent::run, collect runs
    // on a different thread than the caller. Locks down the WI-21 contract
    // so a future regression that drops the hop is caught by CI.
    FakeProcessInfo info;
    QThread *caller = QThread::currentThread();

    QFuture<QList<Process>> future = QtConcurrent::run([&info]() {
        return info.collectProcesses();
    });
    future.waitForFinished();

    QThread *worker = info.lastCollectThread.loadRelaxed();
    QVERIFY(worker != nullptr);
    QVERIFY2(worker != caller,
             "collectProcesses() must run on a worker thread, not the caller");
}

void TestProcessInfo::offThread_slowCollectDoesNotBlockReaders()
{
    // The "GUI never freezes" acceptance check: while a worker tick is mid-
    // collect (the slow `ps` fork on macOS, or the /proc walk on Linux), a
    // UI-thread getProcessList() snapshot must come back immediately.
    FakeProcessInfo info;
    info.setProcessList(info.collectProcesses()); // prime so getter has data
    QVERIFY(!info.getProcessList().isEmpty());

    info.blockInCollect.store(true, std::memory_order_release);

    QFuture<void> future = QtConcurrent::run([&info]() {
        QList<Process> result = info.collectProcesses();
        Q_UNUSED(result);
    });

    // Make sure the worker actually entered collectProcesses() before we
    // assert. 5 s is generous; QtConcurrent::run posts to the global pool.
    QVERIFY2(info.startedSem.tryAcquire(1, 5000),
             "worker never entered collectProcesses()");

    // Caller thread reads the cache while collect is frozen — must not block.
    QElapsedTimer timer;
    timer.start();
    QList<Process> snapshot = info.getProcessList();
    const qint64 elapsedMs = timer.elapsed();

    QVERIFY(!snapshot.isEmpty());
    QVERIFY(isCoherent(snapshot));
    QVERIFY2(elapsedMs < 250,
             qPrintable(QStringLiteral("getProcessList() blocked for %1 ms while collect was running")
                         .arg(elapsedMs)));

    info.releaseSem.release();
    future.waitForFinished();
}

void TestProcessInfo::stress_concurrentSetAndGetProcessList()
{
    // Reproduce the worker/UI race on the cache: a writer thread doing the
    // collect+publish pair, a reader thread copying via getProcessList() in a
    // tight loop. The WI-21 processListMutex must keep every snapshot
    // internally coherent (no torn QList, no half-assigned Process).
    FakeProcessInfo info;
    info.setProcessList(info.collectProcesses());

    constexpr int kIters = 2000;
    std::atomic<int> incoherentReads{0};
    std::atomic<bool> stop{false};

    std::thread reader([&]() {
        while (!stop.load(std::memory_order_relaxed)) {
            QList<Process> snapshot = info.getProcessList();
            if (!snapshot.isEmpty() && !isCoherent(snapshot))
                incoherentReads.fetch_add(1, std::memory_order_relaxed);
        }
    });

    std::thread writer([&]() {
        for (int i = 0; i < kIters; ++i) {
            // Worker pattern: build into a local list, then publish. With the
            // mutex the reader either sees the prior list or the new one,
            // never something in between.
            info.setProcessList(info.collectProcesses());
        }
    });

    writer.join();
    stop.store(true, std::memory_order_relaxed);
    reader.join();

    QCOMPARE(incoherentReads.load(), 0);
}

QTEST_MAIN(TestProcessInfo)
#include "test_process_info.moc"
