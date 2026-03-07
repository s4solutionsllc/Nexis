#include <QTest>
#include <QFile>
#include <QTextStream>
#include "Info/memory_info.h"

class TestMemoryInfo : public QObject
{
    Q_OBJECT

private:
    QStringList loadFixture(const QString &name) const
    {
        QString path = QStringLiteral(PROJECT_SOURCE_DIR)
                        + "/tests/fixtures/memory/" + name;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        return QTextStream(&f).readAll().trimmed().split('\n');
    }

    static constexpr quint64 KB = 1024;

private slots:
    void parse_standardMeminfo();
    void parse_reorderedFields();
    void parse_missingOptionalFields();
    void parse_noSwap();
    void parse_largeValues();
    void parse_emptyFile();
    void derive_normalValues();
    void derive_underflowGuard();
    void pressure_psiNormal();
    void pressure_psiWarning();
    void pressure_psiCritical();
    void pressure_heuristicNormal();
    void pressure_heuristicCritical();
    void pressure_unavailable();
};

void TestMemoryInfo::parse_standardMeminfo()
{
    QStringList lines = loadFixture("standard.txt");
    QVERIFY(!lines.isEmpty());

    MemoryParseResult r = MemoryInfo::parseProcMeminfo(lines);

    QCOMPARE(r.memTotal,      quint64(16384000) * KB);
    QCOMPARE(r.memFree,       quint64(2048000)  * KB);
    QCOMPARE(r.buffers,       quint64(512000)   * KB);
    QCOMPARE(r.cached,        quint64(4096000)  * KB);
    QCOMPARE(r.swapTotal,     quint64(8192000)  * KB);
    QCOMPARE(r.swapFree,      quint64(7168000)  * KB);
    QCOMPARE(r.shmem,         quint64(256000)   * KB);
    QCOMPARE(r.sreclaimable,  quint64(768000)   * KB);
    QCOMPARE(r.memAvailable,  quint64(8192000)  * KB);
    QCOMPARE(r.memActive,     quint64(6144000)  * KB);
    QCOMPARE(r.memInactive,   quint64(4096000)  * KB);
}

void TestMemoryInfo::parse_reorderedFields()
{
    // BUG-01 regression: fields in different order must produce same results
    QStringList standard = loadFixture("standard.txt");
    QStringList reordered = loadFixture("reordered.txt");
    QVERIFY(!standard.isEmpty());
    QVERIFY(!reordered.isEmpty());

    MemoryParseResult rStd = MemoryInfo::parseProcMeminfo(standard);
    MemoryParseResult rReord = MemoryInfo::parseProcMeminfo(reordered);

    QCOMPARE(rReord.memTotal,     rStd.memTotal);
    QCOMPARE(rReord.memFree,      rStd.memFree);
    QCOMPARE(rReord.buffers,      rStd.buffers);
    QCOMPARE(rReord.cached,       rStd.cached);
    QCOMPARE(rReord.shmem,        rStd.shmem);
    QCOMPARE(rReord.sreclaimable, rStd.sreclaimable);
    QCOMPARE(rReord.swapTotal,    rStd.swapTotal);
    QCOMPARE(rReord.swapFree,     rStd.swapFree);
}

void TestMemoryInfo::parse_missingOptionalFields()
{
    // Old kernels (pre-3.14) may lack MemAvailable, SReclaimable, Shmem
    QStringList lines = loadFixture("minimal.txt");
    QVERIFY(!lines.isEmpty());

    MemoryParseResult r = MemoryInfo::parseProcMeminfo(lines);

    QCOMPARE(r.memTotal,      quint64(16384000) * KB);
    QCOMPARE(r.memFree,       quint64(2048000)  * KB);
    QCOMPARE(r.shmem,         quint64(0));
    QCOMPARE(r.sreclaimable,  quint64(0));
    QCOMPARE(r.memAvailable,  quint64(0));
    QCOMPARE(r.memActive,     quint64(0));
    QCOMPARE(r.memInactive,   quint64(0));
}

void TestMemoryInfo::parse_noSwap()
{
    QStringList lines = loadFixture("no_swap.txt");
    QVERIFY(!lines.isEmpty());

    MemoryParseResult r = MemoryInfo::parseProcMeminfo(lines);

    QCOMPARE(r.swapTotal, quint64(0));
    QCOMPARE(r.swapFree,  quint64(0));
}

