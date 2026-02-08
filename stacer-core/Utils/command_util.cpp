#include "command_util.h"

#include <QProcess>
#include <QTextStream>
#include <QStandardPaths>
#include <QDebug>

#include <memory>

QString CommandUtil::sudoExec(const QString &cmd, QStringList args, QByteArray data)
{
    QString result("");

#ifdef Q_OS_LINUX
    args.push_front(cmd);

    try {
        result = CommandUtil::exec("pkexec", args, data);
    } catch (QString &ex) {
        qCritical() << ex;
    }
#elif defined(Q_OS_MACOS)
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
#endif

    return result;
}

QString CommandUtil::exec(const QString &cmd, QStringList args, QByteArray data)
{
    std::unique_ptr<QProcess> process(new QProcess());
    process->start(cmd, args);

    if (! data.isEmpty()) {
        process->write(data);
        process->waitForBytesWritten();
        process->closeWriteChannel();
    }

    // 10 minutes
    process->waitForFinished(600*1000);

    QTextStream stdOut(process->readAllStandardOutput());

    QString err = process->errorString();

    process->kill();
    process->close();

    if (process->error() != QProcess::UnknownError)
        throw err;

    return stdOut.readAll().trimmed();
}

bool CommandUtil::isExecutable(const QString &cmd)
{
    return !QStandardPaths::findExecutable(cmd).isEmpty();
}
