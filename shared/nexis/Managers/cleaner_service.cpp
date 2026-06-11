#include "cleaner_service.h"
#include "setting_manager.h"
#include <Managers/info_manager.h>
#include <Managers/tool_manager.h>
#include <Services/snapshot_service.h>
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
        case SNAP_FLATPAK_REVISIONS: return QObject::tr("Snap/Flatpak Revisions");
        case DOWNLOADS_AGED:    return QObject::tr("Old Downloads");
    }
    return QString();
}

QList<CleanerService::CleanCategory> CleanerService::allCategories()
{
    return { PACKAGE_CACHE, CRASH_REPORTS, APPLICATION_LOGS,
             APPLICATION_CACHES, TRASH, DEV_TOOL_CACHES, BROKEN_SYMLINKS,
             BROWSER_PRIVACY, SNAP_FLATPAK_REVISIONS, DOWNLOADS_AGED };
}

QList<CleanerService::ExclusionEntry> CleanerService::loadExclusions()
{
    QList<ExclusionEntry> entries;
    QString json = SettingManager::ins()->getCleanerExclusions();
    QJsonArray arr = QJsonDocument::fromJson(json.toUtf8()).array();
    for (const QJsonValue &v : arr) {
        QJsonObject obj = v.toObject();
        ExclusionEntry e;
        e.type = (obj["type"].toString() == "folder") ? ExclusionEntry::Folder : ExclusionEntry::File;
        e.path = obj["path"].toString();
        if (!e.path.isEmpty())
            entries.append(e);
    }
    return entries;
}

