#ifndef BATTERY_INFO_LINUX_H
#define BATTERY_INFO_LINUX_H

#include <Info/battery_info.h>
#include <QStringList>

class BatteryInfoLinux : public BatteryInfo
{
public:
    BatteryInfoLinux();

    void updateBatteryInfo() override;
    int batteryCount() const override;
    BatteryData getBatteryData(int index) const override;

protected:
    void discoverBattery() override;

private:
    QStringList mBatteryPaths;
    QList<BatteryData> mBatteries;

    BatteryData readBatteryData(const QString &path, const QString &name) const;
    void aggregate();
};

#endif // BATTERY_INFO_LINUX_H
