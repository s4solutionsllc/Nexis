// SSO-23857: unit tests for the pure argv-building and output-parsing halves
// of MacDefaultsTool. The exec-glue methods (readValue/writeValue/
// revertToDefault) just shell out to /usr/bin/defaults via CommandUtil,
// which doesn't exist off macOS — those paths are exercised manually/in
// integration testing, mirroring BatteryChargeThreshold's write-path note.

#include <QTest>

#include "Tools/mac_defaults_tool.h"

class TestMacDefaultsTool : public QObject
{
    Q_OBJECT

private slots:
    void buildReadArgs_ordersDomainThenKey();
    void buildWriteArgs_bool_true();
    void buildWriteArgs_bool_false();
    void buildWriteArgs_int();
    void buildWriteArgs_string();
    void buildDeleteArgs_ordersDomainThenKey();

    void parseReadOutput_bool_one();
    void parseReadOutput_bool_zero();
    void parseReadOutput_int();
    void parseReadOutput_int_malformed();
    void parseReadOutput_string();
    void parseReadOutput_notSet_noError();
    void parseReadOutput_genuineError();
    void parseReadOutput_emptyOutput();
};

void TestMacDefaultsTool::buildReadArgs_ordersDomainThenKey()
{
    const QStringList args = MacDefaultsTool::buildReadArgs("com.apple.finder", "AppleShowAllFiles");
    QCOMPARE(args, QStringList({"read", "com.apple.finder", "AppleShowAllFiles"}));
}

void TestMacDefaultsTool::buildWriteArgs_bool_true()
{
    const QStringList args = MacDefaultsTool::buildWriteArgs(
        "com.apple.dock", "autohide", MacDefaultsValueType::Bool, true);
    QCOMPARE(args, QStringList({"write", "com.apple.dock", "autohide", "-bool", "true"}));
}

void TestMacDefaultsTool::buildWriteArgs_bool_false()
{
    const QStringList args = MacDefaultsTool::buildWriteArgs(
        "com.apple.dock", "autohide", MacDefaultsValueType::Bool, false);
    QCOMPARE(args, QStringList({"write", "com.apple.dock", "autohide", "-bool", "false"}));
}

void TestMacDefaultsTool::buildWriteArgs_int()
{
    const QStringList args = MacDefaultsTool::buildWriteArgs(
        "com.apple.dock", "tilesize", MacDefaultsValueType::Int, 64);
    QCOMPARE(args, QStringList({"write", "com.apple.dock", "tilesize", "-int", "64"}));
}

void TestMacDefaultsTool::buildWriteArgs_string()
{
    const QStringList args = MacDefaultsTool::buildWriteArgs(
        "com.apple.screencapture", "type", MacDefaultsValueType::String, QStringLiteral("jpg"));
    QCOMPARE(args, QStringList({"write", "com.apple.screencapture", "type", "-string", "jpg"}));
}

void TestMacDefaultsTool::buildDeleteArgs_ordersDomainThenKey()
{
    const QStringList args = MacDefaultsTool::buildDeleteArgs("com.apple.finder", "ShowPathbar");
    QCOMPARE(args, QStringList({"delete", "com.apple.finder", "ShowPathbar"}));
}

void TestMacDefaultsTool::parseReadOutput_bool_one()
{
    const MacDefaultsReadResult r =
        MacDefaultsTool::parseReadOutput(MacDefaultsValueType::Bool, "1\n", 0, QString());
    QVERIFY(r.found);
    QCOMPARE(r.value.toBool(), true);
    QVERIFY(r.errorMsg.isEmpty());
}

void TestMacDefaultsTool::parseReadOutput_bool_zero()
{
    const MacDefaultsReadResult r =
        MacDefaultsTool::parseReadOutput(MacDefaultsValueType::Bool, "0\n", 0, QString());
    QVERIFY(r.found);
    QCOMPARE(r.value.toBool(), false);
}

void TestMacDefaultsTool::parseReadOutput_int()
{
    const MacDefaultsReadResult r =
        MacDefaultsTool::parseReadOutput(MacDefaultsValueType::Int, "48\n", 0, QString());
    QVERIFY(r.found);
    QCOMPARE(r.value.toInt(), 48);
}

void TestMacDefaultsTool::parseReadOutput_int_malformed()
{
    const MacDefaultsReadResult r =
        MacDefaultsTool::parseReadOutput(MacDefaultsValueType::Int, "not-a-number\n", 0, QString());
    QVERIFY(!r.found);
    QVERIFY(!r.errorMsg.isEmpty());
}

void TestMacDefaultsTool::parseReadOutput_string()
{
    const MacDefaultsReadResult r =
        MacDefaultsTool::parseReadOutput(MacDefaultsValueType::String, "png\n", 0, QString());
    QVERIFY(r.found);
    QCOMPARE(r.value.toString(), QStringLiteral("png"));
}

void TestMacDefaultsTool::parseReadOutput_notSet_noError()
{
    // The common case: key has never been overridden — not an error.
    const MacDefaultsReadResult r = MacDefaultsTool::parseReadOutput(
        MacDefaultsValueType::Bool, QString(), 1,
        QStringLiteral("The domain/default pair of (com.apple.finder, AppleShowAllFiles) does not exist"));
    QVERIFY(!r.found);
    QVERIFY(r.errorMsg.isEmpty());
}

void TestMacDefaultsTool::parseReadOutput_genuineError()
{
    const MacDefaultsReadResult r = MacDefaultsTool::parseReadOutput(
        MacDefaultsValueType::Bool, QString(), 1, QStringLiteral("permission denied"));
    QVERIFY(!r.found);
    QVERIFY(!r.errorMsg.isEmpty());
}

void TestMacDefaultsTool::parseReadOutput_emptyOutput()
{
    const MacDefaultsReadResult r =
        MacDefaultsTool::parseReadOutput(MacDefaultsValueType::String, "   \n", 0, QString());
    QVERIFY(!r.found);
}

QTEST_MAIN(TestMacDefaultsTool)
#include "test_mac_defaults_tool.moc"
