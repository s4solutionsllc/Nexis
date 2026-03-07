#ifndef POWER_PROFILE_INFO_H
#define POWER_PROFILE_INFO_H

#include <QString>
#include <QStringList>
#include "nexis-core_global.h"

enum class PowerBackend {
    None,
    PowerProfilesDaemon,
    Sysfs
};

struct PowerProfileData {
    PowerBackend backend        = PowerBackend::None;
    QString      activeProfile;
    QStringList  availableProfiles;
    QString      scalingDriver;
    QString      conflictWarning;
};

class NEXISCORESHARED_EXPORT PowerProfileInfo
{
public:
    virtual ~PowerProfileInfo() = default;

    virtual void refresh() = 0;
    virtual bool setProfile(const QString &profile) = 0;

    PowerProfileData getData() const;
    bool hasProfiles() const;

    static PowerProfileData parsePowerprofilesctlList(const QString &output);
    static PowerProfileData parseSysfsGovernors(const QString &availableGovernors,
                                                const QString &currentGovernor,
                                                const QString &scalingDriver);
    static QString userLabelToBackendValue(const QString &label, PowerBackend backend);
    static QString backendValueToUserLabel(const QString &value, PowerBackend backend);

protected:
    PowerProfileData mData;
};

#endif // POWER_PROFILE_INFO_H
