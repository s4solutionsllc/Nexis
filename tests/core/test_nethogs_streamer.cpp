// SSO-15379: fixture tests for NethogsStreamer::parseLine(). Compiled
// directly into this test target (FR-127 pattern) so the parser runs on any
// platform, even though nethogs_streamer.cpp is only wired into the real
// nexis-core build on Linux.
//
// Caveat (see nethogs_streamer.h): the tracemode format below reflects
// nethogs' long-documented `-t` output, not a live capture — this
// environment has no nethogs binary and no CAP_NET_RAW/root to run one
// against. If the real output format differs, only this fixture (and the
// parser) need updating, not the collection/UI wiring around it.

#include <QTest>

#include "nethogs_streamer.h"

class TestNethogsStreamer : public QObject
{
    Q_OBJECT

private slots:
    void parseLine_validTabSeparatedRow();
    void parseLine_programFieldContainsSlashes();
    void parseLine_refreshingMarkerRejected();
    void parseLine_emptyLineRejected();
    void parseLine_tooFewFieldsRejected();
    void parseLine_nonNumericPidRejected();
    void parseLine_nonNumericRatesRejected();
    void parseLine_zeroRates();
};

void TestNethogsStreamer::parseLine_validTabSeparatedRow()
{
    pid_t pid = 0;
    double sent = -1;
    double received = -1;
    const bool ok = NethogsStreamer::parseLine(
        QStringLiteral("firefox/4242/1000\t125.5\t42.25"), &pid, &sent, &received);

    QVERIFY(ok);
    QCOMPARE(pid, static_cast<pid_t>(4242));
    QCOMPARE(sent, 125.5);
    QCOMPARE(received, 42.25);
}

void TestNethogsStreamer::parseLine_programFieldContainsSlashes()
{
    // The program field can itself be a path (e.g. a script invoked by
    // interpreter) — pid/uid are always the last two '/'-delimited segments.
    pid_t pid = 0;
    double sent = -1;
    double received = -1;
    const bool ok = NethogsStreamer::parseLine(
        QStringLiteral("/usr/bin/python3/9001/1000\t0.0\t3.75"), &pid, &sent, &received);

    QVERIFY(ok);
    QCOMPARE(pid, static_cast<pid_t>(9001));
    QCOMPARE(sent, 0.0);
    QCOMPARE(received, 3.75);
}

void TestNethogsStreamer::parseLine_refreshingMarkerRejected()
{
    pid_t pid = 0;
    double sent = 0;
    double received = 0;
    QVERIFY(!NethogsStreamer::parseLine(QStringLiteral("Refreshing:"), &pid, &sent, &received));
}

void TestNethogsStreamer::parseLine_emptyLineRejected()
{
    pid_t pid = 0;
    double sent = 0;
    double received = 0;
    QVERIFY(!NethogsStreamer::parseLine(QString(), &pid, &sent, &received));
}

void TestNethogsStreamer::parseLine_tooFewFieldsRejected()
{
    pid_t pid = 0;
    double sent = 0;
    double received = 0;
    QVERIFY(!NethogsStreamer::parseLine(QStringLiteral("firefox/4242/1000\t125.5"),
                                        &pid, &sent, &received));
}

void TestNethogsStreamer::parseLine_nonNumericPidRejected()
{
    pid_t pid = 0;
    double sent = 0;
    double received = 0;
    QVERIFY(!NethogsStreamer::parseLine(QStringLiteral("firefox/not-a-pid/1000\t1.0\t2.0"),
                                        &pid, &sent, &received));
}

void TestNethogsStreamer::parseLine_nonNumericRatesRejected()
{
    pid_t pid = 0;
    double sent = 0;
    double received = 0;
    QVERIFY(!NethogsStreamer::parseLine(QStringLiteral("firefox/4242/1000\tN/A\tN/A"),
                                        &pid, &sent, &received));
}

void TestNethogsStreamer::parseLine_zeroRates()
{
    pid_t pid = 0;
    double sent = -1;
    double received = -1;
    const bool ok = NethogsStreamer::parseLine(
        QStringLiteral("idle-daemon/17/0\t0.000\t0.000"), &pid, &sent, &received);

    QVERIFY(ok);
    QCOMPARE(pid, static_cast<pid_t>(17));
    QCOMPARE(sent, 0.0);
    QCOMPARE(received, 0.0);
}

QTEST_MAIN(TestNethogsStreamer)
#include "test_nethogs_streamer.moc"
