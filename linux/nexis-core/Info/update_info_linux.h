#ifndef UPDATE_INFO_LINUX_H
#define UPDATE_INFO_LINUX_H

#include <Info/update_info.h>

class UpdateInfoLinux : public UpdateInfo
{
public:
    UpdateCheckResult checkForUpdates() override;
    QStringList availableSources() const override;

private:
    void checkApt(UpdateCheckResult &result) const;
    void checkDnf(UpdateCheckResult &result) const;
    void checkPacman(UpdateCheckResult &result) const;
    void checkZypper(UpdateCheckResult &result) const;
    void checkSnap(UpdateCheckResult &result) const;
    void checkFlatpak(UpdateCheckResult &result) const;
};

#endif // UPDATE_INFO_LINUX_H
