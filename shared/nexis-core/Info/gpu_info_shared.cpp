// Shared GpuInfo methods — identical across platforms.
// Platform-specific constructor, discoverGpus(), and updateGpuInfo() live in
// linux/nexis-core/Info/gpu_info.cpp and macos/nexis-core/Info/gpu_info.cpp.

#include "gpu_info.h"

QList<GpuDevice> GpuInfo::getGpuDevices() const
{
    return mDevices;
}

bool GpuInfo::hasGpu() const
{
    return !mDevices.isEmpty();
}
