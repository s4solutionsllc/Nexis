// Shared FanInfo static parsing methods — platform-independent.

#include "fan_info.h"
#include <QRegularExpression>

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
