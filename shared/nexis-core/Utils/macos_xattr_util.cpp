#include "macos_xattr_util.h"

#include <QtGlobal>

#ifdef Q_OS_MACOS
#include <sys/xattr.h>
#include <errno.h>
#include <QDateTime>
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

bool MacOsXattrUtil::setQuarantine(const QString &path)
{
#ifdef Q_OS_MACOS
    // Apple's com.apple.quarantine xattr format is undocumented but stable
    // in practice: "<flags-hex>;<timestamp-hex>;<agent>;<event-uuid>". 0081
    // is the flag combination browsers use for "downloaded from the
    // internet" — it is what makes Gatekeeper run its first-launch check and
    // LaunchServices apply app translocation. The timestamp/agent/UUID
    // fields are metadata only; Gatekeeper does not require them to be
    // populated to honor the flags.
    const QByteArray value = "0081;" +
        QByteArray::number(QDateTime::currentSecsSinceEpoch(), 16) + ";Nexis;";
    const QByteArray local = path.toLocal8Bit();
    if (setxattr(local.constData(), "com.apple.quarantine", value.constData(),
                 static_cast<size_t>(value.size()), 0, 0) == 0) {
        return true;
    }
    qWarning() << "MacOsXattrUtil::setQuarantine: setxattr failed for"
               << path << "errno=" << errno;
    return false;
#else
    Q_UNUSED(path);
    return true;
#endif
}
