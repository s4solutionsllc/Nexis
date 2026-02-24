#ifndef FAN_INFO_MACOS_H
#define FAN_INFO_MACOS_H

#include <Info/fan_info.h>

class FanInfoMacOS : public FanInfo
{
public:
    FanInfoMacOS();
    int getFanSpeed(int index) const override;

protected:
    void discoverSensors() override;
};

#endif // FAN_INFO_MACOS_H
