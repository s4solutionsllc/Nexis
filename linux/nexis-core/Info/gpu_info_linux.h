#ifndef GPU_INFO_LINUX_H
#define GPU_INFO_LINUX_H

#include <Info/gpu_info.h>
#include <QDateTime>

class GpuInfoLinux : public GpuInfo
{
public:
    GpuInfoLinux();

    void updateGpuInfo() override;
    QString getDiagnosticReport() const override;

protected:
    void discoverGpus() override;

private:
    struct DiagEntry {
        QString cardEntry;   // e.g. "card0"
        QString driverName;  // e.g. "nvidia", "simple-framebuffer"
        QString vendorId;    // e.g. "0x10de"
        QString pciBusId;    // e.g. "0000:04:00.0"
        QString deviceName;  // resolved device name
        QString status;      // e.g. "detected", "skipped (unknown vendor)"
    };
    QList<DiagEntry> mDiagEntries;
};

#endif // GPU_INFO_LINUX_H
