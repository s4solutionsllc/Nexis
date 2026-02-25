#include "fan_info_linux.h"
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <Utils/command_util.h>

static constexpr const char *HWMON_BASE = "/sys/class/hwmon";
static constexpr int MAX_SANE_RPM = 30000;

static constexpr const char *THINKPAD_FAN_PROC = "/proc/acpi/ibm/fan";
static constexpr const char *DELL_I8K_PROC = "/proc/i8k";

static QString friendlyFanDeviceName(const QString &driverName)
{
    static const QHash<QString, QString> map = {
        {"thinkpad",          "ThinkPad"},
        {"nct6775",           "Nuvoton"},
        {"nct6776",           "Nuvoton"},
        {"nct6779",           "Nuvoton"},
        {"nct6798",           "Nuvoton"},
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

    discoverHwmon();

    if (mSensors.isEmpty())
        discoverThinkpadProc();

    if (mSensors.isEmpty())
        discoverDellProc();

    if (!hasNvidiaSmiGpuFan())
        discoverNvidiaSmi();
}

void FanInfoLinux::discoverHwmon()
{
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
            sensor.sourceType = FanSourceType::Hwmon;

            mSensors.append(sensor);
        }
    }
}

void FanInfoLinux::discoverThinkpadProc()
{
    QFile f(THINKPAD_FAN_PROC);
    if (!f.exists() || !f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QString content = QString::fromUtf8(f.readAll());
    f.close();

    static QRegularExpression speedRe("speed:\\s+(\\d+)");
    QRegularExpressionMatch match = speedRe.match(content);
    if (!match.hasMatch())
        return;

    int rpm = match.captured(1).toInt();
    if (rpm < 0 || rpm > MAX_SANE_RPM)
        return;

    FanSensor sensor;
    sensor.id = "thinkpad/fan1";
    sensor.deviceName = "thinkpad_acpi";
    sensor.label = "ThinkPad \u2013 Fan";
    sensor.inputPath = THINKPAD_FAN_PROC;
    sensor.minRpm = -1;
    sensor.maxRpm = -1;
    sensor.sourceType = FanSourceType::ThinkpadProc;

    mSensors.append(sensor);
}

void FanInfoLinux::discoverDellProc()
{
    QFile f(DELL_I8K_PROC);
    if (!f.exists() || !f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QString content = QString::fromUtf8(f.readAll()).trimmed();
    f.close();

    // /proc/i8k format: space-delimited fields
    // Field indices: 0=version, 1=BIOS, 2=serial, 3=cpu_temp,
    //   4=left_fan_status, 5=right_fan_status, 6=left_fan_rpm, 7=right_fan_rpm
    QStringList fields = content.split(QRegularExpression("\\s+"));
    if (fields.size() < 8)
        return;

    for (int fanIdx = 0; fanIdx < 2; ++fanIdx) {
        int fieldPos = 6 + fanIdx;
        int rpm = fields.at(fieldPos).toInt();
        if (rpm <= 0)
            continue;

        QString fanName = (fanIdx == 0) ? "Left Fan" : "Right Fan";

        FanSensor sensor;
        sensor.id = QString("dell/fan%1").arg(fanIdx + 1);
        sensor.deviceName = "dell-smm";
        sensor.label = QString("Dell \u2013 %1").arg(fanName);
        sensor.inputPath = DELL_I8K_PROC;
        sensor.minRpm = -1;
        sensor.maxRpm = -1;
        sensor.sourceType = FanSourceType::DellProc;
        sensor.procFieldIndex = fieldPos;

        mSensors.append(sensor);
    }
}

bool FanInfoLinux::hasNvidiaSmiGpuFan() const
{
    for (const FanSensor &s : mSensors) {
        if (s.deviceName == "amdgpu" || s.deviceName == "nouveau")
            return true;
    }
    return false;
}

void FanInfoLinux::discoverNvidiaSmi()
{
    if (!CommandUtil::isExecutable("nvidia-smi"))
        return;

    ExecResult result = CommandUtil::execWithStatus(
        "nvidia-smi",
        {"--query-gpu=fan.speed,name", "--format=csv,noheader,nounits"},
        3000);

    if (result.exitCode != 0)
        return;

    QStringList lines = result.output.trimmed().split('\n', Qt::SkipEmptyParts);
    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines.at(i).trimmed();
        QStringList parts = line.split(',');
        if (parts.size() < 2)
            continue;

        QString fanStr = parts.at(0).trimmed();
        QString gpuName = parts.at(1).trimmed();

        if (fanStr == "[N/A]" || fanStr.isEmpty())
            continue;

        bool ok;
        int percent = fanStr.toInt(&ok);
        if (!ok || percent < 0)
            continue;

        FanSensor sensor;
        sensor.id = QString("nvidia-smi/gpu%1").arg(i);
        sensor.deviceName = "nvidia-smi";
        sensor.label = QString("GPU \u2013 %1 (%%)").arg(gpuName);
        sensor.inputPath = QString::number(i);
        sensor.minRpm = 0;
        sensor.maxRpm = 100;
        sensor.sourceType = FanSourceType::NvidiaSmi;

        mSensors.append(sensor);
    }
}

int FanInfoLinux::getFanSpeed(int index) const
{
    if (index < 0 || index >= mSensors.size())
        return 0;

    const FanSensor &sensor = mSensors.at(index);

    switch (sensor.sourceType) {
    case FanSourceType::Hwmon:
        return readHwmonSpeed(sensor);
    case FanSourceType::ThinkpadProc:
        return readThinkpadSpeed();
    case FanSourceType::DellProc:
        return readDellSpeed(sensor);
    case FanSourceType::NvidiaSmi:
        return readNvidiaSpeed(sensor);
    }

    return 0;
}

int FanInfoLinux::readHwmonSpeed(const FanSensor &sensor) const
{
    int rpm = FileUtil::readStringFromFile(sensor.inputPath)
              .trimmed()
              .toInt();

    return (rpm >= 0 && rpm <= MAX_SANE_RPM) ? rpm : 0;
}

int FanInfoLinux::readThinkpadSpeed() const
{
    QString content = FileUtil::readStringFromFile(THINKPAD_FAN_PROC);
    static QRegularExpression speedRe("speed:\\s+(\\d+)");
    QRegularExpressionMatch match = speedRe.match(content);
    if (!match.hasMatch())
        return 0;

    int rpm = match.captured(1).toInt();
    return (rpm >= 0 && rpm <= MAX_SANE_RPM) ? rpm : 0;
}

int FanInfoLinux::readDellSpeed(const FanSensor &sensor) const
{
    QString content = FileUtil::readStringFromFile(DELL_I8K_PROC).trimmed();
    QStringList fields = content.split(QRegularExpression("\\s+"));

    if (sensor.procFieldIndex < 0 || sensor.procFieldIndex >= fields.size())
        return 0;

    int rpm = fields.at(sensor.procFieldIndex).toInt();
    return (rpm >= 0 && rpm <= MAX_SANE_RPM) ? rpm : 0;
}

int FanInfoLinux::readNvidiaSpeed(const FanSensor &sensor) const
{
    int gpuIndex = sensor.inputPath.toInt();

    ExecResult result = CommandUtil::execWithStatus(
        "nvidia-smi",
        {QString("--id=%1").arg(gpuIndex),
         "--query-gpu=fan.speed",
         "--format=csv,noheader,nounits"},
        3000);

    if (result.exitCode != 0)
        return 0;

    bool ok;
    int percent = result.output.trimmed().toInt(&ok);
    return (ok && percent >= 0 && percent <= 100) ? percent : 0;
}
