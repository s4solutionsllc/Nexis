#include "memory_info.h"
#include <QDebug>
#include <QRegularExpression>

#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/vm_statistics.h>

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

void MemoryInfo::updateMemoryInfo()
{
    // Total physical memory via sysctl
    int64_t physMem = 0;
    size_t len = sizeof(physMem);
    if (sysctlbyname("hw.memsize", &physMem, &len, nullptr, 0) == 0) {
        memTotal = static_cast<quint64>(physMem);
    }

    // VM statistics via Mach
    vm_size_t pageSize;
    host_page_size(mach_host_self(), &pageSize);

    vm_statistics64_data_t vmStat;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    kern_return_t err = host_statistics64(mach_host_self(),
                                          HOST_VM_INFO64,
                                          reinterpret_cast<host_info64_t>(&vmStat),
                                          &count);
    if (err == KERN_SUCCESS) {
        quint64 free = static_cast<quint64>(vmStat.free_count) * pageSize;
        quint64 inactive = static_cast<quint64>(vmStat.inactive_count) * pageSize;
        quint64 purgeable = static_cast<quint64>(vmStat.purgeable_count) * pageSize;

        // macOS "available" memory = free + inactive + purgeable
        memFree = free + inactive + purgeable;
        memUsed = memTotal > memFree ? memTotal - memFree : 0;

        // Cached approximation: inactive + purgeable pages
        cached = inactive + purgeable;
        buffers = 0; // macOS doesn't have a separate buffer concept
    }

    // Swap usage via sysctl
    struct xsw_usage swapUsage;
    len = sizeof(swapUsage);
    if (sysctlbyname("vm.swapusage", &swapUsage, &len, nullptr, 0) == 0) {
        swapTotal = swapUsage.xsu_total;
        swapUsed = swapUsage.xsu_used;
        swapFree = swapUsage.xsu_avail;
    }
}

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
