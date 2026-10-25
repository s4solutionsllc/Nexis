#ifndef UPDATE_INFO_LINUX_H
#define UPDATE_INFO_LINUX_H

#include <Info/update_info.h>
#include <QMap>

class UpdateInfoLinux : public UpdateInfo
{
public:
    UpdateCheckResult checkForUpdates() override;
    QStringList availableSources() const override;

    static void parseAptLines(const QStringList &lines, UpdateCheckResult &result);

    // Parses `apt-cache policy <pkg...>` output and returns, per package
    // name, the Phased-Update-Percentage apt reports for its candidate
    // version. A package with no such field (not phased, or apt doesn't
    // report one) is absent from the result — callers treat that as fully
    // available (GH#524: apt list --upgradable's "[phased N%]" annotation is
    // not reliably present, so detection reads this field directly instead).
    static QMap<QString, int> parseAptCachePolicyPhased(const QString &output);

private:
    void checkApt(UpdateCheckResult &result) const;
    void checkDnf(UpdateCheckResult &result) const;
    void checkPacman(UpdateCheckResult &result) const;
    void checkZypper(UpdateCheckResult &result) const;
    void checkSnap(UpdateCheckResult &result) const;
    void checkFlatpak(UpdateCheckResult &result) const;
};

#endif // UPDATE_INFO_LINUX_H
