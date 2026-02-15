#include "disk_info.h"
#include <QDebug>

QList<Disk> DiskInfo::getDisks() const
{
    return disks;
}

void DiskInfo::updateDiskInfo()
{
    disks.clear();

    QList<QStorageInfo> storageInfoList = QStorageInfo::mountedVolumes();

    for (const QStorageInfo &info : storageInfoList) {
        if (info.isValid()) {
            Disk disk;
            disk.name = info.displayName();
            disk.device = info.device();
            disk.size = info.bytesTotal();
            disk.used = info.bytesTotal() - info.bytesFree();
            disk.free = info.bytesFree();
            disk.fileSystemType = info.fileSystemType();

            disks << disk;
        }
    }
}

QList<QString> DiskInfo::devices()
{
    QSet<QString> set;
    for(const QStorageInfo &info: QStorageInfo::mountedVolumes()) {
        if (info.isValid()) set.insert(info.device());
    }

    return set.values();
}

QList<QString> DiskInfo::fileSystemTypes()
{
    QSet<QString> set;
    for(const QStorageInfo &info: QStorageInfo::mountedVolumes()) {
        if (info.isValid()) set.insert(info.fileSystemType());
    }

    return set.values();
}

