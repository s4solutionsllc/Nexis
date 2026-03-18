#include "disk_health_info.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

void DiskHealthInfo::parseSmartctlJsonInto(const QByteArray &json, DriveHealth &drive)
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

QList<DriveHealth> DiskHealthInfo::getDrives() const
{
    return mDrives;
}

bool DiskHealthInfo::hasDrives() const
{
    return !mDrives.isEmpty();
}

bool DiskHealthInfo::hasSmartctl() const
{
    return mHasSmartctl;
}

void DiskHealthInfo::deriveHealthVerdict(DriveHealth &drive)
{
    // No SMART data was read — don't fabricate a verdict
    if (drive.needsElevation &&
        drive.powerOnHours < 0 &&
        drive.temperatureCelsius < 0 &&
        drive.allAttributes.isEmpty()) {
        drive.healthVerdict = QStringLiteral("Unknown");
        return;
    }

    switch (drive.driveType) {
    case DriveHealth::NVMe: {
        // Critical conditions
        if ((drive.criticalWarning > 0) ||
            (drive.mediaErrors > 0) ||
            (drive.percentageUsed >= 100) ||
            (!drive.smartPassed)) {
            drive.healthVerdict = QStringLiteral("Critical");
        }
        // Caution conditions (use 10% spare threshold, NOT Apple's 99%)
        else if ((drive.availableSpare >= 0 && drive.availableSpare <= 10) ||
                 (drive.percentageUsed >= 80)) {
            drive.healthVerdict = QStringLiteral("Caution");
        }
        else {
            drive.healthVerdict = QStringLiteral("Good");
        }
        // Health percent: 100 - percentageUsed
        if (drive.percentageUsed >= 0)
            drive.healthPercent = qBound(0, 100 - drive.percentageUsed, 100);
        else if (drive.availableSpare >= 0)
            drive.healthPercent = drive.availableSpare;
        break;
    }

    case DriveHealth::SATA_HDD: {
        int critical = qMax(0, drive.reallocatedSectors) +
                       qMax(0, drive.pendingSectors) +
                       qMax(0, drive.uncorrectableSectors);
        if (critical > 100 || !drive.smartPassed) {
            drive.healthVerdict = QStringLiteral("Critical");
        }
        else if (critical > 0) {
            drive.healthVerdict = QStringLiteral("Caution");
        }
        else {
            drive.healthVerdict = QStringLiteral("Good");
        }
        // No universal health % metric for HDDs
        drive.healthPercent = -1;
        break;
    }

    case DriveHealth::SATA_SSD: {
        if ((drive.wearLevelingCount >= 0 && drive.wearLevelingCount < 10) ||
            !drive.smartPassed) {
            drive.healthVerdict = QStringLiteral("Critical");
        }
        else if ((drive.wearLevelingCount >= 0 && drive.wearLevelingCount < 30) ||
                 (drive.reallocatedSectors > 0)) {
            drive.healthVerdict = QStringLiteral("Caution");
        }
        else {
            drive.healthVerdict = QStringLiteral("Good");
        }
        if (drive.wearLevelingCount >= 0)
            drive.healthPercent = qBound(0, drive.wearLevelingCount, 100);
        break;
    }

    default: {
        // Unknown type or diskutil-only (Apple internal)
        if (!drive.smartPassed)
            drive.healthVerdict = QStringLiteral("Critical");
        else
            drive.healthVerdict = QStringLiteral("Good");
        // healthPercent stays -1
        break;
    }
    }
}
