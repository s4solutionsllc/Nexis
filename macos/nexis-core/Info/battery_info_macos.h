#ifndef BATTERY_INFO_MACOS_H
#define BATTERY_INFO_MACOS_H

#include <Info/battery_info.h>

class BatteryInfoMacOS : public BatteryInfo
{
public:
    BatteryInfoMacOS();

    void updateBatteryInfo() override;

protected:
    void discoverBattery() override;
};

#endif // BATTERY_INFO_MACOS_H
