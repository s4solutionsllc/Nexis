// FW-14 (SSO-3742): Health-report export (on-demand and scheduled).

#include "health_report_manager.h"
#include "info_manager.h"
#include "cleaner_service.h"
#include "setting_manager.h"
#include "schedule_manager.h"
#include <Utils/format_util.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>
#include <QUuid>

HealthReportManager *HealthReportManager::instance = nullptr;

HealthReportManager::HealthReportManager() = default;

HealthReportManager *HealthReportManager::ins()
{
    if (!instance)
        instance = new HealthReportManager;
    return instance;
}

// ── Report section builders ──────────────────────────────────────────────────

QString HealthReportManager::buildHeader() const
{
    InfoManager *im = InfoManager::ins();
    QString out;
    QTextStream s(&out);

    s << "# Nexis Health Report\n\n";
    s << "**Generated:** " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n";
    s << "**Host:** " << im->getHostname() << "\n";
    s << "**Platform:** " << im->getPlatform() << "\n";
    s << "**Distribution:** " << im->getDistribution() << "\n";
    s << "**Kernel:** " << im->getKernel() << "\n\n";
    s << "---\n\n";
    return out;
}

QString HealthReportManager::buildCpuSection() const
{
    InfoManager *im = InfoManager::ins();
    QString out;
    QTextStream s(&out);

    s << "## CPU\n\n";
    s << "- **Model:** " << im->getCpuModel() << "\n";
    s << "- **Speed:** " << im->getCpuSpeed() << "\n";
    s << "- **Cores:** " << im->getCpuCoreLabel() << "\n";

    QList<double> loadAvgs = im->getCpuLoadAvgs();
    if (!loadAvgs.isEmpty()) {
        s << "- **Load averages:**";
        const QStringList labels{"1 min", "5 min", "15 min"};
        for (int i = 0; i < loadAvgs.size() && i < labels.size(); ++i)
            s << " " << labels[i] << " = " << QString::number(loadAvgs[i], 'f', 2);
        s << "\n";
    }

    QList<int> percents = im->getCpuPercents();
    if (!percents.isEmpty()) {
        int total = 0;
        for (int p : percents) total += p;
        int avg = percents.isEmpty() ? 0 : total / percents.size();
        s << "- **Current usage:** " << avg << "% (avg across " << percents.size() << " cores)\n";
    }

    s << "\n";
    return out;
}

QString HealthReportManager::buildMemorySection() const
{
    InfoManager *im = InfoManager::ins();
    QString out;
    QTextStream s(&out);

    s << "## Memory\n\n";

    quint64 memTotal = im->getMemTotal();
    quint64 memUsed  = im->getMemUsed();
    s << "- **Total:** " << FormatUtil::formatBytes(memTotal) << "\n";
    s << "- **Used:** " << FormatUtil::formatBytes(memUsed) << "\n";
    if (memTotal > 0)
        s << "- **Usage:** " << QString::number(100.0 * memUsed / memTotal, 'f', 1) << "%\n";

    quint64 swapTotal = im->getSwapTotal();
    quint64 swapUsed  = im->getSwapUsed();
    s << "- **Swap total:** " << FormatUtil::formatBytes(swapTotal) << "\n";
    s << "- **Swap used:** " << FormatUtil::formatBytes(swapUsed) << "\n";

    s << "\n";
    return out;
}

QString HealthReportManager::buildDiskSection() const
{
    InfoManager *im = InfoManager::ins();
    QString out;
    QTextStream s(&out);

    s << "## Disk Usage\n\n";

    QList<Disk> disks = im->getDisks();
    if (disks.isEmpty()) {
        s << "_No disks found._\n\n";
        return out;
    }

    s << "| Mount | Device | Total | Used | Free | Usage |\n";
    s << "|-------|--------|-------|------|------|-------|\n";
    for (const Disk &d : disks) {
        double pct = (d.size > 0) ? (100.0 * d.used / d.size) : 0.0;
        s << "| " << d.name
          << " | " << d.device
          << " | " << FormatUtil::formatBytes(d.size)
          << " | " << FormatUtil::formatBytes(d.used)
          << " | " << FormatUtil::formatBytes(d.free)
          << " | " << QString::number(pct, 'f', 1) << "%"
          << " |\n";
    }
    s << "\n";
    return out;
}

