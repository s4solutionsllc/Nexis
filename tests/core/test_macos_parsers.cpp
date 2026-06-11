// WI-33: fixture tests for the macOS live-tool parsers. The parser sources
// are compiled directly into this test target (FR-127 pattern), so the tests
// run on any platform — even though the parsers are only wired into the real
// nexis-core build on macOS.

#include <QFile>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QVariant>

#include "boot_analysis_info_macos.h"
#include "macos_plist_parser.h"
#include "nettop_streamer.h"

class TestMacosParsers : public QObject
{
    Q_OBJECT

private:
    QByteArray loadFixture(const QString &relPath) const
    {
        const QString path = QStringLiteral(PROJECT_SOURCE_DIR)
                             + "/tests/fixtures/macos/" + relPath;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return {};
        return f.readAll();
    }

private slots:
    // NettopStreamer::parseCsvLine
    void nettop_perProcessLine_bytesAtPositions12();
    void nettop_perProcessLine_bytesAtPositions23();
    void nettop_headerLineRejected();
    void nettop_procFieldWithEmbeddedDots();
    void nettop_emptyLineRejected();
    void nettop_tooFewFieldsRejected();
    void nettop_nonNumericPidRejected();
    void nettop_fixtureBasic();
    void nettop_fixtureProcsWithDots();

    // MacosPlistParser::parse
    void plist_diskutilList_extractsWholeDisks();
    void plist_diskutilInfoAppleFabric_extractsSmartFields();
    void plist_diskutilInfoDiskImage_busProtocol();
    void plist_diskutilInfoExternalSsd_solidStateTrue();
    void plist_emptyInputYieldsEmptyMap();

    // BootAnalysisInfoMacOS::parseKernBoottime
    void bootTime_extractsSecondsFromSysctl();
    void bootTime_malformedReturnsMinusOne();
    void bootTime_extraWhitespaceTolerated();
};

// ── NettopStreamer ─────────────────────────────────────────────────────────────

void TestMacosParsers::nettop_perProcessLine_bytesAtPositions12()
{
    // `procname.PID,N,M,` — bytes in fields 1+2 (trailing-comma 4-field form).
    pid_t pid = 0;
    quint64 in = 0;
    quint64 out = 0;
    QVERIFY(NettopStreamer::parseCsvLine(
        QStringLiteral("firefox.1234,524288,1048576,"),
        &pid, &in, &out));
    QCOMPARE(pid, pid_t(1234));
    QCOMPARE(in, quint64(524288));
    QCOMPARE(out, quint64(1048576));
}

void TestMacosParsers::nettop_perProcessLine_bytesAtPositions23()
{
    // `procname.PID,,N,M,` — bytes in fields 2+3 (5-field, empty interface col).
    // Mirrors the older nettop output shape we keep a fallback for.
    pid_t pid = 0;
    quint64 in = 0;
    quint64 out = 0;
    QVERIFY(NettopStreamer::parseCsvLine(
        QStringLiteral("Safari.5678,,16384,32768,"),
        &pid, &in, &out));
    QCOMPARE(pid, pid_t(5678));
    QCOMPARE(in, quint64(16384));
    QCOMPARE(out, quint64(32768));
}

void TestMacosParsers::nettop_headerLineRejected()
{
    pid_t pid = 0;
    quint64 in = 0;
    quint64 out = 0;
    // "time,,bytes_in,bytes_out" — proc field has no '.' to split a numeric pid
    // off, so the parser rejects it without writing the out-params.
    QVERIFY(!NettopStreamer::parseCsvLine(
        QStringLiteral("time,,bytes_in,bytes_out"),
        &pid, &in, &out));
    QCOMPARE(pid, pid_t(0));
}

void TestMacosParsers::nettop_procFieldWithEmbeddedDots()
{
    // Bundle-style proc names like "com.apple.WebKit.WebContent.4321" must
    // split on the LAST '.' to recover the pid.
    pid_t pid = 0;
    quint64 in = 0;
    quint64 out = 0;
    QVERIFY(NettopStreamer::parseCsvLine(
        QStringLiteral("com.apple.WebKit.WebContent.4321,2048,1024,"),
        &pid, &in, &out));
    QCOMPARE(pid, pid_t(4321));
    QCOMPARE(in, quint64(2048));
    QCOMPARE(out, quint64(1024));
}

