#include "memory_info_macos.h"
#include <QDebug>
#include <QRegularExpression>

#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/vm_statistics.h>

// Constructor and getters are in shared/nexis-core/Info/memory_info_shared.cpp

void MemoryInfoMacOS::updateMemoryInfo()
{
    if (!mConstantsCached) {
        int64_t physMem = 0;
        size_t len = sizeof(physMem);
        if (sysctlbyname("hw.memsize", &physMem, &len, nullptr, 0) == 0)
            mPhysicalMemory = static_cast<quint64>(physMem);
        host_page_size(mach_host_self(), &mPageSize);
        mConstantsCached = true;
    }
    memTotal = mPhysicalMemory;
    vm_size_t pageSize = mPageSize;

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

        // FR-57: Memory breakdown fields
        memWired = static_cast<quint64>(vmStat.wire_count) * pageSize;
        memActive = static_cast<quint64>(vmStat.active_count) * pageSize;
        memInactive = inactive;
        memCompressed = static_cast<quint64>(vmStat.compressor_page_count) * pageSize;
        memAvailable = free + inactive + purgeable;
    }

    // Swap usage via sysctl
    struct xsw_usage swapUsage;
    size_t swapLen = sizeof(swapUsage);
    if (sysctlbyname("vm.swapusage", &swapUsage, &swapLen, nullptr, 0) == 0) {
        swapTotal = swapUsage.xsu_total;
        swapUsed = swapUsage.xsu_used;
        swapFree = swapUsage.xsu_avail;
    }

    // FR-57: Memory pressure level via sysctl
    int level = 0;
    size_t levelLen = sizeof(level);
    if (sysctlbyname("kern.memorystatus_vm_pressure_level", &level, &levelLen, nullptr, 0) == 0) {
        pressureLevel = level;
    } else {
        pressureLevel = -1;
    }
}
