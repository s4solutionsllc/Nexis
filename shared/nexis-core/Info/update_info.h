#ifndef UPDATE_INFO_H
#define UPDATE_INFO_H

#include <QDateTime>
#include <QList>
#include <QString>

#include "nexis-core_global.h"

struct NEXISCORESHARED_EXPORT UpdateEntry {
    QString source;
    QString name;
    QString version;          // available (newer) version
    QString installedVersion; // currently installed version
    bool isCask = false;
};

struct NEXISCORESHARED_EXPORT UpdateCheckResult {
    int totalCount = 0;
    QList<UpdateEntry> entries;
    QDateTime checkTime;
    bool success = false;
    QString errorMessage;
};

class NEXISCORESHARED_EXPORT UpdateInfo
{
public:
    virtual ~UpdateInfo() = default;
    virtual UpdateCheckResult checkForUpdates() = 0;
    virtual QStringList availableSources() const = 0;
};

#endif // UPDATE_INFO_H
