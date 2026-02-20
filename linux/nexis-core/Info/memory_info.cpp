#include "memory_info_linux.h"
#include <QDebug>
#include <QRegularExpression>

static constexpr const char *PROC_MEMINFO = "/proc/meminfo";

// Constructor and getters are in shared/nexis-core/Info/memory_info_shared.cpp

void MemoryInfoLinux::updateMemoryInfo()
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
