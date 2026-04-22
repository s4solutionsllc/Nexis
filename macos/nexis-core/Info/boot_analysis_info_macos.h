#ifndef BOOT_ANALYSIS_INFO_MACOS_H
#define BOOT_ANALYSIS_INFO_MACOS_H

#include <Info/boot_analysis_info.h>

class BootAnalysisInfoMacOS : public BootAnalysisInfo
{
public:
    BootAnalysisData analyze() const override;
};

#endif // BOOT_ANALYSIS_INFO_MACOS_H
