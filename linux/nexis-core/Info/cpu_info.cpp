#include "cpu_info_linux.h"

#include <QDebug>
#include <QDir>
#include <QFileInfoList>
#include <QRegularExpression>
#include "command_util.h"

static constexpr const char *PROC_CPUINFO = "/proc/cpuinfo";
static constexpr const char *LSCPU_COMMAND = "LC_ALL=C lscpu";
static constexpr const char *PROC_LOADAVG = "/proc/loadavg";
static constexpr const char *PROC_STAT    = "/proc/stat";

int CpuInfoLinux::getCpuPhysicalCoreCount() const
{
    static int count = 0;

    if (!count) {
        QStringList cpuinfo = FileUtil::readListFromFile(PROC_CPUINFO);
        if (!cpuinfo.isEmpty())
            count = parsePhysicalCoreCount(cpuinfo);
    }

    return count;
}

int CpuInfoLinux::getCpuCoreCount() const
{
    static int count = 0;

    if (!count) {
        QStringList cpuinfo = FileUtil::readListFromFile(PROC_CPUINFO);
        if (!cpuinfo.isEmpty())
            count = parseLogicalCoreCount(cpuinfo);
    }

    return count;
}

QList<double> CpuInfoLinux::getLoadAvgs() const
{
    return parseLoadAvgs(FileUtil::readStringFromFile(PROC_LOADAVG));
}

double CpuInfoLinux::readSysfsAvgClockMhz() const
{
    // Iterate /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq for a true
    // per-core average rather than just cpu0.
    QDir cpuDir("/sys/devices/system/cpu");
    const QStringList coreDirs = cpuDir.entryList(
        QStringList() << "cpu[0-9]*", QDir::Dirs | QDir::NoDotAndDotDot);

    double sumKhz = 0.0;
    int samples = 0;
    for (const QString &name : coreDirs) {
        const QString path = cpuDir.absoluteFilePath(name) + "/cpufreq/scaling_cur_freq";
        const QString raw = FileUtil::readStringFromFile(path).trimmed();
        if (raw.isEmpty())
            continue;
        bool ok = false;
        double khz = raw.toDouble(&ok);
        if (ok && khz > 0.0) {
            sumKhz += khz;
            ++samples;
        }
    }

    if (samples == 0)
        return 0.0;
    return (sumKhz / samples) / 1000.0;   // kHz -> MHz
}

double CpuInfoLinux::getAvgClock() const
{
    // FR-100: sysfs first on every tick once we've confirmed it works — avoids
    // a bash+lscpu fork per second. Fall through to /proc/cpuinfo and then
    // lscpu only when sysfs is unavailable.
    if (mSysfsCpufreqAvailable != 0) {
        double mhz = readSysfsAvgClockMhz();
        if (mhz > 0.0) {
            mSysfsCpufreqAvailable = 1;
            return mhz;
        }
        if (mSysfsCpufreqAvailable == -1)
            mSysfsCpufreqAvailable = 0;   // don't keep probing a dead path
    }

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

    // Last resort: lscpu. Cache the "useless" verdict so we stop forking bash
    // every second on systems where lscpu returns 0.
    if (mLscpuUseful) {
        try {
            QString lscpuOutput = CommandUtil::exec("bash", {"-c", LSCPU_COMMAND});
            double mhz = parseAvgClockFromLscpu(lscpuOutput);
            if (mhz > 0.0)
                return mhz;
            mLscpuUseful = false;
        } catch (...) {
            qWarning() << "Failed to read CPU clock frequency";
            mLscpuUseful = false;
        }
    }

    return 0.0;
}

QList<double> CpuInfoLinux::getClocks() const
{
    return parseClocksFromProcCpuinfo(FileUtil::readListFromFile(PROC_CPUINFO));
}

QList<int> CpuInfoLinux::getCpuPercents() const
{
    QList<double> cpuTimes;

    QList<int> cpuPercents;

    QStringList times = FileUtil::readListFromFile(PROC_STAT);

    if (! times.isEmpty())
    {
        QRegularExpression sep("\\s+");
        int count = CpuInfoLinux::getCpuCoreCount() + 1;
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

int CpuInfoLinux::getCpuPercent(const QList<double> &cpuTimes, const int &processor) const
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
