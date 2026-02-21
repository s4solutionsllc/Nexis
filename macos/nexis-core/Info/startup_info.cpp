#include "startup_info_macos.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <Utils/file_util.h>

QList<StartupAppData> StartupInfoMacOS::getStartupApps() const
{
    QList<StartupAppData> apps;
    QDir autostartDir(autostartPath(), "*.plist");

    for (const QFileInfo &f : autostartDir.entryInfoList())
    {
        QString identifier = f.completeBaseName();

        // Skip Apple internal agents
        if (identifier.startsWith("com.apple."))
            continue;

        // Check if the plist has a Disabled key
        bool enabled = true;
        QStringList lines = FileUtil::readListFromFile(f.absoluteFilePath());
        for (int i = 0; i < lines.size(); ++i) {
            if (lines.at(i).contains("Disabled") && i + 1 < lines.size()) {
                enabled = !lines.at(i + 1).contains("true");
                break;
            }
        }

        // Derive a human-friendly display name from the reverse-DNS identifier.
        // e.g. "com.jetbrains.toolbox" -> "Jetbrains Toolbox"
        //      "com.Tautulli.Tautulli" -> "Tautulli"
        //      "homebrew.mxcl.redis"   -> "Redis"
        QString appName = identifier;
        QStringList parts = identifier.split('.');
        if (parts.size() >= 3) {
            QStringList nameParts;
            for (int i = 1; i < parts.size(); ++i) {
                if (!nameParts.isEmpty()
                    && nameParts.last().compare(parts.at(i), Qt::CaseInsensitive) == 0)
                    continue;
                QString p = parts.at(i);
                if (!p.isEmpty())
                    p[0] = p[0].toUpper();
                nameParts << p;
            }
            appName = nameParts.join(' ');
        } else if (parts.size() == 2) {
            appName = parts.last();
            if (!appName.isEmpty())
                appName[0] = appName[0].toUpper();
        }

        // Try to derive an icon from ProgramArguments if it points to a .app bundle
        QString iconPath;
        QString content = lines.join("\n");
        QRegularExpression progArgReg("<string>(/[^<]*\\.app)[^<]*</string>");
        QRegularExpressionMatch appMatch = progArgReg.match(content);
        if (appMatch.hasMatch()) {
            iconPath = appMatch.captured(1);
        }

        StartupAppData data;
        data.name = appName;
        data.filePath = f.absoluteFilePath();
        data.iconPath = iconPath;
        data.enabled = enabled;
        apps.append(data);
    }

    return apps;
}

QString StartupInfoMacOS::autostartPath() const
{
    return QDir::homePath() + "/Library/LaunchAgents";
}

bool StartupInfoMacOS::isAutostartDisabled() const
{
    return false;
}
