#ifndef SERVICE_TOOL_MACOS_H
#define SERVICE_TOOL_MACOS_H

#include <Tools/service_tool.h>

class ServiceToolMacOS : public ServiceTool
{
public:
    QList<Service> getServices() override;
    bool serviceIsActive(const QString &serviceName) override;
    bool changeServiceStatus(const QString &sname, bool status) override;
    bool changeServiceActive(const QString &sname, bool status) override;
    bool serviceIsEnabled(const QString &serviceName) override;
    QString getServiceDescription(const QString &serviceName) override;
};

#endif // SERVICE_TOOL_MACOS_H
