#include "battery_charge_threshold.h"

#include <Utils/file_util.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>

static constexpr const char *POWER_SUPPLY_BASE = "/sys/class/power_supply";

static int readSysfsIntLocal(const QString &path)
{
    if (!QFile::exists(path))
        return -1;
    QString val = FileUtil::readStringFromFile(path).trimmed();
    bool ok = false;
    int result = val.toInt(&ok);
    return ok ? result : -1;
}

QString BatteryChargeThreshold::findBatteryPath()
{
    QDir psDir(POWER_SUPPLY_BASE);
    if (!psDir.exists())
        return QString();

    const QStringList entries = psDir.entryList({"BAT*"}, QDir::Dirs, QDir::Name);
    for (const QString &entry : entries) {
        QString path = QString("%1/%2").arg(POWER_SUPPLY_BASE, entry);
        QString typeVal = FileUtil::readStringFromFile(path + "/type").trimmed();
        if (typeVal.compare("Battery", Qt::CaseInsensitive) == 0)
            return path;
    }
    return QString();
}

ChargeThresholdStatus BatteryChargeThreshold::readStatus(const QString &overridePath)
{
    ChargeThresholdStatus s;

    QString path = overridePath.isEmpty() ? findBatteryPath() : overridePath;
    if (path.isEmpty()) {
        s.errorMsg = QStringLiteral("No battery found");
        return s;
    }

    s.batteryPath = path;
    s.batteryName = QFileInfo(path).fileName();

    QString endNode   = path + "/charge_control_end_threshold";
    QString startNode = path + "/charge_control_start_threshold";

    if (!QFile::exists(endNode)) {
        s.errorMsg = QStringLiteral("Charge threshold control not supported by this hardware");
        return s;
    }

    s.available = true;
    s.endPct    = readSysfsIntLocal(endNode);
    s.hasStart  = QFile::exists(startNode);
    if (s.hasStart)
        s.startPct = readSysfsIntLocal(startNode);

    return s;
}

ChargeThresholdResult BatteryChargeThreshold::writeThreshold(const QString &batteryPath,
                                                              bool hasStart,
                                                              int  endPct,
                                                              int  startPct)
{
    ChargeThresholdResult r;

    QString err = validateThreshold(endPct, hasStart ? startPct : -1);
    if (!err.isEmpty()) {
        r.errorMsg = err;
        return r;
    }

    // Write end threshold
    QString endNode = batteryPath + "/charge_control_end_threshold";
    QByteArray endVal = QByteArray::number(endPct);
    if (!FileUtil::writeRootFile(endNode, endVal)) {
        r.errorMsg = QStringLiteral("Failed to write end threshold (pkexec)");
        return r;
    }

    // Write start threshold only if the node exists and a value was requested
    if (hasStart && startPct >= 0) {
        QString startNode = batteryPath + "/charge_control_start_threshold";
        QByteArray startVal = QByteArray::number(startPct);
        if (!FileUtil::writeRootFile(startNode, startVal)) {
            r.errorMsg = QStringLiteral("Failed to write start threshold (pkexec)");
            return r;
        }
    }

    // Read-back verify
    r.verifiedEnd = readSysfsIntLocal(endNode);
    if (r.verifiedEnd != endPct) {
        r.errorMsg = QStringLiteral("Write succeeded but read-back differs (end: wrote %1, read %2)")
                         .arg(endPct).arg(r.verifiedEnd);
        return r;
    }

    if (hasStart && startPct >= 0) {
        QString startNode = batteryPath + "/charge_control_start_threshold";
        r.verifiedStart = readSysfsIntLocal(startNode);
    }

    r.ok = true;
    return r;
}

QString BatteryChargeThreshold::validateThreshold(int endPct, int startPct)
{
    if (endPct < kMinEndThreshold || endPct > 100)
        return QStringLiteral("End threshold must be between %1 and 100").arg(kMinEndThreshold);

    if (startPct >= 0) {
        if (startPct < 0 || startPct > 100)
            return QStringLiteral("Start threshold must be between 0 and 100");
        if (startPct >= endPct)
            return QStringLiteral("Start threshold must be less than end threshold");
    }

    return QString();
}

QString BatteryChargeThreshold::buildUdevRule(const QString &batteryName, int endPct, int startPct)
{
    // Rule re-applies threshold when the battery power supply is detected (e.g. after boot).
    QString rule = QStringLiteral(
        "# Nexis battery charge threshold — managed by Nexis, do not edit manually.\n"
        "ACTION==\"add\", SUBSYSTEM==\"power_supply\", "
        "KERNEL==\"%1\", "
        "ATTR{charge_control_end_threshold}=\"%2\"")
            .arg(batteryName).arg(endPct);

    if (startPct >= 0) {
        rule += QStringLiteral(
            ", ATTR{charge_control_start_threshold}=\"%1\"").arg(startPct);
    }

    rule += QStringLiteral("\n");
    return rule;
}
