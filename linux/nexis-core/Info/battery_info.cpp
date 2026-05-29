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

// deriveCondition and deriveHealthPercent are now static methods on BatteryInfo
// (see battery_info_shared.cpp)

BatteryInfoLinux::BatteryInfoLinux()
{
    discoverBattery();
    if (mData.hasBattery)
        updateBatteryInfo();
}

void BatteryInfoLinux::discoverBattery()
{
    mData = BatteryData();
    mBatteryPaths.clear();
    mBatteries.clear();

    QDir psDir(POWER_SUPPLY_BASE);
    if (!psDir.exists())
        return;

    // Collect all BAT* entries first (sorted so BAT0 < BAT1 < ...)
    const QStringList batEntries = psDir.entryList({"BAT*"}, QDir::Dirs, QDir::Name);
    for (const QString &entry : batEntries) {
        QString path = QString("%1/%2").arg(POWER_SUPPLY_BASE, entry);
        if (readSysfsString(path + "/type").compare("Battery", Qt::CaseInsensitive) == 0)
            mBatteryPaths << path;
    }

    // Also check non-BAT* named entries (some systems use "battery", "BATT", etc.)
    const QStringList allEntries = psDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : allEntries) {
        if (entry.startsWith("BAT", Qt::CaseInsensitive))
            continue;
        QString path = QString("%1/%2").arg(POWER_SUPPLY_BASE, entry);
        if (readSysfsString(path + "/type").compare("Battery", Qt::CaseInsensitive) == 0)
            mBatteryPaths << path;
    }

    mData.hasBattery = !mBatteryPaths.isEmpty();
}

BatteryData BatteryInfoLinux::readBatteryData(const QString &path, const QString &name) const
{
    BatteryData d;
    d.hasBattery = true;
    d.batteryName = name;

    d.status = readSysfsString(path + "/status");
    d.isCharging = (d.status == "Charging");
    d.isPluggedIn = (d.status == "Charging" || d.status == "Not charging" || d.status == "Full");

    int cap = readSysfsInt(path + "/capacity");
    if (cap >= 0)
        d.chargePercent = qBound(0, cap, 100);

    bool useChargeBased = QFile::exists(path + "/charge_full");
    if (useChargeBased) {
        int chargeFull       = readSysfsInt(path + "/charge_full");
        int chargeFullDesign = readSysfsInt(path + "/charge_full_design");
        int chargeNow        = readSysfsInt(path + "/charge_now");
        if (chargeFull > 0)       d.maxCapacityMah    = chargeFull / 1000.0;
        if (chargeFullDesign > 0) d.designCapacityMah = chargeFullDesign / 1000.0;
        if (chargeNow >= 0)       d.currentCapacityMah = chargeNow / 1000.0;
    } else {
        int energyFull       = readSysfsInt(path + "/energy_full");
        int energyFullDesign = readSysfsInt(path + "/energy_full_design");
        int energyNow        = readSysfsInt(path + "/energy_now");
        int voltageNow       = readSysfsInt(path + "/voltage_now");
        double volts = (voltageNow > 0) ? (voltageNow / 1.0e6) : 0.0;
        if (volts > 0.0) {
            if (energyFull > 0)       d.maxCapacityMah    = (energyFull / 1000.0) / volts;
            if (energyFullDesign > 0) d.designCapacityMah = (energyFullDesign / 1000.0) / volts;
            if (energyNow >= 0)       d.currentCapacityMah = (energyNow / 1000.0) / volts;
        }
    }

    int voltageNow = readSysfsInt(path + "/voltage_now");
    if (voltageNow > 0)
        d.voltageMv = voltageNow / 1000;

    int currentNow = readSysfsInt(path + "/current_now");
    if (currentNow >= 0) {
        d.amperageMa = currentNow / 1000.0;
        if (!d.isCharging && d.amperageMa > 0)
            d.amperageMa = -d.amperageMa;
    }

    int powerNow = readSysfsInt(path + "/power_now");
    if (powerNow > 0) {
        d.powerWatts = powerNow / 1.0e6;
    } else if (d.voltageMv > 0 && d.amperageMa != 0.0) {
        d.powerWatts = qAbs(d.voltageMv * d.amperageMa) / 1.0e6;
    }

    int temp = readSysfsInt(path + "/temp");
    if (temp > -500)
        d.temperatureCelsius = temp / 10.0;

    int cycles = readSysfsInt(path + "/cycle_count");
    if (cycles >= 0)
        d.cycleCount = cycles;

    d.manufacturer = readSysfsString(path + "/manufacturer");
    d.model        = readSysfsString(path + "/model_name");
    d.technology   = readSysfsString(path + "/technology");

    d.healthPercent = BatteryInfo::deriveHealthPercent(d.maxCapacityMah, d.designCapacityMah);
    if (d.healthPercent >= 0)
        d.condition = BatteryInfo::deriveCondition(d.healthPercent);

    // Time remaining
    if (!d.isCharging && d.powerWatts > 0) {
        int energyNow = readSysfsInt(path + "/energy_now");
        if (energyNow > 0)
            d.timeRemainingMinutes = static_cast<int>((energyNow / 1.0e6) / d.powerWatts * 60.0);
    } else if (d.isCharging && d.powerWatts > 0) {
        int energyFull = readSysfsInt(path + "/energy_full");
        int energyNow  = readSysfsInt(path + "/energy_now");
        if (energyFull > 0 && energyNow >= 0) {
            double remaining = (energyFull - energyNow) / 1.0e6;
            d.timeRemainingMinutes = static_cast<int>(remaining / d.powerWatts * 60.0);
        }
    }

    int startThreshold = readSysfsInt(path + "/charge_control_start_threshold");
    int stopThreshold  = readSysfsInt(path + "/charge_control_end_threshold");
    if (startThreshold >= 0) d.chargeStartThreshold = startThreshold;
    if (stopThreshold  >= 0) d.chargeStopThreshold  = stopThreshold;

    return d;
}

