#include "cpu_info.h"

#include <QRegularExpression>
#include "command_util.h"

#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/processor_info.h>
#include <mach/mach_host.h>

int CpuInfo::getCpuPhysicalCoreCount() const
{
    static int count = 0;
    if (!count) {
        int val = 0;
        size_t len = sizeof(val);
        if (sysctlbyname("hw.physicalcpu", &val, &len, nullptr, 0) == 0)
            count = val;
    }
    return count;
}

int CpuInfo::getCpuCoreCount() const
{
    static quint8 count = 0;
    if (!count) {
        int val = 0;
        size_t len = sizeof(val);
        if (sysctlbyname("hw.logicalcpu", &val, &len, nullptr, 0) == 0)
            count = static_cast<quint8>(val);
    }
    return count;
}

QList<double> CpuInfo::getLoadAvgs() const
{
    QList<double> avgs = {0, 0, 0};
    double loadavg[3];
    if (getloadavg(loadavg, 3) == 3) {
        avgs[0] = loadavg[0];
        avgs[1] = loadavg[1];
        avgs[2] = loadavg[2];
    }
    return avgs;
}

double CpuInfo::getAvgClock() const
{
    // On macOS, use sysctl to get CPU frequency
    uint64_t freq = 0;
    size_t len = sizeof(freq);
    // Try hw.cpufrequency (Intel Macs)
    if (sysctlbyname("hw.cpufrequency", &freq, &len, nullptr, 0) == 0) {
        return freq / 1e6; // Convert Hz to MHz
    }
    // On Apple Silicon, try to get it from sysctl brand string
    try {
        QString brand = CommandUtil::exec("sysctl", {"-n", "machdep.cpu.brand_string"});
        // e.g. "Apple M1 Pro" — no frequency in brand string on AS
        // Fall back to 0 and let UI handle it
    } catch (...) {}
    return 0.0;
}

QList<double> CpuInfo::getClocks() const
{
    // macOS doesn't expose per-core clock speeds
    QList<double> clocks;
    double avg = getAvgClock();
    int cores = getCpuCoreCount();
    for (int i = 0; i < cores; ++i)
        clocks.append(avg);
    return clocks;
}

QList<int> CpuInfo::getCpuPercents() const
{
    QList<int> cpuPercents;

    natural_t numCPUs = 0;
    processor_info_array_t cpuInfo;
    mach_msg_type_number_t numCpuInfo;

    kern_return_t err = host_processor_info(mach_host_self(),
                                            PROCESSOR_CPU_LOAD_INFO,
                                            &numCPUs,
                                            &cpuInfo,
                                            &numCpuInfo);
    if (err != KERN_SUCCESS)
        return cpuPercents;

    static QVector<double> l_idles;
    static QVector<double> l_totals;
    if (l_idles.size() != static_cast<int>(numCPUs) + 1) {
        l_idles.resize(numCPUs + 1);
        l_totals.resize(numCPUs + 1);
        l_idles.fill(0);
        l_totals.fill(0);
    }

    // Overall CPU (index 0)
    double totalUser = 0, totalSystem = 0, totalIdle = 0, totalNice = 0;
    for (natural_t i = 0; i < numCPUs; ++i) {
        totalUser   += cpuInfo[CPU_STATE_MAX * i + CPU_STATE_USER];
        totalSystem += cpuInfo[CPU_STATE_MAX * i + CPU_STATE_SYSTEM];
        totalIdle   += cpuInfo[CPU_STATE_MAX * i + CPU_STATE_IDLE];
        totalNice   += cpuInfo[CPU_STATE_MAX * i + CPU_STATE_NICE];
    }
    double totalAll = totalUser + totalSystem + totalIdle + totalNice;
    double idleDelta = totalIdle - l_idles[0];
    double totalDelta = totalAll - l_totals[0];
    int utilisation = 0;
    if (totalDelta > 0)
        utilisation = static_cast<int>(100.0 * (totalDelta - idleDelta) / totalDelta);
    if (utilisation > 100) utilisation = 100;
    if (utilisation < 0) utilisation = 0;
    cpuPercents << utilisation;
    l_idles[0] = totalIdle;
    l_totals[0] = totalAll;

    // Per-core
    for (natural_t i = 0; i < numCPUs; ++i) {
        double user   = cpuInfo[CPU_STATE_MAX * i + CPU_STATE_USER];
        double system = cpuInfo[CPU_STATE_MAX * i + CPU_STATE_SYSTEM];
        double idle   = cpuInfo[CPU_STATE_MAX * i + CPU_STATE_IDLE];
        double nice   = cpuInfo[CPU_STATE_MAX * i + CPU_STATE_NICE];
        double total  = user + system + idle + nice;

        int idx = static_cast<int>(i) + 1;
        double id = idle - l_idles[idx];
        double td = total - l_totals[idx];
        int util = 0;
        if (td > 0)
            util = static_cast<int>(100.0 * (td - id) / td);
        if (util > 100) util = 100;
        if (util < 0) util = 0;
        cpuPercents << util;
        l_idles[idx] = idle;
        l_totals[idx] = total;
    }

    vm_deallocate(mach_task_self(), (vm_address_t)cpuInfo,
                  numCpuInfo * sizeof(integer_t));

    return cpuPercents;
}

int CpuInfo::getCpuPercent(const QList<double> &cpuTimes, const int &processor) const
{
    Q_UNUSED(cpuTimes);
    Q_UNUSED(processor);
    return 0;
}
