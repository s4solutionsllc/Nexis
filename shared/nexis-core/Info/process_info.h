#ifndef PROCESS_INFO_H
#define PROCESS_INFO_H

#include <QMutex>
#include <QObject>
#include <QString>

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

    // SSO-15379: per-process network collection can silently fail (missing
    // CAP_BPF/root, no nethogs, unsupported kernel) while mCollectNetIO stays
    // true — the UI must not read that as "zero traffic". Subclasses report
    // which mechanism (if any) is actually feeding netUpRate/netDownRate so
    // ProcessesPage can show an explicit notice instead of blank "—" cells.
    enum class NetIoStatus {
        Disabled,          // columns hidden — collection not requested
        ActiveEbpf,        // Linux: kprobe byte counters attached and readable
        ActiveNetHogs,      // Linux: falling back to the external nethogs binary
        ActiveNetTop,       // macOS: NettopStreamer running
        PermissionDenied,  // Linux: eBPF load/attach failed with EPERM/EACCES
                            // and no nethogs fallback was available either
        Unavailable        // no working mechanism at all (old kernel, no
                            // libbpf at build time, nethogs not installed)
    };
    NetIoStatus netIoStatus() const { return mNetIoStatus; }
    QString netIoStatusDetail() const { return mNetIoStatusDetail; }

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

    NetIoStatus mNetIoStatus = NetIoStatus::Disabled;
    QString mNetIoStatusDetail;
};

#endif // PROCESS_INFO_H
