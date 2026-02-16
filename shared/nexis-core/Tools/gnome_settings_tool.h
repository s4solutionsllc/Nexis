#ifndef GNOME_SETTINGS_TOOL_H
#define GNOME_SETTINGS_TOOL_H

#include <QString>
#include <QSet>

#include "nexis-core_global.h"
#include "gnome_settings_constants.h"

class NEXISCORESHARED_EXPORT GnomeSettingsTool
{
public:
    static bool isAvailable();
    static bool schemaExists(const QString &schema);

    static QString getS(const QString &schema, const QString &key);
    static bool    getB(const QString &schema, const QString &key);
    static int     getI(const QString &schema, const QString &key);
    static double  getD(const QString &schema, const QString &key);

    static bool setS(const QString &schema, const QString &key, const QString &value);
    static bool setB(const QString &schema, const QString &key, bool value);
    static bool setI(const QString &schema, const QString &key, int value);
    static bool setD(const QString &schema, const QString &key, double value);

private:
    static QSet<QString> cachedSchemas();
};

#endif // GNOME_SETTINGS_TOOL_H
