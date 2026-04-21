#include "plist_util.h"

#include "Utils/command_util.h"

#include <QDebug>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace PlistUtil {

AppBundleInfo readAppBundleInfo(const QString &appPath)
{
    AppBundleInfo info;

    const QString plistPath = appPath + "/Contents/Info.plist";
    if (!QFileInfo::exists(plistPath))
        return info;

    try {
        const QString json = CommandUtil::exec("/usr/bin/plutil",
            {"-convert", "json", "-o", "-", plistPath}).trimmed();
        const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
        if (doc.isNull())
            return info;

        const QJsonObject obj = doc.object();
        info.bundleId    = obj.value("CFBundleIdentifier").toString();
        info.displayName = obj.value("CFBundleDisplayName").toString();
        if (info.displayName.isEmpty())
            info.displayName = obj.value("CFBundleName").toString();
        info.version     = obj.value("CFBundleShortVersionString").toString();
    } catch (...) {
        qWarning() << "Failed to parse Info.plist for" << appPath;
    }

    return info;
}

} // namespace PlistUtil
