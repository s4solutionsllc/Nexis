#include "disk_health_info_macos.h"
#include "Utils/command_util.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutexLocker>
#include <QSet>
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

DiskHealthInfoMacOS::DiskHealthInfoMacOS()
{
    mHasSmartctl = CommandUtil::isExecutable("smartctl");
    // FR-96: discovery is deferred off-thread and triggered after the main
    // window paints (via DataRefreshService::onSlowTick on first start).
}

QList<DriveHealth> DiskHealthInfoMacOS::collectDriveHealth()
{
    // WI-03: builds into a local list; the UI thread publishes via setDrives().
    // Never touch mDrives here — discovery runs on a QtConcurrent worker and
    // mDrives is read/written by the UI thread (getDrives, refreshHealthElevated*).
    QList<DriveHealth> drives;

    // Get list of whole disks
    QStringList wholeDisks;
    try {
        QString listOutput = CommandUtil::exec("diskutil", {"list", "-plist"});
        QMap<QString, QVariant> listPlist = parsePlist(listOutput.toUtf8());
        wholeDisks = listPlist.value("WholeDisks").toStringList();
    } catch (...) {
        return drives;
    }

    // FR-109: dedupe by (model, size) before the smartctl fork. On Apple
    // Silicon, WholeDisks commonly contains disk0–disk3 as synthetic
    // virtualizations of the same physical drive — running smartctl (and
    // a plist parse) on each one is 3x wasted per tick.
    QSet<QString> seenKeys;

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

        // Protocol — check early so we can skip the smartctl fork on disk images.
        drive.protocol = info.value("BusProtocol").toString();
        if (drive.protocol.isEmpty() || drive.protocol == "Disk Image")
            continue;

        // Model
        drive.model = info.value("MediaName").toString();
        if (drive.model.isEmpty())
            drive.model = info.value("IORegistryEntryName").toString();

        // Size
        drive.sizeBytes = info.value("TotalSize").toLongLong();
        if (drive.sizeBytes == 0)
            drive.sizeBytes = info.value("IOKitSize").toLongLong();

        // Dedupe now — before the smartctl fork.
        if (!drive.model.isEmpty()) {
            const QString key = drive.model + "|" + QString::number(drive.sizeBytes);
            if (seenKeys.contains(key))
                continue;
            seenKeys.insert(key);
        }

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
                    DiskHealthInfo::parseSmartctlJsonInto(result.output.toUtf8(), drive);
                } else {
                    drive.needsElevation = true;
                }
            } catch (...) { qWarning() << "Failed to read disk info for" << drive.devicePath; }
        }

        deriveHealthVerdict(drive);
        drives.append(drive);
    }

    return drives;
}

void DiskHealthInfoMacOS::refreshHealthElevatedBatch(const QStringList &devices,
                                                      bool applySetcap,
                                                      const QString &smartctlPath)
{
    Q_UNUSED(applySetcap)
    Q_UNUSED(smartctlPath)
    for (const QString &device : devices)
        refreshHealthElevated(device);
}

void DiskHealthInfoMacOS::refreshHealthElevated(const QString &device)
{
    if (!mHasSmartctl)
        return;

    // Run sudo/smartctl outside the lock (may block for seconds), then take
    // the lock only to merge the parsed result back into mDrives.
    QString output;
    try {
        output = CommandUtil::sudoExec("smartctl", {"-j", "-a", device});
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
