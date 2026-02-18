#include "disk_health_info.h"

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
