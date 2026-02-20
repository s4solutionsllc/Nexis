#ifndef CPUINFO_LINUX_H
#define CPUINFO_LINUX_H

#include <Info/cpu_info.h>

class CpuInfoLinux : public CpuInfo
{
public:
    int getCpuPhysicalCoreCount() const override;
    int getCpuCoreCount() const override;
    QList<int> getCpuPercents() const override;
    QList<double> getLoadAvgs() const override;
    double getAvgClock() const override;
    QList<double> getClocks() const override;

private:
    int getCpuPercent(const QList<double> &cpuTimes, const int &processor = 0) const;
};

#endif // CPUINFO_LINUX_H
