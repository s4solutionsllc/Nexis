#include "disk_info.h"
#include <QDebug>

static bool shouldIncludeDisk(const QStorageInfo &info)
{
    if (info.bytesTotal() == 0)
        return false;

    // Exclude virtual/pseudo filesystem types
    static const QSet<QByteArray> excludedFsTypes = {
        "tmpfs", "devtmpfs", "devfs", "sysfs", "procfs",
        "cgroup", "cgroup2", "squashfs", "overlay",
        "fuse.snapfuse", "autofs", "nullfs", "fdescfs",
        "linprocfs", "linsysfs", "map", "rootfs"
    };
    if (excludedFsTypes.contains(info.fileSystemType()))
        return false;

    // Exclude non-block devices (device string is a FS name, not a path)
    const QString dev = info.device();
    static const QSet<QString> pseudoDevices = {
        "tmpfs", "devtmpfs", "overlay", "none", "sysfs",
        "proc", "cgroup", "cgroup2", "devpts", "securityfs",
        "pstore", "efivarfs", "bpf", "tracefs", "debugfs",
        "fusectl", "configfs", "hugetlbfs", "mqueue"
    };
    if (dev.isEmpty() || pseudoDevices.contains(dev))
        return false;

    // Exclude Snap loopback devices on Linux
    if (dev.startsWith("/dev/loop"))
        return false;

    // Exclude mount paths for pseudo-filesystems and runtime mounts
    const QString path = info.rootPath();
    if (path.startsWith("/snap/") || path.startsWith("/run/snap"))
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
        if (path == sysPath || path.startsWith(sysPath + "/"))
            return false;
    }

    return true;
}

QList<Disk> DiskInfo::getDisks() const
{
    return disks;
}

void DiskInfo::updateDiskInfo()
{
    disks.clear();

    QList<QStorageInfo> storageInfoList = QStorageInfo::mountedVolumes();

    for (const QStorageInfo &info : storageInfoList) {
        if (info.isValid() && shouldIncludeDisk(info)) {
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
    for (const QStorageInfo &info : QStorageInfo::mountedVolumes()) {
        if (info.isValid() && shouldIncludeDisk(info))
            set.insert(info.device());
    }

    return set.values();
}

QList<QString> DiskInfo::fileSystemTypes()
{
    QSet<QString> set;
    for (const QStorageInfo &info : QStorageInfo::mountedVolumes()) {
        if (info.isValid() && shouldIncludeDisk(info))
            set.insert(info.fileSystemType());
    }

    return set.values();
}
