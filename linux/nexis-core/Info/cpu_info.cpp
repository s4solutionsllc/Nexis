#include "cpu_info.h"

#include <QRegularExpression>
#include "command_util.h"

static constexpr const char *PROC_CPUINFO = "/proc/cpuinfo";
static constexpr const char *LSCPU_COMMAND = "LC_ALL=C lscpu";
static constexpr const char *PROC_LOADAVG = "/proc/loadavg";
static constexpr const char *PROC_STAT    = "/proc/stat";

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
    static int count = 0;

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
    // Try lscpu first
    try {
        const QStringList lines = CommandUtil::exec("bash",{"-c", LSCPU_COMMAND}).split('\n');
        const QStringList filtered = lines.filter(QRegularExpression("^CPU MHz"));
        if (!filtered.isEmpty()) {
            double mhz = filtered.first().split(":").last().toDouble();
            if (mhz > 0.0)
                return mhz;
        }
    } catch (...) {}

    // Fallback: per-core clocks from /proc/cpuinfo
    const QList<double> clocks = getClocks();
    if (!clocks.isEmpty()) {
        double sum = 0.0;
        for (double c : clocks)
            sum += c;
        double avg = sum / clocks.size();
        if (avg > 0.0)
            return avg;
    }

    // Fallback: sysfs cpufreq (works on modern kernels where /proc/cpuinfo lacks MHz)
    QString freqStr = FileUtil::readStringFromFile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq").trimmed();
    if (!freqStr.isEmpty()) {
        double khz = freqStr.toDouble();
        if (khz > 0.0)
            return khz / 1000.0; // kHz to MHz
    }

    return 0.0;
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
