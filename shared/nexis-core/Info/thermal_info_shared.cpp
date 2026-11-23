// Shared ThermalInfo static parsing methods — platform-independent.

#include "thermal_info.h"

#include <QDir>
#include <QHash>
#include <QRegularExpression>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace {
constexpr double MAX_SANE_TEMP = 200.0;
}

ThermalSummary ThermalInfo::summarize(const QList<double> &readings)
{
    ThermalSummary out;
    double sum = 0.0;
    for (int i = 0; i < readings.size(); ++i) {
        const double t = readings.at(i);
        if (!std::isfinite(t) || t <= 0.0 || t > MAX_SANE_TEMP)
            continue;
        sum += t;
        if (out.count == 0 || t > out.max) {
            out.max = t;
            out.maxIndex = i;
        }
        ++out.count;
    }
    if (out.count > 0)
        out.average = sum / out.count;
    return out;
}

double ThermalInfo::decodeSmcTemperature(const QByteArray &dataType, const QByteArray &bytes)
{
    if (dataType == "sp78" && bytes.size() == 2) {
        const int16_t raw = int16_t((uint16_t(uint8_t(bytes[0])) << 8) | uint8_t(bytes[1]));
        return raw / 256.0;
    }
    if (dataType == "flt " && bytes.size() == 4) {
        const uint32_t bits = uint32_t(uint8_t(bytes[0])) |
                              (uint32_t(uint8_t(bytes[1])) <<  8) |
                              (uint32_t(uint8_t(bytes[2])) << 16) |
                              (uint32_t(uint8_t(bytes[3])) << 24);
        float f;
        std::memcpy(&f, &bits, sizeof f);
        return std::isfinite(f) ? double(f) : -1.0;
    }
    return -1.0;
}

bool ThermalInfo::isPlausibleSmcTemperature(double celsius)
{
    return std::isfinite(celsius) && celsius >= 10.0 && celsius <= 130.0;
}

QString ThermalInfo::smcSensorLabel(const QString &key)
{
    static const QHash<QString, QString> prefixes = {
        {"Tp", "CPU Performance"},
        {"Te", "CPU Efficiency"},
        {"Tg", "GPU"},
        {"Tm", "Memory"},
        {"TB", "Battery"},
        {"TC", "CPU"},
        {"TG", "GPU"},
        {"Ta", "Ambient"},
    };
    const auto it = prefixes.constFind(key.left(2));
    if (it == prefixes.constEnd())
        return QStringLiteral("SMC %1").arg(key);
    return QStringLiteral("%1 (%2)").arg(it.value(), key);
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
        // SSO-15377: DIMM temp sensors — jc42 covers most DDR3/DDR4 SPD
        // thermal sensors; spd5118 is the DDR5 SPD Hub successor in kernel
        // 6.9+ (JEDEC JESD300/JESD403).
        {"jc42",            "DIMM"},
        {"spd5118",         "DIMM"},
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
