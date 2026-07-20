#include <QTest>

#include "Info/nvml_process_sampler.h"

// SSO-15374: FakeNvmlBackend stands in for NvmlLibraryBackend so the
// device-order aggregation logic in NvmlProcessSampler::refresh() — the part
// with actual behavior worth testing — runs in CI without NVIDIA hardware or
// the NVML library present. Per-device responses are scripted per refresh()
// call via nextCompute/nextGraphics/nextUtil (indexed by call count), so a
// test can simulate a multi-GPU system across two refresh() ticks.
class FakeNvmlBackend : public NvmlBackend
{
public:
    bool available = true;
    unsigned int devices = 1;

    // deviceIndex -> list of process samples for computeProcesses()/graphicsProcesses().
    QHash<unsigned int, QList<NvmlProcMemSample>> computeByDevice;
    QHash<unsigned int, QList<NvmlProcMemSample>> graphicsByDevice;
    // deviceIndex -> list of utilization samples for processUtilization().
    QHash<unsigned int, QList<NvmlProcUtilSample>> utilByDevice;

    int initCalls = 0;
    int shutdownCalls = 0;
    QList<quint64> sinceUsSeenPerDevice;

    bool init() override
    {
        ++initCalls;
        return available;
    }

    void shutdown() override { ++shutdownCalls; }

    unsigned int deviceCount() const override { return devices; }

    QList<NvmlProcMemSample> computeProcesses(unsigned int deviceIndex) const override
    {
        return computeByDevice.value(deviceIndex);
    }

    QList<NvmlProcMemSample> graphicsProcesses(unsigned int deviceIndex) const override
    {
        return graphicsByDevice.value(deviceIndex);
    }

    QList<NvmlProcUtilSample> processUtilization(unsigned int deviceIndex, quint64 sinceUs) const override
    {
        const_cast<FakeNvmlBackend *>(this)->sinceUsSeenPerDevice.append(sinceUs);
        return utilByDevice.value(deviceIndex);
    }
};

class TestNvmlProcessSampler : public QObject
{
    Q_OBJECT

private slots:
    void unavailable_backendNeverQueried();
    void singleGpu_singlePid();
    void multiGpu_sumsAcrossDevicesInOrder();
    void computeAndGraphics_sameDevice_takesMaxNotSum();
    void utilization_multipleSamplesPerPid_keepsLatestOnly();
    void pruneDeadPids_dropsMissingEntries();
    void unseenPid_returnsSentinelSample();
};

void TestNvmlProcessSampler::unavailable_backendNeverQueried()
{
    auto fake = std::make_unique<FakeNvmlBackend>();
    fake->available = false;
    FakeNvmlBackend *fakePtr = fake.get();

    NvmlProcessSampler sampler(std::move(fake));
    QVERIFY(!sampler.isAvailable());

    sampler.refresh();   // must no-op, not crash, not call deviceCount()/etc.

    const auto sample = sampler.get(1234);
    QCOMPARE(sample.gpuPercent, -1.0);
    QCOMPARE(sample.vramBytes, static_cast<qint64>(-1));
    QCOMPARE(fakePtr->initCalls, 1);   // isAvailable() probes once and caches.
}

void TestNvmlProcessSampler::singleGpu_singlePid()
{
    auto fake = std::make_unique<FakeNvmlBackend>();
    fake->devices = 1;
    fake->computeByDevice[0] = {{1000, 512ULL * 1024 * 1024}};
    fake->utilByDevice[0] = {{1000, 1000, 42}};

    NvmlProcessSampler sampler(std::move(fake));
    sampler.refresh();

    const auto sample = sampler.get(1000);
    QCOMPARE(sample.gpuPercent, 42.0);
    QCOMPARE(sample.vramBytes, static_cast<qint64>(512ULL * 1024 * 1024));
}

void TestNvmlProcessSampler::multiGpu_sumsAcrossDevicesInOrder()
{
    auto fake = std::make_unique<FakeNvmlBackend>();
    fake->devices = 2;
    // Same PID spans both GPUs — VRAM and % should both sum, in device order.
    fake->computeByDevice[0] = {{2000, 1000ULL * 1024 * 1024}};
    fake->computeByDevice[1] = {{2000, 2000ULL * 1024 * 1024}};
    fake->utilByDevice[0] = {{2000, 1000, 30}};
    fake->utilByDevice[1] = {{2000, 1000, 25}};

    NvmlProcessSampler sampler(std::move(fake));
    sampler.refresh();

    const auto sample = sampler.get(2000);
    QCOMPARE(sample.vramBytes, static_cast<qint64>(3000ULL * 1024 * 1024));
    QCOMPARE(sample.gpuPercent, 55.0);
}

void TestNvmlProcessSampler::computeAndGraphics_sameDevice_takesMaxNotSum()
{
    auto fake = std::make_unique<FakeNvmlBackend>();
    fake->devices = 1;
    // A pid appearing in both the compute and graphics process lists for the
    // same device reports the same underlying allocation from each call —
    // summing them would double-count VRAM.
    fake->computeByDevice[0] = {{3000, 400ULL * 1024 * 1024}};
    fake->graphicsByDevice[0] = {{3000, 900ULL * 1024 * 1024}};

    NvmlProcessSampler sampler(std::move(fake));
    sampler.refresh();

    const auto sample = sampler.get(3000);
    QCOMPARE(sample.vramBytes, static_cast<qint64>(900ULL * 1024 * 1024));
}

void TestNvmlProcessSampler::utilization_multipleSamplesPerPid_keepsLatestOnly()
{
    auto fake = std::make_unique<FakeNvmlBackend>();
    fake->devices = 1;
    // NVML can return several utilization samples per pid within one query
    // window (one per internal sampling tick) — only the newest should count,
    // not the sum of all of them.
    fake->utilByDevice[0] = {
        {4000, 500, 10},
        {4000, 1500, 77},   // newest — should win.
        {4000, 900, 40},
    };

    NvmlProcessSampler sampler(std::move(fake));
    sampler.refresh();

    QCOMPARE(sampler.get(4000).gpuPercent, 77.0);
}

void TestNvmlProcessSampler::pruneDeadPids_dropsMissingEntries()
{
    auto fake = std::make_unique<FakeNvmlBackend>();
    fake->devices = 1;
    fake->computeByDevice[0] = {{5000, 100}, {5001, 200}};

    NvmlProcessSampler sampler(std::move(fake));
    sampler.refresh();

    QVERIFY(sampler.get(5000).vramBytes >= 0);
    QVERIFY(sampler.get(5001).vramBytes >= 0);

    sampler.pruneDeadPids({5000});

    QVERIFY(sampler.get(5000).vramBytes >= 0);
    QCOMPARE(sampler.get(5001).vramBytes, static_cast<qint64>(-1));
}

void TestNvmlProcessSampler::unseenPid_returnsSentinelSample()
{
    auto fake = std::make_unique<FakeNvmlBackend>();
    fake->devices = 1;

    NvmlProcessSampler sampler(std::move(fake));
    sampler.refresh();

    const auto sample = sampler.get(99999);
    QCOMPARE(sample.gpuPercent, -1.0);
    QCOMPARE(sample.vramBytes, static_cast<qint64>(-1));
}

QTEST_MAIN(TestNvmlProcessSampler)
#include "test_nvml_process_sampler.moc"
