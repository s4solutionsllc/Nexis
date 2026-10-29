#include <QTest>
#include "Info/rapl_power_info.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

class TestRaplPowerInfo : public QObject
{
    Q_OBJECT

private slots:
    void delta_normalIncrement();
    void delta_zeroElapsed();
    void delta_wrapAround();
    void delta_wrapWithUnknownRange();
    void delta_exactlyAtMax();
    void update_unavailableWhenEveryEnergyReadFails();
    void update_availableWhenAZoneReads();

private:
    // Writes a single powercap-style zone under `root`: name + (optionally)
    // energy_uj / max_energy_range_uj, mirroring /sys/class/powercap/intel-rapl:0.
    static void writeZone(const QString &root, const QString &zoneDir,
                           const QString &name, bool withEnergyFile);
};

void TestRaplPowerInfo::writeZone(const QString &root, const QString &zoneDir,
                                  const QString &name, bool withEnergyFile)
{
    QDir dir(root);
    QVERIFY(dir.mkpath(zoneDir));
    const QString zonePath = dir.filePath(zoneDir);

    auto writeFile = [](const QString &path, const QString &content) {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream(&f) << content;
    };

    writeFile(zonePath + "/name", name);
    writeFile(zonePath + "/max_energy_range_uj", QStringLiteral("65532610"));

    // CVE-2020-8694: energy_uj has been root-only on Intel since this CVE.
    // Omitting the file here simulates the unreadable/root-only case — the
    // zone is still discovered (name + max_energy_range_uj are world
    // readable) but every energy_uj read fails.
    if (withEnergyFile)
        writeFile(zonePath + "/energy_uj", QStringLiteral("1000"));
}

void TestRaplPowerInfo::delta_normalIncrement()
{
    // No wrap: counter simply advanced.
    quint64 d = RaplPowerInfo::energyDeltaUj(1000, 1500, 65532610);
    QCOMPARE(d, quint64(500));
}

void TestRaplPowerInfo::delta_zeroElapsed()
{
    quint64 d = RaplPowerInfo::energyDeltaUj(1000, 1000, 65532610);
    QCOMPARE(d, quint64(0));
}

void TestRaplPowerInfo::delta_wrapAround()
{
    // Counter was near the top of its range and wrapped back to a small value.
    const quint64 maxRange = 65532610;
    quint64 d = RaplPowerInfo::energyDeltaUj(maxRange - 100, 50, maxRange);
    QCOMPARE(d, quint64(150));
}

void TestRaplPowerInfo::delta_wrapWithUnknownRange()
{
    // If max_energy_range_uj couldn't be read (0), we can't safely infer the
    // wrap distance — treat as no delta rather than reporting a bogus spike.
    quint64 d = RaplPowerInfo::energyDeltaUj(65532500, 50, 0);
    QCOMPARE(d, quint64(0));
}

void TestRaplPowerInfo::delta_exactlyAtMax()
{
    const quint64 maxRange = 65532610;
    quint64 d = RaplPowerInfo::energyDeltaUj(maxRange, 0, maxRange);
    QCOMPARE(d, quint64(0));
}

void TestRaplPowerInfo::update_unavailableWhenEveryEnergyReadFails()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    writeZone(tmp.path(), QStringLiteral("intel-rapl:0"), QStringLiteral("package-0"),
              /*withEnergyFile=*/false);

    RaplPowerInfo info;
    info.setPowercapRootForTesting(tmp.path());
    info.update();

    // Zones were discovered (name/max_energy_range_uj are readable), but
    // every energy_uj read failed — must report unavailable, not "0.0 W".
    QVERIFY(!info.isAvailable());
    QCOMPARE(info.getSnapshot().packages.size(), 0);
}

void TestRaplPowerInfo::update_availableWhenAZoneReads()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    writeZone(tmp.path(), QStringLiteral("intel-rapl:0"), QStringLiteral("package-0"),
              /*withEnergyFile=*/true);

    RaplPowerInfo info;
    info.setPowercapRootForTesting(tmp.path());
    info.update();

    QVERIFY(info.isAvailable());
}

QTEST_MAIN(TestRaplPowerInfo)
#include "test_rapl_power_info.moc"