QString HealthReportManager::buildDriveHealthSection() const
{
    InfoManager *im = InfoManager::ins();
    QString out;
    QTextStream s(&out);

    s << "## Drive Health (SMART)\n\n";

    if (!im->hasSmartctl()) {
        s << "_smartctl not available — install smartmontools for SMART data._\n\n";
        return out;
    }

    QList<DriveHealth> drives = im->getDriveHealth();
    if (drives.isEmpty()) {
        s << "_No SMART data collected yet._\n\n";
        return out;
    }

    for (const DriveHealth &d : drives) {
        s << "### " << d.devicePath << " — " << d.model << "\n\n";
        s << "- **Verdict:** " << d.healthVerdict << "\n";
        if (d.healthPercent >= 0)
            s << "- **Health:** " << d.healthPercent << "%\n";
        if (d.temperatureCelsius >= 0)
            s << "- **Temperature:** " << QString::number(d.temperatureCelsius, 'f', 1) << " °C\n";
        if (d.powerOnHours >= 0)
            s << "- **Power-on hours:** " << d.powerOnHours << " h\n";
        if (d.powerCycles >= 0)
            s << "- **Power cycles:** " << d.powerCycles << "\n";
        s << "- **SMART passed:** " << (d.smartPassed ? "Yes" : "No") << "\n";
        s << "\n";
    }
    return out;
}

QString HealthReportManager::buildThermalSection() const
{
    InfoManager *im = InfoManager::ins();
    QString out;
    QTextStream s(&out);

    s << "## Thermal Sensors\n\n";

    if (!im->hasThermalSensors()) {
        s << "_No thermal sensors found._\n\n";
        return out;
    }

    QList<ThermalSensor> sensors = im->getThermalSensors();
    s << "| Sensor | Temperature |\n";
    s << "|--------|-------------|\n";
    for (int i = 0; i < sensors.size(); ++i) {
        double temp = im->getThermalTemperature(i);
        s << "| " << sensors[i].label
          << " | " << QString::number(temp, 'f', 1) << " °C |\n";
    }
    s << "\n";
    return out;
}

QString HealthReportManager::buildGpuSection() const
{
    InfoManager *im = InfoManager::ins();
    QString out;
    QTextStream s(&out);

    s << "## GPU\n\n";

    if (!im->hasGpu()) {
        s << "_No GPU detected._\n\n";
        return out;
    }

    QList<GpuDevice> gpus = im->getGpuDevices();
    for (const GpuDevice &g : gpus) {
        s << "- **" << g.name << "** (" << g.vendor << ")";
        if (g.utilization >= 0)
            s << " — " << g.utilization << "% utilisation";
        if (!g.driverName.isEmpty())
            s << " — driver: " << g.driverName;
        s << "\n";
    }
    s << "\n";
    return out;
}

QString HealthReportManager::buildBatterySection() const
{
    InfoManager *im = InfoManager::ins();
    QString out;
    QTextStream s(&out);

    s << "## Battery\n\n";

    if (!im->hasBattery()) {
        s << "_No battery detected (desktop or no battery present)._\n\n";
        return out;
    }

    for (int i = 0; i < im->batteryCount(); ++i) {
        BatteryData b = im->getBatteryData(i);
        if (!b.hasBattery) continue;
        if (im->batteryCount() > 1)
            s << "### " << (b.batteryName.isEmpty() ? QString("Battery %1").arg(i) : b.batteryName) << "\n\n";

        s << "- **Charge:** " << b.chargePercent << "%";
        if (!b.status.isEmpty()) s << " (" << b.status << ")";
        s << "\n";
        if (b.healthPercent >= 0)
            s << "- **Health:** " << b.healthPercent << "%\n";
        if (b.cycleCount >= 0)
            s << "- **Cycle count:** " << b.cycleCount;
        if (b.designCycleCount > 0)
            s << " / " << b.designCycleCount;
        if (b.cycleCount >= 0) s << "\n";
        if (!b.condition.isEmpty())
            s << "- **Condition:** " << b.condition << "\n";
        if (b.temperatureCelsius >= 0)
            s << "- **Temperature:** " << QString::number(b.temperatureCelsius, 'f', 1) << " °C\n";
    }
    s << "\n";
    return out;
}

