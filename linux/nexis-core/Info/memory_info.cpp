#include "memory_info_linux.h"
#include <QFile>

static constexpr const char *PROC_MEMINFO = "/proc/meminfo";
static constexpr const char *PROC_PRESSURE_MEMORY = "/proc/pressure/memory";

// Constructor and getters are in shared/nexis-core/Info/memory_info_shared.cpp

void MemoryInfoLinux::updateMemoryInfo()
{
    QStringList allLines = FileUtil::readListFromFile(PROC_MEMINFO);
    MemoryParseResult r = parseProcMeminfo(allLines);
    deriveMemoryValues(r);

    memTotal = r.memTotal;
    memFree = r.memFree;
    memUsed = r.memUsed;
    buffers = r.buffers;
    cached = r.cached;
    sreclaimable = r.sreclaimable;
    shmem = r.shmem;
    swapTotal = r.swapTotal;
    swapFree = r.swapFree;
    swapUsed = r.swapUsed;
    memAvailable = r.memAvailable;
    memActive = r.memActive;
    memInactive = r.memInactive;
    memWired = 0;
    memCompressed = 0;

    QString psiContent;
    QFile psiFile(PROC_PRESSURE_MEMORY);
    if (psiFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        psiContent = QString::fromUtf8(psiFile.readLine());
        psiFile.close();
    }
    pressureLevel = parsePressureLevel(psiContent, memAvailable, memTotal);
}
