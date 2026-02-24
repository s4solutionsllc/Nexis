#include "memory_info_linux.h"
#include <QDebug>
#include <QFile>
#include <QRegularExpression>
#include <QMap>

static constexpr const char *PROC_MEMINFO = "/proc/meminfo";
static constexpr const char *PROC_PRESSURE_MEMORY = "/proc/pressure/memory";

// Constructor and getters are in shared/nexis-core/Info/memory_info_shared.cpp

void MemoryInfoLinux::updateMemoryInfo()
{
    // FR-57: Key-value map parser replaces fragile positional indexing.
    // Read all lines and parse "Key: Value kB" into a map.
    QStringList allLines = FileUtil::readListFromFile(PROC_MEMINFO);
    QMap<QString, quint64> fields;
    QRegularExpression sep("\\s+");

    for (const QString &line : allLines) {
        QStringList parts = line.split(sep);
        if (parts.size() >= 2) {
            QString key = parts.at(0);
            if (key.endsWith(':'))
                key.chop(1);
            quint64 val = parts.at(1).toLongLong() << 10; // kB → bytes
            fields.insert(key, val);
        }
    }

    memTotal = fields.value("MemTotal", 0);
    memFree = fields.value("MemFree", 0);
    buffers = fields.value("Buffers", 0);
    cached = fields.value("Cached", 0);
    swapTotal = fields.value("SwapTotal", 0);
    swapFree = fields.value("SwapFree", 0);
    shmem = fields.value("Shmem", 0);
    sreclaimable = fields.value("SReclaimable", 0);

    cached = (cached + sreclaimable - shmem);
    memUsed = (memTotal - (memFree + buffers + cached));
    swapUsed = (swapTotal - swapFree);

    // FR-57: New breakdown fields
    memAvailable = fields.value("MemAvailable", 0);
    memActive = fields.value("Active", 0);
    memInactive = fields.value("Inactive", 0);
    memWired = 0;       // Not applicable on Linux
    memCompressed = 0;  // Not directly available on Linux

    // FR-57: Memory pressure via PSI or MemAvailable heuristic
    pressureLevel = -1;
    QFile psiFile(PROC_PRESSURE_MEMORY);
    if (psiFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString line = QString::fromUtf8(psiFile.readLine());
        psiFile.close();
        QRegularExpression re("avg10=([\\d.]+)");
        auto match = re.match(line);
        if (match.hasMatch()) {
            double someAvg10 = match.captured(1).toDouble();
            if (someAvg10 > 50.0)
                pressureLevel = 4;      // critical
            else if (someAvg10 > 10.0)
                pressureLevel = 2;      // warning
            else
                pressureLevel = 1;      // normal
        }
    } else if (memAvailable > 0 && memTotal > 0) {
        // Fallback: heuristic from MemAvailable ratio
        double ratio = static_cast<double>(memAvailable) / static_cast<double>(memTotal);
        if (ratio < 0.10)
            pressureLevel = 4;
        else if (ratio < 0.20)
            pressureLevel = 2;
        else
            pressureLevel = 1;
    }
}