QString HealthReportManager::buildCleanableSpaceSection() const
{
    QString out;
    QTextStream s(&out);

    s << "## Cleanable Space\n\n";

    CleanerService::ScanResult result =
        CleanerService::ins()->scan(CleanerService::allCategories());

    if (result.totalSize == 0) {
        s << "_No cleanable files found._\n\n";
        return out;
    }

    s << "| Category | Size |\n";
    s << "|----------|------|\n";
    for (auto it = result.categoryFiles.cbegin(); it != result.categoryFiles.cend(); ++it) {
        quint64 catSize = 0;
        for (const QFileInfo &fi : it.value())
            catSize += static_cast<quint64>(fi.size());
        if (catSize == 0) continue;
        s << "| " << CleanerService::categoryName(it.key())
          << " | " << FormatUtil::formatBytes(catSize) << " |\n";
    }
    s << "\n";
    s << "**Total cleanable:** " << FormatUtil::formatBytes(result.totalSize) << "\n\n";
    return out;
}

QString HealthReportManager::buildFooter() const
{
    return "---\n\n_Report generated by [Nexis](https://github.com/s4solutionsllc/Nexis)._\n";
}

// ── Main report generation ───────────────────────────────────────────────────

QString HealthReportManager::resolveOutputPath(const QString &requested) const
{
    if (!requested.isEmpty()) {
        QFileInfo fi(requested);
        if (fi.isDir()) {
            QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
            return requested + QDir::separator() + "nexis-health-" + ts + ".md";
        }
        return requested;
    }

    QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QDir().mkpath(dir);
    QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
    return dir + QDir::separator() + "nexis-health-" + ts + ".md";
}

QString HealthReportManager::generateReport(const QString &outputPath) const
{
    QString path = resolveOutputPath(outputPath);

    QString content;
    content += buildHeader();
    content += buildCpuSection();
    content += buildMemorySection();
    content += buildDiskSection();
    content += buildDriveHealthSection();
    content += buildThermalSection();
    content += buildGpuSection();
    content += buildBatterySection();
    content += buildCleanableSpaceSection();
    content += buildFooter();

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "HealthReportManager: failed to write report to" << path;
        return {};
    }
    QTextStream out(&f);
    out << content;
    return path;
}

// ── Schedule persistence ─────────────────────────────────────────────────────

void HealthReportManager::loadReportSchedules()
{
    mReportSchedules.clear();
    QString json = SettingManager::ins()->getReportSchedules();
    QJsonArray arr = QJsonDocument::fromJson(json.toUtf8()).array();
    for (const QJsonValue &v : arr) {
        QJsonObject obj = v.toObject();
        ReportSchedule s;
        s.id           = obj["id"].toString();
        s.name         = obj["name"].toString();
        s.enabled      = obj["enabled"].toBool(true);
        s.frequencyInt = obj["frequency"].toInt(1);
        s.everyNDays   = obj["everyNDays"].toInt(7);
        s.dayOfWeek    = obj["dayOfWeek"].toInt(0);
        s.dayOfMonth   = obj["dayOfMonth"].toInt(1);
        s.hour         = obj["hour"].toInt(6);
        s.minute       = obj["minute"].toInt(0);
        s.outputDir    = obj["outputDir"].toString();
        QString lr     = obj["lastRun"].toString();
        if (!lr.isEmpty())
            s.lastRun = QDateTime::fromString(lr, Qt::ISODate);
        mReportSchedules.append(s);
    }
}

