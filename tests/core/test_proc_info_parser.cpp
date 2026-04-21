#include <QDateTime>
#include <QFile>
#include <QTest>

#include "proc_info_parser.h"

class TestProcInfoParser : public QObject
{
    Q_OBJECT

private:
    QByteArray loadFixture(const QString &name) const
    {
        const QString path = QStringLiteral(PROJECT_SOURCE_DIR)
                             + "/tests/fixtures/proc/" + name;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return {};
        return f.readAll();
    }

private slots:
    // parseStat
    void stat_simple();
    void stat_commWithParens();
    void stat_commWithSpaces();
    void stat_malformedMissingParens();
    void stat_malformedTooFewFields();

    // parseStatus
    void status_extractsUidAndGid();
    void status_missingUid();

    // formatCmdline
    void cmdline_nullSeparated();
    void cmdline_emptyFallsBackToComm();
    void cmdline_trailingNulls();

    // parseBootTime / parseMemTotalBytes / parseUptimeSec
    void bootTime_extractedFromProcStat();
    void bootTime_missingReturnsZero();
    void memTotal_convertsKibToBytes();
    void memTotal_missingReturnsZero();
    void uptime_firstFieldOnly();

    // formatStartTime / formatCpuTime
    void startTime_sameDayReturnsHHMM();
    void startTime_pastYearReturnsYear();
    void cpuTime_subDay();
    void cpuTime_multiDay();
    void cpuTime_zeroClkTck();

    // parseDrmFdinfo
    void drm_intelPopulatesAllFields();
    void drm_amdSumsMultipleEngines();
    void drm_nonDrmReturnsFalse();
    void drm_handlesMiBSuffix();
};

// -------- parseStat --------

void TestProcInfoParser::stat_simple()
{
    const QByteArray content = loadFixture("stat_simple.txt");
    QVERIFY(!content.isEmpty());

    ProcInfoParser::StatFields f;
    QVERIFY(ProcInfoParser::parseStat(content, f));

    QCOMPARE(f.comm, QString("bash"));
    QCOMPARE(f.state, QChar('S'));
    QCOMPARE(f.session, qint64(1234));
    QCOMPARE(f.utime, quint64(102));
    QCOMPARE(f.stime, quint64(45));
    QCOMPARE(f.nice, 0);
    QCOMPARE(f.starttime, quint64(987654));
    QCOMPARE(f.vsize, quint64(12345678));
    QCOMPARE(f.rssPages, quint64(456));
}

void TestProcInfoParser::stat_commWithParens()
{
    const QByteArray content = loadFixture("stat_comm_with_parens.txt");
    QVERIFY(!content.isEmpty());

    ProcInfoParser::StatFields f;
    QVERIFY(ProcInfoParser::parseStat(content, f));

    // The LAST ')' must delimit comm — the inner "(foo)" stays intact.
    QCOMPARE(f.comm, QString("Web Content (foo)"));
    QCOMPARE(f.state, QChar('R'));
    QCOMPARE(f.utime, quint64(8000));
    QCOMPARE(f.stime, quint64(2500));
}

void TestProcInfoParser::stat_commWithSpaces()
{
    const QByteArray content = loadFixture("stat_comm_with_spaces.txt");
    QVERIFY(!content.isEmpty());

    ProcInfoParser::StatFields f;
    QVERIFY(ProcInfoParser::parseStat(content, f));

    QCOMPARE(f.comm, QString("pool worker 2"));
    QCOMPARE(f.state, QChar('S'));
    QCOMPARE(f.nice, 0);
}

void TestProcInfoParser::stat_malformedMissingParens()
{
    ProcInfoParser::StatFields f;
    QVERIFY(!ProcInfoParser::parseStat(QByteArray("1234 bash S 1 2 3"), f));
}

