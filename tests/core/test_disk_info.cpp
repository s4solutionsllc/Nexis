#include <QTest>
#include "Info/disk_info.h"

class TestDiskInfo : public QObject
{
    Q_OBJECT

private slots:
    // shouldIncludeDisk
    void filter_realDisk();
    void filter_zeroSize();
    void filter_tmpfs();
    void filter_squashfs();
    void filter_overlay();
    void filter_loopDevice();
    void filter_snapMount();
    void filter_runSnapMount();
    void filter_macosPreboot();
    void filter_macosRecovery();
    void filter_macosVM();
    void filter_macosUpdate();
    void filter_emptyDevice();
    void filter_pseudoDevice();
    void filter_devtmpfs();
    void filter_realNvme();
    void filter_macosDataVolume();
};

void TestDiskInfo::filter_realDisk()
{
    QVERIFY(DiskInfo::shouldIncludeDisk("/dev/sda1", "ext4", "/", 500000000000));
}

void TestDiskInfo::filter_zeroSize()
{
    QVERIFY(!DiskInfo::shouldIncludeDisk("/dev/sda1", "ext4", "/", 0));
}

void TestDiskInfo::filter_tmpfs()
{
    QVERIFY(!DiskInfo::shouldIncludeDisk("tmpfs", "tmpfs", "/tmp", 1048576));
}

void TestDiskInfo::filter_squashfs()
{
    QVERIFY(!DiskInfo::shouldIncludeDisk("/dev/loop0", "squashfs", "/snap/core/123", 67108864));
}

void TestDiskInfo::filter_overlay()
{
    QVERIFY(!DiskInfo::shouldIncludeDisk("overlay", "overlay", "/merged", 1048576));
}

void TestDiskInfo::filter_loopDevice()
{
    QVERIFY(!DiskInfo::shouldIncludeDisk("/dev/loop0", "ext4", "/mnt/disk", 1048576));
    QVERIFY(!DiskInfo::shouldIncludeDisk("/dev/loop15", "ext4", "/mnt/disk", 1048576));
}

void TestDiskInfo::filter_snapMount()
{
    QVERIFY(!DiskInfo::shouldIncludeDisk("/dev/sda1", "ext4", "/snap/firefox/123", 500000000));
}

void TestDiskInfo::filter_runSnapMount()
{
    QVERIFY(!DiskInfo::shouldIncludeDisk("/dev/sda1", "ext4", "/run/snapd/ns", 500000000));
}

void TestDiskInfo::filter_macosPreboot()
{
    QVERIFY(!DiskInfo::shouldIncludeDisk("/dev/disk1s2", "apfs", "/System/Volumes/Preboot", 500000000));
}

void TestDiskInfo::filter_macosRecovery()
{
    QVERIFY(!DiskInfo::shouldIncludeDisk("/dev/disk1s3", "apfs", "/System/Volumes/Recovery", 500000000));
}

void TestDiskInfo::filter_macosVM()
{
    QVERIFY(!DiskInfo::shouldIncludeDisk("/dev/disk1s4", "apfs", "/System/Volumes/VM", 500000000));
}

void TestDiskInfo::filter_macosUpdate()
{
    QVERIFY(!DiskInfo::shouldIncludeDisk("/dev/disk1s5", "apfs", "/System/Volumes/Update", 500000000));
}

void TestDiskInfo::filter_emptyDevice()
{
    QVERIFY(!DiskInfo::shouldIncludeDisk("", "ext4", "/mnt", 500000000));
}

void TestDiskInfo::filter_pseudoDevice()
{
    QVERIFY(!DiskInfo::shouldIncludeDisk("proc", "procfs", "/proc", 1048576));
    QVERIFY(!DiskInfo::shouldIncludeDisk("sysfs", "sysfs", "/sys", 1048576));
    QVERIFY(!DiskInfo::shouldIncludeDisk("none", "ext4", "/mnt", 1048576));
}

void TestDiskInfo::filter_devtmpfs()
{
    QVERIFY(!DiskInfo::shouldIncludeDisk("devtmpfs", "devtmpfs", "/dev", 4096000));
}

void TestDiskInfo::filter_realNvme()
{
    QVERIFY(DiskInfo::shouldIncludeDisk("/dev/nvme0n1p1", "ext4", "/", 1000000000000));
}

void TestDiskInfo::filter_macosDataVolume()
{
    // /System/Volumes/Data is NOT in the exclusion list — it's the real data volume
    QVERIFY(DiskInfo::shouldIncludeDisk("/dev/disk1s1", "apfs", "/System/Volumes/Data", 500000000000));
}

QTEST_MAIN(TestDiskInfo)
#include "test_disk_info.moc"
