#include "battery_info_linux.h"
#include <QDir>
#include <QFile>

static constexpr const char *POWER_SUPPLY_BASE = "/sys/class/power_supply";

// Helper: read an integer from a sysfs file, return -1 if unavailable
static int readSysfsInt(const QString &path)
{
    if (!QFile::exists(path))
        return -1;
    QString val = FileUtil::readStringFromFile(path).trimmed();
    if (val.isEmpty())
        return -1;
    bool ok = false;
    int result = val.toInt(&ok);
    return ok ? result : -1;
}

// Helper: read a string from a sysfs file
static QString readSysfsString(const QString &path)
{
    if (!QFile::exists(path))
        return QString();
    return FileUtil::readStringFromFile(path).trimmed();
}

// Helper: derive condition from health percentage
static QString deriveCondition(int healthPercent)
{
    if (healthPercent >= 80) return QStringLiteral("Good");
    if (healthPercent >= 60) return QStringLiteral("Fair");
    return QStringLiteral("Replace");
}

BatteryInfoLinux::BatteryInfoLinux()
{
    discoverBattery();
    if (mData.hasBattery)
        updateBatteryInfo();
}

void BatteryInfoLinux::discoverBattery()
{
    mData = BatteryData();
    mBatteryPath.clear();

    QDir psDir(POWER_SUPPLY_BASE);
    if (!psDir.exists())
        return;

    // Look for BAT* directories (BAT0, BAT1, etc.)
    QStringList entries = psDir.entryList({"BAT*"}, QDir::Dirs, QDir::Name);

    for (const QString &entry : entries) {
        QString path = QString("%1/%2").arg(POWER_SUPPLY_BASE, entry);
        QString type = readSysfsString(path + "/type");
        if (type.compare("Battery", Qt::CaseInsensitive) == 0) {
            mBatteryPath = path;
            mData.hasBattery = true;
            return;
        }
    }

    // Some systems use different naming (e.g., "battery" instead of "BAT0")
    entries = psDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        if (entry.startsWith("BAT", Qt::CaseInsensitive))
            continue;   // already checked above
        QString path = QString("%1/%2").arg(POWER_SUPPLY_BASE, entry);
        QString type = readSysfsString(path + "/type");
        if (type.compare("Battery", Qt::CaseInsensitive) == 0) {
            mBatteryPath = path;
            mData.hasBattery = true;
            return;
        }
    }
}

