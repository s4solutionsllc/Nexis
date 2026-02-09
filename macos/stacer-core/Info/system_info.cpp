#include "system_info.h"

#include <QObject>
#include <QRegularExpression>
#include <iostream>

#include <sys/sysctl.h>

SystemInfo::SystemInfo()
{
    QString unknown(QObject::tr("Unknown"));

    // CPU Model via sysctl
    try {
        this->cpuModel = CommandUtil::exec("sysctl", {"-n", "machdep.cpu.brand_string"}).trimmed();
        if (this->cpuModel.isEmpty())
            this->cpuModel = unknown;
    } catch (...) {
        this->cpuModel = unknown;
    }

    // CPU Speed
    // Apple Silicon does not expose hw.cpufrequency.  We try several
    // approaches in order of preference.
    uint64_t freq = 0;
    size_t len = sizeof(freq);
    if (sysctlbyname("hw.cpufrequency", &freq, &len, nullptr, 0) == 0 && freq > 0) {
        this->cpuSpeed = QString::number(freq / 1e9, 'f', 2) + " GHz";
    } else {
        // Fallback: parse frequency from the CPU brand string
        // e.g. "Apple M1 Pro" won't have one, but Intel strings like
        // "Intel(R) Core(TM) i7-9750H CPU @ 2.60GHz" do.
        QRegularExpression freqRe("@\\s*([\\d.]+)\\s*GHz", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch m = freqRe.match(this->cpuModel);
        if (m.hasMatch()) {
            this->cpuSpeed = m.captured(1) + " GHz";
        } else {
            // Apple Silicon: report the performance-core frequency from
            // the IODeviceTree if available, otherwise try sysctl hw.tbfrequency
            // as a rough indicator (it's the timebase, not the CPU clock, but
            // better than nothing).
            // For Apple Silicon we can read from sysctl kern.clockrate or
            // use IORegistry to get P-core nominal frequency.
            try {
                QString ioreg = CommandUtil::exec("ioreg", {"-r", "-d1", "-c", "IOPlatformDevice", "-n", "pmgr"});
                // Look for "voltage-states5-sram" or "voltage-states1-sram" which
                // contain P-cluster frequency entries.  The last tuple is the max freq.
                // However this is not always available or parseable.
                // Simpler: use sysctl hw.perflevel0.logicalcpu and the performance
                // cluster name, or just report the chip name (M1/M2/M3 etc.)
                Q_UNUSED(ioreg);
            } catch (...) {}

            // Use sysctl to get the P-core frequency on Apple Silicon
            // macOS 13+ exposes hw.perflevel0.* but not the frequency directly.
            // As a practical solution, extract the chip name and use known freqs.
            // Most reliable: use powermetrics but that needs root.
            // Best effort: read from sysctl machdep.cpu.brand_string for Intel,
            // for Apple Silicon just report the chip marketing name.
            if (this->cpuModel.contains("Apple", Qt::CaseInsensitive)) {
                // Apple Silicon - report chip name as the "speed"
                // since Apple doesn't publicly expose fixed clock speeds
                // (they use dynamic frequency scaling with no user-visible base clock)
                this->cpuSpeed = this->cpuModel;  // e.g. "Apple M1 Pro"
            } else {
                this->cpuSpeed = "N/A";
            }
        }
    }

    // CPU Cores
    CpuInfo ci;
    this->cpuCore = QString::number(ci.getCpuPhysicalCoreCount());

    // Username
    QString name = qgetenv("USER");
    if (name.isEmpty()) {
        try {
            name = CommandUtil::exec("whoami").trimmed();
        } catch (...) {}
    }
    this->username = name;
}

QStringList SystemInfo::getUserList() const
{
    QStringList users;
    try {
        // macOS uses Directory Services; dscl lists all users
        QString output = CommandUtil::exec("dscl", {".", "-list", "/Users"});
        QStringList allUsers = output.trimmed().split('\n');
        for (const QString &user : allUsers) {
            QString trimmed = user.trimmed();
            // Filter out system users (those starting with _)
            if (!trimmed.isEmpty() && !trimmed.startsWith('_'))
                users.append(trimmed);
        }
    } catch (...) {}
    return users;
}

QStringList SystemInfo::getGroupList() const
{
    QStringList groups;
    try {
        QString output = CommandUtil::exec("dscl", {".", "-list", "/Groups"});
        QStringList allGroups = output.trimmed().split('\n');
        for (const QString &group : allGroups) {
            QString trimmed = group.trimmed();
            if (!trimmed.isEmpty() && !trimmed.startsWith('_'))
                groups.append(trimmed);
        }
    } catch (...) {}
    return groups;
}

QFileInfoList SystemInfo::getCrashReports() const
{
    QFileInfoList reports;

    // macOS crash reports (DiagnosticReports)
    QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QDir userReports(homePath + "/Library/Logs/DiagnosticReports");
    reports.append(userReports.entryInfoList(QDir::Files));

    QDir sysReports("/Library/Logs/DiagnosticReports");
    reports.append(sysReports.entryInfoList(QDir::Files));

    return reports;
}

QFileInfoList SystemInfo::getAppLogs() const
{
    QFileInfoList logs;

    // User logs
    QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QDir userLogs(homePath + "/Library/Logs");
    logs.append(userLogs.entryInfoList(QDir::Files | QDir::NoDotAndDotDot));

    // System logs
    QDir sysLogs("/var/log");
    logs.append(sysLogs.entryInfoList(QDir::Files | QDir::NoDotAndDotDot));

    return logs;
}

QFileInfoList SystemInfo::getAppCaches() const
{
    QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);

    // macOS caches are in ~/Library/Caches
    QDir caches(homePath + "/Library/Caches");
    QFileInfoList result = caches.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

    // Include pip cache from PIP_CACHE_DIR if set and outside ~/Library/Caches
    QString pipCacheEnv = qgetenv("PIP_CACHE_DIR");
    if (!pipCacheEnv.isEmpty()) {
        QFileInfo pipInfo(pipCacheEnv);
        if (pipInfo.exists() && !pipInfo.absoluteFilePath().startsWith(caches.absolutePath()))
            result.append(pipInfo);
    }

    return result;
}

// --- Cross-platform methods ---

QString SystemInfo::getUsername() const
{
    return username;
}

QString SystemInfo::getHostname() const
{
    return QSysInfo::machineHostName();
}

QString SystemInfo::getPlatform() const
{
    return QString("%1 %2")
            .arg(QSysInfo::kernelType())
            .arg(QSysInfo::currentCpuArchitecture());
}

QString SystemInfo::getDistribution() const
{
    return QSysInfo::prettyProductName();
}

QString SystemInfo::getKernel() const
{
    return QSysInfo::kernelVersion();
}

QString SystemInfo::getCpuModel() const
{
    return this->cpuModel;
}

QString SystemInfo::getCpuSpeed() const
{
    return this->cpuSpeed;
}

QString SystemInfo::getCpuCore() const
{
    return this->cpuCore;
}
