#ifndef MACOS_XATTR_UTIL_H
#define MACOS_XATTR_UTIL_H

#include <QString>

#include "nexis-core_global.h"

// SSO-3731 (FW-04, MX1): macOS 27 (Golden Gate) refuses to load launchd plists
// that carry the `com.apple.quarantine` extended attribute. Any plist Nexis
// writes under ~/Library/LaunchAgents must have the attribute removed before
// `launchctl load` is invoked, otherwise the agent silently fails to load.
class NEXISCORESHARED_EXPORT MacOsXattrUtil
{
public:
    // Removes the `com.apple.quarantine` xattr from `path`.
    //
    // Returns true if the attribute was removed or was already absent. Returns
    // false on any other error (permission denied, path does not exist, etc.).
    // No-op on non-macOS builds — always returns true.
    static bool stripQuarantine(const QString &path);

private:
    MacOsXattrUtil() = default;
};

#endif // MACOS_XATTR_UTIL_H
