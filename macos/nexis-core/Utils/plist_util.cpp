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

    const ExecResult result = CommandUtil::execWithStatus("/usr/bin/plutil",
        {"-convert", "json", "-o", "-", plistPath});
    if (!result.ok()) {
        qWarning() << "plist_util: plutil failed for" << appPath << ":" << result.error;
        return info;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(result.output.trimmed().toUtf8());
    if (doc.isNull())
        return info;

    const QJsonObject obj = doc.object();
    info.bundleId      = obj.value("CFBundleIdentifier").toString();
    info.displayName   = obj.value("CFBundleDisplayName").toString();
    if (info.displayName.isEmpty())
        info.displayName = obj.value("CFBundleName").toString();
    info.version       = obj.value("CFBundleShortVersionString").toString();
    info.suFeedUrl     = obj.value("SUFeedURL").toString();
    info.suPublicEDKey = obj.value("SUPublicEDKey").toString();

    return info;
}

} // namespace PlistUtil
