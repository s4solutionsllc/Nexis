#include "system_info_linux.h"
#include "cpu_info_linux.h"

#include <QObject>
#include <QRegularExpression>
#include <iostream>

static constexpr const char *LSCPU_COMMAND = "LC_ALL=C lscpu";

SystemInfoLinux::SystemInfoLinux()
{
    QString unknown(QObject::tr("Unknown"));
    QString model = nullptr;
    QString speed = nullptr;

    ExecResult lscpuResult = CommandUtil::execWithStatus("bash", {"-c", LSCPU_COMMAND});
    if (lscpuResult.ok()) {
        QStringList lines = lscpuResult.output.split('\n');

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
    } else {
        qCritical() << "system_info: lscpu failed:" << lscpuResult.error;
        this->cpuModel = unknown;
        this->cpuSpeed = unknown;
    }

    CpuInfoLinux ci;
    this->cpuCore = QString::number(ci.getCpuPhysicalCoreCount());

    // get username
    QString name = qgetenv("USER");

    if (name.isEmpty())
        name = qgetenv("USERNAME");

    if (name.isEmpty()) {
        ExecResult whoamiResult = CommandUtil::execWithStatus("whoami");
        if (whoamiResult.ok())
            name = whoamiResult.output.trimmed();
        else
            qCritical() << "system_info: whoami failed:" << whoamiResult.error;
    }

   this->username = name;
}

QStringList SystemInfoLinux::getUserList() const
{
    QStringList passwdUsers = FileUtil::readListFromFile("/etc/passwd");
    QStringList users;

    for(QString &row: passwdUsers) {
        users.append(row.split(":").at(0));
    }

    return users;
}

QStringList SystemInfoLinux::getGroupList() const
{
    QStringList groupFile = FileUtil::readListFromFile("/etc/group");
    QStringList groups;

    for(QString &row: groupFile) {
        groups.append(row.split(":").at(0));
    }

    return groups;
}

QFileInfoList SystemInfoLinux::getCrashReports() const
{
    QDir reports("/var/crash");

    return reports.entryInfoList(QDir::Files);
}

QFileInfoList SystemInfoLinux::getAppLogs() const
{
    QDir logs("/var/log");

    return logs.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
}

QFileInfoList SystemInfoLinux::getAppCaches() const
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

QFileInfoList SystemInfoLinux::getDevToolCaches() const
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

static void scanBrokenSymlinks(const QString &dirPath, QFileInfoList &result)
{
    QDir dir(dirPath);
    if (!dir.exists())
        return;

    QDirIterator it(dirPath, QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QFileInfo fi = it.fileInfo();
        if (fi.isSymLink() && !fi.exists())
            result.append(fi);
    }
}

QFileInfoList SystemInfoLinux::getBrokenSymlinks() const
{
    QFileInfoList result;
    QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);

    scanBrokenSymlinks(home + "/.local", result);
    scanBrokenSymlinks(home + "/bin", result);
    scanBrokenSymlinks("/usr/local/bin", result);

    return result;
}

QFileInfoList SystemInfoLinux::getBrowserPrivacyArtifacts() const
{
    QFileInfoList result;
    QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);

    struct ChromiumBrowser {
        QString configDir;
        QString cacheDir;
    };

    QList<ChromiumBrowser> chromiumBrowsers = {
        {home + "/.config/google-chrome",
         home + "/.cache/google-chrome"},
        {home + "/.config/microsoft-edge",
         home + "/.cache/microsoft-edge"},
        {home + "/.config/BraveSoftware/Brave-Browser",
         home + "/.cache/BraveSoftware/Brave-Browser"},
    };

    for (const auto &browser : chromiumBrowsers) {
        QDir cacheDir(browser.cacheDir);
        if (cacheDir.exists()) {
            QFileInfoList profiles = cacheDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QFileInfo &profile : profiles) {
                QFileInfo cacheData(profile.absoluteFilePath() + "/Cache/Cache_Data");
                if (cacheData.exists())
                    result.append(cacheData);
            }
        }

        QDir configDir(browser.configDir);
        if (configDir.exists()) {
            QFileInfoList profiles = configDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QFileInfo &profile : profiles) {
                QString pName = profile.fileName();
                if (pName == "Default" || pName.startsWith("Profile ")) {
                    QFileInfo sessDir(profile.absoluteFilePath() + "/Sessions");
                    if (sessDir.exists())
                        result.append(sessDir);
                }
            }
        }
    }

    QDir ffProfilesDir(home + "/.mozilla/firefox");
    if (ffProfilesDir.exists()) {
        QFileInfoList ffProfiles = ffProfilesDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &profile : ffProfiles) {
            QFileInfo ffCache(home + "/.cache/mozilla/firefox/"
                              + profile.fileName() + "/cache2");
            if (ffCache.exists())
                result.append(ffCache);

            QFileInfo sessionStore(profile.absoluteFilePath() + "/sessionstore.jsonlz4");
            if (sessionStore.exists())
                result.append(sessionStore);
            QFileInfo sessionBackups(profile.absoluteFilePath() + "/sessionstore-backups");
            if (sessionBackups.exists())
                result.append(sessionBackups);
        }
    }

    QFileInfo recentlyUsed(home + "/.local/share/recently-used.xbel");
    if (recentlyUsed.exists())
        result.append(recentlyUsed);

    QDir recentDir(home + "/.local/share");
    if (recentDir.exists()) {
        QFileInfoList backups = recentDir.entryInfoList(
            QStringList() << "recently-used.xbel.*",
            QDir::Files);
        result.append(backups);
    }

    return result;
}

// Cross-platform getters are in shared/nexis-core/Info/system_info_shared.cpp
