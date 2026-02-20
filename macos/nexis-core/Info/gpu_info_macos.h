#ifndef GPU_INFO_MACOS_H
#define GPU_INFO_MACOS_H

#include <Info/gpu_info.h>

class GpuInfoMacOS : public GpuInfo
{
public:
    GpuInfoMacOS();

    void updateGpuInfo() override;

protected:
    void discoverGpus() override;
};

#endif // GPU_INFO_MACOS_H
