#include "host_service.h"
#include <QRegularExpression>
#include <QHostAddress>
#include <QDebug>
#include <Utils/file_util.h>
#include <Utils/command_util.h>

HostService *HostService::instance = nullptr;

HostService *HostService::ins()
{
    if (!instance)
        instance = new HostService;
    return instance;
}

HostService::HostService(QObject *parent)
    : QObject(parent)
{
}

QStringList HostService::readHostFile()
{
    return FileUtil::readListFromFile("/etc/hosts");
}

QMap<int, HostEntry> HostService::parseHostEntries(const QStringList &fileContent)
{
    QMap<int, HostEntry> entries;

    int i = 0;
    for (const QString &line : fileContent)
    {
        if (!line.trimmed().startsWith("#") && !line.trimmed().isEmpty())
        {
            QString effective = line.trimmed();
            int commentIdx = effective.indexOf('#');
            if (commentIdx >= 0)
                effective.truncate(commentIdx);
            effective = effective.trimmed();
            if (effective.isEmpty()) { i++; continue; }

            static const QRegularExpression whitespace("\\s+");
            QStringList lineItems = effective.split(whitespace);

            if (lineItems.count() > 1) {
                HostEntry entry;
                entry.ip = lineItems.at(0).trimmed();
                entry.fullQualified = lineItems.at(1).trimmed();
                entry.aliases = lineItems.count() > 2 ? lineItems.mid(2).join(" ") : "";

                entries.insert(i, entry);
            }
        }
        i++;
    }

    return entries;
}

bool HostService::isValidIP(const QString &ip)
{
    QHostAddress addr;
    return addr.setAddress(ip);
}

bool HostService::isValidHostname(const QString &hostname)
{
    if (hostname.isEmpty() || hostname.length() > 253)
        return false;

    static const QRegularExpression labelRegex("^[a-zA-Z0-9]([a-zA-Z0-9_-]{0,61}[a-zA-Z0-9])?$");

    QStringList labels = hostname.split('.');
    for (const QString &label : labels) {
        if (label.isEmpty() || label.length() > 63)
            return false;
        if (label.length() == 1) {
            if (!label.at(0).isLetterOrNumber())
                return false;
        } else if (!labelRegex.match(label).hasMatch()) {
            return false;
        }
    }
    return true;
}

bool HostService::createBackup()
{
    try {
        CommandUtil::sudoExec("cp", {"-p", "/etc/hosts", "/etc/hosts.nexis-backup"});
        return true;
    } catch (const QString &ex) {
        qWarning() << "Backup failed:" << ex;
        emit backupFailed(ex);
        return false;
    }
}

bool HostService::saveHostFile(const QStringList &content)
{
    QString data = content.join("\n") + "\n";
    try {
        QString result = CommandUtil::sudoExec("tee", {"/etc/hosts"}, data.toUtf8());

        if (result.isEmpty() && !data.trimmed().isEmpty()) {
            emit saveFailed(QObject::tr("The operation was cancelled or permission was denied."));
            return false;
        }

        emit saveSucceeded();
        return true;
    } catch (const QString &ex) {
        qCritical() << "Save failed:" << ex;
        emit saveFailed(ex);
        return false;
    }
}