void TestMemoryInfo::parse_largeValues()
{
    // BUG-29 regression: 64 GB system must parse without truncation
    QStringList lines = loadFixture("large_values.txt");
    QVERIFY(!lines.isEmpty());

    MemoryParseResult r = MemoryInfo::parseProcMeminfo(lines);

    QCOMPARE(r.memTotal, quint64(67108864) * KB);  // 64 GiB
    QVERIFY(r.memTotal > quint64(4294967295));      // > 4 GB (exceeds 32-bit)
}

void TestMemoryInfo::parse_emptyFile()
{
    // BUG-27 regression: empty input must not crash
    QStringList lines;
    MemoryParseResult r = MemoryInfo::parseProcMeminfo(lines);

    QCOMPARE(r.memTotal, quint64(0));
    QCOMPARE(r.memFree,  quint64(0));
    QCOMPARE(r.cached,   quint64(0));
}

void TestMemoryInfo::derive_normalValues()
{
    MemoryParseResult r;
    r.memTotal = 16000 * KB;
    r.memFree = 2000 * KB;
    r.buffers = 500 * KB;
    r.cached = 4000 * KB;
    r.sreclaimable = 800 * KB;
    r.shmem = 300 * KB;
    r.swapTotal = 8000 * KB;
    r.swapFree = 7000 * KB;

    MemoryInfo::deriveMemoryValues(r);

    // cached = (4000 + 800 - 300) * KB = 4500 * KB
    QCOMPARE(r.cached, quint64(4500) * KB);
    // memUsed = 16000 - (2000 + 500 + 4500) = 9000
    QCOMPARE(r.memUsed, quint64(9000) * KB);
    // swapUsed = 8000 - 7000 = 1000
    QCOMPARE(r.swapUsed, quint64(1000) * KB);
}

void TestMemoryInfo::derive_underflowGuard()
{
    // BUG-96: shmem > cached + sreclaimable should not wrap quint64
    MemoryParseResult r;
    r.memTotal = 16000 * KB;
    r.memFree = 2000 * KB;
    r.buffers = 500 * KB;
    r.cached = 100 * KB;
    r.sreclaimable = 50 * KB;
    r.shmem = 500 * KB;     // shmem > cached + sreclaimable
    r.swapTotal = 0;
    r.swapFree = 0;

    MemoryInfo::deriveMemoryValues(r);

    QCOMPARE(r.cached, quint64(0));
    QVERIFY(r.memUsed <= r.memTotal);
    QCOMPARE(r.swapUsed, quint64(0));
}

void TestMemoryInfo::pressure_psiNormal()
{
    int level = MemoryInfo::parsePressureLevel(
        "some avg10=2.50 avg60=1.00 avg300=0.50 total=12345", 0, 0);
    QCOMPARE(level, 1);
}

void TestMemoryInfo::pressure_psiWarning()
{
    int level = MemoryInfo::parsePressureLevel(
        "some avg10=25.00 avg60=10.00 avg300=5.00 total=12345", 0, 0);
    QCOMPARE(level, 2);
}

void TestMemoryInfo::pressure_psiCritical()
{
    int level = MemoryInfo::parsePressureLevel(
        "some avg10=75.00 avg60=50.00 avg300=30.00 total=12345", 0, 0);
    QCOMPARE(level, 4);
}

void TestMemoryInfo::pressure_heuristicNormal()
{
    // No PSI content, MemAvailable = 50% of MemTotal → normal
    int level = MemoryInfo::parsePressureLevel(
        "", quint64(8000) * KB, quint64(16000) * KB);
    QCOMPARE(level, 1);
}

void TestMemoryInfo::pressure_heuristicCritical()
{
    // No PSI, MemAvailable = 5% of MemTotal → critical
    int level = MemoryInfo::parsePressureLevel(
        "", quint64(800) * KB, quint64(16000) * KB);
    QCOMPARE(level, 4);
}

void TestMemoryInfo::pressure_unavailable()
{
    // No PSI, no MemAvailable → unavailable
    int level = MemoryInfo::parsePressureLevel("", 0, 0);
    QCOMPARE(level, -1);
}

QTEST_MAIN(TestMemoryInfo)
#include "test_memory_info.moc"
