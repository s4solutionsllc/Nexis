#include "command_util.h"

#include <QDebug>

QString CommandUtil::sudoExec(const QString &cmd, QStringList args, QByteArray data, int timeoutMs)
{
    QString result("");

    args.push_front(cmd);

    try {
        result = CommandUtil::exec("pkexec", args, data, timeoutMs);
    } catch (QString &ex) {
        qCritical() << ex;
    }

    return result;
}
