#include "gpu_info_linux.h"
#include "Utils/command_util.h"
#include "nvidia_smi_cache.h"

#include <QDebug>
#include <QDir>
#include <QRegularExpression>

// PCI vendor IDs
static constexpr const char *PCI_VENDOR_AMD    = "0x1002";
static constexpr const char *PCI_VENDOR_NVIDIA = "0x10de";
static constexpr const char *PCI_VENDOR_INTEL  = "0x8086";

static constexpr const char *DRM_BASE = "/sys/class/drm";

GpuInfoLinux::GpuInfoLinux()
{
    discoverGpus();
}

/**
 * Read the kernel driver name for a DRM card's device.
 * Returns the driver basename (e.g. "nvidia", "amdgpu", "simple-framebuffer").
 */
static QString readDriverName(const QString &cardPath)
{
    QFileInfo driverLink(cardPath + "/device/driver");
    if (driverLink.isSymLink()) {
        QString target = driverLink.symLinkTarget();
        return target.section('/', -1);
    }
    return {};
}

/**
 * Read a device name for a DRM card.
 *
 * Tries (in order):
 *   1. /sys/class/drm/cardN/device/label       (kernel 5.18+)
 *   2. lspci -s <bus-id> output                 (most distros)
 *   3. Generic fallback: "GPU N"
 */
static QString readDeviceName(const QString &cardPath, int cardIndex, const QString &vendor)
{
    // Try device/label (recent kernels)
    QString label = FileUtil::readStringFromFile(cardPath + "/device/label").trimmed();
    if (!label.isEmpty())
        return label;

    // Try to get the PCI bus address and use lspci
    // The symlink /sys/class/drm/cardN/device points to the PCI device
    QFileInfo deviceLink(cardPath + "/device");
    if (deviceLink.isSymLink()) {
        // Extract PCI address from the symlink target, e.g. "0000:03:00.0"
        QString target = deviceLink.symLinkTarget();
        QString busId = target.section('/', -1);  // last component
        // lspci -s accepts full "0000:04:00.0" but prints short "04:00.0"
        QString shortBusId = busId;
        if (shortBusId.startsWith("0000:"))
            shortBusId = shortBusId.mid(5);
        if (!busId.isEmpty() && CommandUtil::isExecutable("lspci")) {
            ExecResult result = CommandUtil::execWithStatus("lspci", {"-s", busId});
            if (result.ok()) {
                QString name = GpuInfo::parseLspciDeviceName(result.output, shortBusId);
                if (!name.isEmpty())
                    return name;
            } else {
                qWarning() << "Failed to resolve GPU name via lspci:" << result.error;
            }
        }
    }

    // Fallback
    return QString("%1 GPU %2").arg(vendor).arg(cardIndex);
}

/**
 * Resolve device name for a framebuffer GPU via its parent PCI bus address.
 * lspci uses short-form addresses (e.g. "04:00.0") while sysfs uses the full
 * domain-prefixed form ("0000:04:00.0"), so we pass the full form to lspci -s
 * (which accepts both) but search in the output for the short form it prints.
 */
static QString readFramebufferDeviceName(const QString &pciBusId, int cardIndex)
{
    if (!pciBusId.isEmpty() && CommandUtil::isExecutable("lspci")) {
        // lspci -s accepts full "0000:04:00.0" but prints short "04:00.0"
        QString shortBusId = pciBusId;
        if (shortBusId.startsWith("0000:"))
            shortBusId = shortBusId.mid(5);
        ExecResult result = CommandUtil::execWithStatus("lspci", {"-s", pciBusId});
        if (result.ok()) {
            QString name = GpuInfo::parseLspciDeviceName(result.output, shortBusId);
            if (!name.isEmpty())
                return name;
        } else {
            qWarning() << "Failed to resolve framebuffer GPU name via lspci:" << result.error;
        }
    }
    return QString("GPU %1").arg(cardIndex);
}

