#include "gpu_info_linux.h"
#include "Utils/command_util.h"

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
        if (!busId.isEmpty() && CommandUtil::isExecutable("lspci")) {
            try {
                QString lspciOut = CommandUtil::exec("lspci", {"-s", busId});
                QString name = GpuInfo::parseLspciDeviceName(lspciOut, busId);
                if (!name.isEmpty())
                    return name;
            } catch (...) { qWarning() << "Failed to resolve GPU name via lspci"; }
        }
    }

    // Fallback
    return QString("%1 GPU %2").arg(vendor).arg(cardIndex);
}

void GpuInfoLinux::discoverGpus()
{
    mDevices.clear();

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

        if (vendorId == PCI_VENDOR_AMD) {
            dev.vendor = "AMD";
            dev.sysfsLoadPath = cardPath + "/device/gpu_busy_percent";
            dev.name = readDeviceName(cardPath, cardIndex, "AMD");

            // Verify the sysfs file exists
            if (!QFile::exists(dev.sysfsLoadPath))
                dev.sysfsLoadPath.clear();

            mDevices.append(dev);

        } else if (vendorId == PCI_VENDOR_NVIDIA) {
            dev.vendor = "NVIDIA";
            dev.name = readDeviceName(cardPath, cardIndex, "NVIDIA");

            // NVIDIA: use nvidia-smi for utilization, addressed by PCI bus ID
            // (DRM card index != nvidia-smi device index when other cards exist)
            if (!pciBusId.isEmpty() && CommandUtil::isExecutable("nvidia-smi"))
                dev.queryCommand = pciBusId;

            mDevices.append(dev);

        } else if (vendorId == PCI_VENDOR_INTEL) {
            dev.vendor = "Intel";
            dev.name = readDeviceName(cardPath, cardIndex, "Intel");

            // Intel: check for gt frequency files (i915 / Xe)
            // /sys/class/drm/cardN/gt_cur_freq_mhz and gt_max_freq_mhz
            QString curFreqPath = cardPath + "/gt_cur_freq_mhz";
            QString maxFreqPath = cardPath + "/gt_max_freq_mhz";
            if (QFile::exists(curFreqPath) && QFile::exists(maxFreqPath)) {
                dev.sysfsLoadPath = curFreqPath;  // store cur freq path
                dev.queryCommand = maxFreqPath;    // reuse field for max freq path
            }

            mDevices.append(dev);
        }
        // Skip unknown vendors
    }

    // DRM card order (card0, card1, ...) is the kernel's native GPU ordering.
    // This matches libdrm-based tools (Mission Center, nvtop) and is the most
    // widely used convention on Linux. No explicit sort needed — QDir::Name
    // already yields card0 before card1.

    if (!mDevices.isEmpty()) {
        qDebug() << "Discovered" << mDevices.size() << "GPU(s):";
        for (const GpuDevice &d : mDevices)
            qDebug() << "  card" << d.deviceIndex
                     << "| PCI" << d.pciBusId
                     << "|" << d.vendor << "|" << d.name
                     << "| sysfs:" << (d.sysfsLoadPath.isEmpty() ? "(none)" : d.sysfsLoadPath);
    }
}

void GpuInfoLinux::updateGpuInfo()
{
    for (int i = 0; i < mDevices.size(); ++i) {
        GpuDevice &dev = mDevices[i];

        if (dev.vendor == "AMD") {
            if (!dev.sysfsLoadPath.isEmpty())
                dev.utilization = parseSysfsUtilization(
                    FileUtil::readStringFromFile(dev.sysfsLoadPath));

        } else if (dev.vendor == "NVIDIA") {
            if (!dev.queryCommand.isEmpty()) {
                try {
                    QString output = CommandUtil::exec("nvidia-smi",
                        {"--query-gpu=utilization.gpu",
                         "--format=csv,noheader,nounits",
                         QString("--id=%1").arg(dev.queryCommand)});
                    dev.utilization = parseNvidiaSmiUtilization(output);
                } catch (...) {
                    qWarning() << "Failed to parse GPU utilization";
                    dev.utilization = -1;
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

// Getters (getGpuDevices, hasGpu) are in shared/nexis-core/Info/gpu_info_shared.cpp
