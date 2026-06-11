#include "command_util.h"

#include <QCoreApplication>
#include <QStringList>
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
// migration. SSO-3367 also uses this hook to find sites that still need to
// migrate to execWithStatus / execAsync.
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

// Core QProcess driver shared by every CommandUtil entry point. Never throws —
// QProcess errors are reported through ExecResult.error / exitCode.
ExecResult runProcess(const QString &cmd, const QStringList &args, const QByteArray &data, int timeoutMs)
{
    auditUiThread(cmd, args);

    ExecResult result;

    std::unique_ptr<QProcess> process(new QProcess());
    process->start(cmd, args);

    if (!process->waitForStarted(timeoutMs)) {
        result.error = process->errorString();
        return result;
    }

    if (!data.isEmpty()) {
        process->write(data);
        process->waitForBytesWritten();
        process->closeWriteChannel();
    }

    process->waitForFinished(timeoutMs);

    result.output = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
    result.error = QString::fromUtf8(process->readAllStandardError()).trimmed();

    // QProcess::error() is UnknownError when nothing went wrong. Treat any
    // start/run/timeout error as a failure: surface errorString() in
    // result.error and force a non-zero exit code so ok() == false.
    if (process->error() != QProcess::UnknownError) {
        if (result.error.isEmpty()) {
            result.error = process->errorString();
        }
        result.exitCode = -1;
    } else if (process->exitStatus() == QProcess::NormalExit) {
        result.exitCode = process->exitCode();
    } else {
        result.exitCode = -1;
    }

    process->kill();
    process->close();

    return result;
}

} // namespace

ExecResult CommandUtil::execWithStatus(const QString &cmd, QStringList args, int timeoutMs)
{
    return runProcess(cmd, args, QByteArray(), timeoutMs);
}

ExecResult CommandUtil::execWithStatus(const QString &cmd, QStringList args, QByteArray data, int timeoutMs)
{
    return runProcess(cmd, args, data, timeoutMs);
}

QString CommandUtil::exec(const QString &cmd, QStringList args, QByteArray data, int timeoutMs)
{
    // SSO-3367: exec() used to throw a raw QString on any QProcess error,
    // including the 30 s default timeout. Now it shares the unified ExecResult
    // contract — failure is logged here and an empty QString is returned, the
    // same outward shape sudoExec has always had. Callers that need
    // authoritative status should migrate to execWithStatus.
    const ExecResult result = runProcess(cmd, args, data, timeoutMs);

    if (!result.ok()) {
        qCritical().nospace() << "CommandUtil::exec failed: " << cmd
                              << " " << args
                              << " (exitCode=" << result.exitCode
                              << ", error=" << result.error << ")";
    }

    return result.output;
}

QFuture<ExecResult> CommandUtil::execAsync(const QString &cmd, QStringList args, int timeoutMs)
{
    return QtConcurrent::run([cmd, args, timeoutMs] {
        return CommandUtil::execWithStatus(cmd, args, timeoutMs);
    });
}

QString CommandUtil::buildMacOsSudoShellCommand(const QString &cmd,
                                                 const QStringList &args)
{
    // Build the bare shell command first, single-quoting each argument to
    // neutralise spaces and shell metacharacters. Embedded single quotes are
    // escaped via the standard close-reopen idiom: `'\''`.
    QString shellCmd = cmd;
    for (const QString &arg : args) {
        QString escaped = arg;
        escaped.replace("'", "'\\''");
        shellCmd += " '" + escaped + "'";
    }
    // Then escape backslashes and double-quotes so the result is safe to drop
    // into an AppleScript double-quoted string literal (`do shell script "…"`).
    shellCmd.replace("\\", "\\\\");
    shellCmd.replace("\"", "\\\"");
    return shellCmd;
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
