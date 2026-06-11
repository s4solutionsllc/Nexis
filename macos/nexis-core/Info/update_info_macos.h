#ifndef UPDATE_INFO_MACOS_H
#define UPDATE_INFO_MACOS_H

#include <Info/update_info.h>

class UpdateInfoMacOS : public UpdateInfo
{
public:
    UpdateCheckResult checkForUpdates() override;
    QStringList availableSources() const override;

    // SSO-3741 (FW-13): pure parser seams for fixture tests. Both helpers are
    // platform-agnostic string parsing, so they can be compiled into the test
    // target on either OS.
    static void parseBrewOutdatedJson(const QByteArray &json, UpdateCheckResult &result);
    static void parseSoftwareUpdateLines(const QStringList &lines, UpdateCheckResult &result);
};

#endif // UPDATE_INFO_MACOS_H
