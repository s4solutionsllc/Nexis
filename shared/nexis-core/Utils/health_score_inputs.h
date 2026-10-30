#ifndef HEALTH_SCORE_INPUTS_H
#define HEALTH_SCORE_INPUTS_H

#include "nexis-core_global.h"

#include <QList>

struct MemorySnapshot;
struct Disk;

// SSO-23854: the CPU/memory/disk score formulas that feed HealthScoreCalculator
// were duplicated between DashboardPage (shared/nexis/Pages/Dashboard) and the
// macOS-only MenuBarMonitor (now retired, SSO-26762) so the Dashboard tile and
// the macOS menu-bar surface agreed. Extracted here so the cross-platform tray
// surface (TrayHealthMonitor) is a caller of the same formulas rather than a copy.
class NEXISCORESHARED_EXPORT HealthScoreInputs
{
public:
    static int cpuScore(int coreCount, double load1MinAvg);
    static int memoryScore(const MemorySnapshot &snap);
    static int diskScore(const QList<Disk> &disks);
};

#endif // HEALTH_SCORE_INPUTS_H
