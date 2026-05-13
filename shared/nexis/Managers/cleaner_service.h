#ifndef CLEANER_SERVICE_H
#define CLEANER_SERVICE_H

#include <QObject>
#include <QMap>
#include <QFileInfoList>
#include <QDateTime>

class CleanerService : public QObject
{
    Q_OBJECT

public:
    struct ExclusionEntry {
        enum Type { File, Folder };
        Type type = File;
        QString path;
    };

    enum CleanCategory {
        PACKAGE_CACHE,
        CRASH_REPORTS,
        APPLICATION_LOGS,
        APPLICATION_CACHES,
        TRASH,
        DEV_TOOL_CACHES,
        BROKEN_SYMLINKS,
        BROWSER_PRIVACY,
        SNAP_FLATPAK_REVISIONS,
        DOWNLOADS_AGED       // FR-113: aged files in the user's Downloads folder
    };

    struct ScanResult {
        QMap<CleanCategory, QFileInfoList> categoryFiles;
        quint64 totalSize = 0;
    };

    struct CleanResult {
        quint64 totalBytesFreed = 0;
        int totalFilesRemoved = 0;
        QMap<CleanCategory, quint64> categoryBreakdown;
        QDateTime timestamp;
        QString scheduleName;
    };

    static CleanerService *ins();

    ScanResult scan(const QList<CleanCategory> &categories);
    CleanResult clean(const QList<CleanCategory> &categories, int minFileAgeSecs = 0);
    CleanResult cleanSchedule(const QString &scheduleId);

    static QString categoryName(CleanCategory cat);
    static QList<CleanCategory> allCategories();

    // FR-114: rolling history of (scan timestamp, size in bytes) per category.
    // Up to 20 samples kept per category in a JSON blob on disk. Written
    // automatically at the end of scan(); consumers read via the getter.
    struct TrendPoint { qint64 timestampSecs = 0; quint64 bytes = 0; };
    QList<TrendPoint> getCategoryTrend(CleanCategory cat) const;

    quint64 cleanTrash();
    quint64 cleanFiles(const QStringList &paths, int minFileAgeSecs = 0,
                       bool moveToTrashInstead = false);

    // FR-112: take a Timeshift / APFS snapshot before a clean if the user
    // has opted in via SettingKeys::PreCleanSnapshotEnabled. Silent no-op
    // when disabled or when the platform tool is unavailable. Always
    // returns (never throws) so the clean itself is never blocked.
    void maybeTakeSnapshot(const QList<CleanCategory> &categories);

    QList<ExclusionEntry> loadExclusions();
    void saveExclusions(const QList<ExclusionEntry> &entries);
    void addExclusion(ExclusionEntry::Type type, const QString &path);
    void removeExclusion(const QString &path);
    static bool isExcluded(const QString &filePath, const QList<ExclusionEntry> &exclusions);

signals:
    void cleaningStarted(QString scheduleName);
    void cleaningFinished(CleanResult result);
    void snapshotTaken(QString toolName);   // emitted on worker thread after a successful pre-clean snapshot

private:
    CleanerService();
    static CleanerService *instance;

    void logCleanResult(const CleanResult &result);
    void persistScanTotals(const ScanResult &result);   // FR-114
};

#endif // CLEANER_SERVICE_H
