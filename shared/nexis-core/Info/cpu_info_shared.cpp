// Shared CpuInfo static parsing methods — platform-independent.
// Platform-specific instance methods live in
// linux/nexis-core/Info/cpu_info.cpp and macos/nexis-core/Info/cpu_info.cpp.

#include "cpu_info.h"
#include <QRegularExpression>
#include <QSet>
#include <QPair>

int CpuInfo::parsePhysicalCoreCount(const QStringList &procCpuinfoLines)
{
    QSet<QPair<int, int>> physicalCoreSet;
    int physical = 0;
    int core = 0;

    for (const QString &line : procCpuinfoLines) {
        if (line.startsWith("physical id")) {
            QStringList fields = line.split(": ");
            if (fields.size() > 1)
                physical = fields[1].toInt();
        }
        if (line.startsWith("core id")) {
            QStringList fields = line.split(": ");
            if (fields.size() > 1)
                core = fields[1].toInt();
            physicalCoreSet.insert(qMakePair(physical, core));
        }
    }

    return physicalCoreSet.size();
}

int CpuInfo::parseLogicalCoreCount(const QStringList &procCpuinfoLines)
{
    return procCpuinfoLines.filter(QRegularExpression("^processor")).count();
}

double CpuInfo::parseAvgClockFromLscpu(const QString &lscpuOutput)
{
    const QStringList lines = lscpuOutput.split('\n');
    const QStringList filtered = lines.filter(QRegularExpression("^CPU MHz"));
    if (!filtered.isEmpty()) {
        double mhz = filtered.first().split(":").last().trimmed().toDouble();
        if (mhz > 0.0)
            return mhz;
    }
    return 0.0;
}

QList<double> CpuInfo::parseClocksFromProcCpuinfo(const QStringList &lines)
{
    QStringList filtered = lines.filter(QRegularExpression("^cpu MHz"));
    QList<double> clocks;
    for (const QString &line : filtered)
        clocks.push_back(line.split(":").last().trimmed().toDouble());
    return clocks;
}

QList<double> CpuInfo::parseLoadAvgs(const QString &procLoadavgContent)
{
    QList<double> avgs = {0, 0, 0};
    QStringList parts = procLoadavgContent.trimmed().split(QRegularExpression("\\s+"));
    if (parts.count() > 2) {
        avgs.clear();
        avgs << parts.at(0).toDouble();
        avgs << parts.at(1).toDouble();
        avgs << parts.at(2).toDouble();
    }
    return avgs;
}
