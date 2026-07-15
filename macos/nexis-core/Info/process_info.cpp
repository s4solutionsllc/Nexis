#include "process_info_macos.h"

#include "nettop_streamer.h"

#include <QDebug>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSet>
#include <libproc.h>
#include <sys/resource.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

ProcessInfoMacOS::ProcessInfoMacOS() = default;
ProcessInfoMacOS::~ProcessInfoMacOS() = default;

QList<Process> ProcessInfoMacOS::collectProcesses()
{
    // WI-21 (audit M2): build into a local list; the UI thread publishes via
    // setProcessList(). Never touch processList here — the `ps ax -weo` fork
    // alone can stall for seconds (M2: 30 s waitForFinished cap), and from
    // SSO-3383 onwards we run this on a QtConcurrent worker once per second.
    // mCollectMutex serialises sync vs worker callers so the per-PID delta
    // state (mPrev*, timers) below stays coherent.
    QMutexLocker collectLocker(&mCollectMutex);
    QList<Process> processes;

    {
        // macOS ps doesn't support --no-headings but supports the same columns
        // Use -weo with slightly different format specifiers
        QStringList columns = { "pid", "rss", "pmem", "vsize", "user", "pcpu", "start",
                                "state", "group", "nice", "cputime", "sess", "command"};

        ExecResult psResult = CommandUtil::execWithStatus("ps", {"ax", "-weo", columns.join(",")});
        if (!psResult.ok()) {
            qCritical() << "process_info: ps ax failed:" << psResult.error;
        } else {
            QStringList lines = psResult.output.trimmed().split(QChar('\n'));

            if (lines.size() > 1) {
                // Remove the header line on macOS
                lines.removeFirst();

                QRegularExpression sep("\\s+");
                for (const QString &line : lines) {
                    QStringList procLine = line.trimmed().split(sep);

                    if (procLine.count() >= 13) {
                        Process proc;

                        proc.setPid(procLine.takeFirst().toLongLong());
                        proc.setRss(procLine.takeFirst().toLongLong() << 10);
                        proc.setPmem(procLine.takeFirst().toDouble());
                        proc.setVsize(procLine.takeFirst().toLongLong() << 10);
                        proc.setUname(procLine.takeFirst());
                        proc.setPcpu(procLine.takeFirst().toDouble());
                        proc.setStartTime(procLine.takeFirst());
                        proc.setState(procLine.takeFirst());
                        proc.setGroup(procLine.takeFirst());
                        proc.setNice(procLine.takeFirst().toInt());
                        proc.setCpuTime(procLine.takeFirst());
                        proc.setSession(procLine.takeFirst());
                        proc.setCmd(procLine.join(" "));

                        processes << proc;
                    }
                }
            }
        }
    }

    // --- Shared elapsed-time bookkeeping for disk and net rate deltas ---
    double elapsedSecs = 0;
    QSet<pid_t> activePids;

    if (mCollectDiskIO || mCollectNetIO) {
        if (!mIoTimerStarted) {
            mIoTimer.start();
            mIoTimerStarted = true;
        } else {
            elapsedSecs = mIoTimer.elapsed() / 1000.0;
            mIoTimer.restart();
        }
    } else {
        mIoTimerStarted = false;
    }

    // --- Per-process disk I/O via proc_pid_rusage() ---
    // FR-108: only collect if the Processes-page disk columns are visible.
    // proc_pid_rusage() is a cheap in-process syscall so this mostly saves
    // the downstream map maintenance. Clear state on disable so stale rates
    // don't linger.
    if (mCollectDiskIO) {
        for (Process &proc : processes) {
            pid_t pid = proc.getPid();
            activePids.insert(pid);

            struct rusage_info_v4 rusage;
            int ret = proc_pid_rusage(pid, RUSAGE_INFO_V4, (rusage_info_t *)&rusage);

            if (ret == 0) {
                quint64 readBytes = rusage.ri_diskio_bytesread;
                quint64 writeBytes = rusage.ri_diskio_byteswritten;

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

        // Prune stale PIDs from disk I/O map
        auto it = mPrevDiskIo.begin();
        while (it != mPrevDiskIo.end()) {
            if (!activePids.contains(it.key()))
                it = mPrevDiskIo.erase(it);
            else
                ++it;
        }
    } else if (!mPrevDiskIo.isEmpty()) {
        mPrevDiskIo.clear();
    }

    // --- Per-process network I/O ---
    // FR-102: read from the persistent NettopStreamer instead of forking
    // nettop every tick. The streamer starts on demand the first time
    // mCollectNetIO is true and stops when it flips off.
    if (!mCollectNetIO) {
        if (mNettopStreamer) {
            mNettopStreamer->stop();
            mNettopStreamer.reset();
        }
        if (!mPrevNetIo.isEmpty())
            mPrevNetIo.clear();
    } else {
        if (!mNettopStreamer) {
            mNettopStreamer = std::make_unique<NettopStreamer>();
            mNettopStreamer->start(1);
        }

        // Rebuild activePids if disk collection skipped it.
        if (activePids.isEmpty()) {
            for (const Process &proc : processes)
                activePids.insert(proc.getPid());
        }

        const QHash<pid_t, QPair<quint64, quint64>> nettopData = mNettopStreamer->snapshot();

        for (Process &proc : processes) {
            pid_t pid = proc.getPid();

            if (nettopData.contains(pid)) {
                auto net = nettopData.value(pid);
                quint64 rxBytes = net.first;
                quint64 txBytes = net.second;

                if (elapsedSecs > 0 && mPrevNetIo.contains(pid)) {
                    auto prev = mPrevNetIo.value(pid);
                    double downRate = (rxBytes >= prev.first)
                        ? (rxBytes - prev.first) / elapsedSecs : 0;
                    double upRate = (txBytes >= prev.second)
                        ? (txBytes - prev.second) / elapsedSecs : 0;
                    proc.setNetDownRate(downRate);
                    proc.setNetUpRate(upRate);
                } else {
                    proc.setNetDownRate(0);
                    proc.setNetUpRate(0);
                }

                mPrevNetIo.insert(pid, qMakePair(rxBytes, txBytes));
            }
        }

        // Prune stale net PIDs
        auto netIt = mPrevNetIo.begin();
        while (netIt != mPrevNetIo.end()) {
            if (!activePids.contains(netIt.key()))
                netIt = mPrevNetIo.erase(netIt);
            else
                ++netIt;
        }

        // SSO-3399: also prune the streamer's own mLatest so long-lived
        // streaming sessions don't accumulate entries for exited processes.
        mNettopStreamer->pruneDeadPids(activePids);
    }

    // --- Per-process GPU utilization (FR-128) ---
    // Walk AGXDeviceUserClient children of IOAccelerator (Apple Silicon).
    // accumulatedGPUTime is a monotonically increasing nanosecond counter;
    // delta / elapsed wall time = GPU%.
    if (!mCollectGpu) {
        if (!mPrevGpuNs.isEmpty())
            mPrevGpuNs.clear();
        mGpuTimerStarted = false;
        return processes;
    }

    double gpuElapsedNs = 0.0;
    if (!mGpuTimerStarted) {
        mGpuTimer.start();
        mGpuTimerStarted = true;
    } else {
        gpuElapsedNs = static_cast<double>(mGpuTimer.nsecsElapsed());
        mGpuTimer.restart();
    }

    QHash<pid_t, quint64> currentGpuNs = collectGpuNs();

    for (Process &proc : processes) {
        pid_t pid = proc.getPid();
        if (currentGpuNs.contains(pid) && gpuElapsedNs > 0 && mPrevGpuNs.contains(pid)) {
            quint64 cur = currentGpuNs.value(pid);
            quint64 prev = mPrevGpuNs.value(pid);
            double pct = (cur >= prev) ? (cur - prev) / gpuElapsedNs * 100.0 : 0.0;
            proc.setGpuPercent(qBound(0.0, pct, 100.0));
        } else {
            proc.setGpuPercent(-1.0);
        }
    }

    // Prune stale GPU PIDs
    auto gpuIt = mPrevGpuNs.begin();
    while (gpuIt != mPrevGpuNs.end()) {
        if (!currentGpuNs.contains(gpuIt.key()))
            gpuIt = mPrevGpuNs.erase(gpuIt);
        else
            ++gpuIt;
    }

    mPrevGpuNs = currentGpuNs;

    return processes;
}

QHash<pid_t, quint64> ProcessInfoMacOS::collectGpuNs()
{
    QHash<pid_t, quint64> result;

    io_iterator_t accelIt = 0;
    if (IOServiceGetMatchingServices(kIOMainPortDefault,
                                     IOServiceMatching("IOAccelerator"),
                                     &accelIt) != KERN_SUCCESS || !accelIt)
        return result;

    io_object_t accel;
    while ((accel = IOIteratorNext(accelIt))) {
        io_iterator_t childIt = 0;
        if (IORegistryEntryGetChildIterator(accel, kIOServicePlane, &childIt) == KERN_SUCCESS) {
            io_object_t child;
            while ((child = IOIteratorNext(childIt))) {
                CFMutableDictionaryRef props = nullptr;
                if (IORegistryEntryCreateCFProperties(child, &props, kCFAllocatorDefault, 0) == KERN_SUCCESS && props) {
                    CFStringRef creatorRef = (CFStringRef)CFDictionaryGetValue(props, CFSTR("IOUserClientCreator"));
                    CFArrayRef usageRef    = (CFArrayRef)CFDictionaryGetValue(props, CFSTR("AppUsage"));

                    if (creatorRef && CFGetTypeID(creatorRef) == CFStringGetTypeID() &&
                        usageRef   && CFGetTypeID(usageRef)   == CFArrayGetTypeID()) {

                        char buf[128] = {};
                        CFStringGetCString(creatorRef, buf, sizeof(buf), kCFStringEncodingUTF8);

                        int pid = 0;
                        if (sscanf(buf, "pid %d,", &pid) == 1 && pid > 0) {
                            quint64 totalNs = 0;
                            CFIndex count = CFArrayGetCount(usageRef);
                            for (CFIndex i = 0; i < count; ++i) {
                                CFDictionaryRef entry = (CFDictionaryRef)CFArrayGetValueAtIndex(usageRef, i);
                                if (entry && CFGetTypeID(entry) == CFDictionaryGetTypeID()) {
                                    CFNumberRef nsRef = (CFNumberRef)CFDictionaryGetValue(entry, CFSTR("accumulatedGPUTime"));
                                    if (nsRef && CFGetTypeID(nsRef) == CFNumberGetTypeID()) {
                                        uint64_t ns = 0;
                                        CFNumberGetValue(nsRef, kCFNumberSInt64Type, &ns);
                                        totalNs += ns;
                                    }
                                }
                            }
                            result[static_cast<pid_t>(pid)] += totalNs;
                        }
                    }
                    CFRelease(props);
                }
                IOObjectRelease(child);
            }
            IOObjectRelease(childIt);
        }
        IOObjectRelease(accel);
    }
    IOObjectRelease(accelIt);

    return result;
}

// getProcessList() is in shared/nexis-core/Info/process_info_shared.cpp
