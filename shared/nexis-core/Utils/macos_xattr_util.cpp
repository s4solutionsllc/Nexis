#include "macos_xattr_util.h"

#include <QtGlobal>

#ifdef Q_OS_MACOS
#include <sys/xattr.h>
#include <errno.h>
#include <QDebug>
#endif

bool MacOsXattrUtil::stripQuarantine(const QString &path)
{
#ifdef Q_OS_MACOS
    const QByteArray local = path.toLocal8Bit();
    if (removexattr(local.constData(), "com.apple.quarantine", XATTR_NOFOLLOW) == 0) {
        return true;
    }
    // ENOATTR: attribute wasn't there — idempotent success.
    if (errno == ENOATTR) {
        return true;
    }
    qWarning() << "MacOsXattrUtil::stripQuarantine: removexattr failed for"
               << path << "errno=" << errno;
    return false;
#else
    Q_UNUSED(path);
    return true;
#endif
}