void BatteryInfoLinux::aggregate()
{
    if (mBatteries.isEmpty()) {
        mData = BatteryData();
        return;
    }

    // Single battery — no aggregation needed
    if (mBatteries.size() == 1) {
        mData = mBatteries.first();
        return;
    }

    // Aggregate across all batteries
    mData = BatteryData();
    mData.hasBattery = true;

    double totalDesign  = 0.0;
    double totalMax     = 0.0;
    double totalCurrent = 0.0;
    double totalPower   = 0.0;
    double maxTemp      = -1.0;
    double weightedCharge  = 0.0;
    double weightedHealth  = 0.0;
    bool anyCharging   = false;
    bool anyPluggedIn  = false;
    bool allFull       = true;

    for (const BatteryData &b : mBatteries) {
        anyCharging  = anyCharging  || b.isCharging;
        anyPluggedIn = anyPluggedIn || b.isPluggedIn;
        allFull      = allFull      && (b.status == QLatin1String("Full"));

        if (b.maxCapacityMah > 0) {
            totalMax     += b.maxCapacityMah;
            if (b.chargePercent >= 0)
                weightedCharge += b.chargePercent * b.maxCapacityMah;
        }
        if (b.designCapacityMah > 0) {
            totalDesign  += b.designCapacityMah;
            if (b.healthPercent >= 0)
                weightedHealth += b.healthPercent * b.designCapacityMah;
        }
        if (b.currentCapacityMah >= 0) totalCurrent += b.currentCapacityMah;
        if (b.powerWatts > 0)          totalPower   += b.powerWatts;
        if (b.temperatureCelsius >= 0) maxTemp = qMax(maxTemp, b.temperatureCelsius);
    }

    mData.isCharging  = anyCharging;
    mData.isPluggedIn = anyPluggedIn;
    mData.status = anyCharging ? QStringLiteral("Charging")
                 : allFull     ? QStringLiteral("Full")
                               : QStringLiteral("Discharging");

    if (totalMax > 0)    mData.chargePercent     = qBound(0, static_cast<int>(weightedCharge / totalMax), 100);
    if (totalDesign > 0) mData.healthPercent     = qBound(0, static_cast<int>(weightedHealth / totalDesign), 100);
    if (totalMax > 0)    mData.maxCapacityMah    = totalMax;
    if (totalDesign > 0) mData.designCapacityMah = totalDesign;
    if (totalCurrent > 0) mData.currentCapacityMah = totalCurrent;
    if (totalPower > 0)  mData.powerWatts        = totalPower;
    if (maxTemp >= 0)    mData.temperatureCelsius = maxTemp;

    if (mData.healthPercent >= 0)
        mData.condition = BatteryInfo::deriveCondition(mData.healthPercent);

    // Time remaining from aggregated values
    if (!mData.isCharging && mData.powerWatts > 0 && mData.currentCapacityMah > 0) {
        double hoursRemaining = (mData.currentCapacityMah / 1000.0) /
                                (mData.powerWatts / (mBatteries.first().voltageMv > 0
                                    ? mBatteries.first().voltageMv / 1000.0 : 1.0));
        mData.timeRemainingMinutes = static_cast<int>(hoursRemaining * 60.0);
    }

    // Non-aggregatable fields from primary battery (index 0)
    const BatteryData &primary = mBatteries.first();
    mData.cycleCount           = primary.cycleCount;
    mData.manufacturer         = primary.manufacturer;
    mData.model                = primary.model;
    mData.technology           = primary.technology;
    mData.voltageMv            = primary.voltageMv;
    mData.chargeStartThreshold = primary.chargeStartThreshold;
    mData.chargeStopThreshold  = primary.chargeStopThreshold;
}

void BatteryInfoLinux::updateBatteryInfo()
{
    if (!mData.hasBattery || mBatteryPaths.isEmpty())
        return;

    mBatteries.clear();
    for (const QString &path : mBatteryPaths) {
        QString name = QFileInfo(path).fileName();
        mBatteries << readBatteryData(path, name);
    }

    aggregate();
}

int BatteryInfoLinux::batteryCount() const
{
    return mBatteries.size();
}

BatteryData BatteryInfoLinux::getBatteryData(int index) const
{
    if (index < 0 || index >= mBatteries.size())
        return mData;
    return mBatteries.at(index);
}
