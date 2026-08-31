#include "health_score_inputs.h"

#include <Info/memory_info.h>
#include <Info/disk_info.h>

#include <QtGlobal>

int HealthScoreInputs::cpuScore(int coreCount, double load1MinAvg)
{
    if (coreCount <= 0 || load1MinAvg <= 0)
        return 100;
    double ratio = load1MinAvg / coreCount;
    return qBound(0, qRound(100.0 * (1.0 - ratio)), 100);
}

int HealthScoreInputs::memoryScore(const MemorySnapshot &snap)
{
    if (snap.total == 0)
        return 100;
    return qBound(0, 100 - (int)(100.0 * snap.used / snap.total), 100);
}

int HealthScoreInputs::diskScore(const QList<Disk> &disks)
{
    qint64 totalSize = 0;
    double weightedScore = 0.0;
    for (const Disk &d : disks) {
        if (d.size == 0) continue;
        int usedPercent = (int)(100.0 * d.used / d.size);
        int score = qBound(0, 100 - usedPercent, 100);
        weightedScore += (double)score * d.size;
        totalSize += d.size;
    }
    if (totalSize <= 0)
        return 100;
    return qBound(0, (int)qRound(weightedScore / totalSize), 100);
}
