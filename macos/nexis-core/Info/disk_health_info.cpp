#include "disk_health_info.h"
#include "Utils/command_util.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QXmlStreamReader>

// Parse a simple plist XML into a flat key→value map.
// Handles <string>, <integer>, <true/>, <false/>, and nested <dict> (flattened).
// For the nested SMARTDeviceSpecificKeys dict, keys are prefixed with "SMART.".
static QMap<QString, QVariant> parsePlist(const QByteArray &data)
{
    QMap<QString, QVariant> result;
    QXmlStreamReader xml(data);

    // Track nested dict for SMART keys
    QString currentKey;
    bool inSmartDict = false;
    int dictDepth = 0;

    while (!xml.atEnd()) {
        xml.readNext();

        if (xml.isStartElement()) {
            if (xml.name() == QStringLiteral("dict")) {
                dictDepth++;
                if (currentKey == "SMARTDeviceSpecificKeysMayVaryNotGuaranteed") {
                    inSmartDict = true;
                    currentKey.clear();
                }
            }
            else if (xml.name() == QStringLiteral("key")) {
                QString key = xml.readElementText();
                if (inSmartDict)
                    currentKey = "SMART." + key;
                else
                    currentKey = key;
            }
            else if (xml.name() == QStringLiteral("string")) {
                result[currentKey] = xml.readElementText();
            }
            else if (xml.name() == QStringLiteral("integer")) {
                result[currentKey] = xml.readElementText().toLongLong();
            }
            else if (xml.name() == QStringLiteral("true")) {
                result[currentKey] = true;
            }
            else if (xml.name() == QStringLiteral("false")) {
                result[currentKey] = false;
            }
            else if (xml.name() == QStringLiteral("array")) {
                // For WholeDisks array, collect strings
                QStringList items;
                while (!xml.atEnd()) {
                    xml.readNext();
                    if (xml.isStartElement() && xml.name() == QStringLiteral("string"))
                        items.append(xml.readElementText());
                    else if (xml.isEndElement() && xml.name() == QStringLiteral("array"))
                        break;
                }
                result[currentKey] = items;
            }
        }
        else if (xml.isEndElement() && xml.name() == QStringLiteral("dict")) {
            dictDepth--;
            if (inSmartDict && dictDepth <= 1)
                inSmartDict = false;
        }
    }

    return result;
}

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

    // Get list of whole disks
    QStringList wholeDisks;
    try {
        QString listOutput = CommandUtil::exec("diskutil", {"list", "-plist"});
        QMap<QString, QVariant> listPlist = parsePlist(listOutput.toUtf8());
        wholeDisks = listPlist.value("WholeDisks").toStringList();
    } catch (...) {
        return;
    }

    for (const QString &diskId : wholeDisks) {
        DriveHealth drive;
        drive.deviceName = diskId;
        drive.devicePath = "/dev/" + diskId;

        // Get detailed info for this disk
        QString infoOutput;
        try {
            infoOutput = CommandUtil::exec("diskutil", {"info", "-plist", drive.devicePath});
        } catch (...) {
            continue;
        }

        QMap<QString, QVariant> info = parsePlist(infoOutput.toUtf8());

        // Model
        drive.model = info.value("MediaName").toString();
        if (drive.model.isEmpty())
            drive.model = info.value("IORegistryEntryName").toString();

        // Size
        drive.sizeBytes = info.value("TotalSize").toLongLong();
        if (drive.sizeBytes == 0)
            drive.sizeBytes = info.value("IOKitSize").toLongLong();

        // Protocol
        drive.protocol = info.value("BusProtocol").toString();

        // SSD detection
        bool isSolid = info.contains("SolidState") && info.value("SolidState").toBool();

        // Drive type classification
        if (drive.protocol.contains("NVMe", Qt::CaseInsensitive) ||
            drive.protocol.contains("Apple Fabric", Qt::CaseInsensitive)) {
            drive.driveType = DriveHealth::NVMe;
        } else if (isSolid) {
            drive.driveType = DriveHealth::SATA_SSD;
        } else {
            drive.driveType = DriveHealth::SATA_HDD;
        }

        // SMART status
        QString smartStatus = info.value("SMARTStatus").toString();
        if (!smartStatus.isEmpty()) {
            drive.smartPassed = (smartStatus == "Verified");
        }

        // Parse NVMe SMART data from diskutil plist
        if (info.contains("SMART.TEMPERATURE")) {
            int kelvin = info.value("SMART.TEMPERATURE").toInt();
            if (kelvin > 0)
                drive.temperatureCelsius = kelvin - 273;
        }
        if (info.contains("SMART.PERCENTAGE_USED"))
            drive.percentageUsed = info.value("SMART.PERCENTAGE_USED").toInt();
        if (info.contains("SMART.AVAILABLE_SPARE"))
            drive.availableSpare = info.value("SMART.AVAILABLE_SPARE").toInt();
        if (info.contains("SMART.AVAILABLE_SPARE_THRESHOLD"))
            drive.availableSpareThreshold = info.value("SMART.AVAILABLE_SPARE_THRESHOLD").toInt();
        if (info.contains("SMART.POWER_ON_HOURS_0"))
            drive.powerOnHours = info.value("SMART.POWER_ON_HOURS_0").toInt();
        if (info.contains("SMART.POWER_CYCLES_0"))
            drive.powerCycles = info.value("SMART.POWER_CYCLES_0").toInt();
        if (info.contains("SMART.UNSAFE_SHUTDOWNS_0"))
            drive.unsafeShutdowns = info.value("SMART.UNSAFE_SHUTDOWNS_0").toInt();
        if (info.contains("SMART.DATA_UNITS_READ_0"))
            drive.dataUnitsRead = info.value("SMART.DATA_UNITS_READ_0").toLongLong();
        if (info.contains("SMART.DATA_UNITS_WRITTEN_0"))
            drive.dataUnitsWritten = info.value("SMART.DATA_UNITS_WRITTEN_0").toLongLong();
        if (info.contains("SMART.NUM_ERROR_INFO_LOG_ENTRIES_0"))
            drive.mediaErrors = info.value("SMART.NUM_ERROR_INFO_LOG_ENTRIES_0").toInt();

        // For non-Apple-Fabric drives, use smartctl if available for richer SATA data
        bool isAppleInternal = drive.protocol.contains("Apple Fabric", Qt::CaseInsensitive);
        if (mHasSmartctl && !isAppleInternal) {
            try {
                ExecResult result = CommandUtil::execWithStatus("smartctl", {"-j", "-a", drive.devicePath});
                if (result.exitCode == 0 || !(result.exitCode & 2)) {
                    parseSmartctlJson(result.output.toUtf8(), drive);
                } else {
                    drive.needsElevation = true;
                }
            } catch (...) {}
        }

        // Filter out virtual disks (APFS containers, disk images)
        // Keep only physical drives that have a BusProtocol
        if (drive.protocol.isEmpty())
            continue;

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
                QString output = CommandUtil::sudoExec("smartctl", {"-j", "-a", device});
                parseSmartctlJson(output.toUtf8(), mDrives[i]);
                mDrives[i].needsElevation = false;
                deriveHealthVerdict(mDrives[i]);
            } catch (...) {}
            break;
        }
    }
}
