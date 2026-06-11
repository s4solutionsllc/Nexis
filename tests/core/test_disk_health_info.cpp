#include <QTest>
#include <QAtomicInt>
#include <QMutexLocker>
#include <QThread>
#include <atomic>
#include <thread>
#include "Info/disk_health_info.h"

// ── Test double for concurrency tests ────────────────────────────────────────
// Concrete DiskHealthInfo so we can exercise the base-class mDrives / mutex
// pair without forking smartctl. Mirrors the worker semantics:
//   - collectDriveHealth() builds a fresh local QList and returns it
//     (must never touch mDrives — that's the whole point of WI-03).
//   - refreshHealthElevated[Batch]() mutates mDrives in place under the lock.
class FakeDiskHealthInfo : public DiskHealthInfo
{
public:
    QAtomicInt collectCalls{0};

    QList<DriveHealth> collectDriveHealth() override
    {
        // The worker pattern: build into a local list, return it. Vary the
        // size per call so a torn read would be detectable (size mismatches
        // path enumeration).
        QList<DriveHealth> drives;
        int n = 2 + (collectCalls.fetchAndAddRelaxed(1) % 4);  // 2..5 drives
        for (int i = 0; i < n; ++i) {
            DriveHealth d;
            d.deviceName = QString("sd%1").arg(QChar('a' + i));
            d.devicePath = QString("/dev/sd%1").arg(QChar('a' + i));
            d.model      = QString("FakeDrive-%1").arg(i);
            d.sizeBytes  = (1ULL + i) * 1024ULL * 1024ULL * 1024ULL;
            d.driveType  = DriveHealth::SATA_SSD;
            d.smartPassed = true;
            d.healthVerdict = QStringLiteral("Good");
            // Yield mid-build to widen the window in which a buggy
            // implementation would race against a concurrent reader.
            QThread::yieldCurrentThread();
            drives.append(d);
        }
        return drives;
    }

    void refreshHealthElevated(const QString &device) override
    {
        QMutexLocker locker(&mDrivesMutex);
        for (int i = 0; i < mDrives.size(); ++i) {
            if (mDrives[i].devicePath == device) {
                mDrives[i].needsElevation = false;
                break;
            }
        }
    }

    void refreshHealthElevatedBatch(const QStringList &devices,
                                     bool, const QString &) override
    {
        QMutexLocker locker(&mDrivesMutex);
        for (const QString &dev : devices) {
            for (int i = 0; i < mDrives.size(); ++i) {
                if (mDrives[i].devicePath == dev)
                    mDrives[i].needsElevation = false;
            }
        }
    }
};

// Internally-consistent: every entry must have devicePath/model populated and
// the per-drive deviceName/devicePath must agree. Catches torn reads where a
// reader sees a half-assigned QList or zero-initialized struct.
static bool isCoherent(const QList<DriveHealth> &snapshot)
{
    for (const DriveHealth &d : snapshot) {
        if (d.devicePath.isEmpty() || d.model.isEmpty())
            return false;
        if (!d.devicePath.endsWith(d.deviceName))
            return false;
    }
    return true;
}

class TestDiskHealthInfo : public QObject
{
    Q_OBJECT

private:
    // Helper to create a minimal NVMe DriveHealth
    DriveHealth makeNvme(int percentageUsed = -1, int availableSpare = -1,
                         int mediaErrors = -1, int criticalWarning = 0,
                         bool smartPassed = true)
    {
        DriveHealth d;
        d.driveType = DriveHealth::NVMe;
        d.smartPassed = smartPassed;
        d.percentageUsed = percentageUsed;
        d.availableSpare = availableSpare;
        d.mediaErrors = mediaErrors;
        d.criticalWarning = criticalWarning;
        return d;
    }

    DriveHealth makeSataHdd(int reallocated = 0, int pending = 0,
                            int uncorrectable = 0, bool smartPassed = true)
    {
        DriveHealth d;
        d.driveType = DriveHealth::SATA_HDD;
        d.smartPassed = smartPassed;
        d.reallocatedSectors = reallocated;
        d.pendingSectors = pending;
        d.uncorrectableSectors = uncorrectable;
        return d;
    }

