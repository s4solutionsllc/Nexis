#ifndef STARTUP_INFO_MACOS_H
#define STARTUP_INFO_MACOS_H

#include <Info/startup_info.h>

#include "btm_parser.h"

class StartupInfoMacOS : public StartupInfo
{
public:
    QList<StartupAppData> getStartupApps() const override;
    QList<StartupAppData> getAllLoginItems() const override;
    QString autostartPath() const override;
    bool isAutostartDisabled() const override;

    // SSO-3738 / FW-10: dump and parse `sfltool dumpbtm`, flagging orphan and
    // duplicate records. Returns an empty list when sfltool is missing or
    // refuses to run; `error` is populated with a translated message in that
    // case so the UI can surface it (e.g. "needs sudo for full state").
    QList<BtmRecord> getBtmRecords(QString *error = nullptr) const;

private:
    QList<StartupAppData> loadPlistDir(const QString &dirPath,
                                       LoginItemCategory category,
                                       bool readOnly) const;
    QSet<QString> queryLaunchctlDisabled() const;
};

#endif // STARTUP_INFO_MACOS_H
