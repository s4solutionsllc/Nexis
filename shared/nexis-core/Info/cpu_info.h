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

    // Static parsing methods for testability (FR-76).
    // Counts unique (physical_id, core_id) pairs from /proc/cpuinfo lines.
    static int parsePhysicalCoreCount(const QStringList &procCpuinfoLines);

    // Counts "processor" entries in /proc/cpuinfo lines.
    static int parseLogicalCoreCount(const QStringList &procCpuinfoLines);

    // Extracts "CPU MHz" value from lscpu output. Returns 0.0 if not found.
    static double parseAvgClockFromLscpu(const QString &lscpuOutput);

    // Extracts per-core "cpu MHz" values from /proc/cpuinfo lines.
    static QList<double> parseClocksFromProcCpuinfo(const QStringList &lines);

    // Parses /proc/loadavg content into 3 load average doubles.
    static QList<double> parseLoadAvgs(const QString &procLoadavgContent);
};

#endif // CPUINFO_H
