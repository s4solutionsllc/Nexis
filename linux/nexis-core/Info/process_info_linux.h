#ifndef PROCESS_INFO_LINUX_H
#define PROCESS_INFO_LINUX_H

#include <Info/process_info.h>
#include <QElapsedTimer>
#include <QHash>
#include <QString>
#include <memory>

#include <sys/types.h>   // uid_t, gid_t

class NetAcctBpfLoader;
class NetHogsStreamer;

class ProcessInfoLinux : public ProcessInfo
{
    Q_OBJECT

public:
    ProcessInfoLinux();
    // Needed out-of-line (defined in process_info.cpp, where NetAcctBpfLoader
    // and NetHogsStreamer are complete types) because of the unique_ptr
    // members below — the implicit destructor would otherwise need their
    // full definitions right here.
    ~ProcessInfoLinux() override;
    QList<Process> collectProcesses() override;

private:
    // FR-127: read once at construction — sysconf() calls aren't free per-tick.
    long    mClkTck        = 100;
    long    mPageSize      = 4096;
    quint64 mBootTimeSec   = 0;
    quint64 mTotalMemBytes = 0;

    // FR-127: baselines for per-process %CPU calc.
    QHash<pid_t, quint64> mPrevCpuTotal;
    QElapsedTimer         mCpuTimer;
    bool                  mCpuTimerStarted = false;

    // FR-127: uid/gid → name cache (~50 unique users on a loaded system).
    QHash<uid_t, QString> mUidNameCache;
    QHash<gid_t, QString> mGidNameCache;

    // FR-108 (Bundle B): disk I/O baseline state — preserved.
    QHash<pid_t, QPair<quint64, quint64>> mPrevDiskIo;
    QElapsedTimer                         mIoTimer;
    bool                                  mIoTimerStarted = false;

    // SSO-15379: per-process network. eBPF (mBpfNet) is tried first and, if
    // loaded, wins outright; mNetHogs is the fallback started lazily the
    // first time eBPF isn't usable and the nethogs binary is found on PATH.
    // mPrevNetIo holds eBPF's cumulative counters for delta-tracking (like
    // mPrevDiskIo above) — nethogs already reports a live rate, so its path
    // doesn't need baseline tracking.
    std::unique_ptr<NetAcctBpfLoader>      mBpfNet;
    std::unique_ptr<NetHogsStreamer>       mNetHogs;
    QHash<pid_t, QPair<quint64, quint64>>  mPrevNetIo;
    QElapsedTimer                          mNetTimer;
    bool                                   mNetTimerStarted = false;
    bool                                   mNetHogsPathChecked = false;
    bool                                   mNetHogsOnPath = false;

    // FR-115: per-PID engine-ns baseline for GPU% delta + wall-clock timer.
    QHash<pid_t, quint64> mPrevGpuEngineNs;
    QElapsedTimer         mGpuTimer;
    bool                  mGpuTimerStarted = false;

    QString lookupUid(uid_t uid);
    QString lookupGid(gid_t gid);

    void collectGpuForPid(pid_t pid, Process &proc, double elapsedSecs);
};

#endif // PROCESS_INFO_LINUX_H
