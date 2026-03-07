#include "power_profile_info_macos.h"

PowerProfileInfoMacOS::PowerProfileInfoMacOS()
{
    mData.backend = PowerBackend::None;
}

void PowerProfileInfoMacOS::refresh()
{
}

bool PowerProfileInfoMacOS::setProfile(const QString &)
{
    return false;
}
