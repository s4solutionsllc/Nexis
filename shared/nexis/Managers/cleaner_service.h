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

protected:
    CleanerService();
    virtual ~CleanerService() = default;

    // WI-08 / audit H9: test seam for the elevated `rm -rf` branch. The
    // production implementation pipes through `sudoExec("rm", "-rf", "--", paths)`;
    // tests override to record the call and avoid touching the real filesystem
    // with elevated privileges. The `--` end-of-options guard prevents any path
    // beginning with `-` from being interpreted as an rm option.
    virtual void removeElevated(const QStringList &paths);

    // WI-08: test seam for cleanTrash(). Production returns the platform's
    // user Trash directory; tests override to point at a QTemporaryDir.
    virtual QString trashRoot() const;

    // SSO-3399 / SSO-3704: test seam for the user-vs-root ownership split in
    // cleanFiles(). Production compares ownerId() to geteuid(); tests override
    // to force paths through the elevated branch (or keep them in the user
    // branch) without needing root or specific filesystem ownership.
    virtual bool currentUserOwns(const QString &path) const;

private:
    static CleanerService *instance;

    void logCleanResult(const CleanResult &result);
    void persistScanTotals(const ScanResult &result);   // FR-114

    // WI-08: walk `dirPath` and remove entries that pass the exclusion + age
    // predicate at every depth. Returns total bytes freed (entries that were
    // actually deleted; excluded or kept entries are not counted).
    quint64 removeDirContentsRespectingExclusions(
        const QString &dirPath,
        const QList<ExclusionEntry> &exclusions,
        int minFileAgeSecs,
        const QDateTime &cutoff);
};

#endif // CLEANER_SERVICE_H
