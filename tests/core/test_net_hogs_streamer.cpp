// SSO-15379: pure trace-mode line parser for the nethogs fallback. The
// nethogs binary itself is Linux-only and typically needs elevated
// privileges to run, but NetHogsStreamer::parseTraceLine has no platform or
// privilege dependency, so (mirroring the FR-127 / WI-33 pattern) this test
// compiles the source directly and runs on any platform.

#include <QTest>

#include "net_hogs_streamer.h"

class TestNetHogsStreamer : public QObject
{
    Q_OBJECT

private slots:
    void dataRow_parsesSentAndReceivedAsBytesPerSec();
    void dataRow_programPathWithSlashesStillFindsPid();
    void refreshingSeparator_rejected();
    void totalSummaryLine_rejected();
    void emptyLine_rejected();
    void nonNumericPid_rejected();
    void nonNumericRate_rejected();
};

void TestNetHogsStreamer::dataRow_parsesSentAndReceivedAsBytesPerSec()
{
    pid_t pid = 0;
    double sentBps = 0;
    double recvBps = 0;

    QVERIFY(NetHogsStreamer::parseTraceLine(
        "/usr/bin/sshd/998/0\t1.500\t0.250", &pid, &sentBps, &recvBps));

    QCOMPARE(pid, static_cast<pid_t>(998));
    QCOMPARE(sentBps, 1.5 * 1024.0);
    QCOMPARE(recvBps, 0.25 * 1024.0);
}

void TestNetHogsStreamer::dataRow_programPathWithSlashesStillFindsPid()
{
    pid_t pid = 0;
    double sentBps = 0;
    double recvBps = 0;

    QVERIFY(NetHogsStreamer::parseTraceLine(
        "/usr/lib/firefox/firefox/12345/1000\t5.360\t120.334", &pid, &sentBps, &recvBps));

    QCOMPARE(pid, static_cast<pid_t>(12345));
    QCOMPARE(sentBps, 5.360 * 1024.0);
    QCOMPARE(recvBps, 120.334 * 1024.0);
}

void TestNetHogsStreamer::refreshingSeparator_rejected()
{
    QVERIFY(!NetHogsStreamer::parseTraceLine("Refreshing:", nullptr, nullptr, nullptr));
}

void TestNetHogsStreamer::totalSummaryLine_rejected()
{
    QVERIFY(!NetHogsStreamer::parseTraceLine("TOTAL\t5.360\t120.334", nullptr, nullptr, nullptr));
}

void TestNetHogsStreamer::emptyLine_rejected()
{
    QVERIFY(!NetHogsStreamer::parseTraceLine(QString(), nullptr, nullptr, nullptr));
}

void TestNetHogsStreamer::nonNumericPid_rejected()
{
    QVERIFY(!NetHogsStreamer::parseTraceLine(
        "/usr/bin/sshd/abc/0\t1.500\t0.250", nullptr, nullptr, nullptr));
}

void TestNetHogsStreamer::nonNumericRate_rejected()
{
    QVERIFY(!NetHogsStreamer::parseTraceLine(
        "/usr/bin/sshd/998/0\tnope\t0.250", nullptr, nullptr, nullptr));
}

QTEST_MAIN(TestNetHogsStreamer)
#include "test_net_hogs_streamer.moc"
