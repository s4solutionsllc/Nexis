#include "gnome_settings_tool.h"
#include "Utils/command_util.h"

#include <QDebug>

#ifdef Q_OS_LINUX

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

#elif defined(Q_OS_MACOS)

// On macOS, we use the `defaults` command instead of `gsettings`.
// The "schema" maps to the macOS domain (e.g., "com.apple.dock")
// The "key" maps to the defaults key.

bool GnomeSettingsTool::isAvailable()
{
    // `defaults` is always available on macOS
    return true;
}

bool GnomeSettingsTool::schemaExists(const QString &schema)
{
    // Check if the domain has any keys
    try {
        QString output = CommandUtil::exec("defaults", {"read", schema});
        return !output.isEmpty();
    } catch (...) {
        return false;
    }
}

QSet<QString> GnomeSettingsTool::cachedSchemas()
{
    static QSet<QString> schemas;
    if (schemas.isEmpty()) {
        // Add known macOS preference domains
        schemas.insert("NSGlobalDomain");
        schemas.insert("com.apple.dock");
        schemas.insert("com.apple.finder");
        schemas.insert("com.apple.systempreferences");
        schemas.insert("com.apple.desktopservices");
        schemas.insert("com.apple.screensaver");
        schemas.insert("com.apple.AppleMultitouchTrackpad");
    }
    return schemas;
}

QString GnomeSettingsTool::getS(const QString &schema, const QString &key)
{
    try {
        QString val = CommandUtil::exec("defaults", {"read", schema, key});
        return val.trimmed();
    } catch (const QString &ex) {
        qCritical() << "GnomeSettingsTool::getS (macOS defaults) failed:" << schema << key << ex;
        return QString();
    }
}

bool GnomeSettingsTool::getB(const QString &schema, const QString &key)
{
    QString val = getS(schema, key);
    return val == "1" || val.toLower() == "true";
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
        CommandUtil::exec("defaults", {"write", schema, key, "-string", value});
    } catch (const QString &ex) {
        qCritical() << "GnomeSettingsTool::setS (macOS defaults) failed:" << schema << key << value << ex;
    }
}

void GnomeSettingsTool::setB(const QString &schema, const QString &key, bool value)
{
    try {
        CommandUtil::exec("defaults", {"write", schema, key, "-bool", value ? "true" : "false"});
    } catch (const QString &ex) {
        qCritical() << "GnomeSettingsTool::setB (macOS defaults) failed:" << schema << key << ex;
    }
}

void GnomeSettingsTool::setI(const QString &schema, const QString &key, int value)
{
    try {
        CommandUtil::exec("defaults", {"write", schema, key, "-int", QString::number(value)});
    } catch (const QString &ex) {
        qCritical() << "GnomeSettingsTool::setI (macOS defaults) failed:" << schema << key << ex;
    }
}

void GnomeSettingsTool::setD(const QString &schema, const QString &key, double value)
{
    try {
        CommandUtil::exec("defaults", {"write", schema, key, "-float", QString::number(value, 'f', 6)});
    } catch (const QString &ex) {
        qCritical() << "GnomeSettingsTool::setD (macOS defaults) failed:" << schema << key << ex;
    }
}

#endif
