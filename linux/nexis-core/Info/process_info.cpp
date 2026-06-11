#include "process_info_linux.h"

#include "nvidia_smi_pmon_streamer.h"
#include "proc_info_parser.h"
#include "Utils/command_util.h"

#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMutexLocker>
#include <QSet>

#include <grp.h>
#include <pwd.h>
#include <unistd.h>

namespace {

// Lazy-constructed singleton. Started the first time we see mCollectGpu
// true and nvidia-smi is present; stays alive for the app lifetime.
NvidiaSmiPmonStreamer *pmonStreamer()
{
    static NvidiaSmiPmonStreamer *s = new NvidiaSmiPmonStreamer();
    return s;
}

} // namespace

using ProcInfoParser::StatFields;
using ProcInfoParser::StatusFields;

namespace {

QByteArray readAll(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}

} // namespace

ProcessInfoLinux::ProcessInfoLinux()
{
    // FR-127: cache sysconf values and one-shot /proc reads.
    long tck = sysconf(_SC_CLK_TCK);
    mClkTck = (tck > 0) ? tck : 100;

    long pg = sysconf(_SC_PAGESIZE);
    mPageSize = (pg > 0) ? pg : 4096;

    mBootTimeSec = ProcInfoParser::parseBootTime(readAll("/proc/stat"));
    mTotalMemBytes = ProcInfoParser::parseMemTotalBytes(readAll("/proc/meminfo"));
}

QString ProcessInfoLinux::lookupUid(uid_t uid)
{
    auto it = mUidNameCache.constFind(uid);
    if (it != mUidNameCache.constEnd())
        return it.value();

    QString name;
    struct passwd pwd;
    struct passwd *result = nullptr;
    // Start small; grow if ERANGE. POSIX doesn't specify an upper bound but
    // 16 KiB is more than enough for any LDAP entry seen in the wild.
    QByteArray buf(4096, Qt::Uninitialized);
    int rc = getpwuid_r(uid, &pwd, buf.data(), buf.size(), &result);
    while (rc == ERANGE && buf.size() < (1 << 14)) {
        buf.resize(buf.size() * 2);
        rc = getpwuid_r(uid, &pwd, buf.data(), buf.size(), &result);
    }
    if (rc == 0 && result && result->pw_name)
        name = QString::fromLocal8Bit(result->pw_name);
    else
        name = QString::number(uid);

    mUidNameCache.insert(uid, name);
    return name;
}

QString ProcessInfoLinux::lookupGid(gid_t gid)
{
    auto it = mGidNameCache.constFind(gid);
    if (it != mGidNameCache.constEnd())
        return it.value();

    QString name;
    struct group grp;
    struct group *result = nullptr;
    QByteArray buf(4096, Qt::Uninitialized);
    int rc = getgrgid_r(gid, &grp, buf.data(), buf.size(), &result);
    while (rc == ERANGE && buf.size() < (1 << 14)) {
        buf.resize(buf.size() * 2);
        rc = getgrgid_r(gid, &grp, buf.data(), buf.size(), &result);
    }
    if (rc == 0 && result && result->gr_name)
        name = QString::fromLocal8Bit(result->gr_name);
    else
        name = QString::number(gid);

    mGidNameCache.insert(gid, name);
    return name;
}