    DriveHealth makeSataSsd(int wearLeveling = -1, int reallocated = 0,
                            bool smartPassed = true)
    {
        DriveHealth d;
        d.driveType = DriveHealth::SATA_SSD;
        d.smartPassed = smartPassed;
        d.wearLevelingCount = wearLeveling;
        d.reallocatedSectors = reallocated;
        return d;
    }

private slots:
    // ── deriveHealthVerdict tests ────────────────────────────────────────────
    void verdict_nvmeGood();
    void verdict_nvmeCaution_lowSpare();
    void verdict_nvmeCaution_highUsage();
    void verdict_nvmeCritical_mediaErrors();
    void verdict_nvmeCritical_100pctUsed();
    void verdict_nvmeCritical_smartFailed();
    void verdict_sataHddGood();
    void verdict_sataHddCaution();
    void verdict_sataHddCritical();
    void verdict_sataSsdGood();
    void verdict_sataSsdCaution_lowWear();
    void verdict_sataSsdCritical();
    void verdict_unknownPassed();
    void verdict_unknownFailed();

    // ── parseSmartctlJsonInto tests ─────────────────────────────────────────
    void parseJson_nvmeAllFields();
    void parseJson_sataHdd();
    void parseJson_sataSsd();
    void parseJson_emptyJson();
    void parseJson_invalidJson();
    void parseJson_smartFailed();

    // ── WI-03 / audit H3: mDrives thread safety ─────────────────────────────
    void publish_collectIsLocalAndDoesNotMutateCache();
    void publish_setDrivesUpdatesCache();
    void stress_concurrentSetAndGetDrives();
    void stress_concurrentCollectAndElevatedRefresh();
};

// ── Verdict Tests ────────────────────────────────────────────────────────────

void TestDiskHealthInfo::verdict_nvmeGood()
{
    DriveHealth d = makeNvme(3, 100, 0);
    DiskHealthInfo::deriveHealthVerdict(d);
    QCOMPARE(d.healthVerdict, QString("Good"));
    QCOMPARE(d.healthPercent, 97);
}

void TestDiskHealthInfo::verdict_nvmeCaution_lowSpare()
{
    DriveHealth d = makeNvme(50, 8, 0);
    DiskHealthInfo::deriveHealthVerdict(d);
    QCOMPARE(d.healthVerdict, QString("Caution"));
    QCOMPARE(d.healthPercent, 50);
}

void TestDiskHealthInfo::verdict_nvmeCaution_highUsage()
{
    DriveHealth d = makeNvme(85, 50, 0);
    DiskHealthInfo::deriveHealthVerdict(d);
    QCOMPARE(d.healthVerdict, QString("Caution"));
    QCOMPARE(d.healthPercent, 15);
}

void TestDiskHealthInfo::verdict_nvmeCritical_mediaErrors()
{
    DriveHealth d = makeNvme(3, 100, 1);
    DiskHealthInfo::deriveHealthVerdict(d);
    QCOMPARE(d.healthVerdict, QString("Critical"));
}

void TestDiskHealthInfo::verdict_nvmeCritical_100pctUsed()
{
    DriveHealth d = makeNvme(100, 50, 0);
    DiskHealthInfo::deriveHealthVerdict(d);
    QCOMPARE(d.healthVerdict, QString("Critical"));
    QCOMPARE(d.healthPercent, 0);
}

void TestDiskHealthInfo::verdict_nvmeCritical_smartFailed()
{
    DriveHealth d = makeNvme(3, 100, 0, 0, false);
    DiskHealthInfo::deriveHealthVerdict(d);
    QCOMPARE(d.healthVerdict, QString("Critical"));
}

void TestDiskHealthInfo::verdict_sataHddGood()
{
    DriveHealth d = makeSataHdd(0, 0, 0);
    DiskHealthInfo::deriveHealthVerdict(d);
    QCOMPARE(d.healthVerdict, QString("Good"));
    QCOMPARE(d.healthPercent, -1);
}

void TestDiskHealthInfo::verdict_sataHddCaution()
{
    DriveHealth d = makeSataHdd(5, 0, 0);
    DiskHealthInfo::deriveHealthVerdict(d);
    QCOMPARE(d.healthVerdict, QString("Caution"));
}

