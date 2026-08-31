#ifndef MAC_OS_VERSION_UTIL_H
#define MAC_OS_VERSION_UTIL_H

// SSO-23857: per-macOS-version gating for the Tweaks pane. toVersionNumber()
// is a pure conversion (unit-testable on any platform by constructing a
// QOperatingSystemVersion by hand); current() wraps the live OS query and is
// only meaningful when actually running on macOS.

#include <QOperatingSystemVersion>
#include <QVersionNumber>

#include "nexis-core_global.h"

class NEXISCORESHARED_EXPORT MacOsVersionUtil
{
public:
    static QVersionNumber toVersionNumber(const QOperatingSystemVersion &osVersion);
    static QVersionNumber current();
};

#endif // MAC_OS_VERSION_UTIL_H
