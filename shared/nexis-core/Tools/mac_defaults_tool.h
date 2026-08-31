#ifndef MAC_DEFAULTS_TOOL_H
#define MAC_DEFAULTS_TOOL_H

// SSO-23857: thin wrapper around the macOS `defaults`(1) CLI used by the
// Tweaks pane. Split into pure argv-building/output-parsing (unit-testable
// on any platform) and exec glue (only meaningful on macOS, but harmless to
// compile/link elsewhere since it just shells out via CommandUtil).

#include <QString>
#include <QStringList>
#include <QVariant>

#include "nexis-core_global.h"

enum class MacDefaultsValueType { Bool, Int, String };

struct NEXISCORESHARED_EXPORT MacDefaultsReadResult {
    bool     found = false;   // key currently has an explicit override
    QVariant value;            // parsed value; only meaningful when found == true
    QString  errorMsg;         // set when the read genuinely failed (not just "unset")
};

struct NEXISCORESHARED_EXPORT MacDefaultsWriteResult {
    bool    ok = false;
    QString errorMsg;
};

class NEXISCORESHARED_EXPORT MacDefaultsTool
{
public:
    static constexpr const char *kDefaultsBinary = "/usr/bin/defaults";
    static constexpr const char *kKillallBinary   = "/usr/bin/killall";

    // ---- Pure argv builders — no process execution, fully unit-testable ----
    static QStringList buildReadArgs(const QString &domain, const QString &key);
    static QStringList buildWriteArgs(const QString &domain, const QString &key,
                                       MacDefaultsValueType type, const QVariant &value);
    static QStringList buildDeleteArgs(const QString &domain, const QString &key);

    // ---- Pure output parsing — no process execution, fully unit-testable ----
    static MacDefaultsReadResult parseReadOutput(MacDefaultsValueType type,
                                                  const QString &stdOut,
                                                  int exitCode,
                                                  const QString &stdErr);

    // ---- Execution glue (shells to /usr/bin/defaults, optionally killall) ----
    static MacDefaultsReadResult readValue(const QString &domain, const QString &key,
                                            MacDefaultsValueType type);
    static MacDefaultsWriteResult writeValue(const QString &domain, const QString &key,
                                              MacDefaultsValueType type, const QVariant &value,
                                              bool requiresSudo,
                                              const QStringList &killApps = QStringList());
    static MacDefaultsWriteResult revertToDefault(const QString &domain, const QString &key,
                                                   bool requiresSudo,
                                                   const QStringList &killApps = QStringList());

private:
    static void killApps(const QStringList &apps);
};

#endif // MAC_DEFAULTS_TOOL_H
