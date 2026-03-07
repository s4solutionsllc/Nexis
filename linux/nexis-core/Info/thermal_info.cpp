#include "thermal_info_linux.h"
#include <QDir>
#include <QHash>
#include <QRegularExpression>

static constexpr const char *HWMON_BASE = "/sys/class/hwmon";
static constexpr double MAX_SANE_TEMP = 200.0;

static QString friendlyDeviceName(const QString &driverName)
{
    static const QHash<QString, QString> map = {
        {"k10temp",    "CPU"},
        {"zenpower",   "CPU"},
        {"coretemp",   "CPU"},
        {"nvme",       "NVMe"},
        {"amdgpu",     "GPU"},
        {"nouveau",    "GPU"},
        {"radeon",     "GPU"},
        {"iwlwifi",    "WiFi"},
        {"mt7921_phy0","WiFi"},
        {"ath10k",     "WiFi"},
        {"ath11k",     "WiFi"},
        {"acpitz",     "ACPI"},
        {"thinkpad",   "ThinkPad"},
    };

    // Exact match
    if (map.contains(driverName))
        return map.value(driverName);

    // Prefix match for drivers like mt7921_phy1, iwlwifi_1, etc.
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        if (driverName.startsWith(it.key()))
            return it.value();
    }

    // Capitalize first letter as fallback
    QString friendly = driverName;
    friendly[0] = friendly[0].toUpper();
    return friendly;
}

ThermalInfoLinux::ThermalInfoLinux()
{
    discoverSensors();
}

void ThermalInfoLinux::discoverSensors()
{
    mSensors.clear();

    QDir hwmonDir(HWMON_BASE);
    if (!hwmonDir.exists())
        return;

    QStringList hwmonEntries = hwmonDir.entryList({"hwmon*"}, QDir::Dirs, QDir::Name);

    for (const QString &entry : hwmonEntries) {
        QString hwmonPath = QString("%1/%2").arg(HWMON_BASE, entry);
        QString deviceName = FileUtil::readStringFromFile(hwmonPath + "/name").trimmed();

        if (deviceName.isEmpty())
            continue;

        // Find all temp*_input files in this hwmon directory
        QDir devDir(hwmonPath);
        QStringList tempInputs = devDir.entryList({"temp*_input"}, QDir::Files, QDir::Name);

        for (const QString &inputFile : tempInputs) {
            // Extract the index from "tempN_input"
            static QRegularExpression re("^temp(\\d+)_input$");
            QRegularExpressionMatch match = re.match(inputFile);
            if (!match.hasMatch())
                continue;

            QString idx = match.captured(1);
            QString inputPath = hwmonPath + "/" + inputFile;

            // Read optional label and build friendly display name
            QString labelPath = QString("%1/temp%2_label").arg(hwmonPath, idx);
            QString rawLabel = FileUtil::readStringFromFile(labelPath).trimmed();
            QString friendly = friendlyDeviceName(deviceName);
            QString label;
            if (!rawLabel.isEmpty())
                label = QString("%1 \u2013 %2").arg(friendly, rawLabel);
            else
                label = QString("%1 \u2013 Sensor %2").arg(friendly, idx);

            double maxTemp = sanitizeTempThreshold(
                FileUtil::readStringFromFile(QString("%1/temp%2_max").arg(hwmonPath, idx)),
                MAX_SANE_TEMP);

            double critTemp = sanitizeTempThreshold(
                FileUtil::readStringFromFile(QString("%1/temp%2_crit").arg(hwmonPath, idx)),
                MAX_SANE_TEMP);

            ThermalSensor sensor;
            sensor.id = QString("%1/temp%2").arg(deviceName, idx);
            sensor.deviceName = deviceName;
            sensor.label = label;
            sensor.inputPath = inputPath;
            sensor.maxTemp = maxTemp;
            sensor.critTemp = critTemp;

            mSensors.append(sensor);
        }
    }
}

double ThermalInfoLinux::getTemperature(int index) const
{
    if (index < 0 || index >= mSensors.size())
        return 0.0;

    return parseSysfsTemperature(
        FileUtil::readStringFromFile(mSensors.at(index).inputPath));
}

