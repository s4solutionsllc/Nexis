// Shared GpuInfo methods — identical across platforms.
// Platform-specific constructor, discoverGpus(), and updateGpuInfo() live in
// linux/nexis-core/Info/gpu_info.cpp and macos/nexis-core/Info/gpu_info.cpp.

#include "gpu_info.h"

QList<GpuDevice> GpuInfo::getGpuDevices() const
{
    return mDevices;
}

bool GpuInfo::hasGpu() const
{
    return !mDevices.isEmpty();
}

int GpuInfo::parseNvidiaSmiUtilization(const QString &nvidiaSmiOutput)
{
    QString val = nvidiaSmiOutput.trimmed().split('\n').first().trimmed();
    if (val.isEmpty())
        return -1;
    bool ok = false;
    int pct = val.toInt(&ok);
    return ok ? qBound(0, pct, 100) : -1;
}

int GpuInfo::parseSysfsUtilization(const QString &sysfsContent)
{
    QString val = sysfsContent.trimmed();
    if (val.isEmpty())
        return -1;
    bool ok = false;
    int pct = val.toInt(&ok);
    return ok ? qBound(0, pct, 100) : -1;
}

int GpuInfo::parseIntelFreqUtilization(const QString &curFreqStr, const QString &maxFreqStr)
{
    bool okCur = false, okMax = false;
    double cur = curFreqStr.trimmed().toDouble(&okCur);
    double max = maxFreqStr.trimmed().toDouble(&okMax);
    if (okCur && okMax && max > 0.0)
        return qBound(0, static_cast<int>(cur / max * 100.0), 100);
    return -1;
}

QString GpuInfo::parseLspciDeviceName(const QString &lspciOutput, const QString &busId)
{
    int colonPos = lspciOutput.indexOf(": ", lspciOutput.indexOf(busId));
    if (colonPos >= 0) {
        QString name = lspciOutput.mid(colonPos + 2).trimmed();
        name = name.split('\n').first().trimmed();
        if (!name.isEmpty())
            return name;
    }
    return {};
}
