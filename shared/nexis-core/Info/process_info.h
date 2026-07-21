#ifndef PROCESS_INFO_H
#define PROCESS_INFO_H

#include <QMutex>
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

    // SSO-15379: lets the Processes page distinguish "no data because the
    // column is hidden / nothing to report yet" from "no data because the
    // required tool/permission is missing" so it can show a real message
    // instead of silently rendering blank/zero. Platforms whose net-IO
    // source doesn't need an external tool (e.g. macOS's built-in nettop)
    // simply report Available always.
    enum class NetIoAvailability {
        Available,
        ToolMissing,        // e.g. nethogs not installed (Linux)
        PermissionDenied,   // tool present but couldn't get a capture socket
    };
    virtual NetIoAvailability netIoAvailability() const { return NetIoAvailability::Available; }

    // WI-21 (audit M2): thread-safe publish pair, mirrors
    // DiskHealthInfo::collectDriveHealth()/setDrives() (WI-03).
    // collectProcesses() runs the per-PID walk and any forks (`ps` on
    // macOS) into a local QList<Process> without touching processList —
    // safe to call from a QtConcurrent worker. setProcessList() assigns
    // the cache from the UI thread (DataRefreshService hops via
    // QMetaObject::invokeMethod). mCollectMutex serialises concurrent
    // collect calls so the per-PID state (mPrev* deltas, timers) in the
    // subclasses stays coherent if a sync caller (e.g. the HTML report)
    // races the worker tick.
    virtual QList<Process> collectProcesses() = 0;
    void setProcessList(QList<Process> processes);

public slots:
    // Legacy entry point: collects and publishes on the calling thread.
    // Kept for the synchronous HTML-report path; the periodic tick uses
    // collectProcesses() + setProcessList() across a worker hop.
    virtual void updateProcesses();

protected:
    QList<Process> processList;
    mutable QMutex processListMutex;
    QMutex mCollectMutex;
    bool mCollectDiskIO = false;
    bool mCollectNetIO  = false;
    bool mCollectGpu    = false;
};

#endif // PROCESS_INFO_H
