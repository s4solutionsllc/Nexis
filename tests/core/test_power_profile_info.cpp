#include <QTest>
#include "Info/power_profile_info.h"

class TestPowerProfileInfo : public QObject
{
    Q_OBJECT

private slots:
    // parsePowerprofilesctlList
    void ppd_standardThreeProfiles();
    void ppd_performanceActive();
    void ppd_singleProfile();
    void ppd_emptyOutput();

    // parseSysfsGovernors
    void sysfs_fullGovernorList();
    void sysfs_intelPstateTwoGovernors();
    void sysfs_emptyGovernors();

    // userLabelToBackendValue
    void labelToValue_ppdPerformance();
    void labelToValue_ppdBalanced();
    void labelToValue_ppdPowerSaver();
    void labelToValue_sysfsPerformance();
    void labelToValue_sysfsBalanced();
    void labelToValue_sysfsPowerSaver();
    void labelToValue_unknownPassthrough();

    // backendValueToUserLabel
    void valueToLabel_ppdPerformance();
    void valueToLabel_ppdBalanced();
    void valueToLabel_ppdPowerSaver();
    void valueToLabel_sysfsPerformance();
    void valueToLabel_sysfsPowersave();
    void valueToLabel_sysfsSchedutil();
    void valueToLabel_sysfsOndemand();
    void valueToLabel_unknownPassthrough();
};

static const char *kPpdStandard =
    "  power-saver:\n"
    "    CpuDriver:    intel_pstate\n"
    "    PlatformDriver:  platform_profile\n"
    "\n"
    "* balanced:\n"
    "    CpuDriver:    intel_pstate\n"
    "    PlatformDriver:  platform_profile\n"
    "\n"
    "  performance:\n"
    "    CpuDriver:    intel_pstate\n"
    "    PlatformDriver:  platform_profile\n";

static const char *kPpdPerformanceActive =
    "  power-saver:\n"
    "    CpuDriver:    intel_pstate\n"
    "\n"
    "  balanced:\n"
    "    CpuDriver:    intel_pstate\n"
    "\n"
    "* performance:\n"
    "    CpuDriver:    intel_pstate\n";

static const char *kPpdSingle =
    "* balanced:\n"
    "    CpuDriver:    intel_pstate\n";

void TestPowerProfileInfo::ppd_standardThreeProfiles()
{
    PowerProfileData d = PowerProfileInfo::parsePowerprofilesctlList(kPpdStandard);
    QCOMPARE(d.backend, PowerBackend::PowerProfilesDaemon);
    QCOMPARE(d.availableProfiles.size(), 3);
    QVERIFY(d.availableProfiles.contains("power-saver"));
    QVERIFY(d.availableProfiles.contains("balanced"));
    QVERIFY(d.availableProfiles.contains("performance"));
    QCOMPARE(d.activeProfile, QString("balanced"));
}

void TestPowerProfileInfo::ppd_performanceActive()
{
    PowerProfileData d = PowerProfileInfo::parsePowerprofilesctlList(kPpdPerformanceActive);
    QCOMPARE(d.activeProfile, QString("performance"));
    QCOMPARE(d.availableProfiles.size(), 3);
}

void TestPowerProfileInfo::ppd_singleProfile()
{
    PowerProfileData d = PowerProfileInfo::parsePowerprofilesctlList(kPpdSingle);
    QCOMPARE(d.availableProfiles.size(), 1);
    QCOMPARE(d.activeProfile, QString("balanced"));
}

void TestPowerProfileInfo::ppd_emptyOutput()
{
    PowerProfileData d = PowerProfileInfo::parsePowerprofilesctlList("");
    QCOMPARE(d.backend, PowerBackend::PowerProfilesDaemon);
    QVERIFY(d.availableProfiles.isEmpty());
    QVERIFY(d.activeProfile.isEmpty());
}

void TestPowerProfileInfo::sysfs_fullGovernorList()
{
    PowerProfileData d = PowerProfileInfo::parseSysfsGovernors(
        "performance powersave ondemand conservative schedutil userspace",
        "schedutil",
        "acpi-cpufreq");
    QCOMPARE(d.backend, PowerBackend::Sysfs);
    QCOMPARE(d.availableProfiles.size(), 6);
    QCOMPARE(d.activeProfile, QString("schedutil"));
    QCOMPARE(d.scalingDriver, QString("acpi-cpufreq"));
}