void CleanerService::saveExclusions(const QList<ExclusionEntry> &entries)
{
    QJsonArray arr;
    for (const ExclusionEntry &e : entries) {
        QJsonObject obj;
        obj["type"] = (e.type == ExclusionEntry::Folder) ? "folder" : "file";
        obj["path"] = e.path;
        arr.append(obj);
    }
    SettingManager::ins()->setCleanerExclusions(
        QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

void CleanerService::addExclusion(ExclusionEntry::Type type, const QString &path)
{
    QList<ExclusionEntry> entries = loadExclusions();
    for (const ExclusionEntry &e : entries) {
        if (e.path == path)
            return;
    }
    ExclusionEntry entry;
    entry.type = type;
    entry.path = path;
    entries.append(entry);
    saveExclusions(entries);
}

void CleanerService::removeExclusion(const QString &path)
{
    QList<ExclusionEntry> entries = loadExclusions();
    entries.erase(std::remove_if(entries.begin(), entries.end(),
        [&path](const ExclusionEntry &e) { return e.path == path; }),
        entries.end());
    saveExclusions(entries);
}

bool CleanerService::isExcluded(const QString &filePath, const QList<ExclusionEntry> &exclusions)
{
    QFileInfo inputInfo(filePath);
    QString canonicalInput = inputInfo.isSymLink() ? inputInfo.canonicalFilePath() : QString();

    for (const ExclusionEntry &e : exclusions) {
        QString ePath = e.path;
        QString canonicalExcl = QFileInfo(ePath).canonicalFilePath();

        if (e.type == ExclusionEntry::File) {
            if (filePath == ePath)
                return true;
            if (!canonicalExcl.isEmpty() && !canonicalInput.isEmpty() &&
                canonicalInput == canonicalExcl)
                return true;
        } else {
            if (filePath == ePath || filePath.startsWith(ePath + '/'))
                return true;
            if (!canonicalExcl.isEmpty()) {
                if (!canonicalInput.isEmpty()) {
                    if (canonicalInput == canonicalExcl ||
                        canonicalInput.startsWith(canonicalExcl + '/'))
                        return true;
                }
                if (filePath == canonicalExcl || filePath.startsWith(canonicalExcl + '/'))
                    return true;
            }
        }
    }
    return false;
}

CleanerService::ScanResult CleanerService::scan(const QList<CleanCategory> &categories)
{
    ScanResult result;
    InfoManager *im = InfoManager::ins();
    ToolManager *tmr = ToolManager::ins();
    QList<ExclusionEntry> exclusions = loadExclusions();

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
            case SNAP_FLATPAK_REVISIONS: {
                // Stale snap revisions are real files — convert to QFileInfoList
                QList<StaleSnapRevision> snapRevs = tmr->getStaleSnapRevisions();
                for (const StaleSnapRevision &rev : snapRevs) {
                    QFileInfo fi(rev.filePath);
                    if (fi.exists())
                        files.append(fi);
                }
                // Unused flatpak runtimes — attempt to find their install directories
                QStringList flatpakRefs = tmr->getUnusedFlatpakRuntimes();
                for (const QString &ref : flatpakRefs) {
                    // Flatpak installs runtimes under /var/lib/flatpak/runtime/<ref-parts>
                    // Use the ref as a display-only entry with its first component as dir
                    QString runtimeDir = "/var/lib/flatpak/runtime/" + ref.section('/', 0, 0);
                    QFileInfo fi(runtimeDir);
                    if (fi.exists())
                        files.append(fi);
                }
                break;
            }
            case TRASH: {
#ifdef Q_OS_MACOS
                QString trashPath = QDir::homePath() + "/.Trash/";
#else
                QString trashPath = QDir::homePath() + "/.local/share/Trash/";
#endif
                files = { QFileInfo(trashPath) };
                break;
            }
            case DOWNLOADS_AGED: {
                // FR-113: walk the user's configured Downloads folder and
                // keep only entries older than N days. Respects the same
                // CleanerExclusions filter below.
                SettingManager *sm = SettingManager::ins();
                if (!sm->getDownloadsAutoCleanEnabled())
                    break;
                const QString dir = sm->getDownloadsAutoCleanPath();
                const int days = sm->getDownloadsAutoCleanDays();
                if (dir.isEmpty() || days <= 0)
                    break;
                const QDateTime cutoff = QDateTime::currentDateTime().addDays(-days);
                const QFileInfoList entries = QDir(dir).entryInfoList(
                    QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);
                for (const QFileInfo &fi : entries) {
                    if (fi.lastModified() < cutoff)
                        files.append(fi);
                }
                break;
            }
        }

        if (!exclusions.isEmpty() && cat != TRASH) {
            QFileInfoList filtered;
            for (const QFileInfo &fi : files) {
                if (!isExcluded(fi.absoluteFilePath(), exclusions))
                    filtered.append(fi);
            }
            files = filtered;
        }

        result.categoryFiles[cat] = files;

        for (const QFileInfo &fi : files) {
            result.totalSize += FileUtil::getFileSize(fi.absoluteFilePath());
        }
    }

    // FR-114: record per-category sizes for the trend sparkline on the
    // System Cleaner page. Done once per scan.
    persistScanTotals(result);

    return result;
}

void CleanerService::persistScanTotals(const ScanResult &result)
{
    constexpr int MAX_SAMPLES = 20;
    SettingManager *sm = SettingManager::ins();
    QJsonObject root = QJsonDocument::fromJson(
        sm->getCleanerCategoryTrends().toUtf8()).object();

    const qint64 now = QDateTime::currentSecsSinceEpoch();

    for (auto it = result.categoryFiles.constBegin();
         it != result.categoryFiles.constEnd(); ++it) {
        const CleanCategory cat = it.key();

        // Sum bytes for this category.
        quint64 catBytes = 0;
        for (const QFileInfo &fi : it.value())
            catBytes += FileUtil::getFileSize(fi.absoluteFilePath());

        const QString key = QString::number(static_cast<int>(cat));
        QJsonArray samples = root.value(key).toArray();
        QJsonObject point;
        point.insert("t", QJsonValue(now));
        point.insert("b", QJsonValue(static_cast<qint64>(catBytes)));
        samples.append(point);

        while (samples.size() > MAX_SAMPLES)
            samples.removeFirst();

        root.insert(key, samples);
    }

    sm->setCleanerCategoryTrends(
        QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));
}

