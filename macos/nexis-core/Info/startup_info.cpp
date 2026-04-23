#include "startup_info_macos.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <Utils/file_util.h>
#include <unistd.h>

static QString deriveDisplayName(const QString &identifier)
{
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
        return nameParts.join(' ');
    } else if (parts.size() == 2) {
        QString name = parts.last();
        if (!name.isEmpty())
            name[0] = name[0].toUpper();
        return name;
    }
    return identifier;
}

QSet<QString> StartupInfoMacOS::queryLaunchctlDisabled() const
{
    QSet<QString> disabled;
    QProcess proc;
    proc.start(QStringLiteral("launchctl"),
               {QStringLiteral("print-disabled"),
                QStringLiteral("user/%1").arg(getuid())});
    if (!proc.waitForFinished(3000))
        return disabled;

    static const QRegularExpression lineRx(
        QStringLiteral("\"([^\"]+)\"\\s*=>\\s*disabled"));
    const QString output = QString::fromUtf8(proc.readAllStandardOutput());
    QRegularExpressionMatchIterator it = lineRx.globalMatch(output);
    while (it.hasNext())
        disabled.insert(it.next().captured(1));
    return disabled;
}

QList<StartupAppData> StartupInfoMacOS::loadPlistDir(const QString &dirPath,
                                                      LoginItemCategory category,
                                                      bool readOnly) const
{
    QList<StartupAppData> apps;
    QDir dir(dirPath, QStringLiteral("*.plist"));
    if (!dir.exists())
        return apps;

    for (const QFileInfo &f : dir.entryInfoList()) {
        QString identifier = f.completeBaseName();

        if (identifier.startsWith(QStringLiteral("com.apple.")))
            continue;

        QStringList lines = FileUtil::readListFromFile(f.absoluteFilePath());
        const QString content = lines.join('\n');

        bool enabled = true;
        for (int i = 0; i < lines.size(); ++i) {
            if (lines.at(i).contains(QStringLiteral("Disabled")) && i + 1 < lines.size()) {
                enabled = !lines.at(i + 1).contains(QStringLiteral("true"));
                break;
            }
        }

        QString iconPath;
        QRegularExpression progArgReg(QStringLiteral("<string>(/[^<]*\\.app)[^<]*</string>"));
        QRegularExpressionMatch appMatch = progArgReg.match(content);
        if (appMatch.hasMatch())
            iconPath = appMatch.captured(1);

        StartupAppData data;
        data.name = deriveDisplayName(identifier);
        data.identifier = identifier;
        data.filePath = f.absoluteFilePath();
        data.iconPath = iconPath;
        data.enabled = enabled;
        data.readOnly = readOnly;
        data.category = category;
        apps.append(data);
    }

    return apps;
}

QList<StartupAppData> StartupInfoMacOS::getStartupApps() const
{
    QSet<QString> disabledByLaunchctl = queryLaunchctlDisabled();

    QList<StartupAppData> apps = loadPlistDir(autostartPath(),
                                              LoginItemCategory::UserAgent, false);
    for (StartupAppData &d : apps) {
        if (!d.identifier.isEmpty() && disabledByLaunchctl.contains(d.identifier))
            d.enabled = false;
    }
    return apps;
}

QList<StartupAppData> StartupInfoMacOS::getAllLoginItems() const
{
    QList<StartupAppData> all = getStartupApps();

    all += loadPlistDir(QStringLiteral("/Library/LaunchAgents"),
                        LoginItemCategory::SystemAgent, true);
    all += loadPlistDir(QStringLiteral("/Library/LaunchDaemons"),
                        LoginItemCategory::SystemDaemon, true);

    return all;
}

QString StartupInfoMacOS::autostartPath() const
{
    return QDir::homePath() + QStringLiteral("/Library/LaunchAgents");
}

bool StartupInfoMacOS::isAutostartDisabled() const
{
    return false;
}