void TestDiskHealthInfo::verdict_sataHddCritical()
{
    DriveHealth d = makeSataHdd(50, 30, 25);
    DiskHealthInfo::deriveHealthVerdict(d);
    QCOMPARE(d.healthVerdict, QString("Critical"));
}

void TestDiskHealthInfo::verdict_sataSsdGood()
{
    DriveHealth d = makeSataSsd(95);
    DiskHealthInfo::deriveHealthVerdict(d);
    QCOMPARE(d.healthVerdict, QString("Good"));
    QCOMPARE(d.healthPercent, 95);
}

void TestDiskHealthInfo::verdict_sataSsdCaution_lowWear()
{
    DriveHealth d = makeSataSsd(25);
    DiskHealthInfo::deriveHealthVerdict(d);
    QCOMPARE(d.healthVerdict, QString("Caution"));
}

void TestDiskHealthInfo::verdict_sataSsdCritical()
{
    DriveHealth d = makeSataSsd(5);
    DiskHealthInfo::deriveHealthVerdict(d);
    QCOMPARE(d.healthVerdict, QString("Critical"));
    QCOMPARE(d.healthPercent, 5);
}

void TestDiskHealthInfo::verdict_unknownPassed()
{
    DriveHealth d;
    d.driveType = DriveHealth::Unknown;
    d.smartPassed = true;
    DiskHealthInfo::deriveHealthVerdict(d);
    QCOMPARE(d.healthVerdict, QString("Good"));
}

void TestDiskHealthInfo::verdict_unknownFailed()
{
    DriveHealth d;
    d.driveType = DriveHealth::Unknown;
    d.smartPassed = false;
    DiskHealthInfo::deriveHealthVerdict(d);
    QCOMPARE(d.healthVerdict, QString("Critical"));
}

// ── JSON Parsing Tests ───────────────────────────────────────────────────────

void TestDiskHealthInfo::parseJson_nvmeAllFields()
{
    QByteArray json = R"({
        "model_name": "Samsung 970 EVO Plus",
        "serial_number": "S4EWNG0M123456",
        "firmware_version": "2B2QEXM7",
        "device": { "type": "nvme" },
        "smart_status": { "passed": true },
        "nvme_smart_health_information_log": {
            "critical_warning": 0,
            "temperature": 38,
            "available_spare": 100,
            "available_spare_threshold": 10,
            "percentage_used": 3,
            "data_units_read": 12345678,
            "data_units_written": 9876543,
            "power_cycles": 500,
            "power_on_hours": 2000,
            "unsafe_shutdowns": 10,
            "media_errors": 0
        }
    })";

    DriveHealth d;
    DiskHealthInfo::parseSmartctlJsonInto(json, d);

    QCOMPARE(d.model, QString("Samsung 970 EVO Plus"));
    QCOMPARE(d.serial, QString("S4EWNG0M123456"));
    QCOMPARE(d.firmware, QString("2B2QEXM7"));
    QCOMPARE(d.driveType, DriveHealth::NVMe);
    QVERIFY(d.smartPassed);
    QCOMPARE(d.criticalWarning, 0);
    QCOMPARE(d.temperatureCelsius, 38.0);
    QCOMPARE(d.availableSpare, 100);
    QCOMPARE(d.availableSpareThreshold, 10);
    QCOMPARE(d.percentageUsed, 3);
    QCOMPARE(d.dataUnitsRead, static_cast<qint64>(12345678));
    QCOMPARE(d.dataUnitsWritten, static_cast<qint64>(9876543));
    QCOMPARE(d.powerCycles, 500);
    QCOMPARE(d.powerOnHours, 2000);
    QCOMPARE(d.unsafeShutdowns, 10);
    QCOMPARE(d.mediaErrors, 0);
}

