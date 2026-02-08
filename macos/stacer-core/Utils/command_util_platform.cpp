#include "command_util.h"

#include <QDebug>

QString CommandUtil::sudoExec(const QString &cmd, QStringList args, QByteArray data)
{
    QString result("");

    // On macOS, use osascript for privilege escalation
    // Build the full command string with proper shell escaping
    QString fullCmd = cmd;
    for (const QString &arg : args) {
        // Escape single quotes and wrap each argument in single quotes
        QString escaped = arg;
        escaped.replace("'", "'\\''");
        fullCmd += " '" + escaped + "'";
    }
    // Escape backslashes and double quotes for the AppleScript string
    fullCmd.replace("\\", "\\\\");
    fullCmd.replace("\"", "\\\"");

    try {
        result = CommandUtil::exec("osascript",
            {"-e", QString("do shell script \"%1\" with administrator privileges").arg(fullCmd)},
            data);
    } catch (QString &ex) {
        qCritical() << ex;
    }

    return result;
}
