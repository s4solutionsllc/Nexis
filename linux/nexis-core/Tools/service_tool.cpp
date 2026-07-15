#include "service_tool_linux.h"
#include "Utils/command_util.h"

#include <QDebug>
#include <QRegularExpression>
#include <QDir>

// Service constructor is in shared/nexis-core/Tools/service_tool_shared.cpp

QList<Service> ServiceToolLinux::getServices()
{
    QList<Service> services = {};

    QStringList args = { "list-unit-files", "-t", "service", "-a", "--state=enabled,disabled" };

    ExecResult result = CommandUtil::execWithStatus("systemctl", args);
    if (!result.ok()) {
        qCritical() << result.error;
        return services;
    }

    QStringList lines = result.output
            .split(QChar('\n'))
            .filter(QRegularExpression("[^@].service"));

    QRegularExpression sep("\\s+");
    services.reserve(lines.size());
    for (const QString &line : lines)
    {
        // e.g apache2.service          [enabled|disabled]
        QStringList s = line.trimmed().split(sep);

        QString name = s.first().trimmed().replace(".service", "");
        QString description = getServiceDescription(s.first().trimmed());
        bool status = ! s.last().trimmed().compare("enabled");
        bool active = serviceIsActive(s.first().trimmed());

        services.push_back({name, description, status, active});
    }

    return services;
}

QString ServiceToolLinux::getServiceDescription(const QString &serviceName)
{
    QStringList args = { "cat", serviceName };

    QString result("Unknown");

    ExecResult execResult = CommandUtil::execWithStatus("systemctl", args);
    if (!execResult.ok()) {
        qCritical() << execResult.error;
        return result;
    }

    QStringList content = execResult.output
            .split(QChar('\n'))
            .filter(QRegularExpression("^Description"));

    if (content.length() > 0) {
        QStringList desc = content.first().split(QChar('='));
        if (desc.length() > 0)
            result = desc.last();
    }

    return result;
}


bool ServiceToolLinux::serviceIsActive(const QString &serviceName)
{
    QStringList args = { "is-active", serviceName };

    ExecResult result = CommandUtil::execWithStatus("systemctl", args);
    return ! result.output.trimmed().compare("active");
}

bool ServiceToolLinux::serviceIsEnabled(const QString &serviceName)
{
    QStringList args = { "is-enabled", serviceName };

    ExecResult result = CommandUtil::execWithStatus("systemctl", args);
    return ! result.output.trimmed().compare("enabled");
}

bool ServiceToolLinux::changeServiceStatus(const QString &sname, bool status)
{
    QStringList args = { (status ? "enable" : "disable") , sname };

    ExecResult result = CommandUtil::sudoExecWithStatus("systemctl", args);
    if (!result.ok()) {
        qCritical() << result.error;
        return false;
    }

    return true;
}

bool ServiceToolLinux::changeServiceActive(const QString &sname, bool status)
{
    QStringList args = { (status ? "start" : "stop") , sname };

    ExecResult result = CommandUtil::sudoExecWithStatus("systemctl", args);
    if (!result.ok()) {
        qCritical() << result.error;
        return false;
    }

    return true;
}
