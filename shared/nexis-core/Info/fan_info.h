#ifndef FAN_INFO_H
#define FAN_INFO_H

#include <QList>
#include <QString>
#include "Utils/file_util.h"
#include "nexis-core_global.h"

enum class FanSourceType {
    Hwmon,
    ThinkpadProc,
    DellProc,
    NvidiaSmi
};

struct FanSensor {
    QString id;
    QString deviceName;
    QString label;
    QString inputPath;
    int minRpm;
    int maxRpm;
    FanSourceType sourceType = FanSourceType::Hwmon;
    int procFieldIndex = -1;
};

class NEXISCORESHARED_EXPORT FanInfo
{
public:
    virtual ~FanInfo() = default;

    QList<FanSensor> getSensors() const { return mSensors; }
    virtual int getFanSpeed(int index) const = 0;
    bool hasSensors() const { return !mSensors.isEmpty(); }

    // Static parsing methods for testability (FR-76).
    static int parseThinkpadFanSpeed(const QString &procFanContent);
    static QList<int> parseDellI8kFanSpeeds(const QString &i8kContent);
    static int parseNvidiaSmiGpuFanPercent(const QString &csvLine);

protected:
    virtual void discoverSensors() = 0;
    QList<FanSensor> mSensors;
};

#endif // FAN_INFO_H