void TestPowerProfileInfo::sysfs_intelPstateTwoGovernors()
{
    PowerProfileData d = PowerProfileInfo::parseSysfsGovernors(
        "performance powersave",
        "powersave",
        "intel_pstate");
    QCOMPARE(d.availableProfiles.size(), 2);
    QCOMPARE(d.activeProfile, QString("powersave"));
    QCOMPARE(d.scalingDriver, QString("intel_pstate"));
}

void TestPowerProfileInfo::sysfs_emptyGovernors()
{
    PowerProfileData d = PowerProfileInfo::parseSysfsGovernors("", "", "");
    QCOMPARE(d.backend, PowerBackend::Sysfs);
    QVERIFY(d.availableProfiles.isEmpty());
}

void TestPowerProfileInfo::labelToValue_ppdPerformance()
{
    QCOMPARE(PowerProfileInfo::userLabelToBackendValue("Performance", PowerBackend::PowerProfilesDaemon),
             QString("performance"));
}

void TestPowerProfileInfo::labelToValue_ppdBalanced()
{
    QCOMPARE(PowerProfileInfo::userLabelToBackendValue("Balanced", PowerBackend::PowerProfilesDaemon),
             QString("balanced"));
}

void TestPowerProfileInfo::labelToValue_ppdPowerSaver()
{
    QCOMPARE(PowerProfileInfo::userLabelToBackendValue("Power Saver", PowerBackend::PowerProfilesDaemon),
             QString("power-saver"));
}

void TestPowerProfileInfo::labelToValue_sysfsPerformance()
{
    QCOMPARE(PowerProfileInfo::userLabelToBackendValue("Performance", PowerBackend::Sysfs),
             QString("performance"));
}

void TestPowerProfileInfo::labelToValue_sysfsBalanced()
{
    QCOMPARE(PowerProfileInfo::userLabelToBackendValue("Balanced", PowerBackend::Sysfs),
             QString("schedutil"));
}

void TestPowerProfileInfo::labelToValue_sysfsPowerSaver()
{
    QCOMPARE(PowerProfileInfo::userLabelToBackendValue("Power Saver", PowerBackend::Sysfs),
             QString("powersave"));
}

void TestPowerProfileInfo::labelToValue_unknownPassthrough()
{
    QCOMPARE(PowerProfileInfo::userLabelToBackendValue("custom-governor", PowerBackend::Sysfs),
             QString("custom-governor"));
}

void TestPowerProfileInfo::valueToLabel_ppdPerformance()
{
    QCOMPARE(PowerProfileInfo::backendValueToUserLabel("performance", PowerBackend::PowerProfilesDaemon),
             QString("Performance"));
}

void TestPowerProfileInfo::valueToLabel_ppdBalanced()
{
    QCOMPARE(PowerProfileInfo::backendValueToUserLabel("balanced", PowerBackend::PowerProfilesDaemon),
             QString("Balanced"));
}

void TestPowerProfileInfo::valueToLabel_ppdPowerSaver()
{
    QCOMPARE(PowerProfileInfo::backendValueToUserLabel("power-saver", PowerBackend::PowerProfilesDaemon),
             QString("Power Saver"));
}

void TestPowerProfileInfo::valueToLabel_sysfsPerformance()
{
    QCOMPARE(PowerProfileInfo::backendValueToUserLabel("performance", PowerBackend::Sysfs),
             QString("Performance"));
}

void TestPowerProfileInfo::valueToLabel_sysfsPowersave()
{
    QCOMPARE(PowerProfileInfo::backendValueToUserLabel("powersave", PowerBackend::Sysfs),
             QString("Power Saver"));
}

void TestPowerProfileInfo::valueToLabel_sysfsSchedutil()
{
    QCOMPARE(PowerProfileInfo::backendValueToUserLabel("schedutil", PowerBackend::Sysfs),
             QString("Balanced"));
}

void TestPowerProfileInfo::valueToLabel_sysfsOndemand()
{
    QCOMPARE(PowerProfileInfo::backendValueToUserLabel("ondemand", PowerBackend::Sysfs),
             QString("Balanced"));
}

void TestPowerProfileInfo::valueToLabel_unknownPassthrough()
{
    QCOMPARE(PowerProfileInfo::backendValueToUserLabel("alien-governor", PowerBackend::Sysfs),
             QString("alien-governor"));
}

QTEST_MAIN(TestPowerProfileInfo)
#include "test_power_profile_info.moc"
