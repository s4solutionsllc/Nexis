#include "file_shredder_service.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QtConcurrent>

#include <algorithm>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

FileShredderService *FileShredderService::instance = nullptr;

FileShredderService *FileShredderService::ins()
{
    if (!instance)
        instance = new FileShredderService;
    return instance;
}

FileShredderService::FileShredderService(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<ShredItem>("ShredItem");
    qRegisterMetaType<ShredPlan>("ShredPlan");
}

FileShredderService::~FileShredderService()
{
    cancel();
    mPreviewFuture.waitForFinished();
    mShredFuture.waitForFinished();
}

bool FileShredderService::isBusy() const
{
    return mPreviewFuture.isRunning() || mShredFuture.isRunning();
}

void FileShredderService::cancel()
{
    mCancelled.storeRelaxed(1);
}

bool FileShredderService::overwriteAndUnlinkFile(const QString &path, quint64 fileSize)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadWrite))
        return false;

    constexpr qint64 chunkSize = 1 << 20; // 1 MiB
    const QByteArray zeros(
        static_cast<int>(qMin<qint64>(chunkSize, fileSize > 0 ? static_cast<qint64>(fileSize) : 1)),
        '\0');

    f.seek(0);
    qint64 remaining = static_cast<qint64>(fileSize);
    bool writeOk = true;
    while (remaining > 0) {
        const qint64 n = qMin<qint64>(chunkSize, remaining);
        if (f.write(zeros.constData(), n) != n) {
            writeOk = false;
            break;
        }
        remaining -= n;
    }

    if (writeOk) {
        f.flush();
#ifdef Q_OS_UNIX
        // Force the overwrite pass to actual storage before the unlink so
        // the on-disk content is genuinely replaced, not just buffered —
        // this is the basis for the "single-pass overwrite" claim surfaced
        // in the shredder view's disclosure copy.
        int fd = f.handle();
        if (fd >= 0)
            fsync(fd);
#endif
    }
    f.close();

    if (!writeOk)
        return false;

    return QFile::remove(path);
}

bool FileShredderService::unlinkSymlink(const QString &path)
{
    // QFile::remove() unlinks the link itself on POSIX, never the target.
    return QFile::remove(path);
}

bool FileShredderService::removeDir(const QString &path)
{
    return QDir().rmdir(path);
}

FileShredderService::WalkResult FileShredderService::walk(const QString &root, QAtomicInt &cancelled)
{
    WalkResult result;
    QFileInfo rootInfo(root);

    // Symlinks are unlinked as-is — never dereferenced, so a link pointing
    // outside the selection can't cause the shredder to walk (or destroy)
    // unrelated data.
    if (rootInfo.isSymLink()) {
        result.symlinks << root;
        return result;
    }
    if (!rootInfo.exists())
        return result;

    if (rootInfo.isDir()) {
        QDirIterator it(root, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (cancelled.loadRelaxed())
                return {};

            it.next();
            const QFileInfo fi = it.fileInfo();
            if (fi.isSymLink()) {
                result.symlinks << fi.filePath();
            } else if (fi.isDir()) {
                result.dirs << fi.filePath();
            } else {
                result.files << qMakePair(fi.filePath(), static_cast<quint64>(fi.size()));
                result.totalBytes += static_cast<quint64>(fi.size());
            }
        }
        result.dirs << root;
    } else {
        result.files << qMakePair(root, static_cast<quint64>(rootInfo.size()));
        result.totalBytes += static_cast<quint64>(rootInfo.size());
    }

    return result;
}

ShredItem FileShredderService::statItem(const QString &path, QAtomicInt &cancelled)
{
    ShredItem item;
    item.path = path;

    const QFileInfo fi(path);
    item.isDir = fi.isDir() && !fi.isSymLink();

    const WalkResult w = walk(path, cancelled);
    item.bytes = w.totalBytes;
    item.fileCount = w.files.size() + w.symlinks.size();
    return item;
}

