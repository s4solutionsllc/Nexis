#ifndef BOOT_ANALYSIS_INFO_LINUX_H
#define BOOT_ANALYSIS_INFO_LINUX_H

#include <Info/boot_analysis_info.h>

class BootAnalysisInfoLinux : public BootAnalysisInfo
{
public:
    BootAnalysisData analyze() const override;
};

#endif // BOOT_ANALYSIS_INFO_LINUX_H