void TestDiskHealthInfo::parseJson_sataHdd()
{
    // Attribute 194 raw.value is a realistic packed 48-bit value (current=30, min=19, max=43).
    // The top-level "temperature.current" must win; without it, temperatureCelsius would be
    // 184684838942 — the bug this test guards against.
    QByteArray json = R"({
        "model_name": "WDC WD10EZEX",
        "serial_number": "WD-WMC3T0123456",
        "firmware_version": "01.01A01",
        "device": { "type": "sat" },
        "rotation_rate": 7200,
        "smart_status": { "passed": true },
        "temperature": { "current": 35 },
        "ata_smart_attributes": {
            "table": [
                { "id": 5,   "name": "Reallocated_Sector_Ct", "value": 100, "worst": 100, "thresh": 10, "raw": { "value": 2 } },
                { "id": 9,   "name": "Power_On_Hours",        "value": 90,  "worst": 90,  "thresh": 0,  "raw": { "value": 5000 } },
                { "id": 12,  "name": "Power_Cycle_Count",     "value": 100, "worst": 100, "thresh": 0,  "raw": { "value": 300 } },
                { "id": 194, "name": "Temperature_Celsius",   "value": 30,  "worst": 40,  "thresh": 0,  "raw": { "value": 184684838942 } },
                { "id": 197, "name": "Current_Pending_Sector","value": 100, "worst": 100, "thresh": 0,  "raw": { "value": 0 } },
                { "id": 198, "name": "Offline_Uncorrectable", "value": 100, "worst": 100, "thresh": 0,  "raw": { "value": 0 } }
            ]
        }
    })";

    DriveHealth d;
    DiskHealthInfo::parseSmartctlJsonInto(json, d);

    QCOMPARE(d.driveType, DriveHealth::SATA_HDD);
    QCOMPARE(d.model, QString("WDC WD10EZEX"));
    QVERIFY(d.smartPassed);
    QCOMPARE(d.reallocatedSectors, 2);
    QCOMPARE(d.powerOnHours, 5000);
    QCOMPARE(d.powerCycles, 300);
    QCOMPARE(d.pendingSectors, 0);
    QCOMPARE(d.uncorrectableSectors, 0);
    QVERIFY(d.allAttributes.size() == 6);
    QCOMPARE(d.temperatureCelsius, 35.0);  // must come from temperature.current, not packed raw
}

void TestDiskHealthInfo::parseJson_sataSsd()
{
    QByteArray json = R"({
        "model_name": "Samsung SSD 860 EVO",
        "serial_number": "S3YVNB0K123456",
        "device": { "type": "sat" },
        "rotation_rate": 0,
        "smart_status": { "passed": true },
        "ata_smart_attributes": {
            "table": [
                { "id": 5,   "name": "Reallocated_Sector_Ct", "value": 100, "worst": 100, "thresh": 10, "raw": { "value": 0 } },
                { "id": 177, "name": "Wear_Leveling_Count",   "value": 95,  "worst": 95,  "thresh": 0,  "raw": { "value": 50 } },
                { "id": 190, "name": "Airflow_Temperature",   "value": 70,  "worst": 55,  "thresh": 0,  "raw": { "value": 30 } }
            ]
        }
    })";

    DriveHealth d;
    DiskHealthInfo::parseSmartctlJsonInto(json, d);

    QCOMPARE(d.driveType, DriveHealth::SATA_SSD);
    QCOMPARE(d.wearLevelingCount, 95);
    QCOMPARE(d.reallocatedSectors, 0);
}

void TestDiskHealthInfo::parseJson_emptyJson()
{
    // Empty JSON: device type is empty so parser enters SATA branch,
    // rotation_rate defaults to 0 → classified as SATA_SSD
    DriveHealth d;
    DiskHealthInfo::parseSmartctlJsonInto(QByteArray("{}"), d);
    QCOMPARE(d.driveType, DriveHealth::SATA_SSD);
    QVERIFY(d.model.isEmpty());
}

void TestDiskHealthInfo::parseJson_invalidJson()
{
    DriveHealth d;
    DiskHealthInfo::parseSmartctlJsonInto(QByteArray("not json at all"), d);
    QCOMPARE(d.driveType, DriveHealth::Unknown);
    QVERIFY(d.model.isEmpty());
}

void TestDiskHealthInfo::parseJson_smartFailed()
{
    QByteArray json = R"({
        "device": { "type": "nvme" },
        "smart_status": { "passed": false },
        "nvme_smart_health_information_log": {}
    })";

    DriveHealth d;
    DiskHealthInfo::parseSmartctlJsonInto(json, d);
    QVERIFY(!d.smartPassed);
}

