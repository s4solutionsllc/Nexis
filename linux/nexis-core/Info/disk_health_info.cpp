#include "disk_health_info_linux.h"
#include "Utils/command_util.h"
#include "Utils/file_util.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>

namespace {
// WI-21 (SSO-3383, audit M2): pkexec opens a polkit password prompt that
// the user has to interact with — the default 30 s CommandUtil::exec
// timeout races slow password entry and the unlock silently fails. Five
// minutes is plenty for any human-driven prompt while still bounding a
// wedged polkit agent.
constexpr int kSmartElevatedTimeoutMs = 5 * 60 * 1000;
} // namespace
DiskHealthInfoLinux::DiskHealthInfoLinux()
{
    mHasSmartctl = CommandUtil::isExecutable("smartctl");
    // FR-96: discovery is deferred off-thread and triggered after the main
    // window paints (via DataRefreshService::onSlowTick on first start).
}

QList<DriveHealth> DiskHealthInfoLinux::collectDriveHealth()
{
    // WI-03: builds into a local list; the UI thread publishes via setDrives().
    // Never touch mDrives here — discovery runs on a QtConcurrent worker and
    // mDrives is read/written by the UI thread (getDrives, refreshHealthElevated*).
    QList<DriveHealth> drives;

    QDir blocks("/sys/block");
    if (!blocks.exists())
        return drives;

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
        drives.append(drive);
    }

    return drives;
}

void DiskHealthInfoLinux::refreshHealthElevatedBatch(const QStringList &devices,
                                                      bool applySetcap,
                                                      const QString &smartctlPath)
{
    if (!mHasSmartctl || devices.isEmpty())
        return;

    QString cmd;
    if (applySetcap) {
        // Use the provided path, or discover it inside the root shell
        // (smartctl is often in /usr/sbin which may not be in the user's PATH)
        if (!smartctlPath.isEmpty())
            cmd += QString("setcap cap_sys_rawio,cap_dac_override+ep %1; ").arg(smartctlPath);
        else
            cmd += "SMPATH=$(which smartctl 2>/dev/null) && setcap cap_sys_rawio,cap_dac_override+ep \"$SMPATH\"; ";
    }

    for (int i = 0; i < devices.size(); ++i) {
        if (i > 0) cmd += "; ";
        cmd += QString("smartctl -j -a %1").arg(devices[i]);
    }

    try {
        // pkexec/smartctl can block for many seconds — run it without holding
        // the mDrives mutex, then take the lock only to merge results in.
        // WI-21: bump the timeout well past the default 30 s so a slow polkit
        // password entry does not race the wait cap.
        QString output = CommandUtil::exec("pkexec", {"sh", "-c", cmd},
                                           QByteArray(), kSmartElevatedTimeoutMs);
        QList<QByteArray> blocks = DiskHealthInfo::splitSmartctlOutput(output);
        QMutexLocker locker(&mDrivesMutex);
        for (int i = 0; i < blocks.size() && i < devices.size(); ++i) {
            for (int j = 0; j < mDrives.size(); ++j) {
                if (mDrives[j].devicePath == devices[i]) {
                    DiskHealthInfo::parseSmartctlJsonInto(blocks[i], mDrives[j]);
                    mDrives[j].needsElevation = false;
                    deriveHealthVerdict(mDrives[j]);
                    break;
                }
            }
        }
    } catch (...) {
        qWarning() << "Failed to batch-read SMART data";
    }
}

void DiskHealthInfoLinux::refreshHealthElevated(const QString &device)
{
    if (!mHasSmartctl)
        return;

    // Run pkexec/smartctl outside the lock (may block for seconds), then
    // take the lock only to merge the parsed result back into mDrives.
    // WI-21: bump the timeout well past the default 30 s so a slow polkit
    // password entry does not race the wait cap.
    QString output;
    try {
        output = CommandUtil::exec("pkexec", {"smartctl", "-j", "-a", device},
                                   QByteArray(), kSmartElevatedTimeoutMs);
    } catch (...) {
        qWarning() << "Failed to read SMART data for" << device;
        return;
    }

    QMutexLocker locker(&mDrivesMutex);
    for (int i = 0; i < mDrives.size(); ++i) {
        if (mDrives[i].devicePath == device) {
            DiskHealthInfo::parseSmartctlJsonInto(output.toUtf8(), mDrives[i]);
            mDrives[i].needsElevation = false;
            deriveHealthVerdict(mDrives[i]);
            break;
        }
    }
}
