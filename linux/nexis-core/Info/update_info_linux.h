#ifndef UPDATE_INFO_LINUX_H
#define UPDATE_INFO_LINUX_H

#include <Info/update_info.h>

class UpdateInfoLinux : public UpdateInfo
{
public:
    UpdateCheckResult checkForUpdates() override;
    QStringList availableSources() const override;

    // SSO-3741 (FW-13): pure parser seams. Each consumes the raw `--upgradable`
    // /list output from the package manager and appends UpdateEntry rows to the
    // result. Exposed so tests/core/test_update_info.cpp can exercise the
    // parsing without spawning the underlying tool.
    static void parseAptLines(const QStringList &lines, UpdateCheckResult &result);
    static void parseDnfCheckUpdateLines(const QStringList &lines, UpdateCheckResult &result);
    static void parsePacmanQuLines(const QStringList &lines, UpdateCheckResult &result);
    static void parseZypperListUpdatesLines(const QStringList &lines, UpdateCheckResult &result);
    static void parseSnapRefreshLines(const QStringList &lines, UpdateCheckResult &result);
    static void parseFlatpakUpdateLines(const QStringList &lines, UpdateCheckResult &result);

private:
    void checkApt(UpdateCheckResult &result) const;
    void checkDnf(UpdateCheckResult &result) const;
    void checkPacman(UpdateCheckResult &result) const;
    void checkZypper(UpdateCheckResult &result) const;
    void checkSnap(UpdateCheckResult &result) const;
    void checkFlatpak(UpdateCheckResult &result) const;
};

#endif // UPDATE_INFO_LINUX_H
