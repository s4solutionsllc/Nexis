#include "system_info.h"

#include <QObject>
#include <QRegularExpression>
#include <iostream>

static constexpr const char *LSCPU_COMMAND = "LC_ALL=C lscpu";

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

        double speedMHz = speed.toDouble();

        // Fallback: sysfs cpufreq if lscpu returned 0
        if (speedMHz <= 0.0) {
            QString freqStr = FileUtil::readStringFromFile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq").trimmed();
            if (!freqStr.isEmpty())
                speedMHz = freqStr.toDouble() / 1000.0; // kHz to MHz
        }

        speed = QString::number(speedMHz / 1000.0) + "GHz";

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
    QFileInfoList result = caches.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

    // Include pip cache from PIP_CACHE_DIR if set and outside ~/.cache
    QString pipCacheEnv = qgetenv("PIP_CACHE_DIR");
    if (!pipCacheEnv.isEmpty()) {
        QFileInfo pipInfo(pipCacheEnv);
        if (pipInfo.exists() && !pipInfo.absoluteFilePath().startsWith(caches.absolutePath()))
            result.append(pipInfo);
    }

    return result;
}

QFileInfoList SystemInfo::getDevToolCaches() const
{
    QFileInfoList result;
    QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);

    // Electron / Chromium app caches under ~/.config/*/Cache and ~/.config/*/GPUCache
    QDir configDir(homePath + "/.config");
    if (configDir.exists()) {
        QFileInfoList entries = configDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &entry : entries) {
            QDir subDir(entry.absoluteFilePath());
            QFileInfo cacheDir(subDir.absoluteFilePath("Cache"));
            QFileInfo gpuCacheDir(subDir.absoluteFilePath("GPUCache"));
            if (cacheDir.exists() && cacheDir.isDir())
                result.append(cacheDir);
            if (gpuCacheDir.exists() && gpuCacheDir.isDir())
                result.append(gpuCacheDir);
        }
    }

    // Well-known dev tool cache paths
    QStringList devToolPaths = {
        homePath + "/.npm",
        homePath + "/.bun/install/cache",
        homePath + "/.gradle/caches",
        homePath + "/.m2/repository",
        homePath + "/.expo",
        homePath + "/.yarn/cache",
        homePath + "/.nuget/packages",
        homePath + "/.cargo/registry",
    };

    for (const QString &path : devToolPaths) {
        QFileInfo info(path);
        if (info.exists())
            result.append(info);
    }

    return result;
}

// Cross-platform getters are in shared/nexis-core/Info/system_info_shared.cpp
