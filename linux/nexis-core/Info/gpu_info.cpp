#include "gpu_info_linux.h"
#include "Utils/command_util.h"

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
                // Output: "03:00.0 VGA compatible controller: NVIDIA Corporation GeForce ..."
                int colonPos = lspciOut.indexOf(": ", lspciOut.indexOf(busId));
                if (colonPos >= 0) {
                    QString name = lspciOut.mid(colonPos + 2).trimmed();
                    // Remove trailing newlines
                    name = name.split('\n').first().trimmed();
                    if (!name.isEmpty())
                        return name;
                }
            } catch (...) {}
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

        GpuDevice dev;
        dev.utilization = -1;
        dev.deviceIndex = cardIndex;

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
            if (CommandUtil::isExecutable("nvidia-smi")) {
                QFileInfo devLink(cardPath + "/device");
                if (devLink.isSymLink()) {
                    // e.g. "0000:07:00.0" from the symlink target
                    dev.queryCommand = devLink.symLinkTarget().section('/', -1);
                }
            }

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
}

void GpuInfoLinux::updateGpuInfo()
{
    for (int i = 0; i < mDevices.size(); ++i) {
        GpuDevice &dev = mDevices[i];

        if (dev.vendor == "AMD") {
            // AMD: read gpu_busy_percent directly from sysfs
            if (!dev.sysfsLoadPath.isEmpty()) {
                QString val = FileUtil::readStringFromFile(dev.sysfsLoadPath).trimmed();
                if (!val.isEmpty()) {
                    bool ok = false;
                    int pct = val.toInt(&ok);
                    dev.utilization = ok ? qBound(0, pct, 100) : -1;
                } else {
                    dev.utilization = -1;
                }
            }

        } else if (dev.vendor == "NVIDIA") {
            // NVIDIA: query nvidia-smi using PCI bus ID (stored in queryCommand)
            if (!dev.queryCommand.isEmpty()) {
                try {
                    QString output = CommandUtil::exec("nvidia-smi",
                        {"--query-gpu=utilization.gpu",
                         "--format=csv,noheader,nounits",
                         QString("--id=%1").arg(dev.queryCommand)});
                    QString val = output.trimmed().split('\n').first().trimmed();
                    bool ok = false;
                    int pct = val.toInt(&ok);
                    dev.utilization = ok ? qBound(0, pct, 100) : -1;
                } catch (...) {
                    dev.utilization = -1;
                }
            }

        } else if (dev.vendor == "Intel") {
            // Intel: approximate utilization from frequency ratio
            // cur_freq / max_freq * 100 (rough approximation)
            if (!dev.sysfsLoadPath.isEmpty() && !dev.queryCommand.isEmpty()) {
                QString curStr = FileUtil::readStringFromFile(dev.sysfsLoadPath).trimmed();
                QString maxStr = FileUtil::readStringFromFile(dev.queryCommand).trimmed();
                bool okCur = false, okMax = false;
                double cur = curStr.toDouble(&okCur);
                double max = maxStr.toDouble(&okMax);
                if (okCur && okMax && max > 0.0) {
                    dev.utilization = qBound(0, static_cast<int>(cur / max * 100.0), 100);
                } else {
                    dev.utilization = -1;
                }
            }
        }
    }
}

// Getters (getGpuDevices, hasGpu) are in shared/nexis-core/Info/gpu_info_shared.cpp
