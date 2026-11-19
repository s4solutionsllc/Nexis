#include "process_cpu_util.h"

#include <algorithm>
#include <cmath>

#ifdef Q_OS_MACOS
#include <sys/sysctl.h>
#include <sys/types.h>
#else
#include <unistd.h>
#endif

int ProcessCpuUtil::logicalCpuCount()
{
    static const int count = [] {
        int n = 0;
#ifdef Q_OS_MACOS
        int val = 0;
        size_t len = sizeof(val);
        if (sysctlbyname("hw.logicalcpu", &val, &len, nullptr, 0) == 0)
            n = val;
#else
        n = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
#endif
        return n > 0 ? n : 1;
    }();
    return count;
}

double ProcessCpuUtil::toSystemPercent(double perCorePercent, int logicalCpus)
{
    if (!(perCorePercent > 0.0))
        return 0.0;
    const int cpus = std::max(1, logicalCpus);
    return std::min(100.0, perCorePercent / cpus);
}

int ProcessCpuUtil::migratePerCoreThreshold(int perCorePercent, int logicalCpus)
{
    if (perCorePercent <= 0)
        return 0;
    const int cpus = std::max(1, logicalCpus);
    const int scaled = static_cast<int>(std::ceil(static_cast<double>(perCorePercent) / cpus));
    return std::clamp(scaled, 1, 100);
}

QString ProcessCpuUtil::formatPercent(double percent)
{
    return QString::number(percent, 'f', 1);
}
