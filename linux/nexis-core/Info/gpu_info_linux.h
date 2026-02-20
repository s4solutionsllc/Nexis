#ifndef GPU_INFO_LINUX_H
#define GPU_INFO_LINUX_H

#include <Info/gpu_info.h>

class GpuInfoLinux : public GpuInfo
{
public:
    GpuInfoLinux();

    void updateGpuInfo() override;

protected:
    void discoverGpus() override;
};

#endif // GPU_INFO_LINUX_H
