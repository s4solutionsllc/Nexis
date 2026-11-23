#ifndef THERMAL_INFO_H
#define THERMAL_INFO_H

#include <QByteArray>
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

// Aggregate over the valid readings of a sensor set. Averages hide a single hot
// sensor, so the max (and which sensor produced it) always travels with it.
struct ThermalSummary {
    int count = 0;        // readings that passed validation
    double average = 0.0;
    double max = 0.0;
    int maxIndex = -1;    // index into the input list, -1 when count == 0
    bool isValid() const { return count > 0; }
};

class NEXISCORESHARED_EXPORT ThermalInfo
{
public:
    virtual ~ThermalInfo() = default;

    QList<ThermalSensor> getSensors() const { return mSensors; }
    virtual double getTemperature(int index) const = 0;
    bool hasSensors() const { return !mSensors.isEmpty(); }

    // Dashboard tile input key meaning "all sensors: average + max" instead of one sensor.
    static QString summaryInputKey() { return QStringLiteral("__summary__"); }

    // Ignores non-finite, non-positive and above-sane (>200 C) readings, which
    // sensors report when absent or faulted and would drag the average.
    static ThermalSummary summarize(const QList<double> &readings);

    // Static parsing methods for testability (FR-76).
    static double parseSysfsTemperature(const QString &millidegStr);
    static double sanitizeTempThreshold(const QString &millidegStr, double maxSaneTemp = 200.0);

    // Map a hwmon driver name (e.g. "k10temp", "asus", "hp") to the user-facing
    // device label. Exposed for testing FW-16 vendor WMI surfaces (asus-wmi,
    // hp-wmi, legion-laptop) added in kernel 7.0.
    static QString friendlyDeviceName(const QString &driverName);

    // Decode an AppleSMC temperature value: "sp78" (signed 7.8 fixed point, big
    // endian, 2 bytes) or "flt " (IEEE float, little endian, 4 bytes). Returns -1.0
    // for any other type or a short payload.
    static double decodeSmcTemperature(const QByteArray &dataType, const QByteArray &bytes);

    // True for a reading that can be a live temperature in C. SMC keys report 0,
    // negative sentinels or garbage when the sensor is absent or powered down.
    static bool isPlausibleSmcTemperature(double celsius);

    // User-facing label for an SMC temperature key (e.g. "Tp01" -> "CPU Performance (Tp01)").
    static QString smcSensorLabel(const QString &key);

    // Walk a hwmon root (sysfs-shaped: <root>/hwmon*/name + temp*_input + temp*_label)
    // and return the discovered sensors. Linux passes /sys/class/hwmon; tests
    // pass a QTemporaryDir.
    static QList<ThermalSensor> enumerateHwmonSensors(const QString &hwmonRoot);

protected:
    virtual void discoverSensors() = 0;
    QList<ThermalSensor> mSensors;
};

#endif // THERMAL_INFO_H
