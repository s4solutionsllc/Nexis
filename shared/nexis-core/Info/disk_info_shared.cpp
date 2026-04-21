#include "disk_info.h"
#include <QDebug>

bool DiskInfo::shouldIncludeDisk(const QString &device, const QByteArray &fsType,
                                  const QString &rootPath, qint64 bytesTotal)
{
    if (bytesTotal == 0)
        return false;

    // Exclude virtual/pseudo filesystem types
    static const QSet<QByteArray> excludedFsTypes = {
        "tmpfs", "devtmpfs", "devfs", "sysfs", "procfs",
        "cgroup", "cgroup2", "squashfs", "overlay",
        "fuse.snapfuse", "autofs", "nullfs", "fdescfs",
        "linprocfs", "linsysfs", "map", "rootfs"
    };
    if (excludedFsTypes.contains(fsType))
        return false;

    // Exclude non-block devices (device string is a FS name, not a path)
    static const QSet<QString> pseudoDevices = {
        "tmpfs", "devtmpfs", "overlay", "none", "sysfs",
        "proc", "cgroup", "cgroup2", "devpts", "securityfs",
        "pstore", "efivarfs", "bpf", "tracefs", "debugfs",
        "fusectl", "configfs", "hugetlbfs", "mqueue"
    };
    if (device.isEmpty() || pseudoDevices.contains(device))
        return false;

    // Exclude Snap loopback devices on Linux
    if (device.startsWith("/dev/loop"))
        return false;

    // Exclude mount paths for pseudo-filesystems and runtime mounts
    if (rootPath.startsWith("/snap/") || rootPath.startsWith("/run/snap"))
        return false;

    // Exclude macOS hidden system APFS volumes
    static const QStringList macSystemPaths = {
        "/System/Volumes/Preboot",
        "/System/Volumes/Recovery",
        "/System/Volumes/VM",
        "/System/Volumes/Update",
        "/System/Volumes/xarts",
        "/System/Volumes/iSCPreboot",
        "/System/Volumes/Hardware"
    };
    for (const QString &sysPath : macSystemPaths) {
        if (rootPath == sysPath || rootPath.startsWith(sysPath + "/"))
            return false;
    }

    return true;
}

QList<Disk> DiskInfo::getDisks() const
{
    return disks;
}

QList<Disk> DiskInfo::collectDiskInfo() const
{
    QList<Disk> result;
    const QList<QStorageInfo> storageInfoList = QStorageInfo::mountedVolumes();

    for (const QStorageInfo &info : storageInfoList) {
        if (info.isValid() && shouldIncludeDisk(info.device(), info.fileSystemType(),
                                                 info.rootPath(), info.bytesTotal())) {
            Disk disk;
            disk.name = info.displayName();
            disk.device = info.device();
            disk.size = info.bytesTotal();
            disk.used = info.bytesTotal() - info.bytesFree();
            disk.free = info.bytesFree();
            disk.fileSystemType = info.fileSystemType();

            result << disk;
        }
    }

    return result;
}

void DiskInfo::updateDiskInfo()
{
    disks = collectDiskInfo();
}

QList<QString> DiskInfo::devices()
{
    QSet<QString> set;
    for (const QStorageInfo &info : QStorageInfo::mountedVolumes()) {
        if (info.isValid() && shouldIncludeDisk(info.device(), info.fileSystemType(),
                                                 info.rootPath(), info.bytesTotal()))
            set.insert(info.device());
    }

    return set.values();
}

QList<QString> DiskInfo::fileSystemTypes()
{
    QSet<QString> set;
    for (const QStorageInfo &info : QStorageInfo::mountedVolumes()) {
        if (info.isValid() && shouldIncludeDisk(info.device(), info.fileSystemType(),
                                                 info.rootPath(), info.bytesTotal()))
            set.insert(info.fileSystemType());
    }

    return set.values();
}