void TestProcInfoParser::stat_malformedTooFewFields()
{
    ProcInfoParser::StatFields f;
    // Has parens but only a handful of trailing fields.
    QVERIFY(!ProcInfoParser::parseStat(QByteArray("1234 (bash) S 1 2 3"), f));
}

// -------- parseStatus --------

void TestProcInfoParser::status_extractsUidAndGid()
{
    const QByteArray content = loadFixture("status_simple.txt");
    QVERIFY(!content.isEmpty());

    ProcInfoParser::StatusFields f;
    QVERIFY(ProcInfoParser::parseStatus(content, f));
    QVERIFY(f.hasUid);
    QVERIFY(f.hasGid);
    QCOMPARE(f.uid, quint32(1000));
    QCOMPARE(f.gid, quint32(1000));
}

void TestProcInfoParser::status_missingUid()
{
    ProcInfoParser::StatusFields f;
    QVERIFY(!ProcInfoParser::parseStatus(QByteArray("Name:\tfoo\n"), f));
    QVERIFY(!f.hasUid);
}

// -------- formatCmdline --------

void TestProcInfoParser::cmdline_nullSeparated()
{
    QByteArray cl;
    cl.append("python3");
    cl.append('\0');
    cl.append("script.py");
    cl.append('\0');
    cl.append("--flag");
    cl.append('\0');
    QCOMPARE(ProcInfoParser::formatCmdline(cl, "python3"),
             QString("python3 script.py --flag"));
}

void TestProcInfoParser::cmdline_emptyFallsBackToComm()
{
    QCOMPARE(ProcInfoParser::formatCmdline(QByteArray(), "kthreadd"),
             QString("[kthreadd]"));
}

void TestProcInfoParser::cmdline_trailingNulls()
{
    QByteArray cl;
    cl.append("echo");
    cl.append('\0');
    cl.append("hi");
    cl.append('\0');
    cl.append('\0');
    cl.append('\0');
    QCOMPARE(ProcInfoParser::formatCmdline(cl, "echo"), QString("echo hi"));
}

// -------- parseBootTime / parseMemTotalBytes / parseUptimeSec --------

void TestProcInfoParser::bootTime_extractedFromProcStat()
{
    const QByteArray content = loadFixture("stat_line.txt");
    QVERIFY(!content.isEmpty());
    QCOMPARE(ProcInfoParser::parseBootTime(content), quint64(1745100000));
}

void TestProcInfoParser::bootTime_missingReturnsZero()
{
    QCOMPARE(ProcInfoParser::parseBootTime(QByteArray("cpu 1 2 3\nintr 0\n")),
             quint64(0));
}

void TestProcInfoParser::memTotal_convertsKibToBytes()
{
    const QByteArray content = loadFixture("meminfo.txt");
    QVERIFY(!content.isEmpty());
    QCOMPARE(ProcInfoParser::parseMemTotalBytes(content),
             quint64(16318540) * 1024ULL);
}

void TestProcInfoParser::memTotal_missingReturnsZero()
{
    QCOMPARE(ProcInfoParser::parseMemTotalBytes(QByteArray("Cached: 100 kB\n")),
             quint64(0));
}

void TestProcInfoParser::uptime_firstFieldOnly()
{
    const QByteArray content = loadFixture("uptime.txt");
    QVERIFY(!content.isEmpty());
    QCOMPARE(ProcInfoParser::parseUptimeSec(content), 123456.78);
}

// -------- formatStartTime / formatCpuTime --------

void TestProcInfoParser::startTime_sameDayReturnsHHMM()
{
    const quint64 boot = 1745100000;   // fixed
    const long clkTck = 100;
    // Process started 5 min after boot → 1745100300 = same-day-ish for "now".
    const quint64 starttimeTicks = 5 * 60 * 100;
    const qint64 now = 1745100000 + 600;   // 10 min after boot

    QString s = ProcInfoParser::formatStartTime(boot, starttimeTicks, clkTck, now);
    // Should be "HH:mm" — not a long year/date string.
    QVERIFY(s.length() == 5);
    QVERIFY(s.contains(':'));
}

