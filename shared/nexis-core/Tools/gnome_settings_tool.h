#ifndef GNOME_SETTINGS_TOOL_H
#define GNOME_SETTINGS_TOOL_H

#include <QString>
#include <QSet>

#include "nexis-core_global.h"
#include "gnome_settings_constants.h"

class NEXISCORESHARED_EXPORT GnomeSettingsTool
{
public:
    virtual ~GnomeSettingsTool() = default;

    virtual bool isAvailable() = 0;
    virtual bool schemaExists(const QString &schema) = 0;

    virtual QString getS(const QString &schema, const QString &key) = 0;
    virtual bool    getB(const QString &schema, const QString &key) = 0;
    virtual int     getI(const QString &schema, const QString &key) = 0;
    virtual double  getD(const QString &schema, const QString &key) = 0;

    virtual bool setS(const QString &schema, const QString &key, const QString &value) = 0;
    virtual bool setB(const QString &schema, const QString &key, bool value) = 0;
    virtual bool setI(const QString &schema, const QString &key, int value) = 0;
    virtual bool setD(const QString &schema, const QString &key, double value) = 0;
};

#endif // GNOME_SETTINGS_TOOL_H
