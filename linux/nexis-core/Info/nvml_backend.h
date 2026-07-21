#ifndef NVML_BACKEND_H
#define NVML_BACKEND_H

#include <QList>
#include <QtGlobal>

// SSO-15374: per-process NVML sample types. Neutral (no NVML struct/enum
// leaks past this header) so NvmlProcessSampler's aggregation logic can be
// unit tested against a fake backend without linking or replicating the
// real NVML ABI.
struct NvmlProcMemSample {
    unsigned int pid = 0;
    quint64 usedMemoryBytes = 0;
};

struct NvmlProcUtilSample {
    unsigned int pid = 0;
    quint64 timestampUs = 0;
    unsigned int smUtil = 0;
};

// Seam between the aggregation logic (NvmlProcessSampler) and the real NVML
// C ABI (NvmlLibraryBackend). Lets tests substitute a FakeNvmlBackend instead
// of requiring NVIDIA hardware / the NVML library in CI.
class NvmlBackend
{
public:
    virtual ~NvmlBackend() = default;

    // Returns false if the driver/library isn't available. Safe to call
    // repeatedly; implementations should make retries cheap.
    virtual bool init() = 0;
    virtual void shutdown() = 0;

    virtual unsigned int deviceCount() const = 0;
    virtual QList<NvmlProcMemSample> computeProcesses(unsigned int deviceIndex) const = 0;
    virtual QList<NvmlProcMemSample> graphicsProcesses(unsigned int deviceIndex) const = 0;
    // sinceUs: NVML's lastSeenTimeStamp cursor (microseconds since epoch).
    // Pass 0 on the first call to get whatever samples the driver retains.
    virtual QList<NvmlProcUtilSample> processUtilization(unsigned int deviceIndex, quint64 sinceUs) const = 0;
};

// Real backend: dlopen's libnvidia-ml.so.1 and resolves the versioned NVML
// symbols directly, so Nexis doesn't need the CUDA toolkit / nvml.h to
// build. Struct layouts below are hand-mirrored from the public NVML ABI
// (confirmed against nvml.h + NVIDIA's own Python/Go bindings, which do the
// same dlopen/dlsym dance): nvmlProcessInfo_v3_t reuses the v2 struct layout
// (pid, usedGpuMemory, gpuInstanceId, computeInstanceId — no trailing
// protected-memory field, that's a different, newer struct/function pair);
// nvmlProcessUtilizationSample_t is unversioned and unchanged since
// introduction.
class NvmlLibraryBackend : public NvmlBackend
{
public:
    NvmlLibraryBackend();
    ~NvmlLibraryBackend() override;

    bool init() override;
    void shutdown() override;

    unsigned int deviceCount() const override;
    QList<NvmlProcMemSample> computeProcesses(unsigned int deviceIndex) const override;
    QList<NvmlProcMemSample> graphicsProcesses(unsigned int deviceIndex) const override;
    QList<NvmlProcUtilSample> processUtilization(unsigned int deviceIndex, quint64 sinceUs) const override;

private:
    struct Impl;
    Impl *d;

    void *handleFor(unsigned int deviceIndex) const;
};

#endif // NVML_BACKEND_H