void TestProcInfoParser::startTime_pastYearReturnsYear()
{
    // Use a mid-year UTC timestamp so any reasonable local timezone still
    // lands on the same calendar year.
    const quint64 boot = 962409600;   // 2000-07-01 12:00 UTC
    const long clkTck = 100;
    const quint64 starttimeTicks = 60 * 100;   // 1 min after boot
    const qint64 now = 1745100000;             // 2025-ish

    QString s = ProcInfoParser::formatStartTime(boot, starttimeTicks, clkTck, now);
    QCOMPARE(s.length(), 4);   // "2000"
    QCOMPARE(s, QString("2000"));
}

void TestProcInfoParser::cpuTime_subDay()
{
    // 3725 seconds = 01:02:05
    const quint64 ticks = 3725 * 100;
    QCOMPARE(ProcInfoParser::formatCpuTime(ticks, 100), QString("01:02:05"));
}

void TestProcInfoParser::cpuTime_multiDay()
{
    // 2 days 3h 4m 5s
    const quint64 totalSec = 2 * 86400 + 3 * 3600 + 4 * 60 + 5;
    QCOMPARE(ProcInfoParser::formatCpuTime(totalSec * 100, 100),
             QString("2-03:04:05"));
}

void TestProcInfoParser::cpuTime_zeroClkTck()
{
    QCOMPARE(ProcInfoParser::formatCpuTime(12345, 0), QString("00:00:00"));
}

// -------- parseDrmFdinfo --------

void TestProcInfoParser::drm_intelPopulatesAllFields()
{
    const QByteArray content = loadFixture("fdinfo_drm_intel.txt");
    QVERIFY(!content.isEmpty());

    ProcInfoParser::DrmFdinfo f;
    QVERIFY(ProcInfoParser::parseDrmFdinfo(content, f));

    QCOMPARE(f.driver, QString("i915"));
    QCOMPARE(f.clientId, qint64(12345));
    QCOMPARE(f.engineNs, quint64(1234567890));        // render + video (0)
    QCOMPARE(f.memVramB, quint64(524288) * 1024ULL);  // KiB → bytes
}

void TestProcInfoParser::drm_amdSumsMultipleEngines()
{
    const QByteArray content = loadFixture("fdinfo_drm_amd.txt");
    QVERIFY(!content.isEmpty());

    ProcInfoParser::DrmFdinfo f;
    QVERIFY(ProcInfoParser::parseDrmFdinfo(content, f));

    QCOMPARE(f.driver, QString("amdgpu"));
    // gfx + compute + dma
    QCOMPARE(f.engineNs, quint64(5000000000) + 2000000000 + 100000000);
    QCOMPARE(f.memVramB, quint64(2097152) * 1024ULL);
}

void TestProcInfoParser::drm_nonDrmReturnsFalse()
{
    const QByteArray content = loadFixture("fdinfo_non_drm.txt");
    QVERIFY(!content.isEmpty());

    ProcInfoParser::DrmFdinfo f;
    QVERIFY(!ProcInfoParser::parseDrmFdinfo(content, f));
    QCOMPARE(f.engineNs, quint64(0));
    QCOMPARE(f.memVramB, quint64(0));
}

void TestProcInfoParser::drm_handlesMiBSuffix()
{
    const QByteArray content =
        "drm-driver:\txe\n"
        "drm-engine-render:\t100 ns\n"
        "drm-memory-vram:\t512 MiB\n";

    ProcInfoParser::DrmFdinfo f;
    QVERIFY(ProcInfoParser::parseDrmFdinfo(content, f));
    QCOMPARE(f.memVramB, quint64(512) * 1024ULL * 1024ULL);
}

QTEST_MAIN(TestProcInfoParser)
#include "test_proc_info_parser.moc"
