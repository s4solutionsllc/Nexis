#include "nvml_backend.h"

#include <dlfcn.h>

namespace {

using nvmlReturn_t = int;
constexpr nvmlReturn_t kNvmlSuccess = 0;
constexpr nvmlReturn_t kNvmlErrorInsufficientSize = 7;

using nvmlDevice_t = void *;

// Mirrors nvmlProcessInfo_v2_st, which nvmlDeviceGet{Compute,Graphics}
// RunningProcesses_v3 both reuse verbatim (see nvml_backend.h for why there's
// no trailing usedGpuCcProtectedMemory field here).
struct nvmlProcessInfo_v3_t {
    unsigned int pid;
    unsigned long long usedGpuMemory;
    unsigned int gpuInstanceId;
    unsigned int computeInstanceId;
};

struct nvmlProcessUtilizationSample_t {
    unsigned int pid;
    unsigned long long timeStamp;
    unsigned int smUtil;
    unsigned int memUtil;
    unsigned int encUtil;
    unsigned int decUtil;
};

using PFN_nvmlInit_v2 = nvmlReturn_t (*)();
using PFN_nvmlShutdown = nvmlReturn_t (*)();
using PFN_nvmlDeviceGetCount_v2 = nvmlReturn_t (*)(unsigned int *);
using PFN_nvmlDeviceGetHandleByIndex_v2 = nvmlReturn_t (*)(unsigned int, nvmlDevice_t *);
using PFN_nvmlDeviceGetComputeRunningProcesses_v3 =
    nvmlReturn_t (*)(nvmlDevice_t, unsigned int *, nvmlProcessInfo_v3_t *);
using PFN_nvmlDeviceGetGraphicsRunningProcesses_v3 =
    nvmlReturn_t (*)(nvmlDevice_t, unsigned int *, nvmlProcessInfo_v3_t *);
using PFN_nvmlDeviceGetProcessUtilization =
    nvmlReturn_t (*)(nvmlDevice_t, nvmlProcessUtilizationSample_t *, unsigned int *, unsigned long long);

template <typename Fn>
bool resolve(void *lib, const char *name, Fn &out)
{
    out = reinterpret_cast<Fn>(dlsym(lib, name));
    return out != nullptr;
}

// Shared two-call NVML pattern: first call with a null buffer to learn the
// required count (returns NVML_ERROR_INSUFFICIENT_SIZE), then call again
// with a buffer of that size.
template <typename QueryFn, typename RawSample>
QList<NvmlProcMemSample> queryProcessMemList(QueryFn query, nvmlDevice_t device)
{
    QList<NvmlProcMemSample> out;
    if (!query || !device)
        return out;

    unsigned int count = 0;
    nvmlReturn_t rc = query(device, &count, nullptr);
    if (rc != kNvmlErrorInsufficientSize && rc != kNvmlSuccess)
        return out;
    if (count == 0)
        return out;

    QList<RawSample> buf(static_cast<int>(count));
    rc = query(device, &count, buf.data());
    if (rc != kNvmlSuccess)
        return out;

    out.reserve(static_cast<int>(count));
    for (unsigned int i = 0; i < count; ++i)
        out.append({buf[i].pid, buf[i].usedGpuMemory});
    return out;
}

} // namespace

struct NvmlLibraryBackend::Impl {
    void *lib = nullptr;
    bool initialized = false;

    PFN_nvmlInit_v2 nvmlInit_v2 = nullptr;
    PFN_nvmlShutdown nvmlShutdown = nullptr;
    PFN_nvmlDeviceGetCount_v2 nvmlDeviceGetCount_v2 = nullptr;
    PFN_nvmlDeviceGetHandleByIndex_v2 nvmlDeviceGetHandleByIndex_v2 = nullptr;
    PFN_nvmlDeviceGetComputeRunningProcesses_v3 nvmlDeviceGetComputeRunningProcesses_v3 = nullptr;
    PFN_nvmlDeviceGetGraphicsRunningProcesses_v3 nvmlDeviceGetGraphicsRunningProcesses_v3 = nullptr;
    PFN_nvmlDeviceGetProcessUtilization nvmlDeviceGetProcessUtilization = nullptr;
};

NvmlLibraryBackend::NvmlLibraryBackend() : d(new Impl) {}

NvmlLibraryBackend::~NvmlLibraryBackend()
{
    shutdown();
    delete d;
}

