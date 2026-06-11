#include <QTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include "Info/gpu_info.h"

namespace {
// Helper: write a freq file with a fixed value so QFile::exists() returns true.
void writeFreqFile(const QString &path, const QString &value = "300")
{
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly | QIODevice::Text),
             qPrintable(QString("open %1: %2").arg(path, f.errorString())));
    f.write(value.toUtf8());
    f.close();
}
}  // namespace

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

    // normalizePciBusId
    void normPci_sysfsFormat();
    void normPci_nvidiaSmiFormat();
    void normPci_uppercase();
    void normPci_shortForm();
    void normPci_empty();

    // findIntelXeFreqDir (GH#91)
    void xeFreqDir_iGpuTile0Gt0();
    void xeFreqDir_picksFirstTileFirstGt();
    void xeFreqDir_skipsTileMissingFreqFiles();
    void xeFreqDir_emptyWhenNoTiles();
    void xeFreqDir_emptyWhenI915Layout();
    void xeFreqDir_emptyWhenPathMissing();
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

// --- normalizePciBusId ---

void TestGpuInfo::normPci_sysfsFormat()
{
    QCOMPARE(GpuInfo::normalizePciBusId("0000:07:00.0"), "07:00.0");
}

void TestGpuInfo::normPci_nvidiaSmiFormat()
{
    QCOMPARE(GpuInfo::normalizePciBusId("00000000:07:00.0"), "07:00.0");
}

void TestGpuInfo::normPci_uppercase()
{
    QCOMPARE(GpuInfo::normalizePciBusId("00000000:0A:00.0"), "0a:00.0");
}

void TestGpuInfo::normPci_shortForm()
{
    QCOMPARE(GpuInfo::normalizePciBusId("07:00.0"), "07:00.0");
}

void TestGpuInfo::normPci_empty()
{
    QCOMPARE(GpuInfo::normalizePciBusId(""), "");
}

// --- findIntelXeFreqDir (GH#91) ---

void TestGpuInfo::xeFreqDir_iGpuTile0Gt0()
{
    // Single-tile, single-GT iGPU layout (Raptor Lake-P with the xe driver):
    //   <device>/tile0/gt0/freq0/{cur_freq,max_freq}
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QString freq0 = tmp.filePath("tile0/gt0/freq0");
    QVERIFY(QDir().mkpath(freq0));
    writeFreqFile(freq0 + "/cur_freq", "450");
    writeFreqFile(freq0 + "/max_freq", "1300");

    QString found = GpuInfo::findIntelXeFreqDir(tmp.path());
    QCOMPARE(found, freq0);
}

void TestGpuInfo::xeFreqDir_picksFirstTileFirstGt()
{
    // Multi-tile / multi-GT cards: probe tile0/gt0 first (QDir::Name order).
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    for (const QString &t : {"tile0", "tile1"}) {
        for (const QString &g : {"gt0", "gt1"}) {
            QString freq0 = tmp.filePath(t + "/" + g + "/freq0");
            QVERIFY(QDir().mkpath(freq0));
            writeFreqFile(freq0 + "/cur_freq", "500");
            writeFreqFile(freq0 + "/max_freq", "1500");
        }
    }

    QString found = GpuInfo::findIntelXeFreqDir(tmp.path());
    QCOMPARE(found, tmp.filePath("tile0/gt0/freq0"));
}

void TestGpuInfo::xeFreqDir_skipsTileMissingFreqFiles()
{
    // tile0/gt0/freq0 exists but is empty; tile0/gt1 is fully wired. The probe
    // should advance to gt1 instead of returning a dir that lacks the files
    // the utilization reader will need.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(QDir().mkpath(tmp.filePath("tile0/gt0/freq0")));
    QString gt1Freq = tmp.filePath("tile0/gt1/freq0");
    QVERIFY(QDir().mkpath(gt1Freq));
    writeFreqFile(gt1Freq + "/cur_freq", "600");
    writeFreqFile(gt1Freq + "/max_freq", "1600");

    QString found = GpuInfo::findIntelXeFreqDir(tmp.path());
    QCOMPARE(found, gt1Freq);
}

void TestGpuInfo::xeFreqDir_emptyWhenNoTiles()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(GpuInfo::findIntelXeFreqDir(tmp.path()).isEmpty());
}

void TestGpuInfo::xeFreqDir_emptyWhenI915Layout()
{
    // i915 puts cur/max directly under <cardN>/, not under <device>/tile*/gt*/.
    // The xe probe must not match these and must let the caller fall back.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    writeFreqFile(tmp.filePath("gt_cur_freq_mhz"), "300");
    writeFreqFile(tmp.filePath("gt_max_freq_mhz"), "1300");
    QVERIFY(GpuInfo::findIntelXeFreqDir(tmp.path()).isEmpty());
}

void TestGpuInfo::xeFreqDir_emptyWhenPathMissing()
{
    QVERIFY(GpuInfo::findIntelXeFreqDir("/does/not/exist/nexis-test").isEmpty());
}

QTEST_MAIN(TestGpuInfo)
#include "test_gpu_info.moc"
