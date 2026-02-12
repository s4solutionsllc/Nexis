#ifndef DISK_USAGE_WORKER_H
#define DISK_USAGE_WORKER_H

#include <QObject>
#include <QDir>
#include <QFileInfo>
#include <QStorageInfo>
#include <QAtomicInt>
#include "stacer-core_global.h"

struct STACERCORESHARED_EXPORT DirEntry {
    QString name;           // Display name (folder or file name)
    QString absolutePath;   // Full path
    quint64 size;           // Total size in bytes (recursive for dirs)
    bool isDir;             // true = directory, false = file
    int childCount;         // Number of direct children (dirs only, -1 if not scanned)
};

class STACERCORESHARED_EXPORT DiskUsageWorker : public QObject
{
    Q_OBJECT

public:
    explicit DiskUsageWorker(QObject *parent = nullptr);

    // Scan immediate children of `path` and compute their sizes.
    // Each child directory's size is computed recursively.
    // Emits scanFinished() when done.
    void scanDirectory(const QString &path);

    // Cancel a running scan
    void cancel();

signals:
    // Emitted when scan completes. Results are the direct children of the
    // scanned path, sorted by size descending.
    void scanFinished(const QString &path, const QList<DirEntry> &entries, quint64 totalSize);

    // Progress updates during scan
    void scanProgress(const QString &currentDir, int itemsScanned);

private:
    quint64 calculateDirSize(const QString &path, int &itemCount);

    QAtomicInt mCancelled;
};

#endif // DISK_USAGE_WORKER_H
