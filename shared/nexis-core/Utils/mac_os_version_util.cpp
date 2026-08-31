#include "mac_os_version_util.h"

QVersionNumber MacOsVersionUtil::toVersionNumber(const QOperatingSystemVersion &osVersion)
{
    return QVersionNumber(osVersion.majorVersion(), osVersion.minorVersion(), osVersion.microVersion());
}

QVersionNumber MacOsVersionUtil::current()
{
    return toVersionNumber(QOperatingSystemVersion::current());
}
