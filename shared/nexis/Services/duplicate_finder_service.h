#ifndef DUPLICATE_FINDER_SERVICE_H
#define DUPLICATE_FINDER_SERVICE_H

#include <QObject>
#include <QFileInfo>
#include <QAtomicInt>
#include <QFuture>

#include "Managers/cleaner_service.h"

struct DuplicateGroup {
    QList<QFileInfo> files;
    quint64 fileSize = 0;
    QByteArray hash;
};

struct LargeFileEntry {
    QFileInfo info;
    quint64 size = 0;
};

class DuplicateFinderService : public QObject
{
    Q_OBJECT

public:
    static DuplicateFinderService *ins();

    void scan(const QStringList &directories, qint64 minSize,
              const QString &globFilter = QString());

    // FW-08: top-N largest files in `directories`. Emits largestScanFinished
    // on completion; honors the CleanerService exclusion engine and skips
    // symlinks. `topN <= 0` returns everything sorted by size descending.
    void scanLargest(const QStringList &directories, int topN);

    // FW-08: empty folders (no entries other than `.`/`..`) under
    // `directories`. Emits emptyFoldersScanFinished on completion; honors
    // the CleanerService exclusion engine.
    void scanEmptyFolders(const QStringList &directories);

    void cancel();
    bool isScanning() const;

    // FW-08: safe delete-to-trash. Filters `paths` through the exclusion
    // engine AND the never-delete-last-copy rule (any path that would
    // empty a duplicate group's "kept" side is dropped), then routes the
    // survivors through moveToTrash(). Returns the paths actually trashed.
    QStringList trashFiles(const QStringList &paths,
                           const QList<DuplicateGroup> &knownGroups);

    // FW-08: pure helpers — exposed for tests and for the UI to gray out
    // checkboxes that would violate the last-copy rule before the user
    // clicks Trash.

    // Returns the subset of `paths` that, if removed, would NOT leave any
    // group in `knownGroups` with fewer than one surviving member.
    static QStringList filterSafeTrashCandidates(
        const QStringList &paths,
        const QList<DuplicateGroup> &knownGroups);

    // Returns true iff removing every path in `toRemove` would leave at
    // least one duplicate group in `knownGroups` with zero survivors.
    static bool wouldRemoveLastCopy(
        const QStringList &toRemove,
        const QList<DuplicateGroup> &knownGroups);

    // Returns the top-N largest entries in `candidates`, sorted by size
    // descending then by path ascending (stable for equal sizes).
    // `topN <= 0` returns the whole list sorted; the input is otherwise
    // taken by value so callers don't need to copy.
    static QList<LargeFileEntry> rankLargest(QList<LargeFileEntry> candidates,
                                             int topN);

signals:
    void progressUpdated(int stage, int current, int total, const QString &message);
    void scanFinished(const QList<DuplicateGroup> &results);
    void largestScanFinished(const QList<LargeFileEntry> &results);
    void emptyFoldersScanFinished(const QStringList &folders);
    void scanCancelled();

protected:
    // FW-08: test seam — production routes through QFile::moveToTrash;
    // tests override to record the call and avoid touching the user's
    // real trash. Mirrors the CleanerService::removeElevated() pattern.
    virtual bool moveToTrash(const QString &path);

    // FW-08: test seam — defaults to CleanerService::ins()->loadExclusions()
    // so the production singleton's exclusion JSON is honored; tests
    // override to inject deterministic rule sets without touching
    // QStandardPaths or the real settings backend.
    virtual QList<CleanerService::ExclusionEntry> loadExclusions() const;

    explicit DuplicateFinderService(QObject *parent = nullptr);
    virtual ~DuplicateFinderService();

private:
    static DuplicateFinderService *instance;

    QList<DuplicateGroup> runPipeline(
        const QStringList &directories,
        qint64 minSize,
        const QString &globFilter,
        const QList<CleanerService::ExclusionEntry> &exclusions);

    QList<LargeFileEntry> runLargestPipeline(
        const QStringList &directories,
        int topN,
        const QList<CleanerService::ExclusionEntry> &exclusions);

    QStringList runEmptyFoldersPipeline(
        const QStringList &directories,
        const QList<CleanerService::ExclusionEntry> &exclusions);

    QAtomicInt mCancelled{0};
    QFuture<void> mWorkerFuture;
};

Q_DECLARE_METATYPE(DuplicateGroup)
Q_DECLARE_METATYPE(LargeFileEntry)

#endif // DUPLICATE_FINDER_SERVICE_H
