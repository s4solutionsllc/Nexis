#ifndef BATTERY_INFO_H
#define BATTERY_INFO_H

#include <QDate>
#include <QList>
#include <QString>
#include "Utils/file_util.h"
#include "nexis-core_global.h"

struct BatteryData {
    bool    hasBattery           = false;
    int     chargePercent        = -1;      // 0-100
    int     healthPercent        = -1;      // 0-100 (maxCapacity / designCapacity * 100)
    int     cycleCount           = -1;      // -1 if unavailable
    int     designCycleCount     = -1;      // rated max cycles (usually 1000)
    double  currentCapacityMah   = -1.0;
    double  maxCapacityMah       = -1.0;
    double  designCapacityMah    = -1.0;
    double  temperatureCelsius   = -1.0;
    int     voltageMv            = -1;
    double  amperageMa           = 0.0;     // negative = discharging
    double  powerWatts           = -1.0;
    bool    isCharging           = false;
    bool    isPluggedIn          = false;
    int     timeRemainingMinutes = -1;      // -1 if calculating/unavailable
    QString status;                         // "Charging", "Discharging", "Full", "Not charging"
    QString condition;                      // "Good", "Fair", "Replace"
    QString batteryName;                    // "BAT0", "BAT1", etc. Empty on macOS.
    QString manufacturer;
    QString model;
    QString technology;                     // "Li-ion", "Li-poly", etc.
    QDate   manufactureDate;                // invalid if unavailable
    int     chargeStartThreshold = -1;      // Linux TLP only (-1 = N/A)
    int     chargeStopThreshold  = -1;      // Linux TLP only (-1 = N/A)
};

class NEXISCORESHARED_EXPORT BatteryInfo
{
public:
    virtual ~BatteryInfo() = default;

    BatteryData getBatteryData() const;
    virtual BatteryData getBatteryData(int index) const;
    virtual int batteryCount() const;
    bool hasBattery() const;
    virtual void updateBatteryInfo() = 0;

    // Static parsing methods for testability (FR-76).
    static QString deriveCondition(int healthPercent);
    static int deriveHealthPercent(double maxCapacityMah, double designCapacityMah);

protected:
    virtual void discoverBattery() = 0;
    BatteryData mData;
};

#endif // BATTERY_INFO_H
