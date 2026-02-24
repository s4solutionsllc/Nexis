#ifndef PROCESS_INFO_LINUX_H
#define PROCESS_INFO_LINUX_H

#include <Info/process_info.h>
#include <QHash>
#include <QElapsedTimer>

class ProcessInfoLinux : public ProcessInfo
{
    Q_OBJECT

public:
    void updateProcesses() override;

private:
    QHash<pid_t, QPair<quint64, quint64>> mPrevDiskIo;
    QElapsedTimer mIoTimer;
    bool mIoTimerStarted = false;
};

#endif // PROCESS_INFO_LINUX_H
