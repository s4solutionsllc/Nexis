#include "cleaner_service.h"
#include "setting_manager.h"
#include <Managers/info_manager.h>
#include <Managers/tool_manager.h>
#include <Utils/command_util.h>
#include <Utils/file_util.h>
#include <Utils/format_util.h>
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTextStream>
#include <QDebug>

CleanerService *CleanerService::instance = nullptr;

CleanerService::CleanerService() : QObject(nullptr)
{
}

CleanerService *CleanerService::ins()
{
    if (!instance) {
        instance = new CleanerService;
    }
    return instance;
}

QString CleanerService::categoryName(CleanCategory cat)
{
    switch (cat) {
        case PACKAGE_CACHE:     return QObject::tr("Package Caches");
        case CRASH_REPORTS:     return QObject::tr("Crash Reports");
        case APPLICATION_LOGS:  return QObject::tr("Application Logs");
        case APPLICATION_CACHES:return QObject::tr("Application Caches");
        case TRASH:             return QObject::tr("Trash");
        case DEV_TOOL_CACHES:   return QObject::tr("Dev Tool Caches");
        case BROKEN_SYMLINKS:   return QObject::tr("Broken Symlinks");
        case BROWSER_PRIVACY:   return QObject::tr("Browser Privacy");
    }
    return QString();
}

QList<CleanerService::CleanCategory> CleanerService::allCategories()
{
    return { PACKAGE_CACHE, CRASH_REPORTS, APPLICATION_LOGS,
             APPLICATION_CACHES, TRASH, DEV_TOOL_CACHES, BROKEN_SYMLINKS,
             BROWSER_PRIVACY };
}

CleanerService::ScanResult CleanerService::scan(const QList<CleanCategory> &categories)
{
    ScanResult result;
    InfoManager *im = InfoManager::ins();
    ToolManager *tmr = ToolManager::ins();

    for (CleanCategory cat : categories) {
        QFileInfoList files;
        switch (cat) {
            case PACKAGE_CACHE:
                files = tmr->getPackageCaches();
                break;
            case CRASH_REPORTS:
                files = im->getCrashReports();
                break;
            case APPLICATION_LOGS:
                files = im->getAppLogs();
                break;
            case APPLICATION_CACHES:
                files = im->getAppCaches();
                break;
            case DEV_TOOL_CACHES:
                files = im->getDevToolCaches();
                break;
            case BROKEN_SYMLINKS:
                files = im->getBrokenSymlinks();
                break;
            case BROWSER_PRIVACY:
                files = im->getBrowserPrivacyArtifacts();
                break;
            case TRASH: {
#ifdef Q_OS_MACOS
                QString trashPath = QDir::homePath() + "/.Trash/";
#else
                QString trashPath = QDir::homePath() + "/.local/share/Trash/";
#endif
                files = { QFileInfo(trashPath) };
                break;
            }
        }
        result.categoryFiles[cat] = files;

        for (const QFileInfo &fi : files) {
            result.totalSize += FileUtil::getFileSize(fi.absoluteFilePath());
        }
    }

    return result;
}

CleanerService::CleanResult CleanerService::clean(const QList<CleanCategory> &categories, int minFileAgeSecs)
{
    CleanResult result;
    result.timestamp = QDateTime::currentDateTime();

    ScanResult scanResult = scan(categories);

    for (CleanCategory cat : categories) {
        quint64 catBytes = 0;

        if (cat == TRASH) {
            catBytes = cleanTrash();
        } else {
            QStringList paths;
            for (const QFileInfo &fi : scanResult.categoryFiles[cat]) {
                paths << fi.absoluteFilePath();
            }
            catBytes = cleanFiles(paths, minFileAgeSecs);
        }

        result.categoryBreakdown[cat] = catBytes;
        result.totalBytesFreed += catBytes;
    }

    return result;
}

quint64 CleanerService::cleanTrash()
{
#ifdef Q_OS_MACOS
    QString trashPath = QDir::homePath() + "/.Trash";
#else
    QString trashPath = QDir::homePath() + "/.local/share/Trash";
#endif

    quint64 sizeBefore = FileUtil::getFileSize(trashPath);

#ifdef Q_OS_MACOS
    QDir trashDir(trashPath);
    for (const QFileInfo &entry : trashDir.entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot)) {
        if (entry.isDir()) {
            QDir(entry.absoluteFilePath()).removeRecursively();
        } else {
            QFile::remove(entry.absoluteFilePath());
        }
    }
