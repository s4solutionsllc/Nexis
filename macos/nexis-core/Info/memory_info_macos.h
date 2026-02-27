#ifndef MEMORY_INFO_MACOS_H
#define MEMORY_INFO_MACOS_H

#include <Info/memory_info.h>
#include <mach/mach_types.h>

class MemoryInfoMacOS : public MemoryInfo
{
public:
    void updateMemoryInfo() override;

private:
    quint64 mPhysicalMemory = 0;
    vm_size_t mPageSize = 0;
    bool mConstantsCached = false;
};

#endif // MEMORY_INFO_MACOS_H