/**
 * Query nvidia-smi once at startup to build a map from normalized PCI bus ID
 * to nvidia-smi device index.  Returns empty map if nvidia-smi is unavailable.
 */
static QHash<QString, int> buildNvidiaIndexMap()
{
    QHash<QString, int> map;
    if (!CommandUtil::isExecutable("nvidia-smi"))
        return map;

    ExecResult result = CommandUtil::execWithStatus(
        "nvidia-smi",
        {"--query-gpu=index,pci.bus_id", "--format=csv,noheader,nounits"},
        5000);

    if (result.exitCode != 0) {
        qWarning() << "gpu_info: nvidia-smi PCI index query failed:" << result.error;
        return map;
    }

    for (const QString &line : result.output.trimmed().split('\n', Qt::SkipEmptyParts)) {
        QStringList parts = line.split(',');
        if (parts.size() < 2)
            continue;
        bool ok = false;
        int idx = parts.at(0).trimmed().toInt(&ok);
        if (!ok)
            continue;
        map.insert(GpuInfo::normalizePciBusId(parts.at(1).trimmed()), idx);
    }
    return map;
}

/**
 * Map a PCI vendor ID string to a vendor name.
 */
static QString vendorFromPciId(const QString &vendorId)
{
    if (vendorId == PCI_VENDOR_AMD)    return "AMD";
    if (vendorId == PCI_VENDOR_NVIDIA) return "NVIDIA";
    if (vendorId == PCI_VENDOR_INTEL)  return "Intel";
    return {};
}

