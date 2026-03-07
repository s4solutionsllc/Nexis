#include <QTest>
#include <QFile>
#include <QTextStream>
#include "Info/cpu_info.h"

class TestCpuInfo : public QObject
{
    Q_OBJECT

private:
    QStringList loadFixtureLines(const QString &name) const
    {
        QString path = QStringLiteral(PROJECT_SOURCE_DIR)
                        + "/tests/fixtures/cpu/" + name;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        return QTextStream(&f).readAll().trimmed().split('\n');
    }

    QString loadFixtureString(const QString &name) const
    {
        QString path = QStringLiteral(PROJECT_SOURCE_DIR)
                        + "/tests/fixtures/cpu/" + name;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        return QTextStream(&f).readAll();
    }

private slots:
    // parsePhysicalCoreCount
    void physical_x86SingleSocket();
    void physical_dualSocket();
    void physical_arm64NoCoreId();
    void physical_emptyInput();

    // parseLogicalCoreCount
    void logical_x86SingleSocket();
    void logical_dualSocket();
    void logical_arm64();
    void logical_emptyInput();

    // parseAvgClockFromLscpu
    void avgClock_standardLscpu();
    void avgClock_noMhzField();
    void avgClock_emptyInput();

    // parseClocksFromProcCpuinfo
    void clocks_x86SingleSocket();
    void clocks_arm64NoMhz();
    void clocks_emptyInput();

    // parseLoadAvgs
    void loadAvgs_normal();
    void loadAvgs_high();
    void loadAvgs_emptyInput();
    void loadAvgs_insufficientFields();
};

// --- parsePhysicalCoreCount tests ---

void TestCpuInfo::physical_x86SingleSocket()
{
    QStringList lines = loadFixtureLines("proc_cpuinfo_x86_single.txt");
    QVERIFY(!lines.isEmpty());

    int count = CpuInfo::parsePhysicalCoreCount(lines);
    // 1 socket, 6 cores, 12 logical (HT) → 6 unique (physical_id, core_id) pairs
    QCOMPARE(count, 6);
}

void TestCpuInfo::physical_dualSocket()
{
    QStringList lines = loadFixtureLines("proc_cpuinfo_dual_socket.txt");
    QVERIFY(!lines.isEmpty());

    int count = CpuInfo::parsePhysicalCoreCount(lines);
    // 2 sockets x 4 cores = 8 unique pairs
    QCOMPARE(count, 8);
}

void TestCpuInfo::physical_arm64NoCoreId()
{
    QStringList lines = loadFixtureLines("proc_cpuinfo_arm64.txt");
    QVERIFY(!lines.isEmpty());

    int count = CpuInfo::parsePhysicalCoreCount(lines);
    // ARM /proc/cpuinfo has no physical id / core id fields → 0
    QCOMPARE(count, 0);
}

void TestCpuInfo::physical_emptyInput()
{
    QStringList lines;
    int count = CpuInfo::parsePhysicalCoreCount(lines);
    QCOMPARE(count, 0);
}

// --- parseLogicalCoreCount tests ---

void TestCpuInfo::logical_x86SingleSocket()
{
    QStringList lines = loadFixtureLines("proc_cpuinfo_x86_single.txt");
    QVERIFY(!lines.isEmpty());

    int count = CpuInfo::parseLogicalCoreCount(lines);
    QCOMPARE(count, 12);
}

void TestCpuInfo::logical_dualSocket()
{
    QStringList lines = loadFixtureLines("proc_cpuinfo_dual_socket.txt");
    QVERIFY(!lines.isEmpty());

    int count = CpuInfo::parseLogicalCoreCount(lines);
    QCOMPARE(count, 8);
}

void TestCpuInfo::logical_arm64()
{
    QStringList lines = loadFixtureLines("proc_cpuinfo_arm64.txt");
    QVERIFY(!lines.isEmpty());

    int count = CpuInfo::parseLogicalCoreCount(lines);
    QCOMPARE(count, 4);
}

void TestCpuInfo::logical_emptyInput()
{
    QStringList lines;
    int count = CpuInfo::parseLogicalCoreCount(lines);
    QCOMPARE(count, 0);
}

