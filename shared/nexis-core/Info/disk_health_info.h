#ifndef DISK_HEALTH_INFO_H
#define DISK_HEALTH_INFO_H

#include <QList>
#include <QString>
#include <QStringList>
#include "nexis-core_global.h"

struct SmartAttribute {
    int     id        = -1;
    QString name;
    int     value     = -1;      // normalized (0-253, higher is better)
    int     worst     = -1;
    int     threshold = -1;
    qint64  rawValue  = -1;
    QString status;              // "ok", "warning", "failing"
};

struct DriveHealth {
    // Identity
    QString devicePath;          // "/dev/disk0" or "/dev/nvme0n1"
    QString deviceName;          // "disk0" or "nvme0n1"
    QString model;
    QString serial;
    QString firmware;
    quint64 sizeBytes = 0;

    // Drive type
    enum DriveType { Unknown, NVMe, SATA_SSD, SATA_HDD };
    DriveType driveType = Unknown;
    QString protocol;            // "NVMe", "SATA", "Apple Fabric", "USB"

    // Health summary
    int     healthPercent  = -1; // 0-100, derived from SMART data (-1 = unavailable)
    QString healthVerdict;       // "Good", "Caution", "Critical", "Unknown"
    bool    smartPassed    = true;
    bool    needsElevation = false;

    // Common metrics
    double  temperatureCelsius = -1.0;
    int     powerOnHours       = -1;
    int     powerCycles        = -1;

    // NVMe-specific
    int     percentageUsed          = -1;   // 0-100+ (endurance consumed)
    int     availableSpare          = -1;   // 0-100
    int     availableSpareThreshold = -1;
    int     criticalWarning         = -1;   // bitmask
    int     unsafeShutdowns         = -1;
    int     mediaErrors             = -1;
    qint64  dataUnitsRead           = -1;   // units of 512KB * 1000
    qint64  dataUnitsWritten        = -1;

    // SATA-specific (key attributes)
    int     reallocatedSectors   = -1;      // ID 5
    int     pendingSectors       = -1;      // ID 197
    int     uncorrectableSectors = -1;      // ID 198
    int     reallocatedEvents    = -1;      // ID 196
    int     wearLevelingCount    = -1;      // ID 177 (normalized)

    // Full attribute table (for future detailed view)
    QList<SmartAttribute> allAttributes;
};

class NEXISCORESHARED_EXPORT DiskHealthInfo
{
public:
    virtual ~DiskHealthInfo() = default;

    QList<DriveHealth> getDrives() const;
    bool hasDrives() const;
    bool hasSmartctl() const;
    virtual void refreshHealth() = 0;
    virtual void refreshHealthElevated(const QString &device) = 0;
    virtual void refreshHealthElevatedBatch(const QStringList &devices,
                                             bool applySetcap,
                                             const QString &smartctlPath) = 0;

    // FR-96: discovery is now public so it can be driven off-thread from
    // DataRefreshService after the main window has painted (rather than
    // synchronously from the constructor during app launch).
    virtual void discoverDrives() = 0;

    // Public for testability (FR-36). Operates purely on the DriveHealth struct.
    static void parseSmartctlJsonInto(const QByteArray &json, DriveHealth &drive);
    static void deriveHealthVerdict(DriveHealth &drive);
    static QList<QByteArray> splitSmartctlOutput(const QString &output);

protected:
    QList<DriveHealth> mDrives;
    bool mHasSmartctl = false;
};

#endif // DISK_HEALTH_INFO_H
