#ifndef BATTERY_INFO_LINUX_H
#define BATTERY_INFO_LINUX_H

#include <Info/battery_info.h>

class BatteryInfoLinux : public BatteryInfo
{
public:
    BatteryInfoLinux();

    void updateBatteryInfo() override;

protected:
    void discoverBattery() override;

private:
    QString mBatteryPath;
};

#endif // BATTERY_INFO_LINUX_H
