#ifndef PROCESS_INFO_MACOS_H
#define PROCESS_INFO_MACOS_H

#include <Info/process_info.h>
#include <QHash>
#include <QElapsedTimer>
#include <memory>

class NettopStreamer;

class ProcessInfoMacOS : public ProcessInfo
{
    Q_OBJECT

public:
    ProcessInfoMacOS();
    ~ProcessInfoMacOS() override;

    void updateProcesses() override;

private:
    QHash<pid_t, QPair<quint64, quint64>> mPrevDiskIo;
    QHash<pid_t, QPair<quint64, quint64>> mPrevNetIo;
    QElapsedTimer mIoTimer;
    bool mIoTimerStarted = false;

    // FR-102: one long-lived nettop child instead of forking per tick.
    std::unique_ptr<NettopStreamer> mNettopStreamer;

    // FR-128: per-process GPU% via AGXDeviceUserClient tree walk
    QHash<pid_t, quint64> mPrevGpuNs;
    QElapsedTimer mGpuTimer;
    bool mGpuTimerStarted = false;
    static QHash<pid_t, quint64> collectGpuNs();
};

#endif // PROCESS_INFO_MACOS_H
