#ifndef PROCESS_INFO_LINUX_H
#define PROCESS_INFO_LINUX_H

#include <Info/process_info.h>
#include <QElapsedTimer>
#include <QHash>
#include <QString>

#include <sys/types.h>   // uid_t, gid_t

class ProcessInfoLinux : public ProcessInfo
{
    Q_OBJECT

public:
    ProcessInfoLinux();
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

    // FR-115: per-PID engine-ns baseline for GPU% delta + wall-clock timer.
    QHash<pid_t, quint64> mPrevGpuEngineNs;
    QElapsedTimer         mGpuTimer;
    bool                  mGpuTimerStarted = false;

    QString lookupUid(uid_t uid);
    QString lookupGid(gid_t gid);

    void collectGpuForPid(pid_t pid, Process &proc, double elapsedSecs);
};

#endif // PROCESS_INFO_LINUX_H
