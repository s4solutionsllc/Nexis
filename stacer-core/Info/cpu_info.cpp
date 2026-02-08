#include "cpu_info.h"

#include <QRegularExpression>
#include "command_util.h"

#ifdef Q_OS_LINUX

int CpuInfo::getCpuPhysicalCoreCount() const
{
    static int count = 0;

    if (! count) {
        QStringList cpuinfo = FileUtil::readListFromFile(PROC_CPUINFO);

        if (! cpuinfo.isEmpty()) {
	    QSet<QPair<int, int> > physicalCoreSet;
	    int physical = 0;
	    int core = 0;
	    for (int i = 0; i < cpuinfo.size(); ++i) {
	        const QString& line = cpuinfo[i];
		if (line.startsWith("physical id")) {
		    QStringList fields = line.split(": ");
		    if (fields.size() > 1)
		        physical = fields[1].toInt();
		}
		if (line.startsWith("core id")) {
		    QStringList fields = line.split(": ");
		    if (fields.size() > 1)
		        core = fields[1].toInt();
		    // We assume core id appears after physical id.
		    physicalCoreSet.insert(qMakePair(physical, core));
		}
	    }
	    count = physicalCoreSet.size();
	}
    }

    return count;
}

int CpuInfo::getCpuCoreCount() const
{
    static quint8 count = 0;

    if (! count) {
        QStringList cpuinfo = FileUtil::readListFromFile(PROC_CPUINFO);

        if (! cpuinfo.isEmpty())
            count = cpuinfo.filter(QRegularExpression("^processor")).count();
    }

    return count;
}

QList<double> CpuInfo::getLoadAvgs() const
{
    QList<double> avgs = {0, 0, 0};

    QStringList strListAvgs = FileUtil::readStringFromFile(PROC_LOADAVG).split(QRegularExpression("\\s+"));

    if (strListAvgs.count() > 2) {
        avgs.clear();
        avgs << strListAvgs.takeFirst().toDouble();
        avgs << strListAvgs.takeFirst().toDouble();
        avgs << strListAvgs.takeFirst().toDouble();
    }

    return avgs;
}

double CpuInfo::getAvgClock() const
{
    const QStringList lines = CommandUtil::exec("bash",{"-c", LSCPU_COMMAND}).split('\n');
    const QStringList filtered = lines.filter(QRegularExpression("^CPU MHz"));
    if (!filtered.isEmpty()) {
        return filtered.first().split(":").last().toDouble();
    }
    // Fallback: average the per-core clocks from /proc/cpuinfo
    const QList<double> clocks = getClocks();
    if (clocks.isEmpty())
        return 0.0;
    double sum = 0.0;
    for (double c : clocks)
        sum += c;
    return sum / clocks.size();
}

QList<double> CpuInfo::getClocks() const
{
    QStringList lines = FileUtil::readListFromFile(PROC_CPUINFO)
            .filter(QRegularExpression("^cpu MHz"));

    QList<double> clocks;
    for(auto line: lines){
        clocks.push_back(line.split(":").last().toDouble());
    }
    return clocks;
}

QList<int> CpuInfo::getCpuPercents() const
{
    QList<double> cpuTimes;

    QList<int> cpuPercents;

    QStringList times = FileUtil::readListFromFile(PROC_STAT);

    if (! times.isEmpty())
    {
        QRegularExpression sep("\\s+");
        int count = CpuInfo::getCpuCoreCount() + 1;
        for (int i = 0; i < count; ++i)
        {
            QStringList n_times = times.at(i).split(sep);
            n_times.removeFirst();
            for (const QString &t : n_times)
                cpuTimes << t.toDouble();

            cpuPercents << getCpuPercent(cpuTimes, i);

            cpuTimes.clear();
        }
    }

    return cpuPercents;
}

int CpuInfo::getCpuPercent(const QList<double> &cpuTimes, const int &processor) const
{
    const int N = getCpuCoreCount()+1;

    static QVector<double> l_idles(N);
    static QVector<double> l_totals(N);

    int utilisation = 0;

    if (cpuTimes.count() > 0) {

        double idle = cpuTimes.at(3) + cpuTimes.at(4); // get (idle + iowait)
        double total = 0.0;
        for (const double &t : cpuTimes) total += t; // get total time

        double idle_delta  = idle  - l_idles[processor];
        double total_delta = total - l_totals[processor];

        if (total_delta)
            utilisation = 100 * ((total_delta - idle_delta) / total_delta);

        l_idles[processor] = idle;
        l_totals[processor] = total;
    }

    if (utilisation > 100) utilisation = 100;
    else if (utilisation < 0) utilisation = 0;

    return utilisation;
}

#elif defined(Q_OS_MACOS)

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

#endif
