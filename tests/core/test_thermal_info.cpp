#include <QTest>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QTemporaryDir>
#include "Info/thermal_info.h"

namespace {
// Write a sysfs-like text file (trailing newline mimics kernel hwmon output).
void writeHwmonFile(const QString &path, const QString &value)
{
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly | QIODevice::Text),
             qPrintable(QString("open %1: %2").arg(path, f.errorString())));
    f.write(value.toUtf8());
    f.write("\n");
    f.close();
}

// Build a "hwmonN" directory with a name file and a single temp1_input.
// Optional label/max/crit are written when non-empty.
void makeHwmonDevice(const QString &root,
                     const QString &slot,
                     const QString &name,
                     const QString &tempInputMilliC,
                     const QString &tempLabel = QString(),
                     const QString &tempMaxMilliC = QString(),
                     const QString &tempCritMilliC = QString())
{
    const QString devPath = root + "/" + slot;
    QVERIFY(QDir().mkpath(devPath));
    writeHwmonFile(devPath + "/name", name);
    writeHwmonFile(devPath + "/temp1_input", tempInputMilliC);
    if (!tempLabel.isEmpty())
        writeHwmonFile(devPath + "/temp1_label", tempLabel);
    if (!tempMaxMilliC.isEmpty())
        writeHwmonFile(devPath + "/temp1_max", tempMaxMilliC);
    if (!tempCritMilliC.isEmpty())
        writeHwmonFile(devPath + "/temp1_crit", tempCritMilliC);
}
}  // namespace

class TestThermalInfo : public QObject
{
    Q_OBJECT

private slots:
    // parseSysfsTemperature
    void temp_normalValue();
    void temp_zero();
    void temp_negative();
    void temp_emptyInput();
    void temp_withWhitespace();

    // sanitizeTempThreshold
    void threshold_normalMax();
    void threshold_normalCrit();
    void threshold_bogusHigh();
    void threshold_zero();
    void threshold_emptyInput();
    void threshold_customMaxSane();

    // friendlyDeviceName — vendor WMI surfaces (FW-16, kernel 7.0)
    void friendly_asusWmi();
    void friendly_asusEcSensors();
    void friendly_hpWmi();
    void friendly_legionLaptop();
    void friendly_ideapadLaptop();
    void friendly_existingDriversUnchanged();
    void friendly_unknownDriverCapitalized();

    // enumerateHwmonSensors — fixture-based discovery
    void enumerate_missingRootReturnsEmpty();
    void enumerate_skipsHwmonWithoutName();
    void enumerate_picksUpVendorWmiSensors();
    void enumerate_multipleTempInputsPerDevice();
    void enumerate_clampsBogusThresholds();
    void enumerate_synthesizesLabelWhenAbsent();
    void enumerate_noRegressionWithoutVendorNodes();
};

void TestThermalInfo::temp_normalValue()
{
    // 45000 millideg = 45.0 °C
    QCOMPARE(ThermalInfo::parseSysfsTemperature("45000"), 45.0);
}

void TestThermalInfo::temp_zero()
{
    QCOMPARE(ThermalInfo::parseSysfsTemperature("0"), 0.0);
}

void TestThermalInfo::temp_negative()
{
    // Some sensors can report negative (e.g., cold environments)
    QCOMPARE(ThermalInfo::parseSysfsTemperature("-5000"), -5.0);
}

void TestThermalInfo::temp_emptyInput()
{
    QCOMPARE(ThermalInfo::parseSysfsTemperature(""), 0.0);
}

void TestThermalInfo::temp_withWhitespace()
{
    QCOMPARE(ThermalInfo::parseSysfsTemperature("  72500\n"), 72.5);
}

void TestThermalInfo::threshold_normalMax()
{
    // 85000 millideg = 85.0 °C, within 200.0 sane limit
    QCOMPARE(ThermalInfo::sanitizeTempThreshold("85000"), 85.0);
}

void TestThermalInfo::threshold_normalCrit()
{
    QCOMPARE(ThermalInfo::sanitizeTempThreshold("105000"), 105.0);
}

void TestThermalInfo::threshold_bogusHigh()
{
    // 250000 millideg = 250.0 °C, exceeds 200.0 limit → -1.0
    QCOMPARE(ThermalInfo::sanitizeTempThreshold("250000"), -1.0);
}

void TestThermalInfo::threshold_zero()
{
    // 0 millideg = 0.0 °C → not positive → -1.0
    QCOMPARE(ThermalInfo::sanitizeTempThreshold("0"), -1.0);
}

void TestThermalInfo::threshold_emptyInput()
{
    QCOMPARE(ThermalInfo::sanitizeTempThreshold(""), -1.0);
}

