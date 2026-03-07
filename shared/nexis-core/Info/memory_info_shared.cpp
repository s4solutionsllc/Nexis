// Shared MemoryInfo methods — identical across platforms.
// Platform-specific updateMemoryInfo() and constructor live in
// linux/nexis-core/Info/memory_info.cpp and macos/nexis-core/Info/memory_info.cpp.

#include "memory_info.h"
#include <QRegularExpression>

MemoryInfo::MemoryInfo():
    memTotal(0),
    memFree(0),
    memUsed(0),
    buffers(0),
    cached(0),
    sreclaimable(0),
    shmem(0),
    swapTotal(0),
    swapFree(0),
    swapUsed(0),
    memWired(0),
    memActive(0),
    memInactive(0),
    memCompressed(0),
    memAvailable(0),
    pressureLevel(-1)
{ }

quint64 MemoryInfo::getSwapUsed() const
{
    return swapUsed;
}

quint64 MemoryInfo::getSwapFree() const
{
    return swapFree;
}

quint64 MemoryInfo::getSwapTotal() const
{
    return swapTotal;
}

quint64 MemoryInfo::getMemUsed() const
{
    return memUsed;
}

quint64 MemoryInfo::getMemFree() const
{
    return memFree;
}

quint64 MemoryInfo::getMemTotal() const
{
    return memTotal;
}

quint64 MemoryInfo::getMemWired() const
{
    return memWired;
}

quint64 MemoryInfo::getMemActive() const
{
    return memActive;
}

quint64 MemoryInfo::getMemInactive() const
{
    return memInactive;
}

quint64 MemoryInfo::getMemCompressed() const
{
    return memCompressed;
}

quint64 MemoryInfo::getMemAvailable() const
{
    return memAvailable;
}

int MemoryInfo::getPressureLevel() const
{
    return pressureLevel;
}

MemoryParseResult MemoryInfo::parseProcMeminfo(const QStringList &lines)
{
    MemoryParseResult r;
    QMap<QString, quint64> fields;
    QRegularExpression sep("\\s+");

    for (const QString &line : lines) {
        QStringList parts = line.split(sep);
        if (parts.size() >= 2) {
            QString key = parts.at(0);
            if (key.endsWith(':'))
                key.chop(1);
            quint64 val = static_cast<quint64>(parts.at(1).toLongLong()) << 10;
            fields.insert(key, val);
        }
    }

    r.memTotal = fields.value("MemTotal", 0);
    r.memFree = fields.value("MemFree", 0);
    r.buffers = fields.value("Buffers", 0);
    r.cached = fields.value("Cached", 0);
    r.swapTotal = fields.value("SwapTotal", 0);
    r.swapFree = fields.value("SwapFree", 0);
    r.shmem = fields.value("Shmem", 0);
    r.sreclaimable = fields.value("SReclaimable", 0);
    r.memAvailable = fields.value("MemAvailable", 0);
    r.memActive = fields.value("Active", 0);
    r.memInactive = fields.value("Inactive", 0);

    return r;
}

void MemoryInfo::deriveMemoryValues(MemoryParseResult &r)
{
    quint64 cacheSum = r.cached + r.sreclaimable;
    r.cached = (cacheSum >= r.shmem) ? (cacheSum - r.shmem) : 0;

    quint64 nonUsed = r.memFree + r.buffers + r.cached;
    r.memUsed = (r.memTotal >= nonUsed) ? (r.memTotal - nonUsed) : 0;

    r.swapUsed = (r.swapTotal >= r.swapFree) ? (r.swapTotal - r.swapFree) : 0;
}

int MemoryInfo::parsePressureLevel(const QString &psiContent,
                                   quint64 memAvailable, quint64 memTotal)
{
    if (!psiContent.isEmpty()) {
        QRegularExpression re("avg10=([\\d.]+)");
        auto match = re.match(psiContent);
        if (match.hasMatch()) {
            double someAvg10 = match.captured(1).toDouble();
            if (someAvg10 > 50.0)
                return 4;
            if (someAvg10 > 10.0)
                return 2;
            return 1;
        }
    }

    if (memAvailable > 0 && memTotal > 0) {
        double ratio = static_cast<double>(memAvailable) / static_cast<double>(memTotal);
        if (ratio < 0.10)
            return 4;
        if (ratio < 0.20)
            return 2;
        return 1;
    }

    return -1;
}
