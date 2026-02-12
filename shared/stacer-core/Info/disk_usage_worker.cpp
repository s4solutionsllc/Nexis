#include "disk_usage_worker.h"
#include <QDirIterator>
#include <algorithm>

DiskUsageWorker::DiskUsageWorker(QObject *parent)
    : QObject(parent)
{
    mCancelled.storeRelaxed(0);
}

void DiskUsageWorker::cancel()
{
    mCancelled.storeRelaxed(1);
}

void DiskUsageWorker::scanDirectory(const QString &path)
{
    mCancelled.storeRelaxed(0);

    QDir dir(path);
    if (!dir.exists()) {
        emit scanFinished(path, {}, 0);
        return;
    }

    // Check readability
    QFileInfo dirInfo(path);
    if (!dirInfo.isReadable()) {
        emit scanFinished(path, {}, 0);
        return;
    }

    QList<DirEntry> entries;
    quint64 totalSize = 0;
    int itemsScanned = 0;

    // Enumerate immediate children
    const QFileInfoList children = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
        QDir::DirsFirst | QDir::Name);

    for (const QFileInfo &fi : children) {
        if (mCancelled.loadRelaxed())
            return;

        DirEntry entry;
        entry.name = fi.fileName();
        entry.absolutePath = fi.absoluteFilePath();
        entry.isDir = fi.isDir();

        if (fi.isDir()) {
            // Skip symlinks to avoid infinite loops
            if (fi.isSymLink()) {
                entry.size = 0;
                entry.childCount = 0;
            } else {
                int subCount = 0;
                entry.size = calculateDirSize(fi.absoluteFilePath(), subCount);
                entry.childCount = QDir(fi.absoluteFilePath())
                    .entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden)
                    .count();
            }
        } else {
            entry.size = fi.size();
            entry.childCount = -1; // not a directory
        }

        totalSize += entry.size;
        entries.append(entry);

        itemsScanned++;
        if (itemsScanned % 50 == 0) {
            emit scanProgress(fi.absoluteFilePath(), itemsScanned);
        }
    }

    if (mCancelled.loadRelaxed())
        return;

    // Sort by size descending
    std::sort(entries.begin(), entries.end(),
              [](const DirEntry &a, const DirEntry &b) {
                  return a.size > b.size;
              });

    emit scanFinished(path, entries, totalSize);
}

quint64 DiskUsageWorker::calculateDirSize(const QString &path, int &itemCount)
{
    quint64 total = 0;

    QDirIterator it(path,
                    QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
                    QDirIterator::Subdirectories);

    while (it.hasNext()) {
        if (mCancelled.loadRelaxed())
            return total;

        it.next();
        const QFileInfo fi = it.fileInfo();

        if (fi.isFile() && !fi.isSymLink()) {
            total += fi.size();
        }
        itemCount++;
    }

    return total;
}
