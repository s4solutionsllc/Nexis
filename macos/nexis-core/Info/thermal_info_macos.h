#ifndef THERMAL_INFO_MACOS_H
#define THERMAL_INFO_MACOS_H

#include <Info/thermal_info.h>

class ThermalInfoMacOS : public ThermalInfo
{
public:
    ThermalInfoMacOS();

    double getTemperature(int index) const override;

protected:
    void discoverSensors() override;
};

#endif // THERMAL_INFO_MACOS_H