void TestThermalInfo::threshold_customMaxSane()
{
    // 95000 millideg = 95.0 °C, custom limit of 90.0 → -1.0
    QCOMPARE(ThermalInfo::sanitizeTempThreshold("95000", 90.0), -1.0);
    // But within default limit of 200.0
    QCOMPARE(ThermalInfo::sanitizeTempThreshold("95000", 200.0), 95.0);
}

// --- friendlyDeviceName: vendor WMI surfaces (FW-16) ---

void TestThermalInfo::friendly_asusWmi()
{
    // asus-wmi driver registers hwmon name "asus" (kernel 7.0 fan/backlight/kbd).
    QCOMPARE(ThermalInfo::friendlyDeviceName("asus"), QStringLiteral("ASUS"));
    QCOMPARE(ThermalInfo::friendlyDeviceName("asus_wmi_sensors"), QStringLiteral("ASUS"));
}

void TestThermalInfo::friendly_asusEcSensors()
{
    QCOMPARE(ThermalInfo::friendlyDeviceName("asus-ec-sensors"), QStringLiteral("ASUS"));
}

void TestThermalInfo::friendly_hpWmi()
{
    // hp-wmi (Victus/Omen) registers hwmon name "hp"; some forks use hp_wmi.
    QCOMPARE(ThermalInfo::friendlyDeviceName("hp"), QStringLiteral("HP"));
    QCOMPARE(ThermalInfo::friendlyDeviceName("hp_wmi"), QStringLiteral("HP"));
}

void TestThermalInfo::friendly_legionLaptop()
{
    // legion-laptop registers hwmon name "legion".
    QCOMPARE(ThermalInfo::friendlyDeviceName("legion"), QStringLiteral("Legion"));
}

void TestThermalInfo::friendly_ideapadLaptop()
{
    QCOMPARE(ThermalInfo::friendlyDeviceName("ideapad"), QStringLiteral("IdeaPad"));
}

void TestThermalInfo::friendly_existingDriversUnchanged()
{
    // Sanity: vendor additions did not displace the existing mappings.
    QCOMPARE(ThermalInfo::friendlyDeviceName("k10temp"), QStringLiteral("CPU"));
    QCOMPARE(ThermalInfo::friendlyDeviceName("coretemp"), QStringLiteral("CPU"));
    QCOMPARE(ThermalInfo::friendlyDeviceName("amdgpu"), QStringLiteral("GPU"));
    QCOMPARE(ThermalInfo::friendlyDeviceName("nvme"), QStringLiteral("NVMe"));
    QCOMPARE(ThermalInfo::friendlyDeviceName("thinkpad"), QStringLiteral("ThinkPad"));
}

void TestThermalInfo::friendly_unknownDriverCapitalized()
{
    QCOMPARE(ThermalInfo::friendlyDeviceName("brandnew_driver"),
             QStringLiteral("Brandnew_driver"));
}

// --- enumerateHwmonSensors: fixture-based discovery ---

void TestThermalInfo::enumerate_missingRootReturnsEmpty()
{
    auto sensors = ThermalInfo::enumerateHwmonSensors("/does/not/exist/nexis-test");
    QVERIFY(sensors.isEmpty());
}

void TestThermalInfo::enumerate_skipsHwmonWithoutName()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    // hwmon0 has no name file → should be skipped (kernel sometimes exposes
    // transient nodes during probe).
    QVERIFY(QDir().mkpath(tmp.filePath("hwmon0")));
    writeHwmonFile(tmp.filePath("hwmon0/temp1_input"), "55000");

    auto sensors = ThermalInfo::enumerateHwmonSensors(tmp.path());
    QVERIFY(sensors.isEmpty());
}