#else
    QDir(trashPath + "/files").removeRecursively();
    QDir(trashPath + "/info").removeRecursively();
#endif

    return sizeBefore;
}

quint64 CleanerService::cleanFiles(const QStringList &paths, int minFileAgeSecs)
{
    quint64 totalFreed = 0;
    QDateTime cutoff;
    if (minFileAgeSecs > 0) {
        cutoff = QDateTime::currentDateTime().addSecs(-minFileAgeSecs);
    }

    QStringList filesToRemove;

    for (const QString &path : paths) {
        QFileInfo fi(path);

        if (minFileAgeSecs > 0 && fi.lastModified() > cutoff) {
            continue;
        }

        quint64 size = FileUtil::getFileSize(path);

        if (fi.isDir()) {
            QDir dir(path);
            for (const QFileInfo &entry : dir.entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot)) {
                if (minFileAgeSecs > 0 && entry.lastModified() > cutoff) {
                    continue;
                }
                if (entry.isDir()) {
                    QDir(entry.absoluteFilePath()).removeRecursively();
                } else {
                    QFile::remove(entry.absoluteFilePath());
                }
            }
        } else {
            filesToRemove << path;
        }

        totalFreed += size;
    }

    if (!filesToRemove.isEmpty()) {
        CommandUtil::sudoExec("rm", QStringList() << "-rf" << filesToRemove);
    }

    return totalFreed;
}

CleanerService::CleanResult CleanerService::cleanSchedule(const QString &scheduleId)
{
    SettingManager *sm = SettingManager::ins();
    QJsonArray schedules = QJsonDocument::fromJson(sm->getSchedules().toUtf8()).array();

    QJsonObject found;
    int foundIdx = -1;
    for (int i = 0; i < schedules.size(); ++i) {
        QJsonObject obj = schedules[i].toObject();
        if (obj["id"].toString() == scheduleId) {
            found = obj;
            foundIdx = i;
            break;
        }
    }

    if (foundIdx == -1) {
        qWarning() << "CleanerService: schedule not found:" << scheduleId;
        return CleanResult();
    }

    QList<CleanCategory> categories;
    QJsonArray cats = found["categories"].toArray();
    for (const QJsonValue &v : cats) {
        categories.append(static_cast<CleanCategory>(v.toInt()));
    }

    int minAge = found.value("minFileAgeSecs").toInt(86400);
    QString scheduleName = found["name"].toString();

    bool dryRunCompleted = found.value("dryRunCompleted").toBool(false);

    if (!dryRunCompleted) {
        ScanResult scanResult = scan(categories);

        found["dryRunCompleted"] = true;
        found["lastRun"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        schedules[foundIdx] = found;
        sm->setSchedules(QString(QJsonDocument(schedules).toJson(QJsonDocument::Compact)));

        CleanResult dryResult;
        dryResult.timestamp = QDateTime::currentDateTime();
        dryResult.scheduleName = scheduleName + " (dry run)";
        dryResult.totalBytesFreed = scanResult.totalSize;
        logCleanResult(dryResult);
        return dryResult;
    }

    emit cleaningStarted(scheduleName);

    CleanResult result = clean(categories, minAge);
    result.scheduleName = scheduleName;

    found["lastRun"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    found["lastBytesFreed"] = static_cast<qint64>(result.totalBytesFreed);
    schedules[foundIdx] = found;
    sm->setSchedules(QString(QJsonDocument(schedules).toJson(QJsonDocument::Compact)));

    logCleanResult(result);
    emit cleaningFinished(result);

    return result;
}

void CleanerService::logCleanResult(const CleanResult &result)
{
    QString logPath = SettingManager::ins()->getConfigPath() + "/clean_history.log";
    QFile logFile(logPath);
    if (!logFile.open(QIODevice::Append | QIODevice::Text))
        return;

    QTextStream stream(&logFile);
    QString ts = result.timestamp.toString("yyyy-MM-dd HH:mm:ss");

    QStringList catNames;
    for (auto it = result.categoryBreakdown.constBegin(); it != result.categoryBreakdown.constEnd(); ++it) {
        catNames << categoryName(it.key());
    }

    stream << QString("[%1] Schedule: %2 | Categories: %3 | Cleaned: %4 (%5 files)\n")
              .arg(ts, result.scheduleName, catNames.join(", "),
                   FormatUtil::formatBytes(result.totalBytesFreed),
                   QString::number(result.totalFilesRemoved));
}
