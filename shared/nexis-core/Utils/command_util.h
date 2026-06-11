#ifndef COMMAND_UTIL_H
#define COMMAND_UTIL_H

#include <QFuture>
#include <QStringList>

#include "nexis-core_global.h"

struct NEXISCORESHARED_EXPORT ExecResult {
    QString output;
    QString error;
    int exitCode;
};

class NEXISCORESHARED_EXPORT CommandUtil
{
public:
    // WI-21 (SSO-3383): callers that invoke a polkit/osascript prompt should
    // pass a much larger timeoutMs (5+ minutes) so a slow password entry does
    // not race the default 30 s waitForFinished cap. Use -1 to wait
    // indefinitely (QProcess::waitForFinished's wait-forever semantics).
    static QString sudoExec(const QString &cmd, QStringList args = QStringList(), QByteArray data = QByteArray(), int timeoutMs = 30000);
    static QString exec(const QString &cmd, QStringList args = QStringList(), QByteArray data = QByteArray(), int timeoutMs = 30000);
    static ExecResult execWithStatus(const QString &cmd, QStringList args = QStringList(), int timeoutMs = 30000);
    static QFuture<ExecResult> execAsync(const QString &cmd, QStringList args = QStringList(), int timeoutMs = 30000);
    static bool isExecutable(const QString &cmd);
};

#endif // COMMAND_UTIL_H
