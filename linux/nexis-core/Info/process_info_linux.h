#ifndef PROCESS_INFO_LINUX_H
#define PROCESS_INFO_LINUX_H

#include <Info/process_info.h>

class ProcessInfoLinux : public ProcessInfo
{
    Q_OBJECT

public:
    void updateProcesses() override;
};

#endif // PROCESS_INFO_LINUX_H
