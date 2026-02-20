#ifndef MEMORY_INFO_MACOS_H
#define MEMORY_INFO_MACOS_H

#include <Info/memory_info.h>

class MemoryInfoMacOS : public MemoryInfo
{
public:
    void updateMemoryInfo() override;
};

#endif // MEMORY_INFO_MACOS_H
