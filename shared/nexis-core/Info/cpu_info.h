#ifndef CPUINFO_H
#define CPUINFO_H

#include <QDebug>
#include <QVector>

#include "Utils/file_util.h"

#include "nexis-core_global.h"

class NEXISCORESHARED_EXPORT CpuInfo
{
public:
    virtual ~CpuInfo() = default;

    virtual int getCpuPhysicalCoreCount() const = 0;
    virtual int getCpuCoreCount() const = 0;
    virtual QList<int> getCpuPercents() const = 0;
    virtual QList<double> getLoadAvgs() const = 0;
    virtual double getAvgClock() const = 0;
    virtual QList<double> getClocks() const = 0;
};

#endif // CPUINFO_H
