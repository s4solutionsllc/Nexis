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
    virtual ~ServiceTool() = default;

    virtual QList<Service> getServices() = 0;
    virtual bool serviceIsActive(const QString &serviceName) = 0;
    virtual bool changeServiceStatus(const QString &sname, bool status) = 0;
    virtual bool changeServiceActive(const QString &sname, bool status) = 0;
    virtual bool serviceIsEnabled(const QString &serviceName) = 0;
    virtual QString getServiceDescription(const QString &serviceName) = 0;
};

#endif // SERVICE_TOOL_H
