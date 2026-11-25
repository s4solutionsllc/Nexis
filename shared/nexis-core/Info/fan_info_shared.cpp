// Shared FanInfo static parsing methods — platform-independent.

#include "fan_info.h"
#include <QRegularExpression>
#include <cmath>
#include <cstdint>
#include <cstring>

static constexpr int MAX_SANE_RPM = 30000;

int FanInfo::parseThinkpadFanSpeed(const QString &procFanContent)
{
    static QRegularExpression speedRe("speed:\\s+(\\d+)");
    QRegularExpressionMatch match = speedRe.match(procFanContent);
    if (!match.hasMatch())
        return 0;

    int rpm = match.captured(1).toInt();
    return (rpm >= 0 && rpm <= MAX_SANE_RPM) ? rpm : 0;
}

int FanInfo::decodeSmcFanRpm(const QByteArray &dataType, const QByteArray &bytes)
{
    double rpm;
    if (dataType == "fpe2" && bytes.size() == 2) {
        const uint16_t raw = uint16_t((uint16_t(uint8_t(bytes[0])) << 8) | uint8_t(bytes[1]));
        rpm = raw / 4.0;
    } else if (dataType == "flt " && bytes.size() == 4) {
        const uint32_t bits = uint32_t(uint8_t(bytes[0])) |
                              (uint32_t(uint8_t(bytes[1])) <<  8) |
                              (uint32_t(uint8_t(bytes[2])) << 16) |
                              (uint32_t(uint8_t(bytes[3])) << 24);
        float f;
        std::memcpy(&f, &bits, sizeof f);
        rpm = f;
    } else {
        return -1;
    }
    if (!std::isfinite(rpm) || rpm < 0.0 || rpm > MAX_SANE_RPM)
        return -1;
    return int(std::lround(rpm));
}

QList<int> FanInfo::parseDellI8kFanSpeeds(const QString &i8kContent)
{
    QList<int> speeds;
    QStringList fields = i8kContent.trimmed().split(QRegularExpression("\\s+"));
    if (fields.size() < 8)
        return speeds;

    for (int fanIdx = 0; fanIdx < 2; ++fanIdx) {
        int fieldPos = 6 + fanIdx;
        int rpm = fields.at(fieldPos).toInt();
        if (rpm > 0 && rpm <= MAX_SANE_RPM)
            speeds.append(rpm);
    }

    return speeds;
}

int FanInfo::parseNvidiaSmiGpuFanPercent(const QString &csvLine)
{
    QStringList parts = csvLine.trimmed().split(',');
    if (parts.size() < 2)
        return -1;

    QString fanStr = parts.at(0).trimmed();
    if (fanStr == "[N/A]" || fanStr.isEmpty())
        return -1;

    bool ok;
    int percent = fanStr.toInt(&ok);
    return (ok && percent >= 0 && percent <= 100) ? percent : -1;
}
