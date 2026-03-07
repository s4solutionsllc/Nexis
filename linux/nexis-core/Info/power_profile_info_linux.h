#ifndef POWER_PROFILE_INFO_LINUX_H
#define POWER_PROFILE_INFO_LINUX_H

#include <Info/power_profile_info.h>

class PowerProfileInfoLinux : public PowerProfileInfo
{
public:
    PowerProfileInfoLinux();

    void refresh() override;
    bool setProfile(const QString &profile) override;

private:
    void detectBackend();
    void detectConflicts();

    void refreshPPD();
    void refreshSysfs();

    bool setPPD(const QString &profile);
    bool setSysfs(const QString &governor);
};

#endif // POWER_PROFILE_INFO_LINUX_H
