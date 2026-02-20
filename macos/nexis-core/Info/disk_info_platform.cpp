#include "disk_info_macos.h"
#include <QDir>
#include <QRegularExpression>

#include "Utils/command_util.h"

QList<quint64> DiskInfoMacOS::getDiskIO() const
{
    QList<quint64> diskReadWrite;
    quint64 totalRead = 0;
    quint64 totalWrite = 0;

    // On macOS, use iostat to get disk I/O statistics
    try {
        QString output = CommandUtil::exec("iostat", {"-d", "-c", "1", "-w", "1"});
        // iostat output format varies; parse the totals from the last line
        QStringList lines = output.trimmed().split('\n');
        if (lines.size() >= 3) {
            // Skip header lines, parse last data line
            QStringList parts = lines.last().trimmed().split(QRegularExpression("\\s+"));
            if (parts.size() >= 3) {
                // KB/t, tps, MB/s — multiply MB/s by 1MB to get bytes
                totalRead = static_cast<quint64>(parts.at(2).toDouble() * 1024 * 1024);
                totalWrite = totalRead; // iostat doesn't split r/w by default
            }
        }
    } catch (...) {}

    diskReadWrite.append(totalRead);
    diskReadWrite.append(totalWrite);

    return diskReadWrite;
}

QStringList DiskInfoMacOS::getDiskNames() const
{
    QStringList diskNames;
    // On macOS, list disk devices from /dev/disk*
    QDir devDir("/dev");
    QStringList entries = devDir.entryList({"disk*"}, QDir::System);
    for (const QString &entry : entries) {
        // Only include base disks (disk0, disk1), not partitions (disk0s1)
        if (!entry.contains('s'))
            diskNames.append(entry);
    }
    return diskNames;
}
