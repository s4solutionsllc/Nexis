#include "nvidia_smi_cache.h"

#include "nvidia_smi_streamer.h"

// FR-106 Step C: replace the per-tick fork implementation with a persistent
// `nvidia-smi -l 1` streamer. The public namespace API stays the same so
// GpuInfoLinux and FanInfoLinux call sites don't change — but `refresh()`
// is now a zero-cost no-op in steady state.
namespace NvidiaSmiCache {

namespace {

NvidiaSmiStreamer *streamer()
{
    // Leaked by design — lifetime of the app. QObject parent=nullptr so it
    // doesn't latch onto any QApplication thread; its internal QProcess is
    // parented to `this`.
    static NvidiaSmiStreamer *instance = new NvidiaSmiStreamer();
    return instance;
}

} // namespace

void refresh()
{
    NvidiaSmiStreamer *s = streamer();
    if (!s->isRunning())
        s->start(1);
    // Steady state: nothing to do. The streamer emits one line per sample
    // every `intervalSec`, parsed by its readyReadStandardOutput slot.
}

Sample get(int index)
{
    return streamer()->get(index);
}

int ageMs()
{
    return streamer()->ageMs();
}

} // namespace NvidiaSmiCache
