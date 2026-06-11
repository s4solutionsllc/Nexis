#include <QTest>
#include <atomic>
#include <thread>
#include <vector>

// WI-23 (M4): the unguarded `if (sConn)` check-then-IOServiceOpen in
// thermal_info.cpp could double-open the AppleSMC connection if the wizard
// worker and medium tick raced through smcOpen() at startup. The fix wraps
// the open in std::call_once. These free-function seams in the macOS
// nexis-core build expose the counter for tests without leaking the SMC
// internals into a public header.
unsigned nexis_smcOpenAttemptsForTest();
void     nexis_smcForceOpenForTest();

class TestThermalInfoMacOS : public QObject
{
    Q_OBJECT

private slots:
    void smcOpen_runsExactlyOnceUnderConcurrentCallers();
};

void TestThermalInfoMacOS::smcOpen_runsExactlyOnceUnderConcurrentCallers()
{
#ifndef Q_OS_MACOS
    QSKIP("AppleSMC seam is only present on macOS");
#else
    // The first caller in this process triggered open at static-init time
    // (e.g. ThermalInfoMacOS ctor in another test), so latch the baseline
    // before spinning up contending callers.
    const unsigned baseline = nexis_smcOpenAttemptsForTest();

    constexpr int kThreads = 16;
    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    std::atomic<int> ready{0};
    std::atomic<bool> go{false};

    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&]() {
            ready.fetch_add(1, std::memory_order_relaxed);
            while (!go.load(std::memory_order_acquire))
                std::this_thread::yield();
            nexis_smcForceOpenForTest();
        });
    }
    while (ready.load(std::memory_order_relaxed) < kThreads)
        std::this_thread::yield();
    go.store(true, std::memory_order_release);

    for (auto &t : threads)
        t.join();

    const unsigned attempts = nexis_smcOpenAttemptsForTest();
    // std::call_once guarantees the inner block runs exactly once for the
    // process lifetime, regardless of caller count.
    Q_UNUSED(baseline);
    QVERIFY2(attempts == 1,
             qPrintable(QString("smcOpen attempted %1 times across the process — open-once invariant broken")
                            .arg(attempts)));
#endif
}

QTEST_MAIN(TestThermalInfoMacOS)
#include "test_thermal_info_macos.moc"
