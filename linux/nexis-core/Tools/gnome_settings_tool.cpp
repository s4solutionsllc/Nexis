#include "gnome_settings_tool_linux.h"
#include "Utils/command_util.h"

#include <QDebug>

bool GnomeSettingsToolLinux::isAvailable()
{
    return CommandUtil::isExecutable("gsettings");
}

bool GnomeSettingsToolLinux::schemaExists(const QString &schema)
{
    return cachedSchemas().contains(schema);
}

QSet<QString> GnomeSettingsToolLinux::cachedSchemas()
{
    static QSet<QString> schemas;
    if (schemas.isEmpty()) {
        ExecResult result = CommandUtil::execWithStatus("gsettings", {"list-schemas"});
        if (!result.ok()) {
            qCritical() << "GnomeSettingsTool: failed to list schemas:" << result.error;
            return schemas;
        }
        const QStringList list = result.output.split('\n', Qt::SkipEmptyParts);
        for (const QString &s : list)
            schemas.insert(s.trimmed());
    }
    return schemas;
}

QString GnomeSettingsToolLinux::getS(const QString &schema, const QString &key)
{
    ExecResult result = CommandUtil::execWithStatus("gsettings", {"get", schema, key});
    if (!result.ok()) {
        qCritical() << "GnomeSettingsToolLinux::getS failed:" << schema << key << result.error;
        return QString();
    }

    // gsettings wraps string values in single quotes
    QString val = result.output;
    if (val.startsWith('\'') && val.endsWith('\''))
        val = val.mid(1, val.length() - 2);
    return val;
}

bool GnomeSettingsToolLinux::getB(const QString &schema, const QString &key)
{
    return getS(schema, key) == "true";
}

int GnomeSettingsToolLinux::getI(const QString &schema, const QString &key)
{
    return getS(schema, key).toInt();
}

double GnomeSettingsToolLinux::getD(const QString &schema, const QString &key)
{
    return getS(schema, key).toDouble();
}

bool GnomeSettingsToolLinux::setS(const QString &schema, const QString &key, const QString &value)
{
    ExecResult result = CommandUtil::execWithStatus("gsettings", {"set", schema, key, value});
    if (result.exitCode != 0) {
        qCritical() << "GnomeSettingsToolLinux::setS failed:" << schema << key << value << result.error;
        return false;
    }
    return true;
}

bool GnomeSettingsToolLinux::setB(const QString &schema, const QString &key, bool value)
{
    return setS(schema, key, value ? "true" : "false");
}

bool GnomeSettingsToolLinux::setI(const QString &schema, const QString &key, int value)
{
    return setS(schema, key, QString::number(value));
}

bool GnomeSettingsToolLinux::setD(const QString &schema, const QString &key, double value)
{
    return setS(schema, key, QString::number(value, 'f', 6));
}
