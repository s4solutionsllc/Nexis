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
    uint64_t freq = 0;
    size_t len = sizeof(freq);
    if (sysctlbyname("hw.cpufrequency", &freq, &len, nullptr, 0) == 0) {
        this->cpuSpeed = QString::number(freq / 1e9, 'f', 2) + "GHz";
    } else {
        // Apple Silicon doesn't expose frequency via sysctl
        this->cpuSpeed = "N/A";
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

    return caches.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
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
