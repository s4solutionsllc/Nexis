#ifndef CPU_TUNING_H
#define CPU_TUNING_H

#include <QList>
#include <QString>

#include "nexis-core_global.h"

// FR-117: per-core + whole-CPU tuning on Linux. Reads /sys/devices/system/cpu
// cpufreq + intel_pstate sysfs files, writes them via pkexec. All functions
// are synchronous; callers are expected to invoke from a worker thread.
namespace CpuTuning {

enum class Turbo { On, Off, Unsupported };

struct NEXISCORESHARED_EXPORT CoreSnapshot {
    int     index        = -1;       // cpuN index
    QString governor;                 // e.g. "performance"
    quint64 scalingMinKHz = 0;
    quint64 scalingMaxKHz = 0;
    quint64 cpuinfoMinKHz = 0;        // hardware floor
    quint64 cpuinfoMaxKHz = 0;        // hardware ceiling
};

struct NEXISCORESHARED_EXPORT Snapshot {
    bool                available = false;   // cpufreq visible at all
    Turbo               turbo     = Turbo::Unsupported;
    QString             scalingDriver;       // e.g. "intel_pstate", "amd-pstate"
    QStringList         availableGovernors;
    QList<CoreSnapshot> cores;               // one entry per online cpuN
};

// Read everything in one shot. Never throws — unreadable paths are simply
// absent from the returned snapshot.
NEXISCORESHARED_EXPORT Snapshot readSnapshot();

// Write ALL online CPUs' scaling_min_freq / scaling_max_freq in one pkexec
// call. Returns true on read-back verification.
NEXISCORESHARED_EXPORT bool writeFreqRange(quint64 minKHz, quint64 maxKHz);

// Set turbo boost on/off for the appropriate backend. No-op + returns false
// when Turbo::Unsupported was detected.
NEXISCORESHARED_EXPORT bool writeTurbo(bool on);

// Set the governor for a specific cpuN, or -1 for all online cores. Batches
// multi-cpu writes into a single pkexec.
NEXISCORESHARED_EXPORT bool writeGovernor(int cpuIndex, const QString &governor);

// Write all per-core governors in one pkexec. `perCore` is a parallel
// list keyed by online-cpuN order matching Snapshot::cores.
NEXISCORESHARED_EXPORT bool writePerCoreGovernors(const QList<QPair<int, QString>> &perCore);

} // namespace CpuTuning

#endif // CPU_TUNING_H
