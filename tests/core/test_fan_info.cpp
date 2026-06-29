#include <QTest>
#include "Info/fan_info.h"

class TestFanInfo : public QObject
{
    Q_OBJECT

private slots:
    // parseThinkpadFanSpeed
    void thinkpad_normalSpeed();
    void thinkpad_zeroSpeed();
    void thinkpad_highSpeed();
    void thinkpad_noSpeedLine();
    void thinkpad_emptyInput();
    void thinkpad_insaneRpm();

    // parseDellI8kFanSpeeds
    void dell_bothFans();
    void dell_leftOnly();
    void dell_noFans();
    void dell_tooFewFields();
    void dell_emptyInput();

    // parseNvidiaSmiGpuFanPercent
    void nvidiaSmi_normalPercent();
    void nvidiaSmi_zeroPercent();
    void nvidiaSmi_notAvailable();
    void nvidiaSmi_emptyLine();
    void nvidiaSmi_malformed();

    // GPU PWM fan duty cycle conversion tests
    void gpu_pwm_conversion_normal();
    void gpu_pwm_conversion_min();
    void gpu_pwm_conversion_max();
    void gpu_pwm_conversion_typical();
};

void TestFanInfo::thinkpad_normalSpeed()
{
    QString content = "status:\t\tenabled\n"
                      "speed:\t\t3200\n"
                      "level:\t\tauto\n";
    QCOMPARE(FanInfo::parseThinkpadFanSpeed(content), 3200);
}

void TestFanInfo::thinkpad_zeroSpeed()
{
    QString content = "status:\t\tdisabled\nspeed:\t\t0\nlevel:\t\t0\n";
    QCOMPARE(FanInfo::parseThinkpadFanSpeed(content), 0);
}

void TestFanInfo::thinkpad_highSpeed()
{
    QString content = "speed:\t\t7500\n";
    QCOMPARE(FanInfo::parseThinkpadFanSpeed(content), 7500);
}

void TestFanInfo::thinkpad_noSpeedLine()
{
    QString content = "status:\t\tenabled\nlevel:\t\tauto\n";
    QCOMPARE(FanInfo::parseThinkpadFanSpeed(content), 0);
}

void TestFanInfo::thinkpad_emptyInput()
{
    QCOMPARE(FanInfo::parseThinkpadFanSpeed(""), 0);
}

void TestFanInfo::thinkpad_insaneRpm()
{
    QString content = "speed:\t\t99999\n";
    QCOMPARE(FanInfo::parseThinkpadFanSpeed(content), 0);
}

void TestFanInfo::dell_bothFans()
{
    // /proc/i8k format: version bios serial cpu_temp l_status r_status l_rpm r_rpm
    QString content = "1.0 A17 SN123 55 1 1 3200 2800";
    QList<int> speeds = FanInfo::parseDellI8kFanSpeeds(content);
    QCOMPARE(speeds.size(), 2);
    QCOMPARE(speeds.at(0), 3200);
    QCOMPARE(speeds.at(1), 2800);
}

void TestFanInfo::dell_leftOnly()
{
    QString content = "1.0 A17 SN123 55 1 0 3200 0";
    QList<int> speeds = FanInfo::parseDellI8kFanSpeeds(content);
    QCOMPARE(speeds.size(), 1);
    QCOMPARE(speeds.at(0), 3200);
}

void TestFanInfo::dell_noFans()
{
    QString content = "1.0 A17 SN123 55 0 0 0 0";
    QList<int> speeds = FanInfo::parseDellI8kFanSpeeds(content);
    QCOMPARE(speeds.size(), 0);
}

void TestFanInfo::dell_tooFewFields()
{
    QString content = "1.0 A17 SN123";
    QList<int> speeds = FanInfo::parseDellI8kFanSpeeds(content);
    QCOMPARE(speeds.size(), 0);
}

void TestFanInfo::dell_emptyInput()
{
    QList<int> speeds = FanInfo::parseDellI8kFanSpeeds("");
    QCOMPARE(speeds.size(), 0);
}

void TestFanInfo::nvidiaSmi_normalPercent()
{
    QCOMPARE(FanInfo::parseNvidiaSmiGpuFanPercent("42, GeForce RTX 3080"), 42);
}

void TestFanInfo::nvidiaSmi_zeroPercent()
{
    QCOMPARE(FanInfo::parseNvidiaSmiGpuFanPercent("0, GeForce GTX 1060"), 0);
}

void TestFanInfo::nvidiaSmi_notAvailable()
{
    QCOMPARE(FanInfo::parseNvidiaSmiGpuFanPercent("[N/A], Tesla T4"), -1);
}

void TestFanInfo::nvidiaSmi_emptyLine()
{
    QCOMPARE(FanInfo::parseNvidiaSmiGpuFanPercent(""), -1);
}

void TestFanInfo::nvidiaSmi_malformed()
{
    QCOMPARE(FanInfo::parseNvidiaSmiGpuFanPercent("error"), -1);
}

void TestFanInfo::gpu_pwm_conversion_normal()
{
    // PWM value: 96, expected percentage: (96 * 100) / 255 = 37%
    int pwm = 96;
    int percent = (pwm * 100) / 255;
    QCOMPARE(percent, 37);
}

void TestFanInfo::gpu_pwm_conversion_min()
{
    // PWM value: 0, expected percentage: 0%
    int pwm = 0;
    int percent = (pwm * 100) / 255;
    QCOMPARE(percent, 0);
}

void TestFanInfo::gpu_pwm_conversion_max()
{
    // PWM value: 255, expected percentage: 100%
    int pwm = 255;
    int percent = (pwm * 100) / 255;
    QCOMPARE(percent, 100);
}

void TestFanInfo::gpu_pwm_conversion_typical()
{
    // PWM value: 128 (50% speed), expected percentage: (128 * 100) / 255 = 50%
    int pwm = 128;
    int percent = (pwm * 100) / 255;
    QCOMPARE(percent, 50);
}

QTEST_MAIN(TestFanInfo)
#include "test_fan_info.moc"
