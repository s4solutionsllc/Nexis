#include "mini_monitor_format_util.h"
#include "Info/disk_info.h"

#include <algorithm>

QString MiniMonitorFormatUtil::formatMetricRow(const QString &label, int percent)
{
    const int clamped = std::clamp(percent, 0, 100);
    return QString("%1 %2%").arg(label).arg(clamped);
}

int MiniMonitorFormatUtil::aggregateDiskUsedPercent(const QList<Disk> &disks)
{
    qint64 totalSize = 0;
    double weightedUsed = 0.0;
    for (const Disk &d : disks) {
        if (d.size == 0) continue;
        int usedPercent = static_cast<int>(100.0 * d.used / d.size);
        weightedUsed += static_cast<double>(usedPercent) * d.size;
        totalSize += d.size;
    }
    if (totalSize == 0)
        return 0;
    return std::clamp(static_cast<int>(qRound(weightedUsed / totalSize)), 0, 100);
}

QString MiniMonitorFormatUtil::scoreColorToken(int score)
{
    const int clamped = std::clamp(score, 0, 100);
    if (clamped >= 75)
        return QStringLiteral("@successColor");
    if (clamped >= 40)
        return QStringLiteral("@warningColor");
    return QStringLiteral("@destructiveColor");
}
