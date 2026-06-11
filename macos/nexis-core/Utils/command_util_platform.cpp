#include "command_util.h"

#include <QDebug>

ExecResult CommandUtil::sudoExecWithStatus(const QString &cmd, QStringList args, QByteArray data, int timeoutMs)
{
    // SSO-3367 testing seam: route the call straight through execWithStatus so
    // unit tests can exercise sudoExec error reporting without osascript
    // asking the user for admin credentials. Production code must never set
    // this env var.
    if (qEnvironmentVariableIsSet("NEXIS_SUDO_BYPASS")) {
        return CommandUtil::execWithStatus(cmd, args, data);
    }

    // On macOS, use osascript for privilege escalation. Build the full command
    // string with proper shell escaping (single-quote each argument, escape
    // single quotes inside).
    QString fullCmd = cmd;
    for (const QString &arg : args) {
        QString escaped = arg;
        escaped.replace("'", "'\\''");
        fullCmd += " '" + escaped + "'";
    }
    // Escape backslashes and double quotes for the AppleScript string.
    fullCmd.replace("\\", "\\\\");
    fullCmd.replace("\"", "\\\"");

    return CommandUtil::execWithStatus(
        QStringLiteral("osascript"),
        {QStringLiteral("-e"), QStringLiteral("do shell script \"%1\" with administrator privileges").arg(fullCmd)},
        data,
        timeoutMs);
}

QString CommandUtil::sudoExec(const QString &cmd, QStringList args, QByteArray data, int timeoutMs)
{
    // SSO-3367: legacy thin wrapper. sudoExec used to swallow all errors and
    // return "" — the unified ExecResult path now logs failures while keeping
    // the same outward QString shape for unmigrated callers.
    const ExecResult result = CommandUtil::sudoExecWithStatus(cmd, args, data, timeoutMs);

    if (!result.ok()) {
        qCritical().nospace() << "CommandUtil::sudoExec failed: " << cmd
                              << " " << args
                              << " (exitCode=" << result.exitCode
                              << ", error=" << result.error << ")";
    }

    return result.output;
}
