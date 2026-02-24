#ifndef FAN_INFO_LINUX_H
#define FAN_INFO_LINUX_H

#include <Info/fan_info.h>

class FanInfoLinux : public FanInfo
{
public:
    FanInfoLinux();
    int getFanSpeed(int index) const override;

protected:
    void discoverSensors() override;
};

#endif // FAN_INFO_LINUX_H