// ── WI-03 / audit H3: mDrives thread safety ──────────────────────────────────

void TestDiskHealthInfo::publish_collectIsLocalAndDoesNotMutateCache()
{
    FakeDiskHealthInfo info;
    QVERIFY(info.getDrives().isEmpty());

    QList<DriveHealth> local = info.collectDriveHealth();
    QVERIFY(!local.isEmpty());

    // Worker path must never mutate mDrives — that's the whole point of the
    // publish pattern. Cache should still be empty after collectDriveHealth().
    QVERIFY(info.getDrives().isEmpty());
    QVERIFY(!info.hasDrives());
}

void TestDiskHealthInfo::publish_setDrivesUpdatesCache()
{
    FakeDiskHealthInfo info;
    QList<DriveHealth> local = info.collectDriveHealth();
    int expectedSize = local.size();

    info.setDrives(local);

    QCOMPARE(info.getDrives().size(), expectedSize);
    QVERIFY(info.hasDrives());
    QVERIFY(isCoherent(info.getDrives()));
}

void TestDiskHealthInfo::stress_concurrentSetAndGetDrives()
{
    // Reproduce the original H3 race shape: a writer thread doing the
    // discoverDrives()-equivalent (clear + repeated assign via setDrives()),
    // a reader thread copying via getDrives() in a tight loop. With the WI-03
    // mutex in place this must not crash and every snapshot must be coherent
    // (no torn QList reads, no partial DriveHealth structs).
    FakeDiskHealthInfo info;
    info.setDrives(info.collectDriveHealth());  // prime so reader has something

    constexpr int kIters = 2000;  // CI-modest; ASan/TSan still catches it.
    std::atomic<int> incoherentReads{0};
    std::atomic<bool> stop{false};

    std::thread reader([&]() {
        while (!stop.load(std::memory_order_relaxed)) {
            QList<DriveHealth> snapshot = info.getDrives();
            if (!snapshot.isEmpty() && !isCoherent(snapshot))
                incoherentReads.fetch_add(1, std::memory_order_relaxed);
        }
    });

    std::thread writer([&]() {
        for (int i = 0; i < kIters; ++i) {
            // Each iteration mimics discoverDrives()'s effect: a brand-new
            // QList replaces the cache. With the mutex, the reader either
            // sees the old or the new list, never something in between.
            info.setDrives(info.collectDriveHealth());
        }
    });

    writer.join();
    stop.store(true, std::memory_order_relaxed);
    reader.join();

    QCOMPARE(incoherentReads.load(), 0);
}

void TestDiskHealthInfo::stress_concurrentCollectAndElevatedRefresh()
{
    // refreshHealthElevated() runs on the UI thread (synchronous pkexec/sudo
    // call) and mutates mDrives[i] in place; the worker concurrently rebuilds
    // mDrives via collectDriveHealth() + setDrives(). Both must coexist
    // without corrupting the cache.
    FakeDiskHealthInfo info;
    info.setDrives(info.collectDriveHealth());

    constexpr int kIters = 500;
    std::atomic<int> incoherentReads{0};
    std::atomic<bool> stop{false};

    std::thread workerPublisher([&]() {
        for (int i = 0; i < kIters; ++i)
            info.setDrives(info.collectDriveHealth());
    });

    std::thread elevatedRefresher([&]() {
        while (!stop.load(std::memory_order_relaxed)) {
            // Touch every possible drive path the fake produces.
            for (char c = 'a'; c <= 'e'; ++c)
                info.refreshHealthElevated(QString("/dev/sd%1").arg(QChar(c)));
        }
    });

    std::thread reader([&]() {
        while (!stop.load(std::memory_order_relaxed)) {
            QList<DriveHealth> snapshot = info.getDrives();
            if (!snapshot.isEmpty() && !isCoherent(snapshot))
                incoherentReads.fetch_add(1, std::memory_order_relaxed);
        }
    });

    workerPublisher.join();
    stop.store(true, std::memory_order_relaxed);
    elevatedRefresher.join();
    reader.join();

    QCOMPARE(incoherentReads.load(), 0);
}

QTEST_MAIN(TestDiskHealthInfo)
#include "test_disk_health_info.moc"
