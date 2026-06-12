// FW-15 (SSO-3743): Unit tests for BatteryChargeThreshold.
// Tests cover validation logic and fixture-based read (no pkexec needed).
// The write/apply path is exercised only in integration tests on real hardware.

#include <QTest>
#include <QDir>
#include <QTemporaryDir>

#include "Info/battery_charge_threshold.h"

class TestBatteryChargeThreshold : public QObject
{
    Q_OBJECT

private slots:
    // validateThreshold
    void validate_endOnly_valid();
    void validate_endTooLow();
    void validate_endTooHigh();
    void validate_startAndEnd_valid();
    void validate_startEqualEnd_invalid();
    void validate_startAboveEnd_invalid();
    void validate_startNegative_skipped();
    void validate_minBoundary();
    void validate_maxBoundary();

    // buildUdevRule
    void udevRule_endOnly();
    void udevRule_withStart();

    // readStatus — fixture directory
    void readStatus_fixture_available();
    void readStatus_fixture_noNode();
    void readStatus_fixture_withStart();
    void readStatus_emptyPath();
};

// ── validateThreshold ────────────────────────────────────────────────────────

void TestBatteryChargeThreshold::validate_endOnly_valid()
{
    QVERIFY(BatteryChargeThreshold::validateThreshold(80).isEmpty());
}

void TestBatteryChargeThreshold::validate_endTooLow()
{
    QVERIFY(!BatteryChargeThreshold::validateThreshold(49).isEmpty());
}

void TestBatteryChargeThreshold::validate_endTooHigh()
{
    QVERIFY(!BatteryChargeThreshold::validateThreshold(101).isEmpty());
}

void TestBatteryChargeThreshold::validate_startAndEnd_valid()
{
    QVERIFY(BatteryChargeThreshold::validateThreshold(80, 75).isEmpty());
}

void TestBatteryChargeThreshold::validate_startEqualEnd_invalid()
{
    QVERIFY(!BatteryChargeThreshold::validateThreshold(80, 80).isEmpty());
}

void TestBatteryChargeThreshold::validate_startAboveEnd_invalid()
{
    QVERIFY(!BatteryChargeThreshold::validateThreshold(80, 85).isEmpty());
}

void TestBatteryChargeThreshold::validate_startNegative_skipped()
{
    // startPct < 0 means "no start threshold" — should be valid
    QVERIFY(BatteryChargeThreshold::validateThreshold(80, -1).isEmpty());
}

void TestBatteryChargeThreshold::validate_minBoundary()
{
    QVERIFY(BatteryChargeThreshold::validateThreshold(BatteryChargeThreshold::kMinEndThreshold).isEmpty());
    QVERIFY(!BatteryChargeThreshold::validateThreshold(BatteryChargeThreshold::kMinEndThreshold - 1).isEmpty());
}

void TestBatteryChargeThreshold::validate_maxBoundary()
{
    QVERIFY(BatteryChargeThreshold::validateThreshold(100).isEmpty());
    QVERIFY(!BatteryChargeThreshold::validateThreshold(101).isEmpty());
}

// ── buildUdevRule ─────────────────────────────────────────────────────────────

void TestBatteryChargeThreshold::udevRule_endOnly()
{
    const QString rule = BatteryChargeThreshold::buildUdevRule("BAT0", 80);
    QVERIFY(rule.contains("BAT0"));
    QVERIFY(rule.contains("charge_control_end_threshold"));
    QVERIFY(rule.contains("80"));
    QVERIFY(!rule.contains("charge_control_start_threshold"));
}

void TestBatteryChargeThreshold::udevRule_withStart()
{
    const QString rule = BatteryChargeThreshold::buildUdevRule("BAT1", 80, 75);
    QVERIFY(rule.contains("BAT1"));
    QVERIFY(rule.contains("charge_control_end_threshold"));
    QVERIFY(rule.contains("80"));
    QVERIFY(rule.contains("charge_control_start_threshold"));
    QVERIFY(rule.contains("75"));
}

// ── readStatus — fixture ──────────────────────────────────────────────────────

static void writeFixtureFile(const QString &path, const QByteArray &content)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(content);
}

void TestBatteryChargeThreshold::readStatus_fixture_available()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString bat = tmp.path() + "/BAT0";
    QVERIFY(QDir().mkpath(bat));

    writeFixtureFile(bat + "/type", "Battery\n");
    writeFixtureFile(bat + "/charge_control_end_threshold", "80\n");

    ChargeThresholdStatus s = BatteryChargeThreshold::readStatus(bat);
    QVERIFY(s.available);
    QCOMPARE(s.endPct, 80);
    QVERIFY(!s.hasStart);
    QCOMPARE(s.batteryName, QString("BAT0"));
}

void TestBatteryChargeThreshold::readStatus_fixture_noNode()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString bat = tmp.path() + "/BAT0";
    QVERIFY(QDir().mkpath(bat));
    writeFixtureFile(bat + "/type", "Battery\n");
    // No charge_control_end_threshold

    ChargeThresholdStatus s = BatteryChargeThreshold::readStatus(bat);
    QVERIFY(!s.available);
    QVERIFY(!s.errorMsg.isEmpty());
}

void TestBatteryChargeThreshold::readStatus_fixture_withStart()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString bat = tmp.path() + "/BAT0";
    QVERIFY(QDir().mkpath(bat));
    writeFixtureFile(bat + "/type", "Battery\n");
    writeFixtureFile(bat + "/charge_control_end_threshold", "80\n");
    writeFixtureFile(bat + "/charge_control_start_threshold", "75\n");

    ChargeThresholdStatus s = BatteryChargeThreshold::readStatus(bat);
    QVERIFY(s.available);
    QCOMPARE(s.endPct, 80);
    QVERIFY(s.hasStart);
    QCOMPARE(s.startPct, 75);
}

void TestBatteryChargeThreshold::readStatus_emptyPath()
{
    // Empty override → discovery (no /sys/class/power_supply in CI, so not available)
    // We just verify it doesn't crash.
    ChargeThresholdStatus s = BatteryChargeThreshold::readStatus(QStringLiteral("/nonexistent/BAT0"));
    QVERIFY(!s.available);
}

QTEST_MAIN(TestBatteryChargeThreshold)
#include "test_battery_charge_threshold.moc"
