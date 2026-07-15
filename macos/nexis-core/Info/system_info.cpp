#include "system_info_macos.h"
#include "cpu_info_macos.h"

#include <QDebug>
#include <QObject>
#include <QRegularExpression>
#include <iostream>

#include <sys/sysctl.h>

SystemInfoMacOS::SystemInfoMacOS()
{
    QString unknown(QObject::tr("Unknown"));

    // CPU Model via sysctl
    ExecResult brandResult = CommandUtil::execWithStatus("sysctl", {"-n", "machdep.cpu.brand_string"});
    this->cpuModel = brandResult.output.trimmed();
    if (this->cpuModel.isEmpty()) {
        if (!brandResult.ok())
            qWarning() << "system_info: sysctl machdep.cpu.brand_string failed:" << brandResult.error;
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
    CpuInfoMacOS ci;
    this->cpuCore = QString::number(ci.getCpuPhysicalCoreCount());

    // Username
    QString name = qgetenv("USER");
    if (name.isEmpty()) {
        ExecResult whoamiResult = CommandUtil::execWithStatus("whoami");
        if (whoamiResult.ok())
            name = whoamiResult.output.trimmed();
        else
            qWarning() << "system_info: whoami failed:" << whoamiResult.error;
    }
    this->username = name;
}

QStringList SystemInfoMacOS::getUserList() const
{
    QStringList users;
    // macOS uses Directory Services; dscl lists all users
    ExecResult result = CommandUtil::execWithStatus("dscl", {".", "-list", "/Users"});
    if (!result.ok()) {
        qWarning() << "system_info: dscl -list /Users failed:" << result.error;
        return users;
    }
    QStringList allUsers = result.output.trimmed().split('\n');
    for (const QString &user : allUsers) {
        QString trimmed = user.trimmed();
        // Filter out system users (those starting with _)
        if (!trimmed.isEmpty() && !trimmed.startsWith('_'))
            users.append(trimmed);
    }
    return users;
}

QStringList SystemInfoMacOS::getGroupList() const
{
    QStringList groups;
    ExecResult result = CommandUtil::execWithStatus("dscl", {".", "-list", "/Groups"});
    if (!result.ok()) {
        qWarning() << "system_info: dscl -list /Groups failed:" << result.error;
        return groups;
    }
    QStringList allGroups = result.output.trimmed().split('\n');
    for (const QString &group : allGroups) {
        QString trimmed = group.trimmed();
        if (!trimmed.isEmpty() && !trimmed.startsWith('_'))
            groups.append(trimmed);
    }
    return groups;
}

QFileInfoList SystemInfoMacOS::getCrashReports() const
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

QFileInfoList SystemInfoMacOS::getAppLogs() const
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

QFileInfoList SystemInfoMacOS::getAppCaches() const
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

QFileInfoList SystemInfoMacOS::getDevToolCaches() const
{
    QFileInfoList result;
    QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);

    // Electron / Chromium app caches under ~/Library/Application Support/*/Cache
    // and ~/Library/Application Support/*/GPUCache
    QDir appSupportDir(homePath + "/Library/Application Support");
    if (appSupportDir.exists()) {
        QFileInfoList entries = appSupportDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
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

    // Well-known dev tool cache paths (same dotfile locations on macOS)
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

QFileInfoList SystemInfoMacOS::getBrokenSymlinks() const
{
    QFileInfoList result;
    QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);

    scanBrokenSymlinks(home + "/.local", result);
    scanBrokenSymlinks(home + "/bin", result);

    // Scan Homebrew prefix (varies by Intel/Apple Silicon)
    ExecResult brewResult = CommandUtil::execWithStatus("brew", {"--prefix"});
    if (brewResult.ok()) {
        QString brewPrefix = brewResult.output.trimmed();
        if (!brewPrefix.isEmpty()) {
            scanBrokenSymlinks(brewPrefix + "/bin", result);
            scanBrokenSymlinks(brewPrefix + "/lib", result);
        }
    } else {
        // Homebrew not installed — expected on many systems, not an error.
        qDebug() << "system_info: brew --prefix unavailable:" << brewResult.error;
    }

    return result;
}

QFileInfoList SystemInfoMacOS::getBrowserPrivacyArtifacts() const
{
    QFileInfoList result;
    QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);

    struct ChromiumBrowser {
        QString profileBase;
        QString cacheBase;
    };

    QList<ChromiumBrowser> chromiumBrowsers = {
        {home + "/Library/Application Support/Google/Chrome",
         home + "/Library/Caches/Google/Chrome"},
        {home + "/Library/Application Support/Microsoft Edge",
         home + "/Library/Caches/Microsoft Edge"},
        {home + "/Library/Application Support/BraveSoftware/Brave-Browser",
         home + "/Library/Caches/BraveSoftware/Brave-Browser"},
    };

    for (const auto &browser : chromiumBrowsers) {
        QDir cacheDir(browser.cacheBase);
        if (cacheDir.exists()) {
            QFileInfoList profiles = cacheDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QFileInfo &profile : profiles) {
                QFileInfo cacheData(profile.absoluteFilePath() + "/Cache/Cache_Data");
                if (cacheData.exists())
                    result.append(cacheData);
            }
        }

        QDir profileBase(browser.profileBase);
        if (profileBase.exists()) {
            QFileInfoList profiles = profileBase.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
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

    QDir ffProfilesDir(home + "/Library/Application Support/Firefox/Profiles");
    if (ffProfilesDir.exists()) {
        QFileInfoList ffProfiles = ffProfilesDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &profile : ffProfiles) {
            QFileInfo ffCache(home + "/Library/Caches/Firefox/Profiles/"
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

    QFileInfo safariCache(home + "/Library/Caches/com.apple.Safari");
    if (safariCache.exists())
        result.append(safariCache);

    QFileInfo sharedFileList(home + "/Library/Application Support/com.apple.sharedfilelist");
    if (sharedFileList.exists())
        result.append(sharedFileList);

    return result;
}

// Cross-platform getters are in shared/nexis-core/Info/system_info_shared.cpp
