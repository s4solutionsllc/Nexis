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
    static QString sudoExec(const QString &cmd, QStringList args = QStringList(), QByteArray data = QByteArray());
    static QString exec(const QString &cmd, QStringList args = QStringList(), QByteArray data = QByteArray(), int timeoutMs = 30000);
    static ExecResult execWithStatus(const QString &cmd, QStringList args = QStringList(), int timeoutMs = 30000);
    static QFuture<ExecResult> execAsync(const QString &cmd, QStringList args = QStringList(), int timeoutMs = 30000);
    static bool isExecutable(const QString &cmd);
};

#endif // COMMAND_UTIL_H
