#include "dir_size_scanner.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>
#include <QtConcurrent>

#include <sys/stat.h>
#include <sys/types.h>

namespace {

// Per-scan dedup key for hard links: (deviceId, inode). We track this in a
// QSet keyed off a packed 128-bit-ish string so the algorithm stays portable
// across the platforms we ship on (Linux, macOS).
QString inodeKey(dev_t dev, ino_t ino)
{
    return QString::number(static_cast<quint64>(dev)) + ":" +
           QString::number(static_cast<quint64>(ino));
}

struct ScanCtx {
    QAtomicInt *cancelled = nullptr;
    QSet<QString> seenInodes;       // hard-link dedup across whole scan
    qint64 totalBytes = 0;
    int    totalFiles = 0;
    // Last values reported via progressCb. Tracked per-scan so a second
    // scan reusing the same QtConcurrent worker thread doesn't inherit
    // stale thresholds from the previous run.
    qint64 lastReportedBytes = 0;
    int    lastReportedFiles = 0;
    std::function<void(qint64, int)> progressCb;

    bool isCancelled() const {
        return cancelled && cancelled->loadRelaxed();
    }
};

// Forward decl.
std::unique_ptr<DirSizeNode> scanInto(const QString &path, ScanCtx &ctx);

void emitProgressMaybe(ScanCtx &ctx)
{
    // Throttle progress callbacks — every ~512 files or ~64MiB scanned.
    if (!ctx.progressCb)
        return;
    if (ctx.totalFiles - ctx.lastReportedFiles >= 512 ||
        ctx.totalBytes - ctx.lastReportedBytes >= (64LL << 20)) {
        ctx.lastReportedFiles = ctx.totalFiles;
        ctx.lastReportedBytes = ctx.totalBytes;
        ctx.progressCb(ctx.totalBytes, ctx.totalFiles);
    }
}

std::unique_ptr<DirSizeNode> makeFileNode(const QString &path,
                                          const QString &name,
                                          qint64 size,
                                          bool isSymLink)
{
    auto node = std::make_unique<DirSizeNode>();
    node->name = name;
    node->path = path;
    node->size = size;
    node->isDir = false;
    node->isSymLink = isSymLink;
    return node;
}

std::unique_ptr<DirSizeNode> scanInto(const QString &path, ScanCtx &ctx)
{
    auto node = std::make_unique<DirSizeNode>();
    node->path  = path;
    node->name  = QFileInfo(path).fileName();
    node->isDir = true;
    if (node->name.isEmpty())
        node->name = path; // root like "/" or "C:/"

    if (ctx.isCancelled())
        return node;

    // Use QDir entryInfoList rather than QDirIterator so we can sort children
    // ourselves and avoid the iterator's per-entry symlink follow on macOS.
    QDir dir(path);
    if (!dir.exists())
        return node;

    const auto entries = dir.entryInfoList(
        QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
        QDir::NoSort);

    for (const QFileInfo &fi : entries) {
        if (ctx.isCancelled())
            return node;

        // Skip symlinks entirely — accounting them at their target's size
        // would double-count when the target sits inside the scan root.
        // Surface them as zero-byte leaves so the user sees they exist.
        if (fi.isSymLink()) {
            auto sym = makeFileNode(fi.absoluteFilePath(), fi.fileName(), 0, true);
            node->children.push_back(std::move(sym));
            continue;
        }

        if (fi.isDir()) {
            auto child = scanInto(fi.absoluteFilePath(), ctx);
            node->size      += child->size;
            node->fileCount += child->fileCount;
            node->children.push_back(std::move(child));
            continue;
        }

        // Regular file. Hard-link dedup via (dev, inode).
        struct stat st{};
        qint64 contributed = 0;
        if (::lstat(fi.absoluteFilePath().toLocal8Bit().constData(), &st) == 0) {
            const QString key = inodeKey(st.st_dev, st.st_ino);
            if (st.st_nlink > 1) {
                if (ctx.seenInodes.contains(key)) {
                    // Already counted under another path — show as zero so
                    // the file is visible but doesn't inflate the parent.
                    auto link = makeFileNode(fi.absoluteFilePath(),
                                             fi.fileName(), 0, false);
                    node->children.push_back(std::move(link));
                    continue;
                }
                ctx.seenInodes.insert(key);
            }
            contributed = static_cast<qint64>(st.st_size);
        } else {
            contributed = fi.size();
        }

        node->size      += contributed;
        node->fileCount += 1;
        ctx.totalBytes  += contributed;
        ctx.totalFiles  += 1;

        auto leaf = makeFileNode(fi.absoluteFilePath(), fi.fileName(),
                                 contributed, false);
        node->children.push_back(std::move(leaf));

        emitProgressMaybe(ctx);
    }

    return node;
}

} // namespace

DirSizeScanner::DirSizeScanner(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<DirSizeNodePtr>("DirSizeNodePtr");
}

DirSizeScanner::~DirSizeScanner()
{
    // Wait for the worker before our `this` capture goes dangling.
    // QtConcurrent::run captures `this` for the emit, and the worker may
    // still be mid-recursion when our owner (typically the dialog) closes
    // and triggers WA_DeleteOnClose.
    if (mWorkerFuture.isRunning()) {
        mCancelled.storeRelaxed(1);
        mWorkerFuture.waitForFinished();
    }
}

void DirSizeScanner::start(const QString &rootPath)
{
    if (mWorkerFuture.isRunning())
        return;
    mCancelled.storeRelaxed(0);

    const QString rootCopy = rootPath;
    mWorkerFuture = QtConcurrent::run([this, rootCopy]() {
        auto progressCb = [this](qint64 bytes, int files) {
            emit progress(bytes, files);
        };
        DirSizeNodePtr root = scanSynchronous(rootCopy, &mCancelled, progressCb);
        if (mCancelled.loadRelaxed())
            emit cancelled();
        else
            emit finished(root);
    });
}

void DirSizeScanner::cancel()
{
    mCancelled.storeRelaxed(1);
}

bool DirSizeScanner::isRunning() const
{
    return mWorkerFuture.isRunning();
}

DirSizeNodePtr DirSizeScanner::scanSynchronous(
    const QString &rootPath,
    QAtomicInt *cancelled,
    std::function<void(qint64, int)> progressCb)
{
    ScanCtx ctx;
    ctx.cancelled  = cancelled;
    ctx.progressCb = std::move(progressCb);

    std::unique_ptr<DirSizeNode> root = scanInto(rootPath, ctx);

    // Final progress flush so callers get a count that matches the tree.
    if (ctx.progressCb)
        ctx.progressCb(ctx.totalBytes, ctx.totalFiles);

    return std::shared_ptr<DirSizeNode>(std::move(root));
}
