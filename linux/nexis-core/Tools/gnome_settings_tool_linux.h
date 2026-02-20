#ifndef GNOME_SETTINGS_TOOL_LINUX_H
#define GNOME_SETTINGS_TOOL_LINUX_H

#include <Tools/gnome_settings_tool.h>

class GnomeSettingsToolLinux : public GnomeSettingsTool
{
public:
    bool isAvailable() override;
    bool schemaExists(const QString &schema) override;

    QString getS(const QString &schema, const QString &key) override;
    bool    getB(const QString &schema, const QString &key) override;
    int     getI(const QString &schema, const QString &key) override;
    double  getD(const QString &schema, const QString &key) override;

    bool setS(const QString &schema, const QString &key, const QString &value) override;
    bool setB(const QString &schema, const QString &key, bool value) override;
    bool setI(const QString &schema, const QString &key, int value) override;
    bool setD(const QString &schema, const QString &key, double value) override;

private:
    QSet<QString> cachedSchemas();
};

#endif // GNOME_SETTINGS_TOOL_LINUX_H
