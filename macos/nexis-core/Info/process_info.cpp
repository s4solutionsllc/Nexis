#include "process_info_macos.h"

#include <QDebug>
#include <QRegularExpression>
#include <QSet>
#include <libproc.h>
#include <sys/resource.h>

void ProcessInfoMacOS::updateProcesses()
{
    processList.clear();

    try {
        // macOS ps doesn't support --no-headings but supports the same columns
        // Use -weo with slightly different format specifiers
        QStringList columns = { "pid", "rss", "pmem", "vsize", "user", "pcpu", "start",
                                "state", "group", "nice", "cputime", "sess", "command"};

        QStringList lines = CommandUtil::exec("ps", {"ax", "-weo", columns.join(",")})
                .trimmed()
                .split(QChar('\n'));

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

                    processList << proc;
                }
            }
        }

    } catch (QString &ex) {
        qCritical() << ex;
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
        for (Process &proc : processList) {
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

    // --- Per-process network I/O via nettop ---
    // FR-108: nettop is expensive (50-150 ms init per fork) — skip entirely
    // when both network columns are hidden, which is the default on first run.
    if (!mCollectNetIO) {
        if (!mPrevNetIo.isEmpty())
            mPrevNetIo.clear();
        return;
    }

    // Rebuild activePids if disk collection skipped it.
    if (activePids.isEmpty()) {
        for (const Process &proc : processList)
            activePids.insert(proc.getPid());
    }

    QHash<pid_t, QPair<quint64, quint64>> nettopData;

    QString nettopOutput = CommandUtil::exec("nettop",
        {"-P", "-d", "-L", "1", "-J", "bytes_in,bytes_out", "-t", "external"});

    if (!nettopOutput.isEmpty()) {
        QStringList lines = nettopOutput.trimmed().split('\n');
        for (int i = 1; i < lines.size(); ++i) {
            const QString &line = lines.at(i);
            QStringList parts = line.split(',');
            if (parts.size() >= 3) {
                QString procField = parts.at(0);
                int lastDot = procField.lastIndexOf('.');
                if (lastDot > 0) {
                    bool ok = false;
                    pid_t pid = procField.mid(lastDot + 1).toLongLong(&ok);
                    if (ok && pid > 0) {
                        quint64 bytesIn = parts.at(1).trimmed().toULongLong();
                        quint64 bytesOut = parts.at(2).trimmed().toULongLong();
                        nettopData.insert(pid, qMakePair(bytesIn, bytesOut));
                    }
                }
            }
        }
    }

    for (Process &proc : processList) {
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
}

// getProcessList() is in shared/nexis-core/Info/process_info_shared.cpp