void GpuInfoLinux::discoverGpus()
{
    mDevices.clear();
    mDiagEntries.clear();

    const QHash<QString, int> nvidiaIndexMap = buildNvidiaIndexMap();

    QDir drmDir(DRM_BASE);
    if (!drmDir.exists())
        return;

    // Enumerate card* entries (card0, card1, ...)
    QStringList cardEntries = drmDir.entryList({"card[0-9]*"}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    // Filter to only "cardN" (not "card0-HDMI-A-1" etc.)
    static QRegularExpression reCard("^card\\d+$");

    for (const QString &entry : cardEntries) {
        if (!reCard.match(entry).hasMatch())
            continue;

        QString cardPath = QString("%1/%2").arg(DRM_BASE, entry);
        int cardIndex = entry.mid(4).toInt();  // "card0" → 0

        QString driverName = readDriverName(cardPath);

        // Read PCI vendor ID
        QString vendorId = FileUtil::readStringFromFile(cardPath + "/device/vendor").trimmed();

        // Extract PCI bus address from device symlink (e.g. "0000:03:00.0")
        QFileInfo deviceLink(cardPath + "/device");
        QString pciBusId;
        if (deviceLink.isSymLink())
            pciBusId = deviceLink.symLinkTarget().section('/', -1);

        GpuDevice dev;
        dev.utilization = -1;
        dev.deviceIndex = cardIndex;
        dev.pciBusId = pciBusId;
        dev.driverName = driverName;

        QString vendor = vendorFromPciId(vendorId);

        if (!vendor.isEmpty()) {
            // Standard PCI GPU device with a recognized vendor
            dev.vendor = vendor;
            dev.name = readDeviceName(cardPath, cardIndex, vendor);

            if (vendor == "AMD") {
                dev.sysfsLoadPath = cardPath + "/device/gpu_busy_percent";
                if (!QFile::exists(dev.sysfsLoadPath))
                    dev.sysfsLoadPath.clear();
            } else if (vendor == "NVIDIA") {
                const QString normId = GpuInfo::normalizePciBusId(pciBusId);
                if (nvidiaIndexMap.contains(normId))
                    dev.queryCommand = QString::number(nvidiaIndexMap.value(normId));
            } else if (vendor == "Intel") {
                // GH#91: the xe driver (kernel 6.8+) exposes freq under
                // device/tile<T>/gt<G>/freq0/ rather than the i915 flat paths.
                // Probe xe first; fall back to i915 so existing users don't regress.
                QString xeFreqDir = GpuInfo::findIntelXeFreqDir(cardPath + "/device");
                if (!xeFreqDir.isEmpty()) {
                    dev.sysfsLoadPath = xeFreqDir + "/cur_freq";
                    dev.queryCommand = xeFreqDir + "/max_freq";
                } else {
                    QString curFreqPath = cardPath + "/gt_cur_freq_mhz";
                    QString maxFreqPath = cardPath + "/gt_max_freq_mhz";
                    if (QFile::exists(curFreqPath) && QFile::exists(maxFreqPath)) {
                        dev.sysfsLoadPath = curFreqPath;
                        dev.queryCommand = maxFreqPath;
                    }
                }
            }

            mDevices.append(dev);
            mDiagEntries.append({entry, driverName, vendorId, pciBusId, dev.name, "detected"});

        } else if (driverName == "simple-framebuffer") {
            // Framebuffer-only GPU: trace back to parent PCI device
            QFileInfo cardLink(cardPath);
            QString symlinkTarget = cardLink.isSymLink() ? cardLink.symLinkTarget() : QString();
            QString parentPciBusId = parseFramebufferParentPciBusId(symlinkTarget);

            if (parentPciBusId.isEmpty()) {
                mDiagEntries.append({entry, driverName, vendorId, pciBusId,
                                     QString(), "skipped (no parent PCI device)"});
                continue;
            }

            // Read the parent PCI device's vendor ID
            QString parentVendorId = FileUtil::readStringFromFile(
                QString("/sys/bus/pci/devices/%1/vendor").arg(parentPciBusId)).trimmed();
            QString parentVendor = vendorFromPciId(parentVendorId);

            if (parentVendor.isEmpty()) {
                mDiagEntries.append({entry, driverName, parentVendorId, parentPciBusId,
                                     QString(), "skipped (unknown parent vendor)"});
                continue;
            }

            dev.vendor = parentVendor;
            dev.pciBusId = parentPciBusId;
            dev.name = readFramebufferDeviceName(parentPciBusId, cardIndex);

            // Framebuffer GPUs have no utilization source — leave sysfsLoadPath
            // and queryCommand empty so utilization stays at -1 (shown as "N/A")

            mDevices.append(dev);
            mDiagEntries.append({entry, driverName, parentVendorId, parentPciBusId,
                                 dev.name, "detected (framebuffer-only)"});
        } else {
            mDiagEntries.append({entry, driverName, vendorId, pciBusId,
                                 QString(), "skipped (unknown vendor)"});
        }
    }

    // DRM card order (card0, card1, ...) is the kernel's native GPU ordering.
    // This matches libdrm-based tools (Mission Center, nvtop) and is the most
    // widely used convention on Linux. No explicit sort needed — QDir::Name
    // already yields card0 before card1.

    qDebug() << "GPU discovery:" << mDiagEntries.size() << "DRM card(s) found,"
             << mDevices.size() << "GPU(s) detected:";
    for (const DiagEntry &e : mDiagEntries)
        qDebug() << "  " << e.cardEntry << "|" << e.status
                 << "| driver:" << e.driverName
                 << "| vendor:" << e.vendorId
                 << "| PCI:" << e.pciBusId
                 << "|" << e.deviceName;
}

void GpuInfoLinux::updateGpuInfo()
{
    // FR-106: one nvidia-smi fork per tick covering all devices, shared with
    // FanInfoLinux::readNvidiaSpeed. Previously each NVIDIA device paid a
    // per-tick fork for utilization and a second for fan speed.
    bool anyNvidia = false;
    for (const GpuDevice &dev : mDevices) {
        if (dev.vendor == "NVIDIA" && !dev.queryCommand.isEmpty()) {
            anyNvidia = true;
            break;
        }
    }
    if (anyNvidia)
        NvidiaSmiCache::refresh();

    for (int i = 0; i < mDevices.size(); ++i) {
        GpuDevice &dev = mDevices[i];

        if (dev.vendor == "AMD") {
            if (!dev.sysfsLoadPath.isEmpty())
                dev.utilization = parseSysfsUtilization(
                    FileUtil::readStringFromFile(dev.sysfsLoadPath));

        } else if (dev.vendor == "NVIDIA") {
            if (!dev.queryCommand.isEmpty()) {
                bool ok = false;
                const int idx = dev.queryCommand.toInt(&ok);
                if (ok) {
                    NvidiaSmiCache::Sample s = NvidiaSmiCache::get(idx);
                    dev.utilization = s.utilization;
                }
            }

        } else if (dev.vendor == "Intel") {
            if (!dev.sysfsLoadPath.isEmpty() && !dev.queryCommand.isEmpty()) {
                dev.utilization = parseIntelFreqUtilization(
                    FileUtil::readStringFromFile(dev.sysfsLoadPath),
                    FileUtil::readStringFromFile(dev.queryCommand));
            }
        }
    }
}

QString GpuInfoLinux::getDiagnosticReport() const
{
    QString report;
    report += "=== Nexis GPU Diagnostics (Linux) ===\n";
    report += QString("Date: %1\n").arg(
        QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    report += QString("Kernel: %1\n\n").arg(
        FileUtil::readStringFromFile("/proc/version").trimmed().section(' ', 0, 2));

    report += QString("DRM Cards Scanned: %1\n").arg(mDiagEntries.size());
    for (const DiagEntry &e : mDiagEntries) {
        report += QString("  %1: driver=%2, vendor=%3, pci=%4\n").arg(
            e.cardEntry, e.driverName.isEmpty() ? "(none)" : e.driverName,
            e.vendorId.isEmpty() ? "(none)" : e.vendorId,
            e.pciBusId.isEmpty() ? "(none)" : e.pciBusId);
        if (!e.deviceName.isEmpty())
            report += QString("       name=%1\n").arg(e.deviceName);
        report += QString("       status: %1\n").arg(e.status);
    }

    // nvidia-smi cross-reference
    if (CommandUtil::isExecutable("nvidia-smi")) {
        report += "\nnvidia-smi devices:\n";
        ExecResult result = CommandUtil::execWithStatus("nvidia-smi", {"-L"});
        if (result.ok()) {
            QStringList lines = result.output.trimmed().split('\n');
            for (const QString &line : lines) {
                if (!line.trimmed().isEmpty())
                    report += QString("  %1\n").arg(line.trimmed());
            }
        } else {
            qWarning() << "gpu_info: failed to query nvidia-smi -L:" << result.error;
            report += "  (failed to query nvidia-smi)\n";
        }
    }

    report += QString("\nNexis GPU List: %1 device(s)\n").arg(mDevices.size());
    for (int i = 0; i < mDevices.size(); ++i) {
        const GpuDevice &d = mDevices.at(i);
        report += QString("  [%1] card%2: %3 (%4)\n").arg(i).arg(d.deviceIndex).arg(d.name, d.vendor);
        report += QString("       PCI: %1, Driver: %2\n").arg(
            d.pciBusId.isEmpty() ? "N/A" : d.pciBusId,
            d.driverName.isEmpty() ? "N/A" : d.driverName);
        QString utilStr = d.utilization < 0 ? "N/A"
            : (d.queryCommand.isEmpty() && d.sysfsLoadPath.isEmpty() ? "N/A (no monitoring source)"
               : QString("%1%").arg(d.utilization));
        report += QString("       Utilization: %1\n").arg(utilStr);
    }

    return report;
}

// Getters (getGpuDevices, hasGpu) are in shared/nexis-core/Info/gpu_info_shared.cpp
