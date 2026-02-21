#ifndef SYSTEM_SERVICE_MANAGER_H
#define SYSTEM_SERVICE_MANAGER_H

#include <QObject>
#include <Tools/service_tool.h>

class ToolManager;

class SystemServiceManager : public QObject
{
    Q_OBJECT

public:
    static SystemServiceManager *ins();

    explicit SystemServiceManager(QObject *parent = nullptr,
                                  ToolManager *toolManager = nullptr);

    void fetchServices();

    const QList<Service> &services() const { return mServices; }

signals:
    void servicesFetched();

private:
    static SystemServiceManager *instance;
    ToolManager *mToolManager;
    QList<Service> mServices;
};

#endif // SYSTEM_SERVICE_MANAGER_H
