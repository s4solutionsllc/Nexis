#include "gnome_settings_tool_macos.h"
#include "Utils/command_util.h"

#include <QDebug>

// On macOS, we use the `defaults` command instead of `gsettings`.
// The "schema" maps to the macOS domain (e.g., "com.apple.dock")
// The "key" maps to the defaults key.

bool GnomeSettingsToolMacOS::isAvailable()
{
    // `defaults` is always available on macOS
    return true;
}

bool GnomeSettingsToolMacOS::schemaExists(const QString &schema)
{
    // Check if the domain has any keys
    try {
        QString output = CommandUtil::exec("defaults", {"read", schema});
        return !output.isEmpty();
    } catch (...) {
        return false;
    }
}

QSet<QString> GnomeSettingsToolMacOS::cachedSchemas()
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

QString GnomeSettingsToolMacOS::getS(const QString &schema, const QString &key)
{
    try {
        QString val = CommandUtil::exec("defaults", {"read", schema, key});
        return val.trimmed();
    } catch (const QString &ex) {
        qCritical() << "GnomeSettingsToolMacOS::getS (macOS defaults) failed:" << schema << key << ex;
        return QString();
    }
}

bool GnomeSettingsToolMacOS::getB(const QString &schema, const QString &key)
{
    QString val = getS(schema, key);
    return val == "1" || val.toLower() == "true";
}

int GnomeSettingsToolMacOS::getI(const QString &schema, const QString &key)
{
    return getS(schema, key).toInt();
}

double GnomeSettingsToolMacOS::getD(const QString &schema, const QString &key)
{
    return getS(schema, key).toDouble();
}

bool GnomeSettingsToolMacOS::setS(const QString &schema, const QString &key, const QString &value)
{
    ExecResult result = CommandUtil::execWithStatus("defaults", {"write", schema, key, "-string", value});
    if (result.exitCode != 0) {
        qCritical() << "GnomeSettingsToolMacOS::setS (macOS defaults) failed:" << schema << key << value << result.error;
        return false;
    }
    return true;
}

bool GnomeSettingsToolMacOS::setB(const QString &schema, const QString &key, bool value)
{
    ExecResult result = CommandUtil::execWithStatus("defaults", {"write", schema, key, "-bool", value ? "true" : "false"});
    if (result.exitCode != 0) {
        qCritical() << "GnomeSettingsToolMacOS::setB (macOS defaults) failed:" << schema << key << result.error;
        return false;
    }
    return true;
}

bool GnomeSettingsToolMacOS::setI(const QString &schema, const QString &key, int value)
{
    ExecResult result = CommandUtil::execWithStatus("defaults", {"write", schema, key, "-int", QString::number(value)});
    if (result.exitCode != 0) {
        qCritical() << "GnomeSettingsToolMacOS::setI (macOS defaults) failed:" << schema << key << result.error;
        return false;
    }
    return true;
}

bool GnomeSettingsToolMacOS::setD(const QString &schema, const QString &key, double value)
{
    ExecResult result = CommandUtil::execWithStatus("defaults", {"write", schema, key, "-float", QString::number(value, 'f', 6)});
    if (result.exitCode != 0) {
        qCritical() << "GnomeSettingsToolMacOS::setD (macOS defaults) failed:" << schema << key << result.error;
        return false;
    }
    return true;
}
