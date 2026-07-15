#include "cpu_info_macos.h"

#include <QDebug>
#include <QRegularExpression>
#include "command_util.h"

#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/processor_info.h>
#include <mach/mach_host.h>

int CpuInfoMacOS::getCpuPhysicalCoreCount() const
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

int CpuInfoMacOS::getCpuCoreCount() const
{
    static int count = 0;
    if (!count) {
        int val = 0;
        size_t len = sizeof(val);
        if (sysctlbyname("hw.logicalcpu", &val, &len, nullptr, 0) == 0)
            count = val;
    }
    return count;
}

QList<double> CpuInfoMacOS::getLoadAvgs() const
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

double CpuInfoMacOS::getAvgClock() const
{
    if (mCachedClockMHz >= 0)
        return mCachedClockMHz;

    uint64_t freq = 0;
    size_t len = sizeof(freq);

    // Try hw.cpufrequency (Intel Macs)
    if (sysctlbyname("hw.cpufrequency", &freq, &len, nullptr, 0) == 0 && freq > 0) {
        mCachedClockMHz = freq / 1e6;
        return mCachedClockMHz;
    }

    // Apple Silicon: real-time frequency requires root (powermetrics).
    freq = 0;
    len = sizeof(freq);
    if (sysctlbyname("hw.cpufrequency_max", &freq, &len, nullptr, 0) == 0 && freq > 0) {
        mCachedClockMHz = freq / 1e6;
        return mCachedClockMHz;
    }

    // Last resort: look up known max frequencies by chip name.
    ExecResult brandResult = CommandUtil::execWithStatus("sysctl", {"-n", "machdep.cpu.brand_string"});
    QString brand = brandResult.output.trimmed();
    if (brand.isEmpty()) {
        if (!brandResult.ok())
            qWarning() << "cpu_info: sysctl machdep.cpu.brand_string failed:" << brandResult.error;
        ExecResult modelResult = CommandUtil::execWithStatus("sysctl", {"-n", "hw.model"});
        brand = modelResult.output.trimmed();
        if (brand.isEmpty() && !modelResult.ok())
            qWarning() << "cpu_info: sysctl hw.model failed:" << modelResult.error;
    }

    static const QMap<QString, double> knownFreqs = {
        {"Apple M1",       3200}, {"Apple M1 Pro",   3200},
        {"Apple M1 Max",   3200}, {"Apple M1 Ultra", 3200},
        {"Apple M2",       3500}, {"Apple M2 Pro",   3500},
        {"Apple M2 Max",   3500}, {"Apple M2 Ultra", 3500},
        {"Apple M3",       4050}, {"Apple M3 Pro",   4050},
        {"Apple M3 Max",   4050}, {"Apple M3 Ultra", 4050},
        {"Apple M4",       4400}, {"Apple M4 Pro",   4500},
        {"Apple M4 Max",   4500}, {"Apple M4 Ultra", 4500},
    };

    for (auto it = knownFreqs.constBegin(); it != knownFreqs.constEnd(); ++it) {
        if (brand.contains(it.key(), Qt::CaseInsensitive)) {
            mCachedClockMHz = it.value();
            return mCachedClockMHz;
        }
    }

    mCachedClockMHz = 0.0;
    return mCachedClockMHz;
}

QList<double> CpuInfoMacOS::getClocks() const
{
    // macOS doesn't expose per-core clock speeds
    QList<double> clocks;
    double avg = getAvgClock();
    int cores = getCpuCoreCount();
    for (int i = 0; i < cores; ++i)
        clocks.append(avg);
    return clocks;
}

QList<int> CpuInfoMacOS::getCpuPercents() const
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

