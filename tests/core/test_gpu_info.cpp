#include <QTest>
#include "Info/gpu_info.h"

class TestGpuInfo : public QObject
{
    Q_OBJECT

private slots:
    // parseNvidiaSmiUtilization
    void nvidiaSmi_normalValue();
    void nvidiaSmi_zeroPercent();
    void nvidiaSmi_hundredPercent();
    void nvidiaSmi_withNewlines();
    void nvidiaSmi_emptyInput();
    void nvidiaSmi_nonNumeric();
    void nvidiaSmi_exceedsRange();

    // parseSysfsUtilization
    void sysfs_normalValue();
    void sysfs_zero();
    void sysfs_emptyInput();
    void sysfs_withWhitespace();
    void sysfs_nonNumeric();

    // parseIntelFreqUtilization
    void intelFreq_halfLoad();
    void intelFreq_fullLoad();
    void intelFreq_idle();
    void intelFreq_zeroMax();
    void intelFreq_emptyInput();
    void intelFreq_exceedsMax();

    // parseLspciDeviceName
    void lspci_nvidiaDevice();
    void lspci_amdDevice();
    void lspci_emptyOutput();
    void lspci_busIdNotFound();

    // parseFramebufferParentPciBusId
    void framebuffer_normalPath();
    void framebuffer_multiplePciSegments();
    void framebuffer_noPciParent();
    void framebuffer_noFramebufferInPath();
    void framebuffer_emptyInput();
};

// --- parseNvidiaSmiUtilization ---

void TestGpuInfo::nvidiaSmi_normalValue()
{
    QCOMPARE(GpuInfo::parseNvidiaSmiUtilization("42"), 42);
}

void TestGpuInfo::nvidiaSmi_zeroPercent()
{
    QCOMPARE(GpuInfo::parseNvidiaSmiUtilization("0"), 0);
}

void TestGpuInfo::nvidiaSmi_hundredPercent()
{
    QCOMPARE(GpuInfo::parseNvidiaSmiUtilization("100"), 100);
}

void TestGpuInfo::nvidiaSmi_withNewlines()
{
    QCOMPARE(GpuInfo::parseNvidiaSmiUtilization("73\n"), 73);
}

void TestGpuInfo::nvidiaSmi_emptyInput()
{
    QCOMPARE(GpuInfo::parseNvidiaSmiUtilization(""), -1);
}

void TestGpuInfo::nvidiaSmi_nonNumeric()
{
    QCOMPARE(GpuInfo::parseNvidiaSmiUtilization("N/A"), -1);
}

void TestGpuInfo::nvidiaSmi_exceedsRange()
{
    QCOMPARE(GpuInfo::parseNvidiaSmiUtilization("150"), 100);
}

// --- parseSysfsUtilization ---

void TestGpuInfo::sysfs_normalValue()
{
    QCOMPARE(GpuInfo::parseSysfsUtilization("65"), 65);
}

void TestGpuInfo::sysfs_zero()
{
    QCOMPARE(GpuInfo::parseSysfsUtilization("0"), 0);
}

void TestGpuInfo::sysfs_emptyInput()
{
    QCOMPARE(GpuInfo::parseSysfsUtilization(""), -1);
}

void TestGpuInfo::sysfs_withWhitespace()
{
    QCOMPARE(GpuInfo::parseSysfsUtilization("  88  \n"), 88);
}

void TestGpuInfo::sysfs_nonNumeric()
{
    QCOMPARE(GpuInfo::parseSysfsUtilization("error"), -1);
}

// --- parseIntelFreqUtilization ---

void TestGpuInfo::intelFreq_halfLoad()
{
    // 600 MHz cur / 1200 MHz max = 50%
    QCOMPARE(GpuInfo::parseIntelFreqUtilization("600", "1200"), 50);
}

void TestGpuInfo::intelFreq_fullLoad()
{
    QCOMPARE(GpuInfo::parseIntelFreqUtilization("1200", "1200"), 100);
}

void TestGpuInfo::intelFreq_idle()
{
    QCOMPARE(GpuInfo::parseIntelFreqUtilization("0", "1200"), 0);
}

void TestGpuInfo::intelFreq_zeroMax()
{
    QCOMPARE(GpuInfo::parseIntelFreqUtilization("600", "0"), -1);
}

void TestGpuInfo::intelFreq_emptyInput()
{
    QCOMPARE(GpuInfo::parseIntelFreqUtilization("", ""), -1);
}

void TestGpuInfo::intelFreq_exceedsMax()
{
    // Turbo boost: cur > max should be clamped to 100
    QCOMPARE(GpuInfo::parseIntelFreqUtilization("1500", "1200"), 100);
}

// --- parseLspciDeviceName ---

void TestGpuInfo::lspci_nvidiaDevice()
{
    QString output = "03:00.0 VGA compatible controller: NVIDIA Corporation GeForce RTX 3080 (rev a1)";
    QString name = GpuInfo::parseLspciDeviceName(output, "03:00.0");
    QCOMPARE(name, "NVIDIA Corporation GeForce RTX 3080 (rev a1)");
}

void TestGpuInfo::lspci_amdDevice()
{
    QString output = "06:00.0 VGA compatible controller: Advanced Micro Devices, Inc. [AMD/ATI] Navi 21 [Radeon RX 6800/6800 XT / 6900 XT]";
    QString name = GpuInfo::parseLspciDeviceName(output, "06:00.0");
    QCOMPARE(name, "Advanced Micro Devices, Inc. [AMD/ATI] Navi 21 [Radeon RX 6800/6800 XT / 6900 XT]");
}

void TestGpuInfo::lspci_emptyOutput()
{
    QString name = GpuInfo::parseLspciDeviceName("", "03:00.0");
    QVERIFY(name.isEmpty());
}

void TestGpuInfo::lspci_busIdNotFound()
{
    QString output = "03:00.0 VGA compatible controller: NVIDIA Corporation GeForce RTX 3080";
    QString name = GpuInfo::parseLspciDeviceName(output, "99:00.0");
    QVERIFY(name.isEmpty());
}

// --- parseFramebufferParentPciBusId ---

void TestGpuInfo::framebuffer_normalPath()
{
    QString path = "/sys/devices/pci0000:00/0000:00:01.2/0000:04:00.0/simple-framebuffer.0/drm/card0";
    QCOMPARE(GpuInfo::parseFramebufferParentPciBusId(path), "0000:04:00.0");
}

void TestGpuInfo::framebuffer_multiplePciSegments()
{
    // Multiple PCI addresses in chain — should return the one immediately before simple-framebuffer
    QString path = "/sys/devices/pci0000:00/0000:00:01.2/0000:02:00.2/0000:03:00.0/0000:04:00.0/simple-framebuffer.0/drm/card0";
    QCOMPARE(GpuInfo::parseFramebufferParentPciBusId(path), "0000:04:00.0");
}

void TestGpuInfo::framebuffer_noPciParent()
{
    QString path = "/sys/devices/platform/simple-framebuffer.0/drm/card0";
    QVERIFY(GpuInfo::parseFramebufferParentPciBusId(path).isEmpty());
}

void TestGpuInfo::framebuffer_noFramebufferInPath()
{
    QString path = "/sys/devices/pci0000:00/0000:07:00.0/drm/card1";
    QVERIFY(GpuInfo::parseFramebufferParentPciBusId(path).isEmpty());
}

void TestGpuInfo::framebuffer_emptyInput()
{
    QVERIFY(GpuInfo::parseFramebufferParentPciBusId("").isEmpty());
}

QTEST_MAIN(TestGpuInfo)
#include "test_gpu_info.moc"
