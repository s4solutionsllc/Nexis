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

    // Static parsing methods for testability (FR-76).
    static double parseSysfsTemperature(const QString &millidegStr);
    static double sanitizeTempThreshold(const QString &millidegStr, double maxSaneTemp = 200.0);

    // Map a hwmon driver name (e.g. "k10temp", "asus", "hp") to the user-facing
    // device label. Exposed for testing FW-16 vendor WMI surfaces (asus-wmi,
    // hp-wmi, legion-laptop) added in kernel 7.0.
    static QString friendlyDeviceName(const QString &driverName);

    // Walk a hwmon root (sysfs-shaped: <root>/hwmon*/name + temp*_input + temp*_label)
    // and return the discovered sensors. Linux passes /sys/class/hwmon; tests
    // pass a QTemporaryDir.
    static QList<ThermalSensor> enumerateHwmonSensors(const QString &hwmonRoot);

protected:
    virtual void discoverSensors() = 0;
    QList<ThermalSensor> mSensors;
};

#endif // THERMAL_INFO_H