QList<CleanerService::TrendPoint> CleanerService::getCategoryTrend(CleanCategory cat) const
{
    QList<TrendPoint> out;
    SettingManager *sm = SettingManager::ins();
    const QJsonObject root = QJsonDocument::fromJson(
        sm->getCleanerCategoryTrends().toUtf8()).object();
    const QJsonArray samples = root.value(QString::number(static_cast<int>(cat))).toArray();
    out.reserve(samples.size());
    for (const QJsonValue &v : samples) {
        const QJsonObject obj = v.toObject();
        TrendPoint p;
        p.timestampSecs = obj.value("t").toVariant().toLongLong();
        p.bytes = static_cast<quint64>(obj.value("b").toVariant().toLongLong());
        out.append(p);
    }
    return out;
}

CleanerService::CleanResult CleanerService::clean(const QList<CleanCategory> &categories, int minFileAgeSecs)
{
    CleanResult result;
    result.timestamp = QDateTime::currentDateTime();
    ToolManager *tmr = ToolManager::ins();

    ScanResult scanResult = scan(categories);

    // FR-112: take a snapshot before we start deleting, if the user opted in.
    maybeTakeSnapshot(categories);

    for (CleanCategory cat : categories) {
        quint64 catBytes = 0;

        if (cat == TRASH) {
            catBytes = cleanTrash();
        } else if (cat == SNAP_FLATPAK_REVISIONS) {
            // Special-case: use package manager commands, not file deletion
            for (const QFileInfo &fi : scanResult.categoryFiles[cat])
                catBytes += FileUtil::getFileSize(fi.absoluteFilePath());
            tmr->removeStaleSnapRevisions(tmr->getStaleSnapRevisions());
            tmr->removeUnusedFlatpakRuntimes();
        } else {
            QStringList paths;
            for (const QFileInfo &fi : scanResult.categoryFiles[cat]) {
                paths << fi.absoluteFilePath();
            }
            // FR-113: DOWNLOADS_AGED moves files to Trash rather than rm -rf
            // so the user can recover them if they were wrong about the age
            // threshold.
            const bool moveToTrashInstead = (cat == DOWNLOADS_AGED);
            catBytes = cleanFiles(paths, minFileAgeSecs, moveToTrashInstead);
        }

        result.categoryBreakdown[cat] = catBytes;
        result.totalBytesFreed += catBytes;
    }

    return result;
}

QString CleanerService::trashRoot() const
{
#ifdef Q_OS_MACOS
    return QDir::homePath() + "/.Trash";
#else
    return QDir::homePath() + "/.local/share/Trash";
#endif
}

quint64 CleanerService::cleanTrash()
{
    const QString trashPath = trashRoot();
    const quint64 sizeBefore = FileUtil::getFileSize(trashPath);

    auto emptyDir = [](const QString &dirPath) {
        QDir dir(dirPath);
        if (!dir.exists())
            return;
        for (const QFileInfo &entry : dir.entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot)) {
            if (entry.isSymLink()) {
                QFile::remove(entry.absoluteFilePath());
            } else if (entry.isDir()) {
                QDir(entry.absoluteFilePath()).removeRecursively();
            } else {
                QFile::remove(entry.absoluteFilePath());
            }
        }
    };

#ifdef Q_OS_MACOS
    emptyDir(trashPath);
#else
    emptyDir(trashPath + "/files");
    emptyDir(trashPath + "/info");
#endif

    return sizeBefore;
}

