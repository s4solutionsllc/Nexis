// Shared BatteryInfo methods — identical across platforms.
// Platform-specific constructor, discoverBattery(), and updateBatteryInfo() live in
// linux/nexis-core/Info/battery_info.cpp and macos/nexis-core/Info/battery_info.cpp.

#include "battery_info.h"

BatteryData BatteryInfo::getBatteryData() const
{
    return mData;
}

bool BatteryInfo::hasBattery() const
{
    return mData.hasBattery;
}

QString BatteryInfo::deriveCondition(int healthPercent)
{
    if (healthPercent >= 80) return QStringLiteral("Good");
    if (healthPercent >= 60) return QStringLiteral("Fair");
    return QStringLiteral("Replace");
}

int BatteryInfo::deriveHealthPercent(double maxCapacityMah, double designCapacityMah)
{
    if (maxCapacityMah <= 0 || designCapacityMah <= 0)
        return -1;
    return qBound(0, static_cast<int>((maxCapacityMah / designCapacityMah) * 100.0), 100);
}
