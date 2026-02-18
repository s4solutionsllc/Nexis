#include "disk_health_info.h"
#include "Utils/command_util.h"
#include "Utils/file_util.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

// Parse smartctl JSON output into a DriveHealth struct
static void parseSmartctlJson(const QByteArray &json, DriveHealth &drive)
{
    QJsonDocument doc = QJsonDocument::fromJson(json);
    if (doc.isNull())
        return;
    QJsonObject root = doc.object();

    // Device info
    if (drive.model.isEmpty())
        drive.model = root["model_name"].toString();
    if (drive.serial.isEmpty())
        drive.serial = root["serial_number"].toString();
    if (drive.firmware.isEmpty())
        drive.firmware = root["firmware_version"].toString();

    // Overall SMART status
    QJsonObject smartStatus = root["smart_status"].toObject();
    if (smartStatus.contains("passed"))
        drive.smartPassed = smartStatus["passed"].toBool(true);

    // Detect drive type from smartctl
    QString devType = root["device"].toObject()["type"].toString();

    if (devType == "nvme") {
        drive.driveType = DriveHealth::NVMe;
        QJsonObject nvme = root["nvme_smart_health_information_log"].toObject();
        if (!nvme.isEmpty()) {
            if (nvme.contains("critical_warning"))
                drive.criticalWarning = nvme["critical_warning"].toInt(-1);
            if (nvme.contains("temperature"))
                drive.temperatureCelsius = nvme["temperature"].toInt(-1);
            if (nvme.contains("available_spare"))
                drive.availableSpare = nvme["available_spare"].toInt(-1);
            if (nvme.contains("available_spare_threshold"))
                drive.availableSpareThreshold = nvme["available_spare_threshold"].toInt(-1);
            if (nvme.contains("percentage_used"))
                drive.percentageUsed = nvme["percentage_used"].toInt(-1);
            if (nvme.contains("data_units_read"))
                drive.dataUnitsRead = nvme["data_units_read"].toVariant().toLongLong();
            if (nvme.contains("data_units_written"))
                drive.dataUnitsWritten = nvme["data_units_written"].toVariant().toLongLong();
            if (nvme.contains("power_cycles"))
                drive.powerCycles = nvme["power_cycles"].toInt(-1);
            if (nvme.contains("power_on_hours"))
                drive.powerOnHours = nvme["power_on_hours"].toInt(-1);
            if (nvme.contains("unsafe_shutdowns"))
                drive.unsafeShutdowns = nvme["unsafe_shutdowns"].toInt(-1);
            if (nvme.contains("media_errors"))
                drive.mediaErrors = nvme["media_errors"].toInt(-1);
        }
    }
    else {
        // SATA (ATA)
        int rotationRate = root["rotation_rate"].toInt(0);
        if (rotationRate > 0)
            drive.driveType = DriveHealth::SATA_HDD;
        else
            drive.driveType = DriveHealth::SATA_SSD;

        QJsonObject ataAttrs = root["ata_smart_attributes"].toObject();
        QJsonArray table = ataAttrs["table"].toArray();
        for (const QJsonValue &val : table) {
            QJsonObject attr = val.toObject();
            SmartAttribute sa;
            sa.id = attr["id"].toInt(-1);
            sa.name = attr["name"].toString();
            sa.value = attr["value"].toInt(-1);
            sa.worst = attr["worst"].toInt(-1);
            sa.threshold = attr["thresh"].toInt(-1);
            sa.rawValue = attr["raw"].toObject()["value"].toVariant().toLongLong();
            drive.allAttributes.append(sa);

            switch (sa.id) {
            case 5:   drive.reallocatedSectors = static_cast<int>(sa.rawValue); break;
            case 9:   drive.powerOnHours = static_cast<int>(sa.rawValue); break;
            case 12:  drive.powerCycles = static_cast<int>(sa.rawValue); break;
            case 177: drive.wearLevelingCount = sa.value; break;
            case 190:
            case 194: drive.temperatureCelsius = sa.rawValue; break;
            case 196: drive.reallocatedEvents = static_cast<int>(sa.rawValue); break;
            case 197: drive.pendingSectors = static_cast<int>(sa.rawValue); break;
            case 198: drive.uncorrectableSectors = static_cast<int>(sa.rawValue); break;
            default: break;
            }
        }
    }
}

DiskHealthInfo::DiskHealthInfo()
{
    mHasSmartctl = CommandUtil::isExecutable("smartctl");
    discoverDrives();
}

void DiskHealthInfo::discoverDrives()
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
                parseSmartctlJson(result.output.toUtf8(), drive);
            }
            else if (result.exitCode & 2) {
                // Bit 1 set: permission denied
                drive.needsElevation = true;
                // Try to parse partial data anyway
                if (!result.output.isEmpty())
                    parseSmartctlJson(result.output.toUtf8(), drive);
            }
            else if (!result.output.isEmpty()) {
                // Other error but output was produced — try to parse
                parseSmartctlJson(result.output.toUtf8(), drive);
            }
        }

        deriveHealthVerdict(drive);
        mDrives.append(drive);
    }
}

void DiskHealthInfo::refreshHealth()
{
    discoverDrives();
}

void DiskHealthInfo::refreshHealthElevated(const QString &device)
{
    if (!mHasSmartctl)
        return;

    for (int i = 0; i < mDrives.size(); ++i) {
        if (mDrives[i].devicePath == device) {
            try {
                QString output = CommandUtil::exec("pkexec", {"smartctl", "-j", "-a", device});
                parseSmartctlJson(output.toUtf8(), mDrives[i]);
                mDrives[i].needsElevation = false;
                deriveHealthVerdict(mDrives[i]);
            } catch (...) {}
            break;
        }
    }
}
