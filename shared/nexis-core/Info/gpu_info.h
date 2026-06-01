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
    QString driverName;     // kernel driver (e.g. "nvidia", "amdgpu", "simple-framebuffer")

    // Platform-specific fields used internally for re-reading utilization
    QString sysfsLoadPath;  // Linux: path to gpu_busy_percent or similar
    QString queryCommand;   // Linux/NVIDIA: nvidia-smi device index as decimal string ("0","1",…)
                            // Linux/Intel:  sysfs path to gt_max_freq_mhz
    QString pciBusId;       // PCI bus address (e.g. "0000:03:00.0") for consistent ordering
    int     deviceIndex;    // index within vendor's enumeration
    quint64 registryEntryID = 0; // macOS: IOKit registry entry ID for matching
};

class NEXISCORESHARED_EXPORT GpuInfo
{
public:
    virtual ~GpuInfo() = default;

    QList<GpuDevice> getGpuDevices() const;
    virtual void updateGpuInfo() = 0;
    bool hasGpu() const;

    // Static parsing methods for testability (FR-76).
    // Parses nvidia-smi CSV utilization output. Returns 0-100, or -1 on error.
    static int parseNvidiaSmiUtilization(const QString &nvidiaSmiOutput);

    // Parses a sysfs integer (e.g. gpu_busy_percent). Returns 0-100, or -1 on error.
    static int parseSysfsUtilization(const QString &sysfsContent);

    // Computes utilization % from current/max frequency strings. Returns 0-100, or -1.
    static int parseIntelFreqUtilization(const QString &curFreqStr, const QString &maxFreqStr);

    // Extracts device name from lspci output for a given PCI bus ID.
    static QString parseLspciDeviceName(const QString &lspciOutput, const QString &busId);

    // Extracts the parent PCI bus address from a simple-framebuffer DRM symlink target.
    // e.g. ".../0000:04:00.0/simple-framebuffer.0/drm/card0" → "0000:04:00.0"
    static QString parseFramebufferParentPciBusId(const QString &symlinkTarget);

    // Normalizes a PCI bus ID from any tool format to "bus:device.function" lowercase.
    // e.g. "0000:07:00.0" and "00000000:07:00.0" both become "07:00.0"
    static QString normalizePciBusId(const QString &rawBusId);

    // Returns a structured diagnostic report for GPU troubleshooting.
    virtual QString getDiagnosticReport() const;

protected:
    virtual void discoverGpus() = 0;
    QList<GpuDevice> mDevices;
};

#endif // GPU_INFO_H
