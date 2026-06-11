#ifndef BOOT_ANALYSIS_INFO_MACOS_H
#define BOOT_ANALYSIS_INFO_MACOS_H

#include <Info/boot_analysis_info.h>

#include <QString>

class BootAnalysisInfoMacOS : public BootAnalysisInfo
{
public:
    BootAnalysisData analyze() const override;

    // WI-33: pure parser for `sysctl kern.boottime` output. Returns the boot
    // seconds-since-epoch on success, or -1 if the input did not match the
    // expected `sec = N` shape. Exposed for fixture tests.
    static qint64 parseKernBoottime(const QString &sysctlOutput);
};

#endif // BOOT_ANALYSIS_INFO_MACOS_H
