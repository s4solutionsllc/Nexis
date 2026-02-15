#include "service_tool.h"

#include <QDebug>
#include <QRegularExpression>
#include <QDir>

// Service constructor is in shared/nexis-core/Tools/service_tool_shared.cpp

// macOS uses launchd (launchctl) instead of systemd (systemctl)
// Services are defined as plist files in:
//   /Library/LaunchDaemons/       (system-wide daemons, run as root)
//   /Library/LaunchAgents/        (system-wide agents, run as user)
//   ~/Library/LaunchAgents/       (per-user agents)

QList<Service> ServiceTool::getServicesWithSystemctl()
{
    QList<Service> services = {};

    try {
        // Use launchctl list to get running services
        QStringList lines = CommandUtil::exec("launchctl", {"list"})
                .split(QChar('\n'));

        if (!lines.isEmpty())
            lines.removeFirst(); // Remove header: PID Status Label

        QRegularExpression sep("\\s+");
        for (const QString &line : lines) {
            QStringList parts = line.trimmed().split(sep);
            if (parts.size() < 3) continue;

            QString pid = parts.at(0);
            QString label = parts.at(2);

            // Skip Apple internal services
            if (label.startsWith("com.apple.") || label.startsWith("["))
                continue;

            bool active = (pid != "-" && pid != "0");
            bool enabled = true; // If it shows in launchctl list, it's loaded

            // Try to get description from the plist Label
            QString description = label;

            services.push_back({label, description, enabled, active});
        }
    } catch(QString &ex) {
        qCritical() << ex;
    }

    return services;
}

QString ServiceTool::getServiceDescription(const QString &serviceName)
{
    // On macOS, service descriptions are in plist files
    // For simplicity, return the service label
    return serviceName;
}

bool ServiceTool::serviceIsActive(const QString &serviceName)
{
    try {
        QString result = CommandUtil::exec("launchctl", {"list", serviceName});
        return !result.isEmpty();
    } catch(...) {
        return false;
    }
}

bool ServiceTool::serviceIsEnabled(const QString &serviceName)
{
    // If it appears in launchctl list, it's enabled/loaded
    return serviceIsActive(serviceName);
}

bool ServiceTool::changeServiceStatus(const QString &sname, bool status)
{
    try {
        // Find the plist file for this service
        QStringList searchDirs = {
            "/Library/LaunchDaemons",
            "/Library/LaunchAgents",
            QDir::homePath() + "/Library/LaunchAgents"
        };

        for (const QString &dir : searchDirs) {
            QString plistPath = QString("%1/%2.plist").arg(dir, sname);
            if (QFile::exists(plistPath)) {
                if (status) {
                    CommandUtil::sudoExec("launchctl", {"load", "-w", plistPath});
                } else {
                    CommandUtil::sudoExec("launchctl", {"unload", "-w", plistPath});
                }
                return true;
            }
        }
    } catch(QString &ex) {
        qCritical() << ex;
    }
    return false;
}

bool ServiceTool::changeServiceActive(const QString &sname, bool status)
{
    try {
        if (status) {
            // Start: kickstart the service
            CommandUtil::sudoExec("launchctl", {"start", sname});
        } else {
            CommandUtil::sudoExec("launchctl", {"stop", sname});
        }
        return true;
    } catch(QString &ex) {
        qCritical() << ex;
    }
    return false;
}
