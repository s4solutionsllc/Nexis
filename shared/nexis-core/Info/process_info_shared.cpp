// Shared ProcessInfo methods — identical across platforms.
// Platform-specific collectProcesses() lives in
// linux/nexis-core/Info/process_info.cpp and macos/nexis-core/Info/process_info.cpp.

#include "process_info.h"

#include <QMutexLocker>

QList<Process> ProcessInfo::getProcessList() const
{
    QMutexLocker locker(&processListMutex);
    return processList;
}

void ProcessInfo::setProcessList(QList<Process> processes)
{
    QMutexLocker locker(&processListMutex);
    processList = std::move(processes);
}

void ProcessInfo::updateProcesses()
{
    // WI-21: legacy synchronous path. The periodic tick calls
    // collectProcesses() + setProcessList() across a worker hop directly.
    setProcessList(collectProcesses());
}