bool NvmlLibraryBackend::init()
{
    if (d->initialized)
        return true;

    if (!d->lib) {
        d->lib = dlopen("libnvidia-ml.so.1", RTLD_LAZY | RTLD_LOCAL);
        if (!d->lib)
            d->lib = dlopen("libnvidia-ml.so", RTLD_LAZY | RTLD_LOCAL);
    }
    if (!d->lib)
        return false;

    const bool haveAllSymbols =
        resolve(d->lib, "nvmlInit_v2", d->nvmlInit_v2) &&
        resolve(d->lib, "nvmlShutdown", d->nvmlShutdown) &&
        resolve(d->lib, "nvmlDeviceGetCount_v2", d->nvmlDeviceGetCount_v2) &&
        resolve(d->lib, "nvmlDeviceGetHandleByIndex_v2", d->nvmlDeviceGetHandleByIndex_v2) &&
        resolve(d->lib, "nvmlDeviceGetComputeRunningProcesses_v3", d->nvmlDeviceGetComputeRunningProcesses_v3) &&
        resolve(d->lib, "nvmlDeviceGetGraphicsRunningProcesses_v3", d->nvmlDeviceGetGraphicsRunningProcesses_v3) &&
        resolve(d->lib, "nvmlDeviceGetProcessUtilization", d->nvmlDeviceGetProcessUtilization);

    if (!haveAllSymbols) {
        dlclose(d->lib);
        d->lib = nullptr;
        return false;
    }

    if (d->nvmlInit_v2() != kNvmlSuccess) {
        dlclose(d->lib);
        d->lib = nullptr;
        return false;
    }

    d->initialized = true;
    return true;
}

void NvmlLibraryBackend::shutdown()
{
    if (d->initialized && d->nvmlShutdown)
        d->nvmlShutdown();
    d->initialized = false;

    if (d->lib) {
        dlclose(d->lib);
        d->lib = nullptr;
    }
}

unsigned int NvmlLibraryBackend::deviceCount() const
{
    if (!d->initialized)
        return 0;

    unsigned int count = 0;
    if (d->nvmlDeviceGetCount_v2(&count) != kNvmlSuccess)
        return 0;
    return count;
}

void *NvmlLibraryBackend::handleFor(unsigned int deviceIndex) const
{
    nvmlDevice_t dev = nullptr;
    if (!d->initialized || d->nvmlDeviceGetHandleByIndex_v2(deviceIndex, &dev) != kNvmlSuccess)
        return nullptr;
    return dev;
}

QList<NvmlProcMemSample> NvmlLibraryBackend::computeProcesses(unsigned int deviceIndex) const
{
    nvmlDevice_t dev = handleFor(deviceIndex);
    return queryProcessMemList<PFN_nvmlDeviceGetComputeRunningProcesses_v3, nvmlProcessInfo_v3_t>(
        d->nvmlDeviceGetComputeRunningProcesses_v3, dev);
}

QList<NvmlProcMemSample> NvmlLibraryBackend::graphicsProcesses(unsigned int deviceIndex) const
{
    nvmlDevice_t dev = handleFor(deviceIndex);
    return queryProcessMemList<PFN_nvmlDeviceGetGraphicsRunningProcesses_v3, nvmlProcessInfo_v3_t>(
        d->nvmlDeviceGetGraphicsRunningProcesses_v3, dev);
}

QList<NvmlProcUtilSample> NvmlLibraryBackend::processUtilization(unsigned int deviceIndex, quint64 sinceUs) const
{
    QList<NvmlProcUtilSample> out;
    nvmlDevice_t dev = handleFor(deviceIndex);
    if (!dev || !d->nvmlDeviceGetProcessUtilization)
        return out;

    unsigned int count = 0;
    nvmlReturn_t rc = d->nvmlDeviceGetProcessUtilization(dev, nullptr, &count, sinceUs);
    if (rc != kNvmlErrorInsufficientSize && rc != kNvmlSuccess)
        return out;
    if (count == 0)
        return out;

    QList<nvmlProcessUtilizationSample_t> buf(static_cast<int>(count));
    rc = d->nvmlDeviceGetProcessUtilization(dev, buf.data(), &count, sinceUs);
    if (rc != kNvmlSuccess)
        return out;

    out.reserve(static_cast<int>(count));
    for (unsigned int i = 0; i < count; ++i)
        out.append({buf[i].pid, buf[i].timeStamp, buf[i].smUtil});
    return out;
}