// --- parseAvgClockFromLscpu tests ---

void TestCpuInfo::avgClock_standardLscpu()
{
    QString content = loadFixtureString("lscpu_standard.txt");
    QVERIFY(!content.isEmpty());

    double mhz = CpuInfo::parseAvgClockFromLscpu(content);
    QCOMPARE(mhz, 3192.0);
}

void TestCpuInfo::avgClock_noMhzField()
{
    QString content = loadFixtureString("lscpu_no_mhz.txt");
    QVERIFY(!content.isEmpty());

    double mhz = CpuInfo::parseAvgClockFromLscpu(content);
    QCOMPARE(mhz, 0.0);
}

void TestCpuInfo::avgClock_emptyInput()
{
    double mhz = CpuInfo::parseAvgClockFromLscpu("");
    QCOMPARE(mhz, 0.0);
}

// --- parseClocksFromProcCpuinfo tests ---

void TestCpuInfo::clocks_x86SingleSocket()
{
    QStringList lines = loadFixtureLines("proc_cpuinfo_x86_single.txt");
    QVERIFY(!lines.isEmpty());

    QList<double> clocks = CpuInfo::parseClocksFromProcCpuinfo(lines);
    QCOMPARE(clocks.size(), 12);
    // First core: 3192.000 MHz
    QCOMPARE(clocks.at(0), 3192.0);
    // Second core: 3200.100 MHz
    QCOMPARE(clocks.at(1), 3200.1);
    // All values should be positive
    for (double c : clocks)
        QVERIFY(c > 0.0);
}

void TestCpuInfo::clocks_arm64NoMhz()
{
    QStringList lines = loadFixtureLines("proc_cpuinfo_arm64.txt");
    QVERIFY(!lines.isEmpty());

    QList<double> clocks = CpuInfo::parseClocksFromProcCpuinfo(lines);
    // ARM /proc/cpuinfo typically has no "cpu MHz" lines
    QCOMPARE(clocks.size(), 0);
}

void TestCpuInfo::clocks_emptyInput()
{
    QStringList lines;
    QList<double> clocks = CpuInfo::parseClocksFromProcCpuinfo(lines);
    QCOMPARE(clocks.size(), 0);
}

// --- parseLoadAvgs tests ---

void TestCpuInfo::loadAvgs_normal()
{
    QString content = loadFixtureString("proc_loadavg_normal.txt");
    QVERIFY(!content.isEmpty());

    QList<double> avgs = CpuInfo::parseLoadAvgs(content);
    QCOMPARE(avgs.size(), 3);
    QCOMPARE(avgs.at(0), 0.52);
    QCOMPARE(avgs.at(1), 0.34);
    QCOMPARE(avgs.at(2), 0.28);
}

void TestCpuInfo::loadAvgs_high()
{
    QString content = loadFixtureString("proc_loadavg_high.txt");
    QVERIFY(!content.isEmpty());

    QList<double> avgs = CpuInfo::parseLoadAvgs(content);
    QCOMPARE(avgs.size(), 3);
    QCOMPARE(avgs.at(0), 24.5);
    QCOMPARE(avgs.at(1), 18.3);
    QCOMPARE(avgs.at(2), 12.1);
}

void TestCpuInfo::loadAvgs_emptyInput()
{
    QList<double> avgs = CpuInfo::parseLoadAvgs("");
    QCOMPARE(avgs.size(), 3);
    // Should return defaults {0, 0, 0}
    QCOMPARE(avgs.at(0), 0.0);
    QCOMPARE(avgs.at(1), 0.0);
    QCOMPARE(avgs.at(2), 0.0);
}

void TestCpuInfo::loadAvgs_insufficientFields()
{
    // Only 2 values (need at least 3)
    QList<double> avgs = CpuInfo::parseLoadAvgs("0.50 0.30");
    QCOMPARE(avgs.size(), 3);
    QCOMPARE(avgs.at(0), 0.0);
    QCOMPARE(avgs.at(1), 0.0);
    QCOMPARE(avgs.at(2), 0.0);
}

QTEST_MAIN(TestCpuInfo)
#include "test_cpu_info.moc"
