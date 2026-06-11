#include <QStringList>
#include <QTest>
#include <QThread>
#include <atomic>
#include <thread>
#include "Info/disk_info.h"

namespace {
// Concrete subclass so we can exercise the DiskInfo cache without depending on
// platform IO; the abstract base requires getDiskIO()/getDiskNames() overrides.
class FakeDiskInfo : public DiskInfo
{
public:
    QList<quint64> getDiskIO() const override { return {}; }
    QStringList    getDiskNames() const override { return {}; }
};

// Internally-consistent disk: device path matches name suffix, sizes coherent
// (used <= size, used + free == size). Catches torn reads where a snapshot
// captures a half-mutated entry.
bool isCoherent(const QList<Disk> &snap)
{
    for (const Disk &d : snap) {
        if (d.name.isEmpty() || d.device.isEmpty())
            return false;
        if (!d.device.endsWith(d.name))
            return false;
        if (d.used > d.size)
            return false;
        if (d.size != 0 && d.used + d.free != d.size)
            return false;
    }
    return true;
}

QList<Disk> makeDiskList(int n)
{
    QList<Disk> out;
    out.reserve(n);
    for (int i = 0; i < n; ++i) {
        Disk d;
        d.name = QString("sd%1").arg(QChar('a' + i));
        d.device = QString("/dev/sd%1").arg(QChar('a' + i));
        d.fileSystemType = "ext4";
        d.size = (1ULL + i) * 1024ULL * 1024ULL * 1024ULL;
        d.used = d.size / 2;
        d.free = d.size - d.used;
        // Yield mid-build to widen the race window for a buggy reader.
        QThread::yieldCurrentThread();
        out.append(d);
    }
    return out;
}
} // namespace

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

    // WI-23: prove the publish-on-UI / snapshot-by-value pattern is race-free.
    void snapshot_isStableUnderConcurrentSetDisks();
    void collectDiskInfo_isSafeUnderConcurrentSetDisks();
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

// ── WI-23 concurrency tests ─────────────────────────────────────────────────
//
// The wizard health-score worker previously called DiskInfo::getDisks() from
// a QtConcurrent thread while the DataRefreshService medium tick republished
// DiskInfo::disks via setDisks() — that races the QList move-assign against
// readers. WI-23 closes this by snapshotting on the UI thread before
// launching the worker (preferred path; the wizard implementation in
// maintenance_wizard_dialog.cpp captures the snapshot by value).
//
// These tests pin the two invariants the worker pattern relies on:
//   1. A previously-captured QList<Disk> snapshot is immutable and stays
//      coherent regardless of any later setDisks() calls. (QList is COW;
//      this is what makes "capture by value" safe.)
//   2. collectDiskInfo() is the worker-safe accessor — it never touches
//      the cached `disks` member, so it can run concurrently with setDisks().

void TestDiskInfo::snapshot_isStableUnderConcurrentSetDisks()
{
    FakeDiskInfo info;
    info.setDisks(makeDiskList(4));

    // The wizard's pattern: take the snapshot on the UI thread before
    // launching the worker, then hand it to the worker by value.
    const QList<Disk> snap = info.getDisks();
    QVERIFY(!snap.isEmpty());
    QVERIFY(isCoherent(snap));

    constexpr int kIters = 2000;
    std::atomic<int> incoherentSnapReads{0};
    std::atomic<bool> stop{false};

    std::thread publisher([&]() {
        for (int i = 0; i < kIters; ++i)
            info.setDisks(makeDiskList(2 + (i % 5)));
    });

    // While the publisher mutates the cache, the captured snapshot must
    // stay coherent — the worker reads from `snap`, never the live cache.
    std::thread reader([&]() {
        while (!stop.load(std::memory_order_relaxed)) {
            if (!isCoherent(snap))
                incoherentSnapReads.fetch_add(1, std::memory_order_relaxed);
        }
    });

    publisher.join();
    stop.store(true, std::memory_order_relaxed);
    reader.join();

    QCOMPARE(incoherentSnapReads.load(), 0);
}

void TestDiskInfo::collectDiskInfo_isSafeUnderConcurrentSetDisks()
{
    FakeDiskInfo info;
    info.setDisks(makeDiskList(3));

    constexpr int kIters = 200;
    std::atomic<bool> stop{false};
    std::atomic<int> collectCount{0};

    std::thread publisher([&]() {
        for (int i = 0; i < kIters; ++i)
            info.setDisks(makeDiskList(2 + (i % 5)));
    });

    // collectDiskInfo() walks QStorageInfo without touching `disks`, so it
    // is safe to call from a worker thread concurrent with the UI-thread
    // publisher. The exact mount list varies by platform/CI; we only
    // assert the call does not crash or assert. (TSan/ASan flag races.)
    std::thread worker([&]() {
        while (!stop.load(std::memory_order_relaxed)) {
            QList<Disk> fresh = info.collectDiskInfo();
            (void)fresh;
            collectCount.fetch_add(1, std::memory_order_relaxed);
        }
    });

    publisher.join();
    stop.store(true, std::memory_order_relaxed);
    worker.join();

    QVERIFY(collectCount.load() > 0);
}

QTEST_MAIN(TestDiskInfo)
#include "test_disk_info.moc"