QList<Process> ProcessInfoLinux::collectProcesses()
{
    // WI-21 (audit M2): build into a local list; the UI thread publishes via
    // setProcessList(). Never touch processList here — discovery runs on a
    // QtConcurrent worker once per second, while the UI thread copies
    // processList via getProcessList(). mCollectMutex serialises sync vs
    // worker callers so the per-PID delta state below stays coherent.
    QMutexLocker collectLocker(&mCollectMutex);
    QList<Process> processes;

    // FR-127: walk /proc directly instead of forking `ps ax` every tick.
    // Init-time bootTime / memTotal are captured in the constructor.
    const double uptimeSec = ProcInfoParser::parseUptimeSec(readAll("/proc/uptime"));
    const qint64 nowSec = QDateTime::currentSecsSinceEpoch();

    double cpuElapsedSec = 0.0;
    if (!mCpuTimerStarted) {
        mCpuTimer.start();
        mCpuTimerStarted = true;
    } else {
        cpuElapsedSec = mCpuTimer.elapsed() / 1000.0;
        mCpuTimer.restart();
    }

    // FR-115: GPU collection timer + baseline-clear on disable.
    double gpuElapsedSec = 0.0;
    if (mCollectGpu) {
        if (!mGpuTimerStarted) {
            mGpuTimer.start();
            mGpuTimerStarted = true;
        } else {
            gpuElapsedSec = mGpuTimer.elapsed() / 1000.0;
            mGpuTimer.restart();
        }
    } else if (!mPrevGpuEngineNs.isEmpty()) {
        mPrevGpuEngineNs.clear();
        mGpuTimerStarted = false;
    }

    QSet<pid_t> activeCpuPids;
    QSet<pid_t> activeGpuPids;

    QDir procDir("/proc");
    const QStringList entries = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        bool ok = false;
        const pid_t pid = entry.toInt(&ok);
        if (!ok || pid <= 0)
            continue;

        const QString base = "/proc/" + entry;

        const QByteArray statBytes = readAll(base + "/stat");
        if (statBytes.isEmpty())
            continue;   // short-lived PID — it's gone.

        StatFields sf;
        if (!ProcInfoParser::parseStat(statBytes, sf))
            continue;

        StatusFields sufields;
        ProcInfoParser::parseStatus(readAll(base + "/status"), sufields);

        const QByteArray cmdlineBytes = readAll(base + "/cmdline");

        Process proc;
        proc.setPid(pid);

        // RSS: kernel gives us pages; convert to bytes.
        const quint64 rssBytes = sf.rssPages * static_cast<quint64>(mPageSize);
        proc.setRss(rssBytes);

        if (mTotalMemBytes > 0)
            proc.setPmem(100.0 * static_cast<double>(rssBytes) /
                         static_cast<double>(mTotalMemBytes));
        else
            proc.setPmem(0.0);

        proc.setVsize(sf.vsize);

        proc.setUname(sufields.hasUid ? lookupUid(static_cast<uid_t>(sufields.uid))
                                      : QStringLiteral("?"));
        proc.setGroup(sufields.hasGid ? lookupGid(static_cast<gid_t>(sufields.gid))
                                      : QStringLiteral("?"));

        // %CPU — delta in clock ticks over delta wall seconds.
        const quint64 cpuTotal = sf.utime + sf.stime;
        activeCpuPids.insert(pid);
        double pcpu = 0.0;
        const auto prevIt = mPrevCpuTotal.constFind(pid);
        if (prevIt != mPrevCpuTotal.constEnd() && cpuElapsedSec > 0.0) {
            const quint64 prev = prevIt.value();
            if (cpuTotal >= prev) {
                const double deltaTicks = static_cast<double>(cpuTotal - prev);
                pcpu = (deltaTicks / static_cast<double>(mClkTck)) / cpuElapsedSec * 100.0;
            }
        }
        mPrevCpuTotal.insert(pid, cpuTotal);
        proc.setPcpu(pcpu);

        proc.setStartTime(ProcInfoParser::formatStartTime(
            mBootTimeSec, sf.starttime, mClkTck, nowSec));
        proc.setState(QString(sf.state));
        proc.setNice(sf.nice);
        proc.setCpuTime(ProcInfoParser::formatCpuTime(cpuTotal, mClkTck));
        proc.setSession(QString::number(sf.session));

        proc.setCmd(ProcInfoParser::formatCmdline(cmdlineBytes, sf.comm));

        // FR-115: walk /proc/<pid>/fdinfo/* and fold DRM stats into proc.
        if (mCollectGpu) {
            collectGpuForPid(pid, proc, gpuElapsedSec);
            activeGpuPids.insert(pid);
        }

        processes << proc;
    }

    // Prune CPU baseline map against current tick's PIDs.
    auto cpuIt = mPrevCpuTotal.begin();
    while (cpuIt != mPrevCpuTotal.end()) {
        if (!activeCpuPids.contains(cpuIt.key()))
            cpuIt = mPrevCpuTotal.erase(cpuIt);
        else
            ++cpuIt;
    }

    // Prune GPU baseline similarly.
    if (mCollectGpu) {
        auto gpuIt = mPrevGpuEngineNs.begin();
        while (gpuIt != mPrevGpuEngineNs.end()) {
            if (!activeGpuPids.contains(gpuIt.key()))
                gpuIt = mPrevGpuEngineNs.erase(gpuIt);
            else
                ++gpuIt;
        }

        // SSO-3399: keep the nvidia-smi pmon streamer's mLatest hash from
        // growing across the streamer's app-lifetime singleton — drop entries
        // for any pid we don't see in the current tick.
        if (NvidiaSmiPmonStreamer *s = pmonStreamer(); s && s->isRunning())
            s->pruneDeadPids(activeGpuPids);
    }

    // FR-108: skip the per-PID /proc/<pid>/io reads entirely when both disk
    // I/O columns are hidden. Hundreds of file opens per tick on a loaded
    // system otherwise. Reset the baseline cache when collection is off so
    // we don't display stale rates if the user re-enables the column later.
    if (!mCollectDiskIO) {
        if (!mPrevDiskIo.isEmpty())
            mPrevDiskIo.clear();
        mIoTimerStarted = false;
        return processes;
    }

    double elapsedSecs = 0;
    if (!mIoTimerStarted) {
        mIoTimer.start();
        mIoTimerStarted = true;
    } else {
        elapsedSecs = mIoTimer.elapsed() / 1000.0;
        mIoTimer.restart();
    }

    QSet<pid_t> activePids;

    for (Process &proc : processes) {
        pid_t pid = proc.getPid();
        activePids.insert(pid);

        QString ioContent = FileUtil::readStringFromFile(
            QString("/proc/%1/io").arg(pid));

        if (!ioContent.isEmpty()) {
            quint64 readBytes = 0;
            quint64 writeBytes = 0;

            const QStringList ioLines = ioContent.split('\n');
            for (const QString &ioLine : ioLines) {
                if (ioLine.startsWith(QLatin1String("read_bytes:")))
                    readBytes = ioLine.mid(12).trimmed().toULongLong();
                else if (ioLine.startsWith(QLatin1String("write_bytes:")))
                    writeBytes = ioLine.mid(13).trimmed().toULongLong();
            }

            if (elapsedSecs > 0 && mPrevDiskIo.contains(pid)) {
                auto prev = mPrevDiskIo.value(pid);
                double readRate = (readBytes >= prev.first)
                    ? (readBytes - prev.first) / elapsedSecs : 0;
                double writeRate = (writeBytes >= prev.second)
                    ? (writeBytes - prev.second) / elapsedSecs : 0;
                proc.setDiskReadRate(readRate);
                proc.setDiskWriteRate(writeRate);
            } else {
                proc.setDiskReadRate(0);
                proc.setDiskWriteRate(0);
            }

            mPrevDiskIo.insert(pid, qMakePair(readBytes, writeBytes));
        }
    }

    auto it = mPrevDiskIo.begin();
    while (it != mPrevDiskIo.end()) {
        if (!activePids.contains(it.key()))
            it = mPrevDiskIo.erase(it);
        else
            ++it;
    }

    return processes;
}

