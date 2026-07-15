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
    const ExecResult result = (processUser == currentUser)
        ? CommandUtil::execWithStatus("kill", { QString::number(pid) })
        : CommandUtil::sudoExecWithStatus("kill", { QString::number(pid) });

    if (!result.ok()) {
        qCritical() << result.error;
        emit processKillFailed(pid, result.error);
        return false;
    }
    emit processKilled(pid);
    return true;
}