void TestMacosParsers::nettop_emptyLineRejected()
{
    pid_t pid = 0;
    QVERIFY(!NettopStreamer::parseCsvLine(QString(), &pid, nullptr, nullptr));
}

void TestMacosParsers::nettop_tooFewFieldsRejected()
{
    pid_t pid = 0;
    // Only 3 fields → below the parser's minimum of 4.
    QVERIFY(!NettopStreamer::parseCsvLine(
        QStringLiteral("firefox.1234,524288,1048576"), &pid, nullptr, nullptr));
}

void TestMacosParsers::nettop_nonNumericPidRejected()
{
    pid_t pid = 0;
    QVERIFY(!NettopStreamer::parseCsvLine(
        QStringLiteral("not-a-pid.abcd,,1,2"), &pid, nullptr, nullptr));
}

void TestMacosParsers::nettop_fixtureBasic()
{
    const QByteArray content = loadFixture("nettop/per_process_basic.csv");
    QVERIFY(!content.isEmpty());

    const QStringList lines = QString::fromUtf8(content).split('\n', Qt::SkipEmptyParts);
    QMap<pid_t, QPair<quint64, quint64>> parsed;
    for (const QString &line : lines) {
        pid_t pid = 0;
        quint64 in = 0;
        quint64 out = 0;
        if (NettopStreamer::parseCsvLine(line.trimmed(), &pid, &in, &out))
            parsed.insert(pid, qMakePair(in, out));
    }
    // Header line `time,bytes_in,bytes_out` is rejected; three per-process
    // lines parse — including the one with spaces in the proc name.
    QCOMPARE(parsed.size(), 3);
    QCOMPARE(parsed.value(1234).first, quint64(524288));
    QCOMPARE(parsed.value(1234).second, quint64(1048576));
    QCOMPARE(parsed.value(5678).first, quint64(8192));
    QCOMPARE(parsed.value(99).first, quint64(128));
}

void TestMacosParsers::nettop_fixtureProcsWithDots()
{
    const QByteArray content = loadFixture("nettop/proc_with_dots.csv");
    QVERIFY(!content.isEmpty());

    const QStringList lines = QString::fromUtf8(content).split('\n', Qt::SkipEmptyParts);
    QMap<pid_t, QPair<quint64, quint64>> parsed;
    for (const QString &line : lines) {
        pid_t pid = 0;
        quint64 in = 0;
        quint64 out = 0;
        if (NettopStreamer::parseCsvLine(line.trimmed(), &pid, &in, &out))
            parsed.insert(pid, qMakePair(in, out));
    }
    QCOMPARE(parsed.size(), 2);
    QVERIFY(parsed.contains(4321));
    QVERIFY(parsed.contains(55555));
}

// ── MacosPlistParser ──────────────────────────────────────────────────────────

void TestMacosParsers::plist_diskutilList_extractsWholeDisks()
{
    const QByteArray content = loadFixture("diskutil/list.plist");
    QVERIFY(!content.isEmpty());

    QMap<QString, QVariant> result = MacosPlistParser::parse(content);
    const QStringList wholeDisks = result.value("WholeDisks").toStringList();
    QCOMPARE(wholeDisks.size(), 4);
    QCOMPARE(wholeDisks.at(0), QStringLiteral("disk0"));
    QCOMPARE(wholeDisks.at(3), QStringLiteral("disk3"));
}