void BatteryInfoLinux::updateBatteryInfo()
{
    if (!mData.hasBattery || mBatteryPath.isEmpty())
        return;

    // Status
    mData.status = readSysfsString(mBatteryPath + "/status");
    mData.isCharging = (mData.status == "Charging");
    mData.isPluggedIn = (mData.status == "Charging" || mData.status == "Not charging" || mData.status == "Full");

    // Charge percentage (direct from kernel)
    int cap = readSysfsInt(mBatteryPath + "/capacity");
    if (cap >= 0)
        mData.chargePercent = qBound(0, cap, 100);

    // Capacity values — try charge-based (µAh) first, then energy-based (µWh)
    bool useChargeBased = QFile::exists(mBatteryPath + "/charge_full");

    if (useChargeBased) {
        int chargeFull = readSysfsInt(mBatteryPath + "/charge_full");
        int chargeFullDesign = readSysfsInt(mBatteryPath + "/charge_full_design");
        int chargeNow = readSysfsInt(mBatteryPath + "/charge_now");

        if (chargeFull > 0)
            mData.maxCapacityMah = chargeFull / 1000.0;         // µAh → mAh
        if (chargeFullDesign > 0)
            mData.designCapacityMah = chargeFullDesign / 1000.0;
        if (chargeNow >= 0)
            mData.currentCapacityMah = chargeNow / 1000.0;
    } else {
        // Energy-based (µWh)
        int energyFull = readSysfsInt(mBatteryPath + "/energy_full");
        int energyFullDesign = readSysfsInt(mBatteryPath + "/energy_full_design");
        int energyNow = readSysfsInt(mBatteryPath + "/energy_now");
        int voltageNow = readSysfsInt(mBatteryPath + "/voltage_now");

        // Convert µWh to mAh using voltage: mAh = mWh / V = (µWh/1000) / (µV/1e6)
        double volts = (voltageNow > 0) ? (voltageNow / 1.0e6) : 0.0;
        if (volts > 0.0) {
            if (energyFull > 0)
                mData.maxCapacityMah = (energyFull / 1000.0) / volts;
            if (energyFullDesign > 0)
                mData.designCapacityMah = (energyFullDesign / 1000.0) / volts;
            if (energyNow >= 0)
                mData.currentCapacityMah = (energyNow / 1000.0) / volts;
        }
    }

    // Voltage (µV → mV)
    int voltageNow = readSysfsInt(mBatteryPath + "/voltage_now");
    if (voltageNow > 0)
        mData.voltageMv = voltageNow / 1000;

    // Current draw (µA → mA)
    int currentNow = readSysfsInt(mBatteryPath + "/current_now");
    if (currentNow >= 0) {
        mData.amperageMa = currentNow / 1000.0;
        // Some drivers report unsigned; negate if discharging
        if (!mData.isCharging && mData.amperageMa > 0)
            mData.amperageMa = -mData.amperageMa;
    }

    // Power (µW → W)
    int powerNow = readSysfsInt(mBatteryPath + "/power_now");
    if (powerNow > 0) {
        mData.powerWatts = powerNow / 1.0e6;
    } else if (mData.voltageMv > 0 && mData.amperageMa != 0.0) {
        mData.powerWatts = qAbs(mData.voltageMv * mData.amperageMa) / 1.0e6;
    }

    // Temperature (tenths of °C → °C)
    int temp = readSysfsInt(mBatteryPath + "/temp");
    if (temp > -500)    // reasonable sanity check
        mData.temperatureCelsius = temp / 10.0;

    // Cycle count
    int cycles = readSysfsInt(mBatteryPath + "/cycle_count");
    if (cycles >= 0)
        mData.cycleCount = cycles;

    // Manufacturer, model, technology
    mData.manufacturer = readSysfsString(mBatteryPath + "/manufacturer");
    mData.model = readSysfsString(mBatteryPath + "/model_name");
    mData.technology = readSysfsString(mBatteryPath + "/technology");

    // Derive health percentage
    if (mData.maxCapacityMah > 0 && mData.designCapacityMah > 0)
        mData.healthPercent = qBound(0, static_cast<int>((mData.maxCapacityMah / mData.designCapacityMah) * 100.0), 100);

    // Derive condition
    if (mData.healthPercent >= 0)
        mData.condition = deriveCondition(mData.healthPercent);

    // Time remaining estimate (minutes)
    if (!mData.isCharging && mData.powerWatts > 0) {
        int energyNow = readSysfsInt(mBatteryPath + "/energy_now");
        if (energyNow > 0) {
            double hoursRemaining = (energyNow / 1.0e6) / mData.powerWatts;
            mData.timeRemainingMinutes = static_cast<int>(hoursRemaining * 60.0);
        }
    } else if (mData.isCharging && mData.powerWatts > 0) {
        int energyFull = readSysfsInt(mBatteryPath + "/energy_full");
        int energyNow = readSysfsInt(mBatteryPath + "/energy_now");
        if (energyFull > 0 && energyNow >= 0) {
            double remaining = (energyFull - energyNow) / 1.0e6;
            double hoursToFull = remaining / mData.powerWatts;
            mData.timeRemainingMinutes = static_cast<int>(hoursToFull * 60.0);
        }
    }

    // TLP charge thresholds (read-only)
    int startThreshold = readSysfsInt(mBatteryPath + "/charge_control_start_threshold");
    int stopThreshold = readSysfsInt(mBatteryPath + "/charge_control_end_threshold");
    if (startThreshold >= 0)
        mData.chargeStartThreshold = startThreshold;
    if (stopThreshold >= 0)
        mData.chargeStopThreshold = stopThreshold;
}