quint64 CleanerService::cleanFiles(const QStringList &paths, int minFileAgeSecs,
                                     bool moveToTrashInstead)
{
    quint64 totalFreed = 0;
    QDateTime cutoff;
    if (minFileAgeSecs > 0) {
        cutoff = QDateTime::currentDateTime().addSecs(-minFileAgeSecs);
    }

    // WI-08 / audit H9: load exclusions once and re-check at every depth so
    // an excluded child inside a scanned directory survives. scan() filters
    // only the top level, which means without this guard a recursive directory
    // walk would delete excluded files that the user explicitly protected.
    const QList<ExclusionEntry> exclusions = loadExclusions();

    QStringList filesToRemove;

    for (const QString &path : paths) {
        QFileInfo fi(path);

        if (isExcluded(fi.absoluteFilePath(), exclusions))
            continue;

        // SSO-3370: for the recursive-delete path, a directory's own mtime
        // carries no useful signal — a cache dir we just wrote to looks
        // "recent" even though its aged children should still be cleaned.
        // removeDirContentsRespectingExclusions() applies the cutoff per child,
        // so always recurse into directories regardless of their own mtime.
        // The trash-wholesale path (moveToTrashInstead) keeps the original
        // top-level gate because it moves the whole tree at once.
        const bool willRecurse = fi.isDir() && !moveToTrashInstead;
        if (minFileAgeSecs > 0 && !willRecurse && fi.lastModified() > cutoff) {
            continue;
        }

        if (moveToTrashInstead) {
            // FR-113: DOWNLOADS_AGED path. QFile::moveToTrash preserves
            // the filesystem's "Put Back" metadata on both macOS
            // (NSFileManager trashItemAtURL:) and Linux (XDG trash spec).
            const quint64 size = FileUtil::getFileSize(path);
            QString trashedPath;
            if (!QFile::moveToTrash(path, &trashedPath)) {
                qWarning() << "cleanFiles: moveToTrash failed for" << path;
                continue;   // don't count bytes we didn't actually move.
            }
            totalFreed += size;
        } else if (fi.isSymLink()) {
            const quint64 size = FileUtil::getFileSize(path);
            if (QFile::remove(path))
                totalFreed += size;
        } else if (fi.isDir()) {
            totalFreed += removeDirContentsRespectingExclusions(
                path, exclusions, minFileAgeSecs, cutoff);
        } else {
            filesToRemove << path;
            totalFreed += FileUtil::getFileSize(path);
        }
    }

    if (!filesToRemove.isEmpty() && !moveToTrashInstead) {
        removeElevated(filesToRemove);
    }

    return totalFreed;
}

quint64 CleanerService::removeDirContentsRespectingExclusions(
    const QString &dirPath,
    const QList<ExclusionEntry> &exclusions,
    int minFileAgeSecs,
    const QDateTime &cutoff)
{
    quint64 freed = 0;
    QDir dir(dirPath);
    const QFileInfoList entries = dir.entryInfoList(
        QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);

    for (const QFileInfo &entry : entries) {
        const QString entryPath = entry.absoluteFilePath();

        // WI-08 fix: exclusion + age cutoff must be enforced at every depth,
        // not just on the top-level entry that scan() filtered.
        if (isExcluded(entryPath, exclusions))
            continue;
        if (minFileAgeSecs > 0 && entry.lastModified() > cutoff)
            continue;

        if (entry.isSymLink()) {
            const quint64 size = FileUtil::getFileSize(entryPath);
            if (QFile::remove(entryPath))
                freed += size;
        } else if (entry.isDir()) {
            freed += removeDirContentsRespectingExclusions(
                entryPath, exclusions, minFileAgeSecs, cutoff);
            // If the recursive walk emptied the directory, remove the now-empty
            // shell so a scanned cache directory does not linger.
            QDir sub(entryPath);
            if (sub.isEmpty(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot))
                sub.rmdir(".");
        } else {
            const quint64 size = FileUtil::getFileSize(entryPath);
            if (QFile::remove(entryPath))
                freed += size;
        }
    }

    return freed;
}

void CleanerService::removeElevated(const QStringList &paths)
{
    // `--` is required so any pathname that begins with `-` (e.g. a maliciously
    // named file `-rf`) is not parsed by rm as an additional option flag.
    QStringList args;
    args << "-rf" << "--";
    args.append(paths);
    CommandUtil::sudoExec("rm", args);
}

void CleanerService::maybeTakeSnapshot(const QList<CleanCategory> &categories)
{
    if (!SettingManager::ins()->getPreCleanSnapshotEnabled())
        return;

    SnapshotService *svc = SnapshotService::ins();
    if (!svc->isAvailable()) {
        qWarning() << "CleanerService: snapshot enabled but tool unavailable — skipping";
        return;
    }

    QStringList catNames;
    catNames.reserve(categories.size());
    for (CleanCategory cat : categories)
        catNames << categoryName(cat);

    const QString reason = QStringLiteral("Nexis pre-clean: %1").arg(catNames.join(", "));
    if (svc->takeSnapshot(reason))
        emit snapshotTaken(svc->toolDisplayName());
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
