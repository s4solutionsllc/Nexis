// SSO-3737 / FW-09: built-in disk-space visualizer.
//
// DirSizeScanner walks a directory tree off the UI thread, aggregating
// per-subtree byte counts and child file counts so a treemap can render
// them. Symlinks are not followed (they show up as zero-byte leaves) and
// hard-linked files counted once per scan to match Baobab/DaisyDisk's
// notion of "disk usage" rather than "logical size".
//
// The pure traversal is exposed as a static scanSynchronous() so it can be
// unit-tested against a QTemporaryDir without spinning up a thread.

#ifndef DIR_SIZE_SCANNER_H
#define DIR_SIZE_SCANNER_H

#include <QObject>
#include <QString>
#include <QAtomicInt>
#include <QFuture>

#include <functional>
#include <memory>
#include <vector>

struct DirSizeNode {
    QString name;                                    ///< basename ("." for root)
    QString path;                                    ///< absolute path
    qint64  size      = 0;                           ///< aggregated size in bytes
    int     fileCount = 0;                           ///< files under this subtree
    bool    isDir     = false;
    bool    isSymLink = false;
    std::vector<std::unique_ptr<DirSizeNode>> children;
};

using DirSizeNodePtr = std::shared_ptr<DirSizeNode>;

class DirSizeScanner : public QObject
{
    Q_OBJECT

public:
    explicit DirSizeScanner(QObject *parent = nullptr);
    ~DirSizeScanner() override;

    /// Start an asynchronous scan. Emits progress() periodically and
    /// finished() (or cancelled()) on completion. No-op if a scan is
    /// already running — call cancel() then wait for cancelled() first.
    void start(const QString &rootPath);

    /// Request cancellation. The worker checks the flag between entries.
    void cancel();

    bool isRunning() const;

    /// Synchronous traversal used both by start() and by unit tests.
    /// progressCb (if set) is called periodically with (bytesScanned,
    /// filesScanned). cancelled (if non-null) is polled between entries —
    /// when set, traversal stops and a partial tree is returned.
    static DirSizeNodePtr scanSynchronous(
        const QString &rootPath,
        QAtomicInt *cancelled = nullptr,
        std::function<void(qint64, int)> progressCb = {});

signals:
    void progress(qint64 bytesScanned, int filesScanned);
    void finished(DirSizeNodePtr root);
    void cancelled();

private:
    QAtomicInt    mCancelled{0};
    QFuture<void> mWorkerFuture;
};

Q_DECLARE_METATYPE(DirSizeNodePtr)

#endif // DIR_SIZE_SCANNER_H
