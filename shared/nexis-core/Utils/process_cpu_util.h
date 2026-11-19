#ifndef PROCESS_CPU_UTIL_H
#define PROCESS_CPU_UTIL_H

#include "nexis-core_global.h"

#include <QString>

// Per-process %CPU is reported as a share of the whole machine (0-100),
// not top's per-core "Irix" scale where one busy thread = 100%. Both the
// Linux /proc walker and the macOS `ps pcpu` path produce per-core values
// and normalise through here so the two platforms read identically.
class NEXISCORESHARED_EXPORT ProcessCpuUtil
{
public:
    static int logicalCpuCount();

    static double toSystemPercent(double perCorePercent, int logicalCpus);

    // Alert thresholds saved before normalisation were on the per-core
    // scale; convert once on load, rounding up so a non-zero threshold
    // never collapses to "disabled".
    static int migratePerCoreThreshold(int perCorePercent, int logicalCpus);

    static QString formatPercent(double percent);
};

#endif // PROCESS_CPU_UTIL_H
