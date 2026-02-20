#ifndef SERVICE_TOOL_LINUX_H
#define SERVICE_TOOL_LINUX_H

#include <Tools/service_tool.h>

class ServiceToolLinux : public ServiceTool
{
public:
    QList<Service> getServices() override;
    bool serviceIsActive(const QString &serviceName) override;
    bool changeServiceStatus(const QString &sname, bool status) override;
    bool changeServiceActive(const QString &sname, bool status) override;
    bool serviceIsEnabled(const QString &serviceName) override;
    QString getServiceDescription(const QString &serviceName) override;
};

#endif // SERVICE_TOOL_LINUX_H
