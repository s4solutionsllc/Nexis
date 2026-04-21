#ifndef PROCESS_INFO_H
#define PROCESS_INFO_H

#include <QObject>

#include <Utils/file_util.h>
#include <Utils/command_util.h>
#include "process.h"

#include "nexis-core_global.h"

class NEXISCORESHARED_EXPORT ProcessInfo : public QObject
{
    Q_OBJECT

public:
    virtual ~ProcessInfo() = default;

    QList<Process> getProcessList() const;

    // FR-108: toggles for expensive per-PID data collection. Linux
    // /proc/<pid>/io reads and macOS nettop forks are skipped when the
    // matching columns are all hidden on the Processes page. Defaults
    // mirror the page's initial hidden-column state (both off).
    // FR-115 added mCollectGpu for /proc/<pid>/fdinfo and nvidia-smi pmon.
    void setCollectDiskIO(bool enabled) { mCollectDiskIO = enabled; }
    void setCollectNetIO(bool enabled)  { mCollectNetIO  = enabled; }
    void setCollectGpu(bool enabled)    { mCollectGpu    = enabled; }
    bool collectsDiskIO() const { return mCollectDiskIO; }
    bool collectsNetIO() const  { return mCollectNetIO; }
    bool collectsGpu() const    { return mCollectGpu; }

public slots:
    virtual void updateProcesses() = 0;

protected:
    QList<Process> processList;
    bool mCollectDiskIO = false;
    bool mCollectNetIO  = false;
    bool mCollectGpu    = false;
};

#endif // PROCESS_INFO_H
