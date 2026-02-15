// Shared ProcessInfo methods — identical across platforms.
// Platform-specific updateProcesses() lives in
// linux/nexis-core/Info/process_info.cpp and macos/nexis-core/Info/process_info.cpp.

#include "process_info.h"

QList<Process> ProcessInfo::getProcessList() const
{
    return processList;
}
