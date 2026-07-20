#ifndef FILE_SHREDDER_SERVICE_H
#define FILE_SHREDDER_SERVICE_H

#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QAtomicInt>
#include <QFuture>
#include <QMetaType>

// SSO-15381: one top-level dropped/picked path (a file or a folder) plus its
// recursively-computed footprint — what the shredder view lists as a single
// row and what the confirm dialog sums across.
struct ShredItem {
    QString path;
    bool isDir = false;
    quint64 bytes = 0;      // recursive total for a folder; file size for a file
    int fileCount = 0;      // recursive file+symlink count; 1 for a lone file
};

// Aggregate preview shown before any deletion happens (Design Anchor:
// "always include a 'what will be deleted' size + item count summary before
// destructive actions").
struct ShredPlan {
    QList<ShredItem> items;
    quint64 totalBytes = 0;
    int totalFileCount = 0;
};

class FileShredderService : public QObject
{
    Q_OBJECT

public:
    static FileShredderService *ins();

    // Computes per-item and aggregate size/count for `paths` (each a
    // top-level file or folder selected via drag-and-drop or the file
    // picker) without deleting anything. Runs in the background; emits
    // previewReady() on completion.
    void computePreview(const QStringList &paths);

    // Overwrites (single pass) then unlinks every file under `paths`,
    // removing emptied folders afterward. Runs in the background; emits
    // shredProgress() per item and shredFinished() on completion.
    void shred(const QStringList &paths);

    void cancel();
    bool isBusy() const;

    // Paths that are nested inside another path already present in the
    // list are dropped so a folder and a file inside it never get shredded
    // twice. Exposed for the page (to keep the staged list itself free of
    // redundant rows) and for tests.
    static QStringList dedupeContainedPaths(const QStringList &paths);

signals:
    void previewReady(const ShredPlan &plan);
    void previewCancelled();

    void shredProgress(int current, int total, quint64 bytesDone,
                       quint64 bytesTotal, const QString &currentPath);
    void itemFailed(const QString &path, const QString &reason);
    void shredFinished(int itemsShredded, int itemsFailed, quint64 bytesFreed);
    void shredCancelled();

protected:
    explicit FileShredderService(QObject *parent = nullptr);
    virtual ~FileShredderService();

    // Test seams (mirrors the CleanerService::removeElevated() /
    // DuplicateFinderService::moveToTrash() pattern) — production overwrites
    // `path`'s current on-disk content with a single pass of zeroes, fsyncs,
    // then unlinks it. `fileSize` is the size captured at scan time so the
    // overwrite pass and the progress byte-count agree even if the file
    // changed size concurrently.
    virtual bool overwriteAndUnlinkFile(const QString &path, quint64 fileSize);

    // Removes a symlink without touching whatever it points to.
    virtual bool unlinkSymlink(const QString &path);

    // Removes a (by then empty) directory.
    virtual bool removeDir(const QString &path);

private:
    static FileShredderService *instance;

    struct WalkResult {
        QList<QPair<QString, quint64>> files;   // path + size captured at walk time
        QStringList symlinks;
        QStringList dirs;      // not depth-sorted; caller sorts before removal
        quint64 totalBytes = 0;
    };

    static WalkResult walk(const QString &root, QAtomicInt &cancelled);
    static ShredItem statItem(const QString &path, QAtomicInt &cancelled);

    void runShred(const QStringList &paths);

    QAtomicInt mCancelled{0};
    QFuture<void> mPreviewFuture;
    QFuture<void> mShredFuture;
};

Q_DECLARE_METATYPE(ShredItem)
Q_DECLARE_METATYPE(ShredPlan)

#endif // FILE_SHREDDER_SERVICE_H
