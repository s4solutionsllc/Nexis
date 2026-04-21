#ifndef NVIDIA_SMI_CACHE_H
#define NVIDIA_SMI_CACHE_H

#include <QHash>
#include <QString>

// FR-106: shared cache that batches nvidia-smi queries across GPU and fan
// subsystems. Previously GpuInfoLinux::updateGpuInfo() and
// FanInfoLinux::readNvidiaSpeed() each forked nvidia-smi per device per tick
// — 2-4 forks/sec on a single-GPU machine querying the same device. A single
// call covers every index and both subsystems read from the shared snapshot.
//
// Synchronous: callers block on the nvidia-smi fork. Thread-safe for reads;
// refresh() must be called from a single producer (the tick caller).
namespace NvidiaSmiCache {

struct Sample {
    int utilization = -1;   // percent, -1 = unknown
    int fanPercent  = -1;   // percent, -1 = unknown
};

// Run a single nvidia-smi invocation that queries utilization and fan speed
// for every device, populating the cache. No-op if nvidia-smi is not on PATH.
void refresh();

// Look up a device by its nvidia-smi index. Returns a default Sample
// (utilization=-1, fanPercent=-1) if not present.
Sample get(int index);

// Age in milliseconds since the last successful refresh. Returns INT_MAX
// if never refreshed. Used by callers to decide whether to re-fork.
int ageMs();

} // namespace NvidiaSmiCache

#endif // NVIDIA_SMI_CACHE_H
