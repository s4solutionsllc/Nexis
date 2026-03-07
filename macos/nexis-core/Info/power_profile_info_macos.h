#ifndef POWER_PROFILE_INFO_MACOS_H
#define POWER_PROFILE_INFO_MACOS_H

#include <Info/power_profile_info.h>

class PowerProfileInfoMacOS : public PowerProfileInfo
{
public:
    PowerProfileInfoMacOS();

    void refresh() override;
    bool setProfile(const QString &profile) override;
};

#endif // POWER_PROFILE_INFO_MACOS_H
