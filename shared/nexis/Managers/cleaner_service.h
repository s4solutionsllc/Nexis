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
        DOWNLOADS_AGED,      // FR-113: aged files in the user's Downloads folder
        APP_PROFILES         // FW-12: paths discovered via data-driven cleaning profiles
    };

    // SSO-3732 / FW-05: outcome of an individual deletion. Lets callers tell a
    // policy-level denial (macOS 27 cross-team app-container access blocked by
    // TCC / Privacy & Security) apart from a generic I/O failure so the UI can
    // tell the user how to grant Full Disk Access instead of silently skipping.
    enum class FileRemoval {
        Removed,                 // file is gone
        NotRemoved,              // generic I/O failure
        AccessDeniedByPolicy,    // platform sandbox/permission policy refused the op
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
        // SSO-3732 / FW-05: count of deletions refused by a platform sandbox
        // policy (macOS 27 cross-team app-container TCC denial) during this
        // clean. Non-zero means the user needs to grant Full Disk Access for
        // the cleaner to make further progress on those paths.
        int accessDeniedPaths = 0;
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

    // SSO-3732 / FW-05: number of deletions refused by a sandbox policy in the
    // most recent cleanFiles() call. Reset on entry to cleanFiles(). Read by
    // clean() to populate CleanResult::accessDeniedPaths and by tests via the
    // accessNeededDetected() signal or this getter.
    int lastAccessDeniedCount() const { return mLastAccessDeniedCount; }

    // SSO-3732 / FW-05: deep link the UI passes to QDesktopServices::openUrl()
    // to drop the user directly into the Privacy & Security pane where Full
    // Disk Access is granted. Returns an empty string on non-macOS platforms.
    static QString accessNeededDeepLink();

    // SSO-3732 / FW-05: translatable user-facing message paired with the
    // deep link above. Same wording for the UI banner and for any other
    // surface that wants to explain why cleaning regressed.
    static QString accessNeededMessage();

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

    // GH#182: get all trash directories (home trash + mounted filesystem trash).
    // Public accessor for the protected trashRoots() test seam.
    QStringList getTrashRoots() const;

    // FW-12: when true, aggressive-class cleaning profiles are included in
    // APP_PROFILES scans. Defaults to false; persisted via SettingManager.
    bool isAggressiveProfilesEnabled() const;
    void setAggressiveProfilesEnabled(bool enabled);

signals:
    void cleaningStarted(QString scheduleName);
    void cleaningFinished(CleanResult result);
    void snapshotTaken(QString toolName);   // emitted on worker thread after a successful pre-clean snapshot

    // SSO-3732 / FW-05: emitted when scanning or cleaning detects that a
    // platform policy refused access — concretely, macOS 27 denying
    // cross-team app-container reads/deletes without Full Disk Access.
    // The cleaner page surfaces this as a persistent banner with a button
    // that opens the deep link.
    void accessNeededDetected(QString message, QString deepLink);

protected:
    CleanerService();
    virtual ~CleanerService() = default;

    // WI-08 / audit H9: test seam for the elevated `rm -rf` branch. The
    // production implementation pipes through `sudoExec("rm", "-rf", "--", paths)`;
    // tests override to record the call and avoid touching the real filesystem
    // with elevated privileges. The `--` end-of-options guard prevents any path
    // beginning with `-` from being interpreted as an rm option.
    virtual void removeElevated(const QStringList &paths);

    // WI-08 / GH#182: test seam for cleanTrash() and scan(). Production
    // returns all trash directories the current user has items in: the home
    // trash plus, on Linux, any per-volume .Trash-$UID and .Trash/$UID
    // directories found on mounted filesystems (FreeDesktop Trash spec §1.2).
    // Tests override to return a controlled list of QTemporaryDir paths.
    virtual QStringList trashRoots() const;

    // SSO-3399 / SSO-3704: test seam for the user-vs-root ownership split in
    // cleanFiles(). Production compares ownerId() to geteuid(); tests override
    // to force paths through the elevated branch (or keep them in the user
    // branch) without needing root or specific filesystem ownership.
    virtual bool currentUserOwns(const QString &path) const;

    // SSO-3732 / FW-05: test seam for individual file/symlink removal in the
    // user-branch and the recursive directory walk. Production uses
    // QFile::remove() and maps an EPERM / EACCES error to
    // FileRemoval::AccessDeniedByPolicy so the cleaner can tally "access
    // needed" rather than silently skipping. Tests override to inject the
    // outcome without depending on the host's TCC layer.
    virtual FileRemoval removeFile(const QString &path);

    // SSO-3732 / FW-05: test seam for the scan-side container access probe.
    // Production checks whether `~/Library/Containers` exists but is empty
    // (the silent-denial fingerprint macOS 27 leaves when Full Disk Access
    // is withheld). Tests override to inject the outcome without touching
    // the real home directory.
    virtual bool macOSContainerAccessProbablyDenied() const;

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

    // SSO-3732: bumped by removeFile()'s AccessDeniedByPolicy return inside the
    // cleanFiles()/removeDirContentsRespectingExclusions() recursion. Reset at
    // the top of cleanFiles() so the value reflects the most recent call.
    int mLastAccessDeniedCount = 0;
};

#endif // CLEANER_SERVICE_H