QStringList FileShredderService::dedupeContainedPaths(const QStringList &paths)
{
    QStringList canon;
    canon.reserve(paths.size());
    for (const QString &p : paths) {
        const QFileInfo fi(p);
        canon << (fi.exists() ? fi.canonicalFilePath() : QDir::cleanPath(fi.absoluteFilePath()));
    }

    QStringList kept;
    QSet<QString> seenCanon;
    for (int i = 0; i < paths.size(); ++i) {
        if (seenCanon.contains(canon.at(i)))
            continue; // exact duplicate of an already-kept entry

        bool contained = false;
        for (int j = 0; j < paths.size(); ++j) {
            if (i == j)
                continue;
            if (canon.at(i) != canon.at(j) && canon.at(i).startsWith(canon.at(j) + "/")) {
                contained = true;
                break;
            }
        }

        if (!contained) {
            kept << paths.at(i);
            seenCanon.insert(canon.at(i));
        }
    }
    return kept;
}

void FileShredderService::computePreview(const QStringList &paths)
{
    if (mPreviewFuture.isRunning())
        return;

    mCancelled.storeRelaxed(0);
    const QStringList deduped = dedupeContainedPaths(paths);

    mPreviewFuture = QtConcurrent::run([this, deduped]() {
        ShredPlan plan;
        for (const QString &p : deduped) {
            if (mCancelled.loadRelaxed()) {
                emit previewCancelled();
                return;
            }
            const ShredItem item = statItem(p, mCancelled);
            plan.items << item;
            plan.totalBytes += item.bytes;
            plan.totalFileCount += item.fileCount;
        }

        if (mCancelled.loadRelaxed())
            emit previewCancelled();
        else
            emit previewReady(plan);
    });
}

void FileShredderService::shred(const QStringList &paths)
{
    if (mShredFuture.isRunning())
        return;

    mCancelled.storeRelaxed(0);
    const QStringList deduped = dedupeContainedPaths(paths);

    mShredFuture = QtConcurrent::run([this, deduped]() {
        runShred(deduped);
    });
}

void FileShredderService::runShred(const QStringList &paths)
{
    QList<QPair<QString, quint64>> allFiles;
    QStringList allSymlinks, allDirs;
    quint64 bytesTotal = 0;

    for (const QString &p : paths) {
        if (mCancelled.loadRelaxed()) {
            emit shredCancelled();
            return;
        }
        const WalkResult w = walk(p, mCancelled);
        allFiles += w.files;
        allSymlinks += w.symlinks;
        allDirs += w.dirs;
        bytesTotal += w.totalBytes;
    }

    // Deepest directories first so a parent directory is already empty by
    // the time we try to remove it.
    std::sort(allDirs.begin(), allDirs.end(), [](const QString &a, const QString &b) {
        return a.count('/') > b.count('/');
    });

    const int total = allFiles.size() + allSymlinks.size();
    int done = 0, shredded = 0, failed = 0;
    quint64 bytesDone = 0;

    for (const auto &entry : std::as_const(allFiles)) {
        if (mCancelled.loadRelaxed()) {
            emit shredCancelled();
            return;
        }

        const QString &path = entry.first;
        const quint64 size = entry.second;
        if (overwriteAndUnlinkFile(path, size)) {
            shredded++;
            bytesDone += size;
        } else {
            failed++;
            emit itemFailed(path, tr("Could not overwrite or delete this file."));
        }
        done++;
        emit shredProgress(done, total, bytesDone, bytesTotal, path);
    }

    for (const QString &path : std::as_const(allSymlinks)) {
        if (mCancelled.loadRelaxed()) {
            emit shredCancelled();
            return;
        }

        if (unlinkSymlink(path)) {
            shredded++;
        } else {
            failed++;
            emit itemFailed(path, tr("Could not delete this symlink."));
        }
        done++;
        emit shredProgress(done, total, bytesDone, bytesTotal, path);
    }

    // Best-effort: a directory that still has content (e.g. one of its files
    // failed to delete above) is silently left in place rather than force-
    // removed — the shredder only ever deletes what it explicitly walked.
    for (const QString &dir : std::as_const(allDirs)) {
        if (mCancelled.loadRelaxed())
            break;
        removeDir(dir);
    }

    emit shredFinished(shredded, failed, bytesDone);
}
