#include "disk_health_info_linux.h"
#include "Utils/command_util.h"
#include "Utils/file_util.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
DiskHealthInfoLinux::DiskHealthInfoLinux()
{
    mHasSmartctl = CommandUtil::isExecutable("smartctl");
    discoverDrives();
}

void DiskHealthInfoLinux::discoverDrives()
{
    mDrives.clear();

    QDir blocks("/sys/block");
    if (!blocks.exists())
        return;

    QStringList entries = blocks.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);

    for (const QString &name : entries) {
        QString basePath = QString("/sys/block/%1").arg(name);

        // Only include physical devices (those with a device/ subdirectory)
        if (!QFile::exists(basePath + "/device"))
            continue;

        // Skip loop, ram, and dm- devices
        if (name.startsWith("loop") || name.startsWith("ram") || name.startsWith("dm-"))
            continue;

        DriveHealth drive;
        drive.deviceName = name;
        drive.devicePath = "/dev/" + name;

        // Model from sysfs
        drive.model = FileUtil::readStringFromFile(basePath + "/device/model").trimmed();

        // Size: /sys/block/{name}/size reports 512-byte sectors
        QString sizeStr = FileUtil::readStringFromFile(basePath + "/size").trimmed();
        bool ok = false;
        qint64 sectors = sizeStr.toLongLong(&ok);
        if (ok && sectors > 0)
            drive.sizeBytes = static_cast<quint64>(sectors) * 512;

        // Detect NVMe from name
        bool isNvme = name.startsWith("nvme");

        // Detect SSD vs HDD from rotational flag
        QString rotational = FileUtil::readStringFromFile(basePath + "/queue/rotational").trimmed();
        bool isSsd = (rotational == "0");

        if (isNvme) {
            drive.driveType = DriveHealth::NVMe;
            drive.protocol = QStringLiteral("NVMe");
        } else if (isSsd) {
            drive.driveType = DriveHealth::SATA_SSD;
            drive.protocol = QStringLiteral("SATA");
        } else {
            drive.driveType = DriveHealth::SATA_HDD;
            drive.protocol = QStringLiteral("SATA");
        }

        // Try smartctl for SMART data
        if (mHasSmartctl) {
            ExecResult result = CommandUtil::execWithStatus("smartctl", {"-j", "-a", drive.devicePath});
            if (result.exitCode == 0) {
                DiskHealthInfo::parseSmartctlJsonInto(result.output.toUtf8(), drive);
            }
            else if (result.exitCode & 2) {
                // Bit 1 set: permission denied
                drive.needsElevation = true;
                // Try to parse partial data anyway
                if (!result.output.isEmpty())
                    DiskHealthInfo::parseSmartctlJsonInto(result.output.toUtf8(), drive);
            }
            else if (!result.output.isEmpty()) {
                // Other error but output was produced — try to parse
                DiskHealthInfo::parseSmartctlJsonInto(result.output.toUtf8(), drive);
            }
        }

        deriveHealthVerdict(drive);
        mDrives.append(drive);
    }
}

void DiskHealthInfoLinux::refreshHealth()
{
    discoverDrives();
}

void DiskHealthInfoLinux::refreshHealthElevated(const QString &device)
{
    if (!mHasSmartctl)
        return;

    for (int i = 0; i < mDrives.size(); ++i) {
        if (mDrives[i].devicePath == device) {
            try {
                QString output = CommandUtil::exec("pkexec", {"smartctl", "-j", "-a", device});
                DiskHealthInfo::parseSmartctlJsonInto(output.toUtf8(), mDrives[i]);
                mDrives[i].needsElevation = false;
                deriveHealthVerdict(mDrives[i]);
            } catch (...) { qWarning() << "Failed to read SMART data for" << device; }
            break;
        }
    }
}
