#include "command_util.h"

#include <QProcess>
#include <QTextStream>
#include <QStandardPaths>
#include <QDebug>

#include <memory>

QString CommandUtil::exec(const QString &cmd, QStringList args, QByteArray data, int timeoutMs)
{
    std::unique_ptr<QProcess> process(new QProcess());
    process->start(cmd, args);

    if (! data.isEmpty()) {
        process->write(data);
        process->waitForBytesWritten();
        process->closeWriteChannel();
    }

    process->waitForFinished(timeoutMs);

    QTextStream stdOut(process->readAllStandardOutput());

    QString err = process->errorString();

    process->kill();
    process->close();

    if (process->error() != QProcess::UnknownError)
        throw err;

    return stdOut.readAll().trimmed();
}

ExecResult CommandUtil::execWithStatus(const QString &cmd, QStringList args, int timeoutMs)
{
    ExecResult result;
    result.exitCode = -1;

    std::unique_ptr<QProcess> process(new QProcess());
    process->start(cmd, args);

    if (!process->waitForStarted(timeoutMs)) {
        result.error = process->errorString();
        return result;
    }

    process->waitForFinished(timeoutMs);

    result.output = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
    result.error = QString::fromUtf8(process->readAllStandardError()).trimmed();
    result.exitCode = (process->exitStatus() == QProcess::NormalExit) ? process->exitCode() : -1;

    process->kill();
    process->close();

    return result;
}

bool CommandUtil::isExecutable(const QString &cmd)
{
    return !QStandardPaths::findExecutable(cmd).isEmpty();
}
