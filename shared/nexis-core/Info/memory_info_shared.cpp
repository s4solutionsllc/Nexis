// Shared MemoryInfo methods — identical across platforms.
// Platform-specific updateMemoryInfo() and constructor live in
// linux/nexis-core/Info/memory_info.cpp and macos/nexis-core/Info/memory_info.cpp.

#include "memory_info.h"

MemoryInfo::MemoryInfo():
    memTotal(0),
    memFree(0),
    memUsed(0),
    buffers(0),
    cached(0),
    sreclaimable(0),
    shmem(0),
    swapTotal(0),
    swapFree(0),
    swapUsed(0)
{ }

quint64 MemoryInfo::getSwapUsed() const
{
    return swapUsed;
}

quint64 MemoryInfo::getSwapFree() const
{
    return swapFree;
}

quint64 MemoryInfo::getSwapTotal() const
{
    return swapTotal;
}

quint64 MemoryInfo::getMemUsed() const
{
    return memUsed;
}

quint64 MemoryInfo::getMemFree() const
{
    return memFree;
}

quint64 MemoryInfo::getMemTotal() const
{
    return memTotal;
}
