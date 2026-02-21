#include "process_service.h"
#include <Utils/command_util.h>
#include <QDebug>

ProcessService *ProcessService::instance = nullptr;

ProcessService *ProcessService::ins()
{
    if (!instance)
        instance = new ProcessService;
    return instance;
}

ProcessService::ProcessService(QObject *parent)
    : QObject(parent)
{
}

bool ProcessService::killProcess(pid_t pid, const QString &processUser, const QString &currentUser)
{
    try {
        if (processUser == currentUser) {
            CommandUtil::exec("kill", { QString::number(pid) });
        } else {
            CommandUtil::sudoExec("kill", { QString::number(pid) });
        }
        emit processKilled(pid);
        return true;
    } catch (QString &ex) {
        qCritical() << ex;
        emit processKillFailed(pid, ex);
        return false;
    }
}
