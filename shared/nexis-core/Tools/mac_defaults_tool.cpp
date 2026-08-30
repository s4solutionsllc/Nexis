#include "mac_defaults_tool.h"

#include "Utils/command_util.h"

QStringList MacDefaultsTool::buildReadArgs(const QString &domain, const QString &key)
{
    return {QStringLiteral("read"), domain, key};
}

QStringList MacDefaultsTool::buildWriteArgs(const QString &domain, const QString &key,
                                             MacDefaultsValueType type, const QVariant &value)
{
    QString typeFlag;
    QString valueStr;
    switch (type) {
    case MacDefaultsValueType::Bool:
        typeFlag = QStringLiteral("-bool");
        valueStr = value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        break;
    case MacDefaultsValueType::Int:
        typeFlag = QStringLiteral("-int");
        valueStr = QString::number(value.toInt());
        break;
    case MacDefaultsValueType::String:
        typeFlag = QStringLiteral("-string");
        valueStr = value.toString();
        break;
    }
    return {QStringLiteral("write"), domain, key, typeFlag, valueStr};
}

QStringList MacDefaultsTool::buildDeleteArgs(const QString &domain, const QString &key)
{
    return {QStringLiteral("delete"), domain, key};
}

MacDefaultsReadResult MacDefaultsTool::parseReadOutput(MacDefaultsValueType type,
                                                        const QString &stdOut,
                                                        int exitCode,
                                                        const QString &stdErr)
{
    MacDefaultsReadResult r;
    const QString trimmed = stdOut.trimmed();

    if (exitCode != 0) {
        // `defaults read` exits non-zero both when the domain/key pair is
        // simply unset (the common case — falls back to the system default)
        // and on a genuine error. It prints "does not exist" for the former;
        // treat anything else as a real error worth surfacing.
        r.found = false;
        if (!stdErr.contains(QStringLiteral("does not exist"), Qt::CaseInsensitive))
            r.errorMsg = stdErr.trimmed();
        return r;
    }

    if (trimmed.isEmpty()) {
        r.found = false;
        return r;
    }

    r.found = true;
    switch (type) {
    case MacDefaultsValueType::Bool:
        r.value = (trimmed == QStringLiteral("1") || trimmed.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0);
        break;
    case MacDefaultsValueType::Int: {
        bool ok = false;
        const int v = trimmed.toInt(&ok);
        if (!ok) {
            r.found = false;
            r.errorMsg = QObject::tr("Unexpected non-integer value \"%1\".").arg(trimmed);
            return r;
        }
        r.value = v;
        break;
    }
    case MacDefaultsValueType::String:
        r.value = trimmed;
        break;
    }
    return r;
}

MacDefaultsReadResult MacDefaultsTool::readValue(const QString &domain, const QString &key,
                                                  MacDefaultsValueType type)
{
    const ExecResult res = CommandUtil::execWithStatus(
        QString::fromLatin1(kDefaultsBinary), buildReadArgs(domain, key));
    return parseReadOutput(type, res.output, res.exitCode, res.error);
}

MacDefaultsWriteResult MacDefaultsTool::writeValue(const QString &domain, const QString &key,
                                                    MacDefaultsValueType type, const QVariant &value,
                                                    bool requiresSudo, const QStringList &killList)
{
    const QStringList args = buildWriteArgs(domain, key, type, value);
    const ExecResult res = requiresSudo
        ? CommandUtil::sudoExecWithStatus(QString::fromLatin1(kDefaultsBinary), args)
        : CommandUtil::execWithStatus(QString::fromLatin1(kDefaultsBinary), args);

    MacDefaultsWriteResult r;
    r.ok = res.ok();
    if (!r.ok)
        r.errorMsg = res.error;
    if (r.ok)
        killApps(killList);
    return r;
}

MacDefaultsWriteResult MacDefaultsTool::revertToDefault(const QString &domain, const QString &key,
                                                         bool requiresSudo, const QStringList &killList)
{
    const QStringList args = buildDeleteArgs(domain, key);
    const ExecResult res = requiresSudo
        ? CommandUtil::sudoExecWithStatus(QString::fromLatin1(kDefaultsBinary), args)
        : CommandUtil::execWithStatus(QString::fromLatin1(kDefaultsBinary), args);

    MacDefaultsWriteResult r;
    // "delete" on an already-unset key exits non-zero ("does not exist") —
    // that's still a successful revert (nothing to remove), not a failure.
    r.ok = res.ok() || res.error.contains(QStringLiteral("does not exist"), Qt::CaseInsensitive);
    if (!r.ok)
        r.errorMsg = res.error;
    if (r.ok)
        killApps(killList);
    return r;
}

void MacDefaultsTool::killApps(const QStringList &apps)
{
    for (const QString &app : apps)
        CommandUtil::execWithStatus(QString::fromLatin1(kKillallBinary), {app});
}
