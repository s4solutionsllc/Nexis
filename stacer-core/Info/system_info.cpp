#include "system_info.h"

#include <QObject>
#include <QRegularExpression>
#include <iostream>

#ifdef Q_OS_LINUX

SystemInfo::SystemInfo()
{
    QString unknown(QObject::tr("Unknown"));
    QString model = nullptr;
    QString speed = nullptr;

    try{
        QStringList lines = CommandUtil::exec("bash",{"-c", LSCPU_COMMAND}).split('\n');

        QRegularExpression regexp("\\s+");
        QString space(" ");

        auto filterModel = lines.filter(QRegularExpression("^Model name"));
        QString modelLine = filterModel.isEmpty() ? "error missing model:error missing model" : filterModel.first();
        auto filterSpeed = lines.filter(QRegularExpression("^CPU max MHz"));
        QString speedLine = "error:0.0";
        if (filterSpeed.isEmpty())
        {
            // fallback to CPU MHz
            filterSpeed = lines.filter(QRegularExpression("^CPU MHz"));
        }
        if (!filterSpeed.isEmpty())
            speedLine = filterSpeed.first();

        model = modelLine.split(":").last();
        speed = speedLine.split(":").last();

        model = model.contains('@') ? model.split("@").first() : model; // intel : AMD
        speed = QString::number(speed.toDouble()/1000.0) + "GHz";

        this->cpuModel = model.trimmed().replace(regexp, space);
        this->cpuSpeed = speed.trimmed().replace(regexp, space);
    } catch(QString &ex) {
        this->cpuModel = unknown;
        this->cpuSpeed = unknown;
    }

    CpuInfo ci;
    this->cpuCore = QString::number(ci.getCpuPhysicalCoreCount());

    // get username
    QString name = qgetenv("USER");

    if (name.isEmpty())
        name = qgetenv("USERNAME");

    try {
        if (name.isEmpty())
            name = CommandUtil::exec("whoami").trimmed();
    } catch (const QString &ex) {
        qCritical() << ex;
    }

   this->username = name;
}

QStringList SystemInfo::getUserList() const
{
    QStringList passwdUsers = FileUtil::readListFromFile("/etc/passwd");
    QStringList users;

    for(QString &row: passwdUsers) {
        users.append(row.split(":").at(0));
    }

    return users;
}

QStringList SystemInfo::getGroupList() const
{
    QStringList groupFile = FileUtil::readListFromFile("/etc/group");
    QStringList groups;

    for(QString &row: groupFile) {
        groups.append(row.split(":").at(0));
    }

    return groups;
}

QFileInfoList SystemInfo::getCrashReports() const
{
    QDir reports("/var/crash");

    return reports.entryInfoList(QDir::Files);
}

QFileInfoList SystemInfo::getAppLogs() const
{
    QDir logs("/var/log");

    return logs.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
}

QFileInfoList SystemInfo::getAppCaches() const
{
    QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);

    QDir caches(homePath + "/.cache");

    return caches.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
}

#elif defined(Q_OS_MACOS)

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

#endif

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
