#ifndef THERMAL_INFO_LINUX_H
#define THERMAL_INFO_LINUX_H

#include <Info/thermal_info.h>

class ThermalInfoLinux : public ThermalInfo
{
public:
    ThermalInfoLinux();

    double getTemperature(int index) const override;

protected:
    void discoverSensors() override;
};

#endif // THERMAL_INFO_LINUX_H
