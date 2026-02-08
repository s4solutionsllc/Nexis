#include "thermal_info.h"
#include "command_util.h"
#include <QRegularExpression>

ThermalInfo::ThermalInfo()
{
    discoverSensors();
}

void ThermalInfo::discoverSensors()
{
    mSensors.clear();

    // On macOS, thermal sensors are accessible via IOKit / SMC.
    // The powermetrics tool requires root, and direct SMC access requires
    // a third-party library. For now, we provide a basic sensor list using
    // osx-cpu-temp or similar if available, otherwise we report no sensors.
    // This is a known limitation -- full SMC support would require IOKit framework.

    // Try using the 'sensors' approach via a simple command
    // If osx-cpu-temp is installed (brew install osx-cpu-temp), use it
    if (CommandUtil::isExecutable("osx-cpu-temp")) {
        ThermalSensor sensor;
        sensor.id = "cpu/temp1";
        sensor.deviceName = "CPU";
        sensor.label = "CPU Temperature";
        sensor.inputPath = "osx-cpu-temp"; // We'll use this as a marker
        sensor.maxTemp = 100.0;
        sensor.critTemp = 105.0;
        mSensors.append(sensor);
    }
}

QList<ThermalSensor> ThermalInfo::getSensors() const
{
    return mSensors;
}

double ThermalInfo::getTemperature(int index) const
{
    if (index < 0 || index >= mSensors.size())
        return 0.0;

    // Use osx-cpu-temp command to read temperature
    if (mSensors.at(index).inputPath == "osx-cpu-temp") {
        try {
            QString output = CommandUtil::exec("osx-cpu-temp");
            // Output format: "65.0degC"
            QString tempStr = output.trimmed();
            tempStr.remove(QRegularExpression("[^0-9.]"));
            return tempStr.toDouble();
        } catch (...) {
            return 0.0;
        }
    }

    return 0.0;
}

bool ThermalInfo::hasSensors() const
{
    return !mSensors.isEmpty();
}
