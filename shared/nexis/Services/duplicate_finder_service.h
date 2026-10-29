#ifndef DUPLICATE_FINDER_SERVICE_H
#define DUPLICATE_FINDER_SERVICE_H

#include <QObject>
#include <QFileInfo>
#include <QAtomicInt>
#include <QFuture>
#include <QHash>

#include "Managers/cleaner_service.h"
#include "image_hash.h"

struct DuplicateGroup {
    enum class Kind { Exact, Similar };

    QList<QFileInfo> files;
    quint64 fileSize = 0;
    QByteArray hash;
    Kind kind = Kind::Exact;
    // SSO-26638: the largest member-to-seed Hamming distance in the group.
    // Meaningful for Kind::Similar only; Exact groups leave this 0.
    int maxDistance = 0;
};

// SSO-26638: a candidate image plus its dHash, as produced by the hashing
// stage of the Similar-images pipeline. Exposed (with clusterSimilarImages)
// so clustering correctness can be tested without decoding real images.
struct HashedImage {
    QFileInfo info;
    quint64 hash = 0;
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

    // SSO-26638: near-duplicate image detection. Candidate files are chosen
    // by extension against QImageReader::supportedImageFormats() at
    // runtime. Follows scan()'s shape exactly (running-guard, exclusion
    // snapshot, runsAsynchronously() gating, cancellation) and reuses
    // scanFinished — the result list carries both Kind::Exact and
    // Kind::Similar groups, Exact first.
    void scanSimilarImages(const QStringList &directories, qint64 minSize,
                            int maxDistance);

    // SSO-26638: seed-anchored clustering (spec D2) over pre-computed image
    // hashes. Sorts `images` by path, then walks the list; each image not
    // yet grouped seeds a new Kind::Similar group that collects every other
    // ungrouped image within `maxDistance` of that seed (not of each
    // other), so a chain A~B~C cannot merge visually unrelated A and C.
    // Exposed for tests — clustering can be checked with synthetic hashes,
    // without decoding real images.
    static QList<DuplicateGroup> clusterSimilarImages(QList<HashedImage> images,
                                                       int maxDistance);

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

    // SSO-17858: test seam — scan()/scanLargest()/scanEmptyFolders() emit
    // their *Finished signal from a QThreadPool worker thread by default,
    // so QSignalSpy::wait() has to survive a cross-thread queued-connection
    // hop. Under CPU-throttled CI runners that hop occasionally exceeds the
    // spy's timeout (SSO-13969, SSO-14443, SSO-17858) even though the
    // pipeline itself is correct — the flake is in the delivery latency,
    // not the result. Tests that only care about pipeline correctness
    // override this to run the pipeline and emit synchronously, removing
    // the thread hop (and the flake) entirely.
    virtual bool runsAsynchronously() const { return true; }

    explicit DuplicateFinderService(QObject *parent = nullptr);
    virtual ~DuplicateFinderService();

private:
    static DuplicateFinderService *instance;

    QList<DuplicateGroup> runPipeline(
        const QStringList &directories,
        qint64 minSize,
        const QString &globFilter,
        const QList<CleanerService::ExclusionEntry> &exclusions);

    QList<DuplicateGroup> runSimilarPipeline(
        const QStringList &directories,
        qint64 minSize,
        int maxDistance,
        const QList<CleanerService::ExclusionEntry> &exclusions);

    // Shared by runPipeline() and runSimilarPipeline(): partial-then-full
    // content hashing over files already grouped by size, producing
    // Kind::Exact groups. `baseStage`/`phaseLabel` only affect the
    // progressUpdated text, since the UI does not key behaviour off the
    // stage number (DiskToolsPage::onDupProgress ignores it).
    QList<DuplicateGroup> groupExactByContent(
        QHash<qint64, QList<QFileInfo>> sizeGroups,
        int totalScanned,
        int baseStage,
        const QString &phaseLabel = QString());

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
