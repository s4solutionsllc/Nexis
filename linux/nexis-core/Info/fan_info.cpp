#include "fan_info_linux.h"
#include <QDir>
#include <QRegularExpression>

static constexpr const char *HWMON_BASE = "/sys/class/hwmon";
static constexpr int MAX_SANE_RPM = 30000;

static QString friendlyFanDeviceName(const QString &driverName)
{
    static const QHash<QString, QString> map = {
        {"thinkpad",          "ThinkPad"},
        {"nct6775",           "Nuvoton"},
        {"nct6776",           "Nuvoton"},
        {"nct6779",           "Nuvoton"},
        {"it87",              "ITE"},
        {"dell_smm",          "Dell"},
        {"asus-ec-sensors",   "ASUS"},
        {"applesmc",          "Apple SMC"},
        {"amdgpu",            "GPU"},
        {"nouveau",           "GPU"},
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

FanInfoLinux::FanInfoLinux()
{
    discoverSensors();
}

void FanInfoLinux::discoverSensors()
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

        QDir devDir(hwmonPath);
        QStringList fanInputs = devDir.entryList({"fan*_input"}, QDir::Files, QDir::Name);

        for (const QString &inputFile : fanInputs) {
            static QRegularExpression re("^fan(\\d+)_input$");
            QRegularExpressionMatch match = re.match(inputFile);
            if (!match.hasMatch())
                continue;

            QString idx = match.captured(1);
            QString inputPath = hwmonPath + "/" + inputFile;

            QString labelPath = QString("%1/fan%2_label").arg(hwmonPath, idx);
            QString rawLabel = FileUtil::readStringFromFile(labelPath).trimmed();
            QString friendly = friendlyFanDeviceName(deviceName);
            QString label;
            if (!rawLabel.isEmpty())
                label = QString("%1 \u2013 %2").arg(friendly, rawLabel);
            else
                label = QString("%1 \u2013 Fan %2").arg(friendly, idx);

            int minRpm = -1;
            QString minPath = QString("%1/fan%2_min").arg(hwmonPath, idx);
            QString minStr = FileUtil::readStringFromFile(minPath).trimmed();
            if (!minStr.isEmpty()) {
                int val = minStr.toInt();
                if (val >= 0 && val <= MAX_SANE_RPM)
                    minRpm = val;
            }

            int maxRpm = -1;
            QString maxPath = QString("%1/fan%2_max").arg(hwmonPath, idx);
            QString maxStr = FileUtil::readStringFromFile(maxPath).trimmed();
            if (!maxStr.isEmpty()) {
                int val = maxStr.toInt();
                if (val > 0 && val <= MAX_SANE_RPM)
                    maxRpm = val;
            }

            FanSensor sensor;
            sensor.id = QString("%1/fan%2").arg(deviceName, idx);
            sensor.deviceName = deviceName;
            sensor.label = label;
            sensor.inputPath = inputPath;
            sensor.minRpm = minRpm;
            sensor.maxRpm = maxRpm;

            mSensors.append(sensor);
        }
    }
}

int FanInfoLinux::getFanSpeed(int index) const
{
    if (index < 0 || index >= mSensors.size())
        return 0;

    int rpm = FileUtil::readStringFromFile(mSensors.at(index).inputPath)
              .trimmed()
              .toInt();

    return (rpm >= 0 && rpm <= MAX_SANE_RPM) ? rpm : 0;
}
