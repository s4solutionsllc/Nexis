#include "nvml_process_sampler.h"

#include <QMutexLocker>

NvmlProcessSampler::NvmlProcessSampler(std::unique_ptr<NvmlBackend> backend)
    : mBackend(backend ? std::move(backend) : std::make_unique<NvmlLibraryBackend>())
{
}

NvmlProcessSampler::~NvmlProcessSampler()
{
    if (mInitOk)
        mBackend->shutdown();
}

bool NvmlProcessSampler::isAvailable()
{
    if (!mInitAttempted) {
        mInitAttempted = true;
        mInitOk = mBackend->init();
    }
    return mInitOk;
}

void NvmlProcessSampler::refresh()
{
    if (!isAvailable())
        return;

    QHash<pid_t, quint64> memByPid;
    QHash<pid_t, double> utilByPid;
    quint64 maxTimestampSeen = mLastUtilQueryUs;

    const unsigned int deviceCount = mBackend->deviceCount();
    for (unsigned int dev = 0; dev < deviceCount; ++dev) {
        // A process using both compute and graphics contexts on the same
        // device reports the same underlying allocation from both calls —
        // take the max, not the sum, to avoid double-counting VRAM.
        QHash<pid_t, quint64> devMem;
        for (const auto &s : mBackend->computeProcesses(dev)) {
            const pid_t pid = static_cast<pid_t>(s.pid);
            devMem[pid] = qMax(devMem.value(pid, 0), s.usedMemoryBytes);
        }
        for (const auto &s : mBackend->graphicsProcesses(dev)) {
            const pid_t pid = static_cast<pid_t>(s.pid);
            devMem[pid] = qMax(devMem.value(pid, 0), s.usedMemoryBytes);
        }

        // Sum across devices in index order — a multi-GPU workload's VRAM
        // footprint is the total across every GPU it touches.
        for (auto it = devMem.constBegin(); it != devMem.constEnd(); ++it)
            memByPid[it.key()] += it.value();

        // NVML can return several utilization samples per PID within the
        // query window (one per internal sampling tick) — keep only the
        // most recent before folding into the cross-device sum, or a busy
        // window would silently multiply a single PID's percent.
        QHash<pid_t, NvmlProcUtilSample> latestPerPid;
        for (const auto &u : mBackend->processUtilization(dev, mLastUtilQueryUs)) {
            const pid_t pid = static_cast<pid_t>(u.pid);
            const auto it = latestPerPid.constFind(pid);
            if (it == latestPerPid.constEnd() || u.timestampUs > it->timestampUs)
                latestPerPid[pid] = u;
            if (u.timestampUs > maxTimestampSeen)
                maxTimestampSeen = u.timestampUs;
        }
        for (auto it = latestPerPid.constBegin(); it != latestPerPid.constEnd(); ++it)
            utilByPid[it.key()] += it->smUtil;
    }

    // Advance the cursor to the newest sample timestamp actually observed,
    // not wall-clock "now" — NVML timestamps trail real time by however long
    // the driver took to produce the sample, so anchoring on "now" risks
    // skipping samples on the next call.
    mLastUtilQueryUs = maxTimestampSeen;

    QSet<pid_t> pids;
    for (auto it = memByPid.constBegin(); it != memByPid.constEnd(); ++it)
        pids.insert(it.key());
    for (auto it = utilByPid.constBegin(); it != utilByPid.constEnd(); ++it)
        pids.insert(it.key());

    QMutexLocker lock(&mMutex);
    mLatest.clear();
    for (pid_t pid : pids) {
        Sample s;
        if (memByPid.contains(pid))
            s.vramBytes = static_cast<qint64>(memByPid.value(pid));
        if (utilByPid.contains(pid))
            s.gpuPercent = qBound(0.0, utilByPid.value(pid), 100.0);
        mLatest.insert(pid, s);
    }
}

NvmlProcessSampler::Sample NvmlProcessSampler::get(pid_t pid) const
{
    QMutexLocker lock(&mMutex);
    return mLatest.value(pid, Sample{});
}

void NvmlProcessSampler::pruneDeadPids(const QSet<pid_t> &alivePids)
{
    QMutexLocker lock(&mMutex);
    for (auto it = mLatest.begin(); it != mLatest.end(); ) {
        if (!alivePids.contains(it.key()))
            it = mLatest.erase(it);
        else
            ++it;
    }
}
