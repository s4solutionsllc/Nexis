#ifndef DUPLICATE_FINDER_SERVICE_H
#define DUPLICATE_FINDER_SERVICE_H

#include <QObject>
#include <QFileInfo>
#include <QAtomicInt>
#include <QFuture>
#include <QHash>
#include <QMutex>
#include <QWaitCondition>

#include <functional>

#include "Managers/cleaner_service.h"
#include "image_hash.h"

struct DuplicateGroup {
    // SSO-27146: Directory groups hold whole folders (files[i] is a
    // directory) whose names, structure and file contents are identical;
    // fileSize is one folder's total bytes.
    enum class Kind { Exact, Similar, Directory };

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

// SSO-27146: replace-with-link support. A plan is built (and shown to the
// user) before anything on disk changes; applying it re-verifies every entry.
enum class LinkType { Hard, Symbolic };

struct LinkReplaceEntry {
    enum class Status {
        Ready,
        KeptAsOriginal,       // every group member was selected; this one stays
        SkipNotInGroup,       // not a member of an identical-file group
        SkipMissing,          // path or keeper no longer a regular file
        SkipExcluded,         // protected by the cleaner exclusion engine
        SkipAlreadyLinked,    // already the same inode as the keeper
        SkipCrossFilesystem   // keeper lives on another filesystem
    };

    QString duplicatePath;
    QString keeperPath;
    LinkType type = LinkType::Hard;
    Status status = Status::SkipNotInGroup;
    quint64 size = 0;

    bool isReady() const { return status == Status::Ready; }
};

struct LinkReplaceResult {
    QString path;
    QString keeperPath;
    bool replaced = false;
    QString error;
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

    // SSO-27146: duplicate-folder detection. Two folders are duplicates when
    // they hold the same entry names, the same sub-folder layout and
    // byte-identical files (symlinks must point at the same target). Reuses
    // scanFinished with Kind::Directory groups. Only the outermost match is
    // reported: when A and B are duplicates, A/x and B/x are not listed too.
    // A folder holding any cleaner-excluded or unreadable entry, and a folder
    // with no files at all, is never a candidate. `minSize` is compared with
    // the folder's total size.
    void scanDuplicateDirectories(const QStringList &directories, qint64 minSize);

    void cancel();
    bool isScanning() const;

    // SSO-27146: pause/resume of a running scan or link replacement. The
    // worker parks at its next checkpoint (between files / directory
    // entries / hash chunks) and continues where it left off on resume().
    // In-session only; cancel() releases a paused worker. Does nothing when
    // no scan is running.
    void pause();
    void resume();
    bool isPaused() const;

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

    // SSO-27146: true iff both paths exist and sit on the same filesystem
    // (st_dev). A hard link cannot cross filesystems, and a symlink that
    // does breaks the moment that volume is unmounted, so the planner
    // refuses both.
    static bool onSameFilesystem(const QString &pathA, const QString &pathB);

    // SSO-27146: true iff both paths resolve to the same inode (st_dev +
    // st_ino) — i.e. they are already hard links of one file.
    static bool isSameFile(const QString &pathA, const QString &pathB);

    // SSO-27146: preview for "replace duplicates with links". `selected` are
    // the paths the user ticked in Kind::Exact groups of `knownGroups`. Per
    // group the keeper is the first member NOT selected (list order); when
    // every member is selected the lexicographically smallest path is kept,
    // matching filterSafeTrashCandidates(). Touches nothing on disk — it only
    // stats. Entries that cannot be linked carry a skip Status.
    static QList<LinkReplaceEntry> buildLinkReplacePlan(
        const QStringList &selected,
        const QList<DuplicateGroup> &knownGroups,
        LinkType type,
        const QList<CleanerService::ExclusionEntry> &exclusions);

    // Same, using the production exclusion list.
    QList<LinkReplaceEntry> previewLinkReplace(
        const QStringList &selected,
        const QList<DuplicateGroup> &knownGroups,
        LinkType type) const;

    // Replaces one duplicate with a link to its keeper. Re-validates at call
    // time (still regular files, same filesystem, not already the same
    // inode, byte-identical content), builds the link under a temporary name
    // and renames it over the duplicate, so the path is never missing and a
    // failure leaves the original untouched. Symbolic links are relative.
    // `shouldStop`, if set, aborts the content comparison.
    static LinkReplaceResult applyLinkReplace(
        const LinkReplaceEntry &entry,
        const std::function<bool()> &shouldStop = {});

    // Runs every Ready entry of `plan` off the GUI thread (same threading
    // contract as scan()), then emits linkReplaceFinished with one result
    // per attempted entry. Honors pause()/cancel(); a cancelled run reports
    // the entries completed so far.
    void replaceWithLinks(const QList<LinkReplaceEntry> &plan);

signals:
    void progressUpdated(int stage, int current, int total, const QString &message);
    void scanFinished(const QList<DuplicateGroup> &results);
    void largestScanFinished(const QList<LargeFileEntry> &results);
    void emptyFoldersScanFinished(const QStringList &folders);
    void scanCancelled();
    void pausedChanged(bool paused);
    void linkReplaceFinished(const QList<LinkReplaceResult> &results);

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

    QList<DuplicateGroup> runDirectoryPipeline(
        const QStringList &directories,
        qint64 minSize,
        const QList<CleanerService::ExclusionEntry> &exclusions);

    // Checkpoint used by every long loop in the pipelines: parks the worker
    // while paused, then reports whether the scan has been cancelled.
    bool shouldStop();

    QList<LargeFileEntry> runLargestPipeline(
        const QStringList &directories,
        int topN,
        const QList<CleanerService::ExclusionEntry> &exclusions);

    QStringList runEmptyFoldersPipeline(
        const QStringList &directories,
        const QList<CleanerService::ExclusionEntry> &exclusions);

    QAtomicInt mCancelled{0};
    QAtomicInt mPaused{0};
    QMutex mPauseMutex;
    QWaitCondition mPauseCondition;
    QFuture<void> mWorkerFuture;
};

Q_DECLARE_METATYPE(DuplicateGroup)
Q_DECLARE_METATYPE(LargeFileEntry)
Q_DECLARE_METATYPE(LinkReplaceResult)

#endif // DUPLICATE_FINDER_SERVICE_H
