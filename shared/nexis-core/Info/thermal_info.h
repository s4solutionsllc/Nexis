#ifndef THERMAL_INFO_H
#define THERMAL_INFO_H

#include <QList>
#include <QString>
#include "Utils/file_util.h"
#include "nexis-core_global.h"

struct ThermalSensor {
    QString id;          // unique key, e.g. "k10temp/temp1"
    QString deviceName;  // hwmon name, e.g. "k10temp"
    QString label;       // temp*_label or synthesized "DeviceName temp N"
    QString inputPath;   // full sysfs path to temp*_input
    double maxTemp;      // from temp*_max, or -1.0 if unavailable/bogus
    double critTemp;     // from temp*_crit, or -1.0 if unavailable/bogus
};

class NEXISCORESHARED_EXPORT ThermalInfo
{
public:
    virtual ~ThermalInfo() = default;

    QList<ThermalSensor> getSensors() const { return mSensors; }
    virtual double getTemperature(int index) const = 0;
    bool hasSensors() const { return !mSensors.isEmpty(); }

protected:
    virtual void discoverSensors() = 0;
    QList<ThermalSensor> mSensors;
};

#endif // THERMAL_INFO_H
