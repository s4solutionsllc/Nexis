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
    // Started lazily when mCollectNetIO first flips on; torn down when it
    // flips off.
    std::unique_ptr<NettopStreamer> mNettopStreamer;
};

#endif // PROCESS_INFO_MACOS_H