void ProcessInfoLinux::collectGpuForPid(pid_t pid, Process &proc, double elapsedSecs)
{
    // FR-115: walk /proc/<pid>/fdinfo/* looking for DRM fds. Aggregate engine
    // nanoseconds and vram bytes across dedupe-survivors (one entry per
    // drm-client-id). Permission errors are silent — fdinfo of a foreign
    // user's PIDs isn't readable, so those rows just show em-dash.
    const QString fdinfoDir = QStringLiteral("/proc/%1/fdinfo").arg(pid);
    QDir dir(fdinfoDir);

    QSet<qint64> seenClients;
    quint64 engineNsSum = 0;
    quint64 vramBytesSum = 0;
    bool anyDrmFound = false;

    if (dir.exists()) {
        const QFileInfoList entries =
            dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo &fi : entries) {
            const QByteArray content = readAll(fi.absoluteFilePath());
            if (content.isEmpty())
                continue;

            ProcInfoParser::DrmFdinfo f;
            if (!ProcInfoParser::parseDrmFdinfo(content, f))
                continue;

            anyDrmFound = true;

            // Dedupe by drm-client-id. If the driver doesn't emit an id,
            // count unconditionally — better to over-count than miss.
            if (f.clientId != -1) {
                if (seenClients.contains(f.clientId))
                    continue;
                seenClients.insert(f.clientId);
            }

            engineNsSum += f.engineNs;
            vramBytesSum += f.memVramB;
        }
    }

    if (anyDrmFound) {
        // %GPU via delta against last tick.
        const auto prev = mPrevGpuEngineNs.constFind(pid);
        const bool havePrev = (prev != mPrevGpuEngineNs.constEnd());
        const quint64 prevNs = havePrev ? prev.value() : 0;
        mPrevGpuEngineNs.insert(pid, engineNsSum);

        // Only show a non-sentinel percent once we've seen two ticks —
        // avoids a spurious 0% reading on the first tick after activation.
        if (havePrev && elapsedSecs > 0.0) {
            double pct = 0.0;
            if (engineNsSum >= prevNs) {
                const double deltaNs = static_cast<double>(engineNsSum - prevNs);
                const double elapsedNs = elapsedSecs * 1e9;
                if (elapsedNs > 0)
                    pct = qBound(0.0, (deltaNs / elapsedNs) * 100.0, 100.0);
            }
            proc.setGpuPercent(pct);
        }
        proc.setGpuVramBytes(static_cast<qint64>(vramBytesSum));
        return;
    }

    // FR-115: NVIDIA proprietary driver doesn't populate DRM fdinfo
    // reliably. Fall through to the pmon streamer's cache if nvidia-smi
    // is installed. Start it lazily on first reach.
    static bool probed = false;
    static bool nvidiaPresent = false;
    if (!probed) {
        probed = true;
        nvidiaPresent = CommandUtil::isExecutable("nvidia-smi");
    }
    if (!nvidiaPresent)
        return;

    NvidiaSmiPmonStreamer *s = pmonStreamer();
    if (!s->isRunning())
        s->start();

    const auto sample = s->get(pid);
    if (sample.gpuPercent >= 0)
        proc.setGpuPercent(static_cast<double>(sample.gpuPercent));
    if (sample.vramBytes >= 0)
        proc.setGpuVramBytes(sample.vramBytes);
}

// getProcessList() is in shared/nexis-core/Info/process_info_shared.cpp
