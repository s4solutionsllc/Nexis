#include "process_info_linux.h"

#include <QDebug>
#include <QRegularExpression>
#include <QSet>

void ProcessInfoLinux::updateProcesses()
{
    processList.clear();

    try {

        QStringList columns = { "pid", "rss", "pmem", "vsize", "uname:50", "pcpu", "start_time",
                                "state", "group", "nice", "cputime", "session", "cmd"};

        QStringList lines = CommandUtil::exec("ps", {"ax", "-weo", columns.join(","), "--no-headings"})
                .trimmed()
                .split(QChar('\n'));

        if (! lines.isEmpty()) {
            QRegularExpression sep("\\s+");
            for (const QString &line : lines) {
                QStringList procLine = line.trimmed().split(sep);

                if (procLine.count() >= columns.count()) {
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

    // FR-108: skip the per-PID /proc/<pid>/io reads entirely when both disk
    // I/O columns are hidden. Hundreds of file opens per tick on a loaded
    // system otherwise. Reset the baseline cache when collection is off so
    // we don't display stale rates if the user re-enables the column later.
    if (!mCollectDiskIO) {
        if (!mPrevDiskIo.isEmpty())
            mPrevDiskIo.clear();
        mIoTimerStarted = false;
        return;
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

    for (Process &proc : processList) {
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
}

// getProcessList() is in shared/nexis-core/Info/process_info_shared.cpp
