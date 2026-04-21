#ifndef DISKINFO_H
#define DISKINFO_H

#include "Utils/command_util.h"
#include "Utils/file_util.h"
#include <QStorageInfo>
#include <QSet>
#include "nexis-core_global.h"

struct Disk {
    QString name;
    QString device;
    QString fileSystemType;
    quint64 size = 0;
    quint64 free = 0;
    quint64 used = 0;
};

class NEXISCORESHARED_EXPORT DiskInfo
{
public:
    virtual ~DiskInfo() = default;

    QList<Disk> getDisks() const;
    void updateDiskInfo();

    // Thread-safe: computes a fresh list without touching the cached `disks`
    // member. FR-101 calls this from a worker thread and assigns the result
    // to the cache on the UI thread via setDisks().
    QList<Disk> collectDiskInfo() const;
    void setDisks(QList<Disk> newDisks) { disks = std::move(newDisks); }
    virtual QList<quint64> getDiskIO() const = 0;
    virtual QStringList getDiskNames() const = 0;
    QList<QString> fileSystemTypes();
    QList<QString> devices();

    static bool shouldIncludeDisk(const QString &device, const QByteArray &fsType,
                                  const QString &rootPath, qint64 bytesTotal);

protected:
    QList<Disk> disks;
};

#endif // DISKINFO_H
