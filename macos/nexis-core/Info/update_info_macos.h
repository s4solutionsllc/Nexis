#ifndef UPDATE_INFO_MACOS_H
#define UPDATE_INFO_MACOS_H

#include <Info/update_info.h>

class UpdateInfoMacOS : public UpdateInfo
{
public:
    UpdateCheckResult checkForUpdates() override;
    QStringList availableSources() const override;
};

#endif // UPDATE_INFO_MACOS_H
