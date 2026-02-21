#include "system_service_manager.h"
#include "Managers/tool_manager.h"
#include <QThreadPool>

SystemServiceManager *SystemServiceManager::instance = nullptr;

SystemServiceManager *SystemServiceManager::ins()
{
    if (!instance)
        instance = new SystemServiceManager;
    return instance;
}

SystemServiceManager::SystemServiceManager(QObject *parent, ToolManager *toolManager)
    : QObject(parent),
      mToolManager(toolManager ? toolManager : ToolManager::ins())
{
}

void SystemServiceManager::fetchServices()
{
    QThreadPool::globalInstance()->start([this]() {
        QList<Service> services = mToolManager->getServices();
        mServices = services;
        emit servicesFetched();
    });
}