void HealthReportManager::saveReportSchedules()
{
    QJsonArray arr;
    for (const ReportSchedule &s : mReportSchedules) {
        QJsonObject obj;
        obj["id"]          = s.id;
        obj["name"]        = s.name;
        obj["enabled"]     = s.enabled;
        obj["frequency"]   = s.frequencyInt;
        obj["everyNDays"]  = s.everyNDays;
        obj["dayOfWeek"]   = s.dayOfWeek;
        obj["dayOfMonth"]  = s.dayOfMonth;
        obj["hour"]        = s.hour;
        obj["minute"]      = s.minute;
        obj["outputDir"]   = s.outputDir;
        if (s.lastRun.isValid())
            obj["lastRun"] = s.lastRun.toString(Qt::ISODate);
        arr.append(obj);
    }
    SettingManager::ins()->setReportSchedules(
        QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

QList<HealthReportManager::ReportSchedule> HealthReportManager::getAllReportSchedules() const
{
    return mReportSchedules;
}

HealthReportManager::ReportSchedule HealthReportManager::getReportSchedule(const QString &id) const
{
    for (const ReportSchedule &s : mReportSchedules)
        if (s.id == id) return s;
    return {};
}

QString HealthReportManager::createReportSchedule(const ReportSchedule &s)
{
    ReportSchedule ns = s;
    if (ns.id.isEmpty())
        ns.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    mReportSchedules.append(ns);
    saveReportSchedules();
    return ns.id;
}

void HealthReportManager::updateReportSchedule(const ReportSchedule &s)
{
    for (ReportSchedule &rs : mReportSchedules) {
        if (rs.id == s.id) {
            rs = s;
            break;
        }
    }
    saveReportSchedules();
}

void HealthReportManager::deleteReportSchedule(const QString &id)
{
    for (int i = 0; i < mReportSchedules.size(); ++i) {
        if (mReportSchedules[i].id == id) {
            mReportSchedules.removeAt(i);
            break;
        }
    }
    saveReportSchedules();
}

// ── OS scheduler integration ─────────────────────────────────────────────────

void HealthReportManager::syncReportSchedulesToOS()
{
    // Reuse ScheduleManager's OS-native backend by synthesising a CleaningSchedule
    // with a placeholder category and relying on --report invocation instead of --clean.
    // For now, report schedules are stored in settings and the OS integration is
    // handled by writing a wrapper entry into the native scheduler that calls
    // `nexis --report <id>`.  The actual launchd/systemd/cron write delegates to
    // a private helper that mirrors ScheduleManager's approach but passes --report.

    // Iterate enabled schedules and install system-level timers.
    for (const ReportSchedule &rs : mReportSchedules) {
        if (!rs.enabled) continue;

        // Build a synthetic CleaningSchedule used only for the timing metadata.
        // ScheduleManager's createSystemdTimer / createLaunchdPlist take a schedule
        // struct; we pass --report <id> via a custom environment variable approach.
        // For this initial implementation, emit a cron/launchd entry directly.
#ifdef Q_OS_MACOS
        // launchd plist path
        QString plistDir = QDir::homePath() + "/Library/LaunchAgents/";
        QString plistPath = plistDir + "com.nexis.report." + rs.id + ".plist";

        QDir().mkpath(plistDir);
        QString appPath = QCoreApplication::applicationFilePath();

        QString plist = R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key>
  <string>com.nexis.report.)" + rs.id + R"(</string>
  <key>ProgramArguments</key>
  <array>
    <string>)" + appPath + R"(</string>
    <string>--report</string>
    <string>)" + rs.id + R"(</string>
  </array>
  <key>StartCalendarInterval</key>
  <dict>
    <key>Hour</key>
    <integer>)" + QString::number(rs.hour) + R"(</integer>
    <key>Minute</key>
    <integer>)" + QString::number(rs.minute) + R"(</integer>
  </dict>
  <key>RunAtLoad</key>
  <false/>
</dict>
</plist>
)";
        QFile f(plistPath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text))
            QTextStream(&f) << plist;
#else
        // Write a user crontab entry (systemd-user timer is preferred when available
        // but for simplicity use crontab analogous to ScheduleManager::createCronEntry).
        QString appPath = QCoreApplication::applicationFilePath();
        QString marker  = "# nexis-report-" + rs.id;
        QString entry   = QString("%1 %2 * * * %3 --report %4")
            .arg(rs.minute).arg(rs.hour)
            .arg(appPath, rs.id);

        // Read existing crontab, remove old entry for this id, append new one.
        QProcess proc;
        proc.start("crontab", {"-l"});
        proc.waitForFinished(3000);
        QString crontab = QString::fromUtf8(proc.readAllStandardOutput());
        QStringList lines = crontab.split('\n');
        QStringList out;
        bool skip = false;
        for (const QString &line : lines) {
            if (line.trimmed() == marker) { skip = true; continue; }
            if (skip && !line.startsWith('#')) { skip = false; }
            if (!skip) out << line;
        }
        // Trim trailing blank lines then add new entry
        while (!out.isEmpty() && out.last().trimmed().isEmpty()) out.removeLast();
        out << marker << entry << "";

        QProcess write;
        write.start("crontab", {"-"});
        write.waitForStarted(2000);
        write.write(out.join('\n').toUtf8());
        write.closeWriteChannel();
        write.waitForFinished(3000);
#endif
    }
}

// ── Headless entry-point ─────────────────────────────────────────────────────

QString HealthReportManager::runScheduledReport(const QString &scheduleId)
{
    loadReportSchedules();

    ReportSchedule rs = getReportSchedule(scheduleId);
    if (rs.id.isEmpty()) {
        qWarning() << "HealthReportManager: unknown schedule id" << scheduleId;
        return {};
    }

    QString path = generateReport(rs.outputDir);

    if (!path.isEmpty()) {
        rs.lastRun = QDateTime::currentDateTime();
        updateReportSchedule(rs);
    }
    return path;
}
