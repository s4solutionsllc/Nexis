#include "startup_info_linux.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <Utils/file_util.h>

namespace {

static const QRegularExpression NAME_REG("^Name=.*");
static const QRegularExpression HIDDEN_REG("^Hidden=.*");
static const QRegularExpression GNOME_ENABLED_REG("^X-GNOME-Autostart-enabled=.*");
static const QRegularExpression ICON_REG("^Icon=.*");

QString getDesktopValue(const QRegularExpression &val, const QStringList &lines)
{
    QStringList filteredList = lines.filter(val);
    if (filteredList.count() > 0) {
        QString line = filteredList.first().trimmed();
        int eqPos = line.indexOf('=');
        if (eqPos != -1) {
            return line.mid(eqPos + 1).trimmed();
        }
    }
    return QString("");
}

} // anonymous namespace

QList<StartupAppData> StartupInfoLinux::getStartupApps() const
{
    QList<StartupAppData> apps;
    QDir autostartDir(autostartPath(), "*.desktop");

    QLatin1String enabledStr("true");
    for (const QFileInfo &f : autostartDir.entryInfoList())
    {
        QStringList lines = FileUtil::readListFromFile(f.absoluteFilePath());

        QString appName = getDesktopValue(NAME_REG, lines);

        if (!appName.isEmpty())
        {
            bool enabled = false;

            // Hidden=[true|false]
            QString hidden = getDesktopValue(HIDDEN_REG, lines).toLower();

            // X-GNOME-Autostart-enabled=[true|false]
            QString gnomeEnabled = getDesktopValue(GNOME_ENABLED_REG, lines).toLower();

            if (!hidden.isEmpty()) {
                enabled = (hidden != enabledStr);
            } else {
                enabled = (gnomeEnabled == enabledStr);
            }

            QString iconName = getDesktopValue(ICON_REG, lines);

            StartupAppData data;
            data.name = appName;
            data.filePath = f.absoluteFilePath();
            data.iconPath = iconName;
            data.enabled = enabled;
            apps.append(data);
        }
    }

    return apps;
}

QString StartupInfoLinux::autostartPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart";
}

bool StartupInfoLinux::isAutostartDisabled() const
{
    QString path = autostartPath();
    QFileInfo asfi(path);

    if (asfi.isDir())
        return false;

    // Check if X-GNOME-Autostart-enabled=false is present
    const QByteArray disabled_str("X-GNOME-Autostart-enabled=false");
    QFile autostart_file(path);
    autostart_file.open(QIODevice::ReadOnly | QIODevice::Text);
    return autostart_file.readAll().indexOf(disabled_str, 0) != -1;
}
