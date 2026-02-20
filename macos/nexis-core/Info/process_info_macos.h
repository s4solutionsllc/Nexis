#ifndef PROCESS_INFO_MACOS_H
#define PROCESS_INFO_MACOS_H

#include <Info/process_info.h>

class ProcessInfoMacOS : public ProcessInfo
{
    Q_OBJECT

public:
    void updateProcesses() override;
};

#endif // PROCESS_INFO_MACOS_H