void TestMacosParsers::plist_diskutilInfoAppleFabric_extractsSmartFields()
{
    const QByteArray content = loadFixture("diskutil/info_apple_fabric_nvme.plist");
    QVERIFY(!content.isEmpty());

    QMap<QString, QVariant> info = MacosPlistParser::parse(content);

    // Top-level keys
    QCOMPARE(info.value("BusProtocol").toString(), QStringLiteral("Apple Fabric"));
    QCOMPARE(info.value("MediaName").toString(),   QStringLiteral("APPLE SSD AP1024Z Media"));
    QCOMPARE(info.value("TotalSize").toLongLong(), qint64(994662584320));
    QCOMPARE(info.value("SMARTStatus").toString(), QStringLiteral("Verified"));
    QCOMPARE(info.value("SolidState").toBool(),    true);

    // Nested SMART dict gets flattened with "SMART." prefix.
    QCOMPARE(info.value("SMART.TEMPERATURE").toInt(),        312);
    QCOMPARE(info.value("SMART.PERCENTAGE_USED").toInt(),    3);
    QCOMPARE(info.value("SMART.AVAILABLE_SPARE").toInt(),    100);
    QCOMPARE(info.value("SMART.POWER_ON_HOURS_0").toInt(),   1837);
    QCOMPARE(info.value("SMART.POWER_CYCLES_0").toInt(),     412);
    QCOMPARE(info.value("SMART.UNSAFE_SHUTDOWNS_0").toInt(), 17);
    QCOMPARE(info.value("SMART.DATA_UNITS_READ_0").toLongLong(),
             qint64(123456789));
    QCOMPARE(info.value("SMART.DATA_UNITS_WRITTEN_0").toLongLong(),
             qint64(987654321));
    QCOMPARE(info.value("SMART.NUM_ERROR_INFO_LOG_ENTRIES_0").toInt(), 0);
}

void TestMacosParsers::plist_diskutilInfoDiskImage_busProtocol()
{
    const QByteArray content = loadFixture("diskutil/info_disk_image.plist");
    QVERIFY(!content.isEmpty());

    QMap<QString, QVariant> info = MacosPlistParser::parse(content);
    // Mount-style disk-image drives are filtered out by the caller via
    // BusProtocol; this test only asserts the parser surfaces the value.
    QCOMPARE(info.value("BusProtocol").toString(), QStringLiteral("Disk Image"));
    QCOMPARE(info.value("SolidState").toBool(),    false);
}

void TestMacosParsers::plist_diskutilInfoExternalSsd_solidStateTrue()
{
    const QByteArray content = loadFixture("diskutil/info_external_sata_ssd.plist");
    QVERIFY(!content.isEmpty());

    QMap<QString, QVariant> info = MacosPlistParser::parse(content);
    QCOMPARE(info.value("BusProtocol").toString(), QStringLiteral("USB"));
    QCOMPARE(info.value("SolidState").toBool(),    true);
    QCOMPARE(info.value("SMARTStatus").toString(), QStringLiteral("Not Supported"));
}

void TestMacosParsers::plist_emptyInputYieldsEmptyMap()
{
    QCOMPARE(MacosPlistParser::parse(QByteArray()).size(), 0);
    QCOMPARE(MacosPlistParser::parse(QByteArray("not xml")).size(), 0);
}

// ── BootAnalysisInfoMacOS ─────────────────────────────────────────────────────

void TestMacosParsers::bootTime_extractsSecondsFromSysctl()
{
    const QByteArray content = loadFixture("sysctl/kern_boottime.txt");
    QVERIFY(!content.isEmpty());

    const qint64 bootSec = BootAnalysisInfoMacOS::parseKernBoottime(
        QString::fromUtf8(content));
    QCOMPARE(bootSec, qint64(1717800000));
}

void TestMacosParsers::bootTime_malformedReturnsMinusOne()
{
    const QByteArray content = loadFixture("sysctl/kern_boottime_malformed.txt");
    QVERIFY(!content.isEmpty());

    QCOMPARE(BootAnalysisInfoMacOS::parseKernBoottime(QString::fromUtf8(content)),
             qint64(-1));
}

void TestMacosParsers::bootTime_extraWhitespaceTolerated()
{
    // `sec   =   N` with extra whitespace inside the brace block.
    QCOMPARE(BootAnalysisInfoMacOS::parseKernBoottime(
        QStringLiteral("kern.boottime: { sec   =   1700000000, usec = 0 } ...")),
        qint64(1700000000));
}

QTEST_MAIN(TestMacosParsers)
#include "test_macos_parsers.moc"
