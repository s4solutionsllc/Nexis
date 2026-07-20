#ifndef NVML_PROCESS_SAMPLER_H
#define NVML_PROCESS_SAMPLER_H

#include <QHash>
#include <QMutex>
#include <QSet>
#include <QString>
#include <memory>

#include <sys/types.h>

#include "nvml_backend.h"

// SSO-15374: per-process NVIDIA sampler for the Processes page, replacing
// the FR-115 nvidia-smi-pmon CLI streamer with direct NVML per-PID calls
// (nvmlDeviceGet{Compute,Graphics}RunningProcesses +
// nvmlDeviceGetProcessUtilization). No subprocess: NVML calls are in-process
// library calls, so refresh() is synchronous and meant to be called once per
// collection tick (mirrors the old streamer's Sample/get()/pruneDeadPids()
// contract so process_info.cpp's call site barely changes).
//
// Backend is injectable so the device-order aggregation below — the part
// with actual logic worth testing — can run against a FakeNvmlBackend in
// tests/core/test_nvml_process_sampler.cpp without NVIDIA hardware or the
// NVML library present.
class NvmlProcessSampler
{
public:
    struct Sample {
        double gpuPercent = -1;   // -1 = unknown, matches Process::gpuPercent sentinel
        qint64 vramBytes = -1;    // -1 = unknown, matches Process::gpuVramBytes sentinel
    };

    explicit NvmlProcessSampler(std::unique_ptr<NvmlBackend> backend = nullptr);
    ~NvmlProcessSampler();

    // Lazily dlopen's/initializes the backend on first call and caches the
    // result — NVML absence (no driver, no library) is the common case and
    // shouldn't be re-probed every tick.
    bool isAvailable();

    // Queries every device in index order and rebuilds the per-PID sample
    // map. No-op if the backend isn't available.
    void refresh();

    Sample get(pid_t pid) const;

    // Drop cached entries for PIDs no longer seen this tick, so mLatest
    // doesn't grow unbounded across the sampler's app-lifetime singleton.
    void pruneDeadPids(const QSet<pid_t> &alivePids);

private:
    std::unique_ptr<NvmlBackend> mBackend;
    bool mInitAttempted = false;
    bool mInitOk = false;
    quint64 mLastUtilQueryUs = 0;

    mutable QMutex mMutex;
    QHash<pid_t, Sample> mLatest;
};

#endif // NVML_PROCESS_SAMPLER_H
