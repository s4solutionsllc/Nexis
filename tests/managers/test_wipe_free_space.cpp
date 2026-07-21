// SSO-15382: WipeFreeSpaceService::headroomForVolume() — the safety-margin
// calculation that keeps the fill-and-delete pass from ever attempting to
// consume 100% of free space. Pure static function; the platform-specific
// TRIM detection and the actual fill/delete I/O aren't covered here (they
// need a real mounted filesystem / pkexec prompt and are exercised by CI's
// screenshot/build verification instead).

#include <QtTest>

#include "Services/wipe_free_space_service.h"

class TestWipeFreeSpace : public QObject
{
    Q_OBJECT

private slots:
    void headroom_floorsSmallVolumes();
    void headroom_usesPercentageInRange();
    void headroom_capsHugeVolumes();
    void headroom_zeroBytesDoesNotCrash();
};

void TestWipeFreeSpace::headroom_floorsSmallVolumes()
{
    // 10 GiB total * 5% = 0.5 GiB, below the 1 GiB floor.
    const quint64 total = 10ULL * 1024 * 1024 * 1024;
    QCOMPARE(WipeFreeSpaceService::headroomForVolume(total),
             WipeFreeSpaceService::kMinHeadroomBytes);
}

void TestWipeFreeSpace::headroom_usesPercentageInRange()
{
    // 100 GiB total * 5% = 5 GiB, comfortably between the floor and cap.
    const quint64 total = 100ULL * 1024 * 1024 * 1024;
    const quint64 expected = 5ULL * 1024 * 1024 * 1024;
    QCOMPARE(WipeFreeSpaceService::headroomForVolume(total), expected);
}

void TestWipeFreeSpace::headroom_capsHugeVolumes()
{
    // 1 TiB total * 5% = ~51.2 GiB, above the 8 GiB cap.
    const quint64 total = 1024ULL * 1024 * 1024 * 1024;
    QCOMPARE(WipeFreeSpaceService::headroomForVolume(total),
             WipeFreeSpaceService::kMaxHeadroomBytes);
}

void TestWipeFreeSpace::headroom_zeroBytesDoesNotCrash()
{
    QCOMPARE(WipeFreeSpaceService::headroomForVolume(0),
             WipeFreeSpaceService::kMinHeadroomBytes);
}

QTEST_MAIN(TestWipeFreeSpace)
#include "test_wipe_free_space.moc"
