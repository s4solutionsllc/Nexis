#ifndef SERVICE_TOOL_H
#define SERVICE_TOOL_H

#include <Utils/command_util.h>

#include "nexis-core_global.h"

class NEXISCORESHARED_EXPORT Service
{
public:
    Service(const QString &name, const QString description, const bool status, const bool active);

    QString name;
    QString description;
    bool status;
    bool active;
};

class NEXISCORESHARED_EXPORT ServiceTool
{
public:
    static QList<Service> getServices();
    static bool serviceIsActive(const QString &serviceName);
    static bool changeServiceStatus(const QString &sname, bool status);
    static bool changeServiceActive(const QString &sname, bool status);
    static bool serviceIsEnabled(const QString &serviceName);
    static QString getServiceDescription(const QString &serviceName);
};

#endif // SERVICE_TOOL_H
