#ifndef MEMORY_INFO_LINUX_H
#define MEMORY_INFO_LINUX_H

#include <Info/memory_info.h>

class MemoryInfoLinux : public MemoryInfo
{
public:
    void updateMemoryInfo() override;
};

#endif // MEMORY_INFO_LINUX_H
