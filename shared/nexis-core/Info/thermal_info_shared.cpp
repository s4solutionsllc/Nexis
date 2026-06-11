// Shared ThermalInfo static parsing methods — platform-independent.

#include "thermal_info.h"

#include <QDir>
#include <QHash>
#include <QRegularExpression>

namespace {
constexpr double MAX_SANE_TEMP = 200.0;
}

double ThermalInfo::parseSysfsTemperature(const QString &millidegStr)
{
    QString val = millidegStr.trimmed();
    if (val.isEmpty())
        return 0.0;
    return val.toLong() / 1000.0;
}

double ThermalInfo::sanitizeTempThreshold(const QString &millidegStr, double maxSaneTemp)
{
    QString val = millidegStr.trimmed();
    if (val.isEmpty())
        return -1.0;
    double temp = val.toLong() / 1000.0;
    return (temp > 0.0 && temp <= maxSaneTemp) ? temp : -1.0;
}

QString ThermalInfo::friendlyDeviceName(const QString &driverName)
{
    static const QHash<QString, QString> map = {
        {"k10temp",         "CPU"},
        {"zenpower",        "CPU"},
        {"coretemp",        "CPU"},
        {"nvme",            "NVMe"},
        {"amdgpu",          "GPU"},
        {"nouveau",         "GPU"},
        {"radeon",          "GPU"},
        {"iwlwifi",         "WiFi"},
        {"mt7921_phy0",     "WiFi"},
        {"ath10k",          "WiFi"},
        {"ath11k",          "WiFi"},
        {"acpitz",          "ACPI"},
        {"thinkpad",        "ThinkPad"},
        // FW-16 vendor WMI hwmon surfaces (kernel 7.0+):
        //   asus-wmi registers hwmon name "asus" for fan/backlight/kbd;
        //   asus-ec-sensors covers the extra ROG board sensors.
        {"asus",            "ASUS"},
        {"asus_wmi_sensors","ASUS"},
        {"asus-ec-sensors", "ASUS"},
        //   hp-wmi registers hwmon name "hp" — Victus/Omen fan-control surface.
        {"hp",              "HP"},
        {"hp_wmi",          "HP"},
        //   legion-laptop (Lenovo Legion WMI) registers "legion"; ideapad-laptop
        //   exposes extra battery/charge sensors as "ideapad".
        {"legion",          "Legion"},
        {"ideapad",         "IdeaPad"},
    };

    if (map.contains(driverName))
        return map.value(driverName);

    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        if (driverName.startsWith(it.key()))
            return it.value();
    }

    QString friendly = driverName;
    if (!friendly.isEmpty())
        friendly[0] = friendly[0].toUpper();
    return friendly;
}

QList<ThermalSensor> ThermalInfo::enumerateHwmonSensors(const QString &hwmonRoot)
{
    QList<ThermalSensor> sensors;

    QDir hwmonDir(hwmonRoot);
    if (!hwmonDir.exists())
        return sensors;

    const QStringList hwmonEntries = hwmonDir.entryList({"hwmon*"}, QDir::Dirs, QDir::Name);

    for (const QString &entry : hwmonEntries) {
        const QString hwmonPath = QString("%1/%2").arg(hwmonRoot, entry);
        const QString deviceName = FileUtil::readStringFromFile(hwmonPath + "/name").trimmed();

        if (deviceName.isEmpty())
            continue;

        QDir devDir(hwmonPath);
        const QStringList tempInputs = devDir.entryList({"temp*_input"}, QDir::Files, QDir::Name);

        for (const QString &inputFile : tempInputs) {
            static const QRegularExpression re("^temp(\\d+)_input$");
            QRegularExpressionMatch match = re.match(inputFile);
            if (!match.hasMatch())
                continue;

            const QString idx = match.captured(1);
            const QString inputPath = hwmonPath + "/" + inputFile;

            const QString labelPath = QString("%1/temp%2_label").arg(hwmonPath, idx);
            const QString rawLabel = FileUtil::readStringFromFile(labelPath).trimmed();
            const QString friendly = friendlyDeviceName(deviceName);
            QString label;
            if (!rawLabel.isEmpty())
                label = QString("%1 \u2013 %2").arg(friendly, rawLabel);
            else
                label = QString("%1 \u2013 Sensor %2").arg(friendly, idx);

            const double maxTemp = sanitizeTempThreshold(
                FileUtil::readStringFromFile(QString("%1/temp%2_max").arg(hwmonPath, idx)),
                MAX_SANE_TEMP);

            const double critTemp = sanitizeTempThreshold(
                FileUtil::readStringFromFile(QString("%1/temp%2_crit").arg(hwmonPath, idx)),
                MAX_SANE_TEMP);

            ThermalSensor sensor;
            sensor.id = QString("%1/temp%2").arg(deviceName, idx);
            sensor.deviceName = deviceName;
            sensor.label = label;
            sensor.inputPath = inputPath;
            sensor.maxTemp = maxTemp;
            sensor.critTemp = critTemp;

            sensors.append(sensor);
        }
    }

    return sensors;
}
