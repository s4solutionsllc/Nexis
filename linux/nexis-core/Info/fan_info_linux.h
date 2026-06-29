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

private:
    void discoverHwmon();
    void discoverThinkpadProc();
    void discoverDellProc();
    void discoverNvidiaSmi();
    bool hasNvidiaSmiGpuFan() const;

    int readHwmonSpeed(const FanSensor &sensor) const;
    int readHwmonPwmSpeed(const FanSensor &sensor) const;
    int readThinkpadSpeed() const;
    int readDellSpeed(const FanSensor &sensor) const;
    int readNvidiaSpeed(const FanSensor &sensor) const;
};

#endif // FAN_INFO_LINUX_H
