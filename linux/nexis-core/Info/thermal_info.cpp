#include "thermal_info_linux.h"

static constexpr const char *HWMON_BASE = "/sys/class/hwmon";

ThermalInfoLinux::ThermalInfoLinux()
{
    discoverSensors();
}

void ThermalInfoLinux::discoverSensors()
{
    // FW-16: shared enumerator also surfaces vendor WMI hwmon names
    // (asus, hp, legion, ideapad) added in kernel 7.0.
    mSensors = ThermalInfo::enumerateHwmonSensors(HWMON_BASE);
}

double ThermalInfoLinux::getTemperature(int index) const
{
    if (index < 0 || index >= mSensors.size())
        return 0.0;

    return parseSysfsTemperature(
        FileUtil::readStringFromFile(mSensors.at(index).inputPath));
}
