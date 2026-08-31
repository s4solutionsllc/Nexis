#ifndef MINI_MONITOR_FORMAT_UTIL_H
#define MINI_MONITOR_FORMAT_UTIL_H

#include "nexis-core_global.h"

#include <QList>
#include <QString>

struct Disk;

// SSO-23855: view-model helpers for the compact mini-monitor window. Kept
// separate from MiniMonitorWindow (shared/nexis/Pages/MiniMonitor) so the
// formatting/aggregation logic is unit-testable without a QWidget.
class NEXISCORESHARED_EXPORT MiniMonitorFormatUtil
{
public:
    // "CPU 42%" — clamps percent to 0-100.
    static QString formatMetricRow(const QString &label, int percent);

    // Capacity-weighted average of per-disk used space, matching the
    // weighting HealthScoreCalculator's disk component uses (see
    // MenuBarMonitor::onDiskUsageUpdated) but returning used% directly
    // rather than an inverted health score.
    static int aggregateDiskUsedPercent(const QList<Disk> &disks);

    // Theme color token ("@successColor"/"@warningColor"/"@destructiveColor")
    // for a 0-100 health score, matching HealthScoreTile::recalculate()'s
    // thresholds (>=75 success, >=40 warning, else destructive).
    static QString scoreColorToken(int score);
};

#endif // MINI_MONITOR_FORMAT_UTIL_H
