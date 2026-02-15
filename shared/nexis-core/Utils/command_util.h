#ifndef COMMAND_UTIL_H
#define COMMAND_UTIL_H

#include <QStringList>

#include "nexis-core_global.h"

class NEXISCORESHARED_EXPORT CommandUtil
{
public:
    static QString sudoExec(const QString &cmd, QStringList args = QStringList(), QByteArray data = QByteArray());
    static QString exec(const QString &cmd, QStringList args = QStringList(), QByteArray data = QByteArray(), int timeoutMs = 30000);
    static bool isExecutable(const QString &cmd);
};

#endif // COMMAND_UTIL_H
