// Shared ThermalInfo static parsing methods — platform-independent.

#include "thermal_info.h"

double ThermalInfo::parseSysfsTemperature(const QString &millidegStr)
{
    QString val = millidegStr.trimmed();
    if (val.isEmpty())
        return 0.0;
    return val.toLong() / 1000.0;
}

double ThermalInfo::sanitizeTempThreshold(const QString &millidegStr, double maxSaneTemp)
{
    QString val = millidegStr.trimmed();
    if (val.isEmpty())
        return -1.0;
    double temp = val.toLong() / 1000.0;
    return (temp > 0.0 && temp <= maxSaneTemp) ? temp : -1.0;
}
