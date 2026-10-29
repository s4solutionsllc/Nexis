#include <QTest>

#include "psi_info.h"

class TestPsiInfo : public QObject
{
    Q_OBJECT

private:
    QString fixturePath(const QString &name) const
    {
        return QStringLiteral(PROJECT_SOURCE_DIR) + "/tests/fixtures/psi/" + name;
    }

private slots:
    void parsePressureFile_cpuSomeAndFull();
    void parsePressureFile_memorySomeOnly_noFullLine();
    void parsePressureFile_ioSomeAndFull();
    void parsePressureFile_missingFile_returnsUnavailable();
};

void TestPsiInfo::parsePressureFile_cpuSomeAndFull()
{
    const auto snap = PsiInfo::parsePressureFile(
        fixturePath(QStringLiteral("cpu_some_full.txt")).toLatin1().constData());
    QVERIFY(snap.available);
    QCOMPARE(snap.someAvg10, 2.50);
    QCOMPARE(snap.someAvg60, 1.80);
    QCOMPARE(snap.someAvg300, 0.90);
    QCOMPARE(snap.fullAvg10, 0.10);
    QCOMPARE(snap.fullAvg60, 0.05);
    QCOMPARE(snap.fullAvg300, 0.02);
}

void TestPsiInfo::parsePressureFile_memorySomeOnly_noFullLine()
{
    // /proc/pressure/memory on a host with no memory cgroup contention often
    // reports only the "some" line — "full" simply never appears there.
    const auto snap = PsiInfo::parsePressureFile(
        fixturePath(QStringLiteral("memory_some_only.txt")).toLatin1().constData());
    QVERIFY(snap.available);
    QCOMPARE(snap.someAvg10, 0.0);
    QCOMPARE(snap.fullAvg10, 0.0); // never set by the fixture; struct default
}

void TestPsiInfo::parsePressureFile_ioSomeAndFull()
{
    const auto snap = PsiInfo::parsePressureFile(
        fixturePath(QStringLiteral("io_some_full.txt")).toLatin1().constData());
    QVERIFY(snap.available);
    QCOMPARE(snap.someAvg10, 5.23);
    QCOMPARE(snap.fullAvg10, 3.10);
}

void TestPsiInfo::parsePressureFile_missingFile_returnsUnavailable()
{
    const auto snap = PsiInfo::parsePressureFile("/nonexistent/does-not-exist/pressure");
    QVERIFY(!snap.available);
    QCOMPARE(snap.someAvg10, 0.0);
}

QTEST_MAIN(TestPsiInfo)
#include "test_psi_info.moc"
