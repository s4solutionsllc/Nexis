#include "gnome_settings_tool.h"
#include "Utils/command_util.h"

#include <QDebug>

bool GnomeSettingsTool::isAvailable()
{
    return CommandUtil::isExecutable("gsettings");
}

bool GnomeSettingsTool::schemaExists(const QString &schema)
{
    return cachedSchemas().contains(schema);
}

QSet<QString> GnomeSettingsTool::cachedSchemas()
{
    static QSet<QString> schemas;
    if (schemas.isEmpty()) {
        try {
            QString output = CommandUtil::exec("gsettings", {"list-schemas"});
            const QStringList list = output.split('\n', Qt::SkipEmptyParts);
            for (const QString &s : list)
                schemas.insert(s.trimmed());
        } catch (const QString &ex) {
            qCritical() << "GnomeSettingsTool: failed to list schemas:" << ex;
        }
    }
    return schemas;
}

QString GnomeSettingsTool::getS(const QString &schema, const QString &key)
{
    try {
        QString val = CommandUtil::exec("gsettings", {"get", schema, key});
        // gsettings wraps string values in single quotes
        if (val.startsWith('\'') && val.endsWith('\''))
            val = val.mid(1, val.length() - 2);
        return val;
    } catch (const QString &ex) {
        qCritical() << "GnomeSettingsTool::getS failed:" << schema << key << ex;
        return QString();
    }
}

bool GnomeSettingsTool::getB(const QString &schema, const QString &key)
{
    return getS(schema, key) == "true";
}

int GnomeSettingsTool::getI(const QString &schema, const QString &key)
{
    return getS(schema, key).toInt();
}

double GnomeSettingsTool::getD(const QString &schema, const QString &key)
{
    return getS(schema, key).toDouble();
}

void GnomeSettingsTool::setS(const QString &schema, const QString &key, const QString &value)
{
    try {
        CommandUtil::exec("gsettings", {"set", schema, key, value});
    } catch (const QString &ex) {
        qCritical() << "GnomeSettingsTool::setS failed:" << schema << key << value << ex;
    }
}

void GnomeSettingsTool::setB(const QString &schema, const QString &key, bool value)
{
    setS(schema, key, value ? "true" : "false");
}

void GnomeSettingsTool::setI(const QString &schema, const QString &key, int value)
{
    setS(schema, key, QString::number(value));
}

void GnomeSettingsTool::setD(const QString &schema, const QString &key, double value)
{
    setS(schema, key, QString::number(value, 'f', 6));
}
