#ifndef PROCESS_INFO_MACOS_H
#define PROCESS_INFO_MACOS_H

#include <Info/process_info.h>
#include <QHash>
#include <QElapsedTimer>

class ProcessInfoMacOS : public ProcessInfo
{
    Q_OBJECT

public:
    void updateProcesses() override;

private:
    QHash<pid_t, QPair<quint64, quint64>> mPrevDiskIo;
    QHash<pid_t, QPair<quint64, quint64>> mPrevNetIo;
    QElapsedTimer mIoTimer;
    bool mIoTimerStarted = false;
};

#endif // PROCESS_INFO_MACOS_H
