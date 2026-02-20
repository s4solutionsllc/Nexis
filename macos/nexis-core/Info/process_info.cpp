#include "process_info_macos.h"

#include <QDebug>
#include <QRegularExpression>

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
}

// getProcessList() is in shared/nexis-core/Info/process_info_shared.cpp
