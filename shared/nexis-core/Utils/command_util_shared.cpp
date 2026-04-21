#include "command_util.h"

#include <QCoreApplication>
#include <QDebug>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>
#include <QtConcurrent>

#include <memory>

namespace {

// UI-thread audit: in debug builds, warn (and assert) when a synchronous exec
// runs on the GUI thread. Gated on NEXIS_ASSERT_ASYNC_EXEC so it stays silent
// by default and does not disrupt existing callers during the Bundle B
// migration.
void auditUiThread(const QString &cmd, const QStringList &args)
{
#ifndef QT_NO_DEBUG
    static const bool assertAsync = qEnvironmentVariableIsSet("NEXIS_ASSERT_ASYNC_EXEC");
    if (assertAsync && qApp && QThread::currentThread() == qApp->thread()) {
        qWarning() << "CommandUtil: synchronous exec on UI thread:" << cmd << args;
        Q_ASSERT(false);
    }
#else
    Q_UNUSED(cmd)
    Q_UNUSED(args)
#endif
}

} // namespace

QString CommandUtil::exec(const QString &cmd, QStringList args, QByteArray data, int timeoutMs)
{
    auditUiThread(cmd, args);

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
    auditUiThread(cmd, args);

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

QFuture<ExecResult> CommandUtil::execAsync(const QString &cmd, QStringList args, int timeoutMs)
{
    return QtConcurrent::run([cmd, args, timeoutMs] {
        return CommandUtil::execWithStatus(cmd, args, timeoutMs);
    });
}

bool CommandUtil::isExecutable(const QString &cmd)
{
    // FR-109: cache the PATH walk. update_info.cpp and package_tool.cpp call
    // this 5-12 times per discovery pass — each call stat()s every PATH
    // candidate otherwise. Thread-safe: callers span the UI thread and
    // QtConcurrent workers.
    static QMutex mutex;
    static QHash<QString, bool> cache;

    QMutexLocker lock(&mutex);
    auto it = cache.constFind(cmd);
    if (it != cache.constEnd())
        return it.value();

    const bool ok = !QStandardPaths::findExecutable(cmd).isEmpty();
    cache.insert(cmd, ok);
    return ok;
}
