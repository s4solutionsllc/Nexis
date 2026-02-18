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
