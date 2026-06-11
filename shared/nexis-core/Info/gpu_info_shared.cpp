// Shared GpuInfo methods — identical across platforms.
// Platform-specific constructor, discoverGpus(), and updateGpuInfo() live in
// linux/nexis-core/Info/gpu_info.cpp and macos/nexis-core/Info/gpu_info.cpp.

#include "gpu_info.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSysInfo>
#include <QDateTime>

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

QString GpuInfo::parseFramebufferParentPciBusId(const QString &symlinkTarget)
{
    // Find the PCI bus address immediately before "simple-framebuffer" in the path.
    // Example: ".../0000:04:00.0/simple-framebuffer.0/drm/card0" → "0000:04:00.0"
    int fbPos = symlinkTarget.indexOf("simple-framebuffer");
    if (fbPos < 0)
        return {};

    // Look backwards from simple-framebuffer for a PCI address pattern
    QString prefix = symlinkTarget.left(fbPos);
    static QRegularExpression rePci("(\\d{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}\\.\\d)/?$");
    QRegularExpressionMatch m = rePci.match(prefix);
    if (m.hasMatch())
        return m.captured(1);

    return {};
}

QString GpuInfo::normalizePciBusId(const QString &rawBusId)
{
    QStringList segs = rawBusId.split(':');
    if (segs.size() < 2)
        return rawBusId.toLower();
    return (segs.at(segs.size() - 2) + ":" + segs.last()).toLower();
}

QString GpuInfo::findIntelXeFreqDir(const QString &devicePath)
{
    // xe driver (kernel 6.8+) exposes per-tile, per-GT frequency files under
    //   <devicePath>/tile<T>/gt<G>/freq0/{cur_freq,max_freq,min_freq}
    // For integrated GPUs there is a single tile0/gt0. We scan in deterministic
    // QDir::Name order and return the first freq0 dir that has both cur_freq
    // and max_freq so the caller can wire utilization the same way as i915.
    QDir devDir(devicePath);
    if (!devDir.exists())
        return {};

    const QStringList tiles = devDir.entryList(
        {"tile*"}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &tile : tiles) {
        QDir tileDir(devDir.filePath(tile));
        const QStringList gts = tileDir.entryList(
            {"gt*"}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &gt : gts) {
            QString freqDir = tileDir.filePath(gt) + "/freq0";
            if (QFile::exists(freqDir + "/cur_freq")
                && QFile::exists(freqDir + "/max_freq")) {
                return freqDir;
            }
        }
    }
    return {};
}

QString GpuInfo::getDiagnosticReport() const
{
    QString report;
    report += "=== Nexis GPU Diagnostics ===\n";
    report += QString("Date: %1\n").arg(
        QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    report += QString("Platform: %1 %2\n\n").arg(
        QSysInfo::kernelType(), QSysInfo::kernelVersion());

    report += QString("Detected GPUs: %1\n").arg(mDevices.size());
    for (int i = 0; i < mDevices.size(); ++i) {
        const GpuDevice &d = mDevices.at(i);
        report += QString("  [%1] card%2: %3 (%4)\n").arg(i).arg(d.deviceIndex).arg(d.name, d.vendor);
        if (!d.pciBusId.isEmpty())
            report += QString("       PCI: %1\n").arg(d.pciBusId);
        if (!d.driverName.isEmpty())
            report += QString("       Driver: %1\n").arg(d.driverName);
        report += QString("       Utilization: %1\n").arg(
            d.utilization < 0 ? "N/A" : QString("%1%").arg(d.utilization));
    }

    return report;
}