void TestThermalInfo::enumerate_picksUpVendorWmiSensors()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // hwmon0: ASUS WMI fan-side temperature (asus-wmi)
    makeHwmonDevice(tmp.path(), "hwmon0", "asus", "47000",
                    "CPU Fan", "85000", "100000");
    // hwmon1: HP WMI (Victus) fan controller surface
    makeHwmonDevice(tmp.path(), "hwmon1", "hp", "52000",
                    "Thermal Zone", QString(), QString());
    // hwmon2: Lenovo Legion extra hwmon sensors
    makeHwmonDevice(tmp.path(), "hwmon2", "legion", "61000",
                    "GPU", "90000", "105000");
    // hwmon3: existing driver — must still appear (no regression)
    makeHwmonDevice(tmp.path(), "hwmon3", "k10temp", "39000",
                    "Tctl", QString(), QString());

    auto sensors = ThermalInfo::enumerateHwmonSensors(tmp.path());
    QCOMPARE(sensors.size(), 4);

    // Build a name→sensor index so we don't depend on QDir traversal order
    // for assertions, only that all four show up.
    QHash<QString, ThermalSensor> byDevice;
    for (const auto &s : sensors)
        byDevice.insert(s.deviceName, s);

    QVERIFY(byDevice.contains("asus"));
    QCOMPARE(byDevice.value("asus").label, QStringLiteral("ASUS – CPU Fan"));
    QCOMPARE(byDevice.value("asus").maxTemp, 85.0);
    QCOMPARE(byDevice.value("asus").critTemp, 100.0);

    QVERIFY(byDevice.contains("hp"));
    QCOMPARE(byDevice.value("hp").label, QStringLiteral("HP – Thermal Zone"));
    QCOMPARE(byDevice.value("hp").maxTemp, -1.0);   // not provided
    QCOMPARE(byDevice.value("hp").critTemp, -1.0);

    QVERIFY(byDevice.contains("legion"));
    QCOMPARE(byDevice.value("legion").label, QStringLiteral("Legion – GPU"));

    QVERIFY(byDevice.contains("k10temp"));
    QCOMPARE(byDevice.value("k10temp").label, QStringLiteral("CPU – Tctl"));
}

void TestThermalInfo::enumerate_multipleTempInputsPerDevice()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Legion exposes multiple temps per hwmon node — make sure each
    // tempN_input becomes its own sensor with a distinct id.
    const QString dev = tmp.filePath("hwmon0");
    QVERIFY(QDir().mkpath(dev));
    writeHwmonFile(dev + "/name", "legion");
    writeHwmonFile(dev + "/temp1_input", "55000");
    writeHwmonFile(dev + "/temp1_label", "CPU");
    writeHwmonFile(dev + "/temp2_input", "60000");
    writeHwmonFile(dev + "/temp2_label", "GPU");
    writeHwmonFile(dev + "/temp3_input", "45000");
    // temp3 deliberately has no label → label should be synthesized.

    auto sensors = ThermalInfo::enumerateHwmonSensors(tmp.path());
    QCOMPARE(sensors.size(), 3);

    QHash<QString, ThermalSensor> byId;
    for (const auto &s : sensors)
        byId.insert(s.id, s);

    QVERIFY(byId.contains("legion/temp1"));
    QCOMPARE(byId.value("legion/temp1").label, QStringLiteral("Legion – CPU"));
    QVERIFY(byId.contains("legion/temp2"));
    QCOMPARE(byId.value("legion/temp2").label, QStringLiteral("Legion – GPU"));
    QVERIFY(byId.contains("legion/temp3"));
    QCOMPARE(byId.value("legion/temp3").label, QStringLiteral("Legion – Sensor 3"));
}

void TestThermalInfo::enumerate_clampsBogusThresholds()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    // 250000 mC = 250 °C — over the 200 °C sane ceiling. Sensor must still
    // appear, but the threshold drops to -1.0 so the UI doesn't render a
    // nonsense "Max 250 °C" pill.
    makeHwmonDevice(tmp.path(), "hwmon0", "asus", "48000",
                    "Sensor", "250000", "300000");

    auto sensors = ThermalInfo::enumerateHwmonSensors(tmp.path());
    QCOMPARE(sensors.size(), 1);
    QCOMPARE(sensors.at(0).maxTemp, -1.0);
    QCOMPARE(sensors.at(0).critTemp, -1.0);
}

void TestThermalInfo::enumerate_synthesizesLabelWhenAbsent()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    // hp-wmi sometimes does not expose temp*_label.
    makeHwmonDevice(tmp.path(), "hwmon0", "hp", "44000");

    auto sensors = ThermalInfo::enumerateHwmonSensors(tmp.path());
    QCOMPARE(sensors.size(), 1);
    QCOMPARE(sensors.at(0).label, QStringLiteral("HP – Sensor 1"));
}

void TestThermalInfo::enumerate_noRegressionWithoutVendorNodes()
{
    // Acceptance: "no regression on machines without [vendor WMI sensors]."
    // A machine with only coretemp/nvme must still produce both sensors and
    // none of the vendor surfaces.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    makeHwmonDevice(tmp.path(), "hwmon0", "coretemp", "42000",
                    "Package id 0", "85000", "100000");
    makeHwmonDevice(tmp.path(), "hwmon1", "nvme", "38000",
                    "Composite", QString(), QString());

    auto sensors = ThermalInfo::enumerateHwmonSensors(tmp.path());
    QCOMPARE(sensors.size(), 2);
    for (const auto &s : sensors) {
        QVERIFY(s.deviceName != "asus");
        QVERIFY(s.deviceName != "hp");
        QVERIFY(s.deviceName != "legion");
    }
}

QTEST_MAIN(TestThermalInfo)
#include "test_thermal_info.moc"
