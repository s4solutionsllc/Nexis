#ifndef GPU_INFO_H
#define GPU_INFO_H

#include <QList>
#include <QString>
#include "Utils/file_util.h"
#include "nexis-core_global.h"

struct GpuDevice {
    QString name;           // e.g. "NVIDIA GeForce RTX 3080", "AMD Radeon RX 6800"
    QString vendor;         // "NVIDIA", "AMD", "Intel", "Apple"
    int     utilization;    // 0–100 percent (-1 if unavailable)

    // Platform-specific fields used internally for re-reading utilization
    QString sysfsLoadPath;  // Linux: path to gpu_busy_percent or similar
    QString queryCommand;   // Linux: nvidia-smi command for this GPU (if NVIDIA)
    int     deviceIndex;    // index within vendor's enumeration
};

class NEXISCORESHARED_EXPORT GpuInfo
{
public:
    virtual ~GpuInfo() = default;

    QList<GpuDevice> getGpuDevices() const;
    virtual void updateGpuInfo() = 0;
    bool hasGpu() const;

protected:
    virtual void discoverGpus() = 0;
    QList<GpuDevice> mDevices;
};

#endif // GPU_INFO_H
