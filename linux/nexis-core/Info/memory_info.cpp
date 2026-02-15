#include "memory_info.h"
#include <QDebug>
#include <QRegularExpression>

#define PROC_MEMINFO "/proc/meminfo"

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
    swapUsed(0)
{ }

void MemoryInfo::updateMemoryInfo()
{
    QStringList lines = FileUtil::readListFromFile(PROC_MEMINFO)
            .filter(QRegularExpression("^MemTotal|^MemFree|^Buffers|^Cached|^SwapTotal|^SwapFree|^Shmem|^SReclaimable"));

    if (lines.size() < 8) {
        qWarning() << "MemoryInfo: expected 8 lines from /proc/meminfo, got" << lines.size();
        return;
    }

    QRegularExpression sep("\\s+");

#define getValue(l) lines.at(l).split(sep).at(1).toLongLong() << 10;
    memTotal = getValue(0);
    memFree = getValue(1);
    buffers = getValue(2);
    cached = getValue(3);
    swapTotal = getValue(4);
    swapFree = getValue(5);
    shmem = getValue(6);
    sreclaimable = getValue(7);
#undef getValue

    cached = (cached + sreclaimable - shmem);
    memUsed = (memTotal - (memFree + buffers + cached));
    swapUsed = (swapTotal - swapFree);
}

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
