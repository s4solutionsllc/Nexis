#ifndef FAN_INFO_H
#define FAN_INFO_H

#include <QList>
#include <QString>
#include "Utils/file_util.h"
#include "nexis-core_global.h"

struct FanSensor {
    QString id;
    QString deviceName;
    QString label;
    QString inputPath;
    int minRpm;
    int maxRpm;
};

class NEXISCORESHARED_EXPORT FanInfo
{
public:
    virtual ~FanInfo() = default;

    QList<FanSensor> getSensors() const { return mSensors; }
    virtual int getFanSpeed(int index) const = 0;
    bool hasSensors() const { return !mSensors.isEmpty(); }

protected:
    virtual void discoverSensors() = 0;
    QList<FanSensor> mSensors;
};

#endif // FAN_INFO_H
