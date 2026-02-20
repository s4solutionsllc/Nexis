#ifndef CPUINFO_MACOS_H
#define CPUINFO_MACOS_H

#include <Info/cpu_info.h>

class CpuInfoMacOS : public CpuInfo
{
public:
    int getCpuPhysicalCoreCount() const override;
    int getCpuCoreCount() const override;
    QList<int> getCpuPercents() const override;
    QList<double> getLoadAvgs() const override;
    double getAvgClock() const override;
    QList<double> getClocks() const override;
};

#endif // CPUINFO_MACOS_H
